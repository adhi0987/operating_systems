#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

// Compile-time flag for deadlock avoidance
#ifdef _DLAVOID
#define AVOID_DEADLOCK 1
#else
#define AVOID_DEADLOCK 0
#endif

int n, m; // n threads, m resources
pthread_barrier_t BOS;          // Barrier for beginning of session
pthread_barrier_t REQB;         // Request barrier
pthread_barrier_t *ACKB;        // Array of acknowledgment barriers
pthread_mutex_t rmtx;           // Resource mutex
pthread_mutex_t pmtx;           // Print mutex
pthread_mutex_t *cond_mtx;      // Array of condition mutexes
pthread_cond_t *cond_var;       // Array of condition variables
int *done;                      // Array to track if requests are done

// Resource tracking matrices and vectors
int **need;                     // Matrix of maximum needs
int *available;                 // Vector of available resources
int **allocation;               // Matrix of current allocations
int *total;                     // Vector of total resources

// FIFO queue for pending requests
typedef struct req_node {
    int id;
    int *req;
    struct req_node *next;
} req_node;

typedef struct {
    req_node *front;
    req_node *rear;
    int size;
} request_queue;

request_queue *pending_requests;

// Request structure for communication
typedef struct {
    char type;                  // 'A' for ADDITIONAL, 'R' for RELEASE, 'Q' for QUIT
    int id;                     // Thread ID
    int *req;                   // Resource request vector
} requests;

requests *sharedmem;            // Shared memory for request communication

// Function to initialize the request queue
request_queue* init_queue() {
    request_queue *q = (request_queue*) malloc(sizeof(request_queue));
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
    return q;
}

// Function to check if queue is empty
int is_empty(request_queue *q) {
    return q->size == 0;
}

// Function to enqueue a request
void enqueue(request_queue *q, int id, int *req) {
    pthread_mutex_lock(&pmtx);
    printf("DEBUG: Enqueuing request from thread %d\n", id);
    pthread_mutex_unlock(&pmtx);
    
    req_node *new_node = (req_node*) malloc(sizeof(req_node));
    new_node->id = id;
    new_node->req = (int*) malloc(m * sizeof(int));
    memcpy(new_node->req, req, m * sizeof(int));
    new_node->next = NULL;
    
    if (is_empty(q)) {
        q->front = new_node;
        q->rear = new_node;
    } else {
        q->rear->next = new_node;
        q->rear = new_node;
    }
    q->size++;
    
    pthread_mutex_lock(&pmtx);
    printf("DEBUG: Queue size after enqueue: %d\n", q->size);
    pthread_mutex_unlock(&pmtx);
}

// Function to dequeue a request
req_node* dequeue(request_queue *q) {
    if (is_empty(q)) return NULL;
    
    req_node *temp = q->front;
    q->front = q->front->next;
    q->size--;
    
    pthread_mutex_lock(&pmtx);
    printf("DEBUG: Dequeuing request from thread %d\n", temp->id);
    printf("DEBUG: Queue size after dequeue: %d\n", q->size);
    pthread_mutex_unlock(&pmtx);
    
    if (q->size == 0) {
        q->rear = NULL;
    }
    
    return temp;
}

// Print the current state of the system
void print_system_state(const char* message) {
    pthread_mutex_lock(&pmtx);
    printf("\n--- %s ---\n", message);
    printf("Available resources: ");
    for (int i = 0; i < m; i++) {
        printf("%d ", available[i]);
    }
    printf("\n");
    
    printf("Allocation matrix:\n");
    for (int i = 0; i < n; i++) {
        printf("Thread %2d: ", i);
        for (int j = 0; j < m; j++) {
            printf("%d ", allocation[i][j]);
        }
        printf("\n");
    }
    
    printf("Need matrix:\n");
    for (int i = 0; i < n; i++) {
        printf("Thread %2d: ", i);
        for (int j = 0; j < m; j++) {
            printf("%d ", need[i][j]);
        }
        printf("\n");
    }
    
    printf("Pending requests in queue: %d\n", pending_requests->size);
    printf("---------------------\n\n");
    pthread_mutex_unlock(&pmtx);
}

// Implementation of banker's algorithm for safety check
int banker_algo(int **alloc, int **need, int *avail, int *req, int id) {
    pthread_mutex_lock(&pmtx);
    printf("DEBUG: Running banker's algorithm for thread %d\n", id);
    pthread_mutex_unlock(&pmtx);
    
    // Create temporary copies of the system state
    int *temp_avail = malloc(m * sizeof(int));
    int **temp_alloc = malloc(n * sizeof(int *));
    int **temp_need = malloc(n * sizeof(int *));
    
    for (int i = 0; i < m; i++) {
        temp_avail[i] = avail[i];
    }
    
    for (int i = 0; i < n; i++) {
        temp_alloc[i] = malloc(m * sizeof(int));
        temp_need[i] = malloc(m * sizeof(int));
        for (int j = 0; j < m; j++) {
            temp_alloc[i][j] = alloc[i][j];
            temp_need[i][j] = need[i][j];
        }
    }
    
    // Simulate allocation
    for (int i = 0; i < m; i++) {
        temp_avail[i] -= req[i];
        temp_alloc[id][i] += req[i];
        temp_need[id][i] -= req[i];
        
        // Check if this would result in negative available resources
        if (temp_avail[i] < 0) {
            pthread_mutex_lock(&pmtx);
            printf("DEBUG: Banker's algorithm - Not enough resources of type %d\n", i);
            pthread_mutex_unlock(&pmtx);
            
            // Clean up
            for (int i = 0; i < n; i++) {
                free(temp_alloc[i]);
                free(temp_need[i]);
            }
            free(temp_alloc);
            free(temp_need);
            free(temp_avail);
            return 0;
        }
    }
    
    // Check if the resulting state is safe
    int *work = malloc(m * sizeof(int));
    for (int i = 0; i < m; i++) {
        work[i] = temp_avail[i];
    }
    
    int *finish = calloc(n, sizeof(int)); // Initialize all to 0
    int finished = 0;
    
    pthread_mutex_lock(&pmtx);
    printf("DEBUG: Checking safety of resulting state\n");
    pthread_mutex_unlock(&pmtx);
    
    // Safety algorithm
    while (finished < n) {
        int found = 0;
        for (int i = 0; i < n; i++) {
            if (!finish[i]) {
                int can_finish = 1;
                for (int j = 0; j < m; j++) {
                    if (temp_need[i][j] > work[j]) {
                        can_finish = 0;
                        break;
                    }
                }
                
                if (can_finish) {
                    pthread_mutex_lock(&pmtx);
                    printf("DEBUG: Thread %d can finish in safety check\n", i);
                    pthread_mutex_unlock(&pmtx);
                    
                    for (int j = 0; j < m; j++) {
                        work[j] += temp_alloc[i][j];
                    }
                    finish[i] = 1;
                    finished++;
                    found = 1;
                }
            }
        }
        
        // If no thread can finish, the system is unsafe
        if (!found) {
            pthread_mutex_lock(&pmtx);
            printf("DEBUG: Safety check failed - system would be in unsafe state\n");
            pthread_mutex_unlock(&pmtx);
            
            // Clean up
            for (int i = 0; i < n; i++) {
                free(temp_alloc[i]);
                free(temp_need[i]);
            }
            free(temp_alloc);
            free(temp_need);
            free(temp_avail);
            free(work);
            free(finish);
            return 0;
        }
    }
    
    pthread_mutex_lock(&pmtx);
    printf("DEBUG: Safety check passed - system would be in safe state\n");
    pthread_mutex_unlock(&pmtx);
    
    // Clean up
    for (int i = 0; i < n; i++) {
        free(temp_alloc[i]);
        free(temp_need[i]);
    }
    free(temp_alloc);
    free(temp_need);
    free(temp_avail);
    free(work);
    free(finish);
    return 1;
}

// Process pending requests in the queue
void process_pending_requests() {
    pthread_mutex_lock(&pmtx);
    printf("DEBUG: Processing pending requests. Queue size: %d\n", pending_requests->size);
    pthread_mutex_unlock(&pmtx);
    
    if (is_empty(pending_requests)) return;
    
    req_node *current = pending_requests->front;
    req_node *prev = NULL;
    int requests_processed = 0;
    
    while (current != NULL) {
        int thread_id = current->id;
        int *request = current->req;
        
        pthread_mutex_lock(&pmtx);
        printf("DEBUG: Checking pending request from thread %d\n", thread_id);
        pthread_mutex_unlock(&pmtx);
        
        // Check if request can be granted
        int can_grant = 1;
        for (int i = 0; i < m; i++) {
            if (request[i] > available[i]) {
                can_grant = 0;
                break;
            }
        }
        
        // If deadlock avoidance is enabled, run banker's algorithm
        if (can_grant && AVOID_DEADLOCK) {
            can_grant = banker_algo(allocation, need, available, request, thread_id);
        }
        
        if (can_grant) {
            pthread_mutex_lock(&pmtx);
            printf("Master thread granting resource request of thread %2d\n", thread_id);
            pthread_mutex_unlock(&pmtx);
            
            // Update system state
            for (int i = 0; i < m; i++) {
                available[i] -= request[i];
                allocation[thread_id][i] += request[i];
                need[thread_id][i] -= request[i];
            }
            
            // Remove from queue
            req_node *to_remove = current;
            
            if (prev == NULL) {
                // First node in queue
                pending_requests->front = current->next;
                if (pending_requests->front == NULL) {
                    pending_requests->rear = NULL;
                }
            } else {
                prev->next = current->next;
                if (current->next == NULL) {
                    pending_requests->rear = prev;
                }
            }
            
            current = current->next;
            pending_requests->size--;
            
            // Signal the thread that its request is granted
            pthread_mutex_lock(&cond_mtx[thread_id]);
            done[thread_id] = 1;
            pthread_cond_signal(&cond_var[thread_id]);
            pthread_mutex_unlock(&cond_mtx[thread_id]);
            
            // Free the node
            free(to_remove->req);
            free(to_remove);
            
            requests_processed++;
            
            print_system_state("After granting request");
        } else {
            pthread_mutex_lock(&pmtx);
            printf("DEBUG: Cannot grant request from thread %d yet\n", thread_id);
            pthread_mutex_unlock(&pmtx);
            
            // Move to next request
            prev = current;
            current = current->next;
        }
    }
    
    pthread_mutex_lock(&pmtx);
    printf("DEBUG: Processed %d pending requests\n", requests_processed);
    pthread_mutex_unlock(&pmtx);
}

// Thread function for user threads
void *user_thread(void *arg) {
    int id = *((int *)arg);
    free(arg); // Free the memory allocated for the ID
    
    pthread_mutex_lock(&pmtx);
    printf("\tThread %2d born\n", id);
    fflush(stdout);
    pthread_mutex_unlock(&pmtx);
    
    // Wait for all threads to be ready
    pthread_barrier_wait(&BOS);
    
    pthread_mutex_lock(&pmtx);
    printf("DEBUG: Thread %d started execution after barrier\n", id);
    pthread_mutex_unlock(&pmtx);
    
    // Construct filename
    char filename[50];
    if (id < 10) {
        sprintf(filename, "./input/thread0%d.txt", id);
    } else {
        sprintf(filename, "./input/thread%d.txt", id);
    }
    
    // Open thread's input file
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        pthread_mutex_lock(&pmtx);
        printf("Error: Could not open file %s\n", filename);
        fflush(stdout);
        pthread_mutex_unlock(&pmtx);
        return NULL;
    }
    
    // Allocate memory for request vectors
    int *req = malloc(m * sizeof(int));
    int *max_req = malloc(m * sizeof(int));
    int *cur_holding = calloc(m, sizeof(int)); // Initialize to zeros
    
    // Read maximum needs
    for (int i = 0; i < m; i++) {
        fscanf(file, "%d", &max_req[i]);
        need[id][i] = max_req[i];
    }
    
    pthread_mutex_lock(&pmtx);
    printf("DEBUG: Thread %d maximum needs: ", id);
    for (int i = 0; i < m; i++) {
        printf("%d ", max_req[i]);
    }
    printf("\n");
    pthread_mutex_unlock(&pmtx);
    
    // Process requests
    int delay;
    char charii;
    char type;
    
    while (1) {
        // Read delay and request type
        fscanf(file, "%d", &delay);
        fscanf(file, " %c", &charii);
        
        pthread_mutex_lock(&pmtx);
        printf("DEBUG: Thread %d read delay: %d, type: %c\n", id, delay, charii);
        pthread_mutex_unlock(&pmtx);
        
        // Simulate delay
        usleep(delay * 1000);
        
        // Process request based on type
        if (charii == 'Q') {
            // Quit request - release all resources
            type = 'Q';
            for (int i = 0; i < m; i++) {
                req[i] = -cur_holding[i]; // Negative values for release
                cur_holding[i] = 0;
            }
            
            pthread_mutex_lock(&pmtx);
            printf("\tThread %2d preparing to quit and release all resources\n", id);
            pthread_mutex_unlock(&pmtx);
        } else {
            // Resource request (could be RELEASE or ADDITIONAL)
            type = 'R'; // Assume RELEASE initially
            for (int i = 0; i < m; i++) {
                fscanf(file, "%d", &req[i]);
                if (req[i] > 0) type = 'A'; // If any request is positive, it's ADDITIONAL
            }
            
            pthread_mutex_lock(&pmtx);
            printf("\tThread %2d sends resource request: type = %s\n", id, type == 'A' ? "ADDITIONAL" : "RELEASE");
            printf("DEBUG: Thread %d request vector: ", id);
            for (int i = 0; i < m; i++) {
                printf("%d ", req[i]);
            }
            printf("\n");
            pthread_mutex_unlock(&pmtx);
        }
        
        // Acquire resource mutex to update shared memory
        pthread_mutex_lock(&rmtx);
        
        // Copy request details to shared memory
        sharedmem->type = type;
        sharedmem->id = id;
        memcpy(sharedmem->req, req, m * sizeof(int));
        
        pthread_mutex_lock(&pmtx);
        printf("DEBUG: Thread %d waiting on REQB barrier\n", id);
        pthread_mutex_unlock(&pmtx);
        
        // Signal master thread that request is ready
        pthread_barrier_wait(&REQB);
        
        pthread_mutex_lock(&pmtx);
        printf("DEBUG: Thread %d passed REQB barrier, waiting on ACKB\n", id);
        pthread_mutex_unlock(&pmtx);
        
        // Wait for master thread to acknowledge
        pthread_barrier_wait(&ACKB[id]);
        
        pthread_mutex_lock(&pmtx);
        printf("DEBUG: Thread %d passed ACKB barrier\n", id);
        pthread_mutex_unlock(&pmtx);
        
        // Release resource mutex
        pthread_mutex_unlock(&rmtx);
        
        // If quitting, break the loop
        if (charii == 'Q') {
            pthread_mutex_lock(&pmtx);
            printf("\tThread %2d going to quit\n", id);
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);
            break;
        }
        
        // For ADDITIONAL requests, wait for grant
        if (type == 'A') {
            pthread_mutex_lock(&cond_mtx[id]);
            
            pthread_mutex_lock(&pmtx);
            printf("DEBUG: Thread %d waiting for request to be granted\n", id);
            pthread_mutex_unlock(&pmtx);
            
            while (done[id] == 0) {
                pthread_cond_wait(&cond_var[id], &cond_mtx[id]);
            }
            
            pthread_mutex_lock(&pmtx);
            printf("\tThread %2d is granted its resource request\n", id);
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);
            
            done[id] = 0; // Reset for next request
            pthread_mutex_unlock(&cond_mtx[id]);
        }
        
        // Update current holdings
        for (int i = 0; i < m; i++) {
            cur_holding[i] += req[i];
        }
        
        pthread_mutex_lock(&pmtx);
        printf("DEBUG: Thread %d current holdings: ", id);
        for (int i = 0; i < m; i++) {
            printf("%d ", cur_holding[i]);
        }
        printf("\n");
        pthread_mutex_unlock(&pmtx);
    }
    
    // Cleanup
    fclose(file);
    free(req);
    free(max_req);
    free(cur_holding);
    
    pthread_mutex_lock(&pmtx);
    printf("DEBUG: Thread %d exiting\n", id);
    pthread_mutex_unlock(&pmtx);
    
    return NULL;
}

int main() {
    pthread_mutex_lock(&pmtx);
    printf("Starting Banker's Algorithm Simulation\n");
    printf("Deadlock Avoidance: %s\n", AVOID_DEADLOCK ? "Enabled" : "Disabled");
    pthread_mutex_unlock(&pmtx);
    
    int total_finished = 0;
    
    // Initialize mutexes
    pthread_mutex_init(&rmtx, NULL);
    pthread_mutex_init(&pmtx, NULL);
    
    // Read system configuration
    FILE *file = fopen("./input/system.txt", "r");
    if (file == NULL) {
        pthread_mutex_lock(&pmtx);
        printf("Error: Could not open file ./input/system.txt\n");
        fflush(stdout);
        pthread_mutex_unlock(&pmtx);
        return 1;
    }
    
    // Read m (resources) and n (threads)
    fscanf(file, "%d", &m);
    fscanf(file, "%d", &n);
    
    pthread_mutex_lock(&pmtx);
    printf("System configuration: %d resources, %d threads\n", m, n);
    pthread_mutex_unlock(&pmtx);
    
    // Allocate memory for resource tracking
    total = malloc(m * sizeof(int));
    available = malloc(m * sizeof(int));
    allocation = malloc(n * sizeof(int *));
    need = malloc(n * sizeof(int *));
    
    for (int i = 0; i < n; i++) {
        allocation[i] = calloc(m, sizeof(int)); // Initialize to zeros
        need[i] = malloc(m * sizeof(int));
    }
    
    // Read total resources
    for (int i = 0; i < m; i++) {
        fscanf(file, "%d", &total[i]);
        available[i] = total[i]; // Initially all resources are available
    }
    
    pthread_mutex_lock(&pmtx);
    printf("Total resources: ");
    for (int i = 0; i < m; i++) {
        printf("%d ", total[i]);
    }
    printf("\n");
    pthread_mutex_unlock(&pmtx);
    
    fclose(file);
    
    // Initialize request queue
    pending_requests = init_queue();
    
    // Initialize shared memory for request communication
    sharedmem = malloc(sizeof(requests));
    sharedmem->req = malloc(m * sizeof(int));
    
    // Initialize synchronization primitives
    pthread_barrier_init(&BOS, NULL, n + 1); // n threads + master
    pthread_barrier_init(&REQB, NULL, 2);    // For request handshaking
    
    ACKB = malloc(n * sizeof(pthread_barrier_t));
    cond_mtx = malloc(n * sizeof(pthread_mutex_t));
    cond_var = malloc(n * sizeof(pthread_cond_t));
    done = calloc(n, sizeof(int)); // Initialize to zeros
    
    for (int i = 0; i < n; i++) {
        pthread_barrier_init(&ACKB[i], NULL, 2);
        pthread_mutex_init(&cond_mtx[i], NULL);
        pthread_cond_init(&cond_var[i], NULL);
    }
    
    pthread_mutex_lock(&pmtx);
    printf("Synchronization primitives initialized\n");
    pthread_mutex_unlock(&pmtx);
    
    // Create user threads
    pthread_t *th = malloc(n * sizeof(pthread_t));
    for (int i = 0; i < n; i++) {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&th[i], NULL, user_thread, (void *)id);
    }
    
    pthread_mutex_lock(&pmtx);
    printf("All threads created, waiting at barrier\n");
    pthread_mutex_unlock(&pmtx);
    
    // Wait for all threads to be ready
    pthread_barrier_wait(&BOS);
    
    pthread_mutex_lock(&pmtx);
    printf("All threads synchronized, starting simulation\n");
    print_system_state("Initial State");
    pthread_mutex_unlock(&pmtx);
    
    // Main processing loop
    char next_req_type;
    int next_req_id;
    int *next_req = malloc(m * sizeof(int));
    
    while (1) {
        pthread_mutex_lock(&pmtx);
        printf("DEBUG: Master waiting for next request\n");
        pthread_mutex_unlock(&pmtx);
        
        // Wait for a request
        pthread_barrier_wait(&REQB);
        
        // Read request details
        next_req_type = sharedmem->type;
        next_req_id = sharedmem->id;
        memcpy(next_req, sharedmem->req, m * sizeof(int));
        
        pthread_mutex_lock(&pmtx);
        printf("DEBUG: Master received request: type=%c, id=%d\n", next_req_type, next_req_id);
        printf("DEBUG: Request vector: ");
        for (int i = 0; i < m; i++) {
            printf("%d ", next_req[i]);
        }
        printf("\n");
        pthread_mutex_unlock(&pmtx);
        
        // Signal acknowledgment
        pthread_barrier_wait(&ACKB[next_req_id]);
        
        // Process request based on type
        if (next_req_type == 'Q') {
            // Handle QUIT request
            total_finished++;
            
            pthread_mutex_lock(&pmtx);
            printf("Master thread releases resources of thread %2d\n", next_req_id);
            pthread_mutex_unlock(&pmtx);
            
            // Return all resources held by the thread
            for (int i = 0; i < m; i++) {
                available[i] += allocation[next_req_id][i];
                allocation[next_req_id][i] = 0;
                need[next_req_id][i] = 0;
            }
            
            pthread_mutex_lock(&pmtx);
            printf("%d threads left\n", n - total_finished);
            printf("Available resources: ");
            for (int i = 0; i < m; i++) {
                printf("%d ", available[i]);
            }
            printf("\n");
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);
            
            // Process any pending requests that might now be satisfiable
            process_pending_requests();
            
            // Check if all threads have finished
            if (total_finished == n) {
                pthread_mutex_lock(&pmtx);
                printf("All threads have finished. Exiting.\n");
                pthread_mutex_unlock(&pmtx);
                break;
            }
        } else if (next_req_type == 'R') {
            // Handle RELEASE request
            pthread_mutex_lock(&pmtx);
            printf("Master thread processing RELEASE request from thread %2d\n", next_req_id);
            pthread_mutex_unlock(&pmtx);
            
            // Update system state - release resources
            for (int i = 0; i < m; i++) {
                if (next_req[i] < 0) {
                    available[i] -= next_req[i]; // Subtract negative value = add
                    allocation[next_req_id][i] += next_req[i]; // Add negative value = subtract
                }
            }
            
            print_system_state("After RELEASE request");
            
            // Process any pending requests that might now be satisfiable
            process_pending_requests();
        } else if (next_req_type == 'A') {
            // Handle ADDITIONAL request
            pthread_mutex_lock(&pmtx);
            printf("Master thread stores resource request of thread %2d\n", next_req_id);
            pthread_mutex_unlock(&pmtx);
            
            // Process release components first (negative values)
            for (int i = 0; i < m; i++) {
                if (next_req[i] < 0) {
                    available[i] -= next_req[i]; // Subtract negative value = add
                    allocation[next_req_id][i] += next_req[i]; // Add negative value = subtract
                    next_req[i] = 0; // Set to zero after processing
                }
            }
            
            // Enqueue the request for additional resources
            enqueue(pending_requests, next_req_id, next_req);
            
            // Try to process pending requests
            process_pending_requests();
        }
    }
    
    // Join all threads
    for (int i = 0; i < n; i++) {
        pthread_join(th[i], NULL);
    }
    
    pthread_mutex_lock(&pmtx);
    printf("All threads joined, cleaning up\n");
    pthread_mutex_unlock(&pmtx);
    
    // Cleanup
    // Free synchronization primitives
    pthread_barrier_destroy(&BOS);
    pthread_barrier_destroy(&REQB);
    pthread_mutex_destroy(&rmtx);
    pthread_mutex_destroy(&pmtx);
    
    for (int i = 0; i < n; i++) {
        pthread_barrier_destroy(&ACKB[i]);
        pthread_mutex_destroy(&cond_mtx[i]);
        pthread_cond_destroy(&cond_var[i]);
    }
    
    // Free request queue
    while (!is_empty(pending_requests)) {
        req_node *node = dequeue(pending_requests);
        free(node->req);
        free(node);
    }
    free(pending_requests);
    
    // Free arrays and matrices
    free(sharedmem->req);
    free(sharedmem);
    free(ACKB);
    free(cond_mtx);
    free(cond_var);
    free(done);
    free(total);
    free(available);
    free(next_req);
    free(th);
    
    for (int i = 0; i < n; i++) {
        free(allocation[i]);
        free(need[i]);
    }
    free(allocation);
    free(need);
    
    printf("Simulation complete\n");
    return 0;
}