#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
//semaphore structure
typedef struct {
    int value;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
} semaphore;

// Semaphore operations
void P(semaphore *s) {
    pthread_mutex_lock(&s->mtx);
    while (s->value <= 0) {
        pthread_cond_wait(&s->cv, &s->mtx);
    }
    s->value--;
    pthread_mutex_unlock(&s->mtx);
}

void V(semaphore *s) {
    pthread_mutex_lock(&s->mtx);
    s->value++;
    pthread_cond_signal(&s->cv);
    pthread_mutex_unlock(&s->mtx);
}

// Global variables
semaphore boat_sem[20] ; 
semaphore rider_sem = {0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};
pthread_mutex_t bntx = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t EOS;

int m, n; // Number of boats and visitors
int *BA;  // Boat availability array
int *BC;  // Boat visitor array
int *BT;  // Boat ride time array
pthread_barrier_t *BB; // Boat-specific barriers

int visitors_remaining; // Track remaining visitors

// Boat thread function
void* boat_thread(void* arg) {
    int id = *((int*)arg);
    printf("Boat\t\t%3d\t\tReady\n", id+1);
    // Mark the boat as available and wait for a rider
    pthread_mutex_lock(&bntx);
    BA[id] = 1;
    BC[id] = -1;
    pthread_mutex_unlock(&bntx);

    while (1) {
        // Signal that the boat is available
        V(&rider_sem);
        // printf("Boat %3d: Signaling rider semaphore\n", id);
        // Wait for a rider
        P(&boat_sem[id]);
        if(visitors_remaining==0 ) break;
        // printf("Boat %3d: Received signal from rider semaphore\n", id);
        // Read the ride time and simulate the ride
        pthread_mutex_lock(&bntx);
        int ride_time = BT[id];
        BA[id] = 0;
        int rider=BC[id];
        pthread_mutex_unlock(&bntx);
        // Wait for a rider to join the barrier
        // printf("Boat %3d: Waiting for rider to join barrier\n", id);
        pthread_barrier_wait(&BB[id]);
        printf("Boat\t\t%3d\t\tStart of ride for visitor %3d\n", id+1, rider);
        usleep(ride_time * 100000); // Simulate ride time

        printf("Boat\t\t%3d\t\tEnd of  ride for visitor %3d\n", id+1, BC[id]);
        pthread_mutex_lock(&bntx);
        BA[id] = 1;
        BC[id] = -1;
        pthread_barrier_init(&BB[id],NULL,2);
        // Check if all visitors have completed their rides
        visitors_remaining--;
        // printf("-->Visitors Remaining\t\t%3d\n",visitors_remaining);
        if (visitors_remaining == 0) {
            pthread_mutex_unlock(&bntx);
            // printf("Boat %3d: No more visitors, synchronizing with main thread\n", id);
            break;
        }
        pthread_mutex_unlock(&bntx);
    }
    // printf("Boat\t\t%3d Waiting On Main Thread Barrier\n",id+1);
    pthread_barrier_wait(&EOS);
    // printf("Boat\t\t%3d\t\tExiting\n", id+1);
    return NULL;
}

// Visitor thread function
void* visitor_thread(void* arg) {
    int id = *((int*)arg);
    int visit_time = rand() % (120 - 30 + 1) + 30; // Random visit time (30-120 minutes)
    int ride_time = rand() % (60 - 15 + 1) + 15;   // Random ride time (15-60 minutes)

    printf("Visitor\t\t%3d\t\tStarts sightseeing for %3d minutes\n", id, visit_time);
    usleep(visit_time * 100000); // Simulate visit time

    printf("Visitor\t\t%3d\t\tReady to ride a boat (ride time = %3d)\n", id, ride_time);

    P(&rider_sem);
    // Signal that the visitor is ready
    // printf("Visitor %3d: Signaling boat semaphore\n", id);

    // Wait for a boat
    // printf("Visitor %3d: Received signal from rider semaphore\n", id);

    // Find an available boat
    int boat_id = -1;
    while (1) {
        pthread_mutex_lock(&bntx);
        for (int i = 0; i < m; i++) {
            if (BA[i] == 1 && BC[i] == -1) {
                boat_id = i;
                BC[i] = id;
                BT[i] = ride_time;
                V(&boat_sem[i]);
                break;
            }
        }
        pthread_mutex_unlock(&bntx);

        if (boat_id != -1) {
            printf("Visitor\t\t%3d\t\tFound boat\t%3d\n", id, boat_id+1);
            break;
        }

        // Small sleep to avoid busy waiting
        usleep(10000); // 10 ms
    }

    // Join the boat's barrier to start the ride
    // printf("Visitor %3d: Joining barrier for boat %3d\n", id, boat_id);
    pthread_barrier_wait(&BB[boat_id]);
    usleep(ride_time*100000); //visitor riding boat 
    usleep(100);  //For delay in printing /avoiding parallel printing/ printing before boat statements 
    printf("Visitor\t\t%3d\t\tLeaving\n", id);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <number of boats> <number of visitors>\n", argv[0]);
        return 1;
    }

    m = atoi(argv[1]);
    n = atoi(argv[2]);
    visitors_remaining = n;

    // Initialize arrays
    BA = (int*)malloc(m * sizeof(int));
    BC = (int*)malloc(m * sizeof(int));
    BT = (int*)malloc(m * sizeof(int));
    BB = (pthread_barrier_t*)malloc(m * sizeof(pthread_barrier_t));

    for (int i = 0; i < 20; i++)
    {
        boat_sem[i]=(semaphore){0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};
    }
    
    // Initialize barriers
    for (int i = 0; i < m; i++) {
        BA[i] = 0;
        BC[i] = -1;
        pthread_barrier_init(&BB[i], NULL, 2);
    }

    // Initialize the end-of-session barrier
    pthread_barrier_init(&EOS, NULL, m+1);

    //Initialize Time
    srand(time(NULL));
    // Create boat threads
    pthread_t boats[m];
    int boat_ids[m];
    for (int i = 0; i < m; i++) {
        boat_ids[i] = i;
        pthread_create(&boats[i], NULL, boat_thread, &boat_ids[i]);
    }

    // Create visitor threads
    pthread_t visitors[n];
    int visitor_ids[n];
    for (int i = 0; i < n; i++) {
        visitor_ids[i] = i + 1;
        pthread_create(&visitors[i], NULL, visitor_thread, &visitor_ids[i]);
    }
    while(1)
    {
        if(visitors_remaining==0)
        {
            for(int i=0;i<20;i++)
            {
                V(&boat_sem[i]);
            }
            break;
        }
        usleep(100000);
    }
    
    // Wait for the last boat and visitor to finish
    pthread_barrier_wait(&EOS);

    // Clean up
    for (int i = 0; i < m; i++) {
        pthread_barrier_destroy(&BB[i]);
    }
    pthread_barrier_destroy(&EOS);
    free(BA);
    free(BC);
    free(BT);
    free(BB);

    printf("Simulation complete.\n");
    return 0;
}