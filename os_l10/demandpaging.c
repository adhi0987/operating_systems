#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Define constants as given in the assignment
#define TOTAL_MEMORY (64 * 1024 * 1024)       // 64 MB
#define PAGE_SIZE (4 * 1024)                  // 4 KB
#define OS_USAGE (16 * 1024 * 1024)           // 16 MB
#define USER_SPACE (TOTAL_MEMORY - OS_USAGE)  // 48 MB
#define NUM_USER_FRAMES (USER_SPACE / PAGE_SIZE)  // 12288
#define MAX_PROCESSES 500
#define MIN_PROCESSES 50
#define MAX_SEARCHES 100
#define MIN_SEARCHES 10
#define VM_SIZE 2048                          // Virtual memory size in pages
#define ESSENTIAL_PAGES 10                    // Pages 0-9 are essential
#define NFFMIN 1000                           // Minimum number of free frames

// Uncomment to enable verbose output
// #define VERBOSE

// Structure to represent a free frame in the FFLIST
typedef struct {
    int frame_number;  // Frame number
    int last_owner;    // PID of the last owner (-1 if none)
    int page_number;   // Page number of the last owner (-1 if none)
} FreeFrame;

typedef struct {
    unsigned short int pageTableEntry;      // Page table entry (valid bit, reference bit, frame number)
    unsigned short int history;  // For LRU approximation
} PageTableEntry;

typedef struct {
    PageTableEntry pages[2048];  // 2048 pages per process
} PageTable;

// Structure to represent a process
typedef struct {
    int pid;                     // Process ID (0 to n-1)
    int array_size;              // Size of array A
    int search_keys[MAX_SEARCHES];      // Search keys for binary searches
    int searches_completed;      // Number of searches completed
    int current_search;          // Current search in progress
    PageTable page_table;        // Page table for this process
    int num_frames_allocated;    // Number of frames currently allocated
    int num_page_accesses;
    int num_page_faults;
    int num_page_replacements[4]; 
} PCB;

// Global variables
FreeFrame* FFLIST;           // Free frame list
int FFLIST_size;             // Current size of FFLIST
int NFF;                     // Number of free frames
PCB processes[MAX_PROCESSES]; // Array of processes
int n, m;                    // Number of processes and searches
int ready_queue[MAX_PROCESSES]; // Simple array queue for round robin
int queue_front, queue_rear;    // Queue pointers

// Function prototypes
void read_input();
void initialize_page_tables();
void initialize_fflist();
void allocate_essential_pages();
int victim_page_identification(PCB *process);
int victim_frame_identification(PCB *process, int page_number);
void handle_page_fault(PCB *process, int page_number);
void release_process_memory(int pid);
void simulate_round_robin();
void print_stats();

// Queue functions
void init_queue() {
    queue_front = queue_rear = -1;
}

int is_queue_empty() {
    return queue_front == -1;
}

void enqueue(int pid) {
    if (queue_front == -1)
        queue_front = 0;
    queue_rear = (queue_rear + 1) % MAX_PROCESSES;
    ready_queue[queue_rear] = pid;
}

int dequeue() {
    if (is_queue_empty())
        return -1;
    
    int pid = ready_queue[queue_front];
    
    if (queue_front == queue_rear)
        queue_front = queue_rear = -1;
    else
        queue_front = (queue_front + 1) % MAX_PROCESSES;
    
    return pid;
}

// Function to add a frame to FFLIST
void add_frame_to_fflist(FreeFrame frame) {
    FFLIST[FFLIST_size++] = frame;
}

// Function to remove a frame from FFLIST at a specific index
void remove_frame_from_fflist(int index) {
    if (index >= 0 && index < FFLIST_size) {
        // Shift all elements after index to the left
        for (int i = index; i < FFLIST_size - 1; i++) {
            FFLIST[i] = FFLIST[i + 1];
        }
        FFLIST_size--;
    }
}

void read_input() {
    FILE *infile = fopen("search.txt", "r");
    if (!infile) {
        fprintf(stderr, "Error: Cannot open search.txt\n");
        exit(1);
    }
    
    fscanf(infile, "%d %d", &n, &m);
    
    if (n < MIN_PROCESSES || n > MAX_PROCESSES) {
        fprintf(stderr, "Error: Number of processes should be between %d and %d\n", MIN_PROCESSES, MAX_PROCESSES);
        exit(1);
    }
    
    if (m < MIN_SEARCHES || m > MAX_SEARCHES) {
        fprintf(stderr, "Error: Number of searches should be between %d and %d\n", MIN_SEARCHES, MAX_SEARCHES);
        exit(1);
    }

    // Read search keys for each process
    for (int i = 0; i < n; i++) {
        processes[i].pid = i;
        fscanf(infile, "%d", &processes[i].array_size);  // Read the array size
        for (int j = 0; j < m; j++) {
            fscanf(infile, "%d", &processes[i].search_keys[j]);  // Read search keys
        }
        processes[i].searches_completed = 0;  // Initialize searches completed
        processes[i].current_search = 0;      // Initialize current search index
        processes[i].num_frames_allocated = 0;
        processes[i].num_page_accesses = 0;
        processes[i].num_page_faults = 0;
        processes[i].num_page_replacements[0] = 0;
        processes[i].num_page_replacements[1] = 0;
        processes[i].num_page_replacements[2] = 0;
        processes[i].num_page_replacements[3] = 0;
    }

    fclose(infile);
}

void initialize_page_tables() {
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < VM_SIZE; ++j) {
            processes[i].page_table.pages[j].pageTableEntry = 0x0000; // valid bit = 0, ref bit = 0, frame = 0
            processes[i].page_table.pages[j].history = 0;
        }
    }
}

// Function to initialize the FFLIST with all user frames
void initialize_fflist() {
    FFLIST = (FreeFrame*)malloc(NUM_USER_FRAMES * sizeof(FreeFrame));
    if (!FFLIST) {
        fprintf(stderr, "Error: Memory allocation failed for FFLIST\n");
        exit(1);
    }
    
    FFLIST_size = 0;
    for (int i = 0; i < NUM_USER_FRAMES; ++i) {
        FreeFrame f;
        f.frame_number = i;
        f.last_owner = -1;
        f.page_number = -1;
        add_frame_to_fflist(f);
    }
    NFF = NUM_USER_FRAMES;
}

void allocate_essential_pages() {
    for (int i = 0; i < n; ++i) {
        for (int page_num = 0; page_num < ESSENTIAL_PAGES; ++page_num) {
            if (FFLIST_size == 0) {
                fprintf(stderr, "Error: Not enough free frames.\n");
                exit(1);
            }

            // Allocate the first available frame
            FreeFrame f = FFLIST[0];
            remove_frame_from_fflist(0); // Remove from free list

            // Assign ownership
            f.last_owner = processes[i].pid;
            f.page_number = page_num;

            // Update page table
            processes[i].page_table.pages[page_num].pageTableEntry = (1 << 15) | (0 << 14) | f.frame_number;
            processes[i].page_table.pages[page_num].history = 0xFFFF;

            processes[i].num_frames_allocated++;
            NFF--;  // Frame allocated
        }
    }
}

int victim_page_identification(PCB *process) {
    int victim_page = -1;
    int victim_history = 0xFFFF; // Max possible history value

    for (int i = 10; i < 2048; i++) {  // Ignore essential pages 0-9
        if ((process->page_table.pages[i].pageTableEntry & 0x8000) &&  // Valid bit is 1
            process->page_table.pages[i].history < victim_history) {
            victim_page = i;
            victim_history = process->page_table.pages[i].history;
        }
    }

    return victim_page;  // Return victim page number
}

int victim_frame_identification(PCB *process, int page_number) {
    int free_frame = -1;
    int free_index = -1;
    int attempt_type = -1;

    // Rule 1: Check for a frame with same process as last owner and requested page
    for (int i = 0; i < FFLIST_size; i++) {
        if (FFLIST[i].last_owner == process->pid && FFLIST[i].page_number == page_number) {
            free_frame = FFLIST[i].frame_number;
            free_index = i;
            process->num_page_replacements[0]++;
            attempt_type = 1;
            break;
        }
    }

    // Rule 2: Check for a frame with no owner
    if (free_frame == -1) {
        for (int i = 0; i < FFLIST_size; i++) {
            if (FFLIST[i].last_owner == -1) {
                free_frame = FFLIST[i].frame_number;
                free_index = i;
                process->num_page_replacements[1]++;
                attempt_type = 2;
                break;
            }
        }
    }

    // Rule 3: Check for a frame with the same process as last owner (any page)
    if (free_frame == -1) {
        for (int i = 0; i < FFLIST_size; i++) {
            if (FFLIST[i].last_owner == process->pid) {
                free_frame = FFLIST[i].frame_number;
                free_index = i;
                process->num_page_replacements[2]++;
                attempt_type = 3;
                break;
            }
        }
    }

    // Rule 4: Pick a random free frame
    if (free_frame == -1) {
        free_index = rand() % FFLIST_size;
        free_frame = FFLIST[free_index].frame_number;
        process->num_page_replacements[3]++;
        attempt_type = 4;
    }

#ifdef VERBOSE
    printf("        Attempt %d: Free frame %d ", attempt_type, FFLIST[free_index].frame_number);
    
    if (FFLIST[free_index].last_owner == -1) {
        printf("owned by no process found\n");
    } else {
        printf("owned by process %d page %d found\n", FFLIST[free_index].last_owner, 
               FFLIST[free_index].page_number);
    }
#endif

    return free_index;
}

void handle_page_fault(PCB *process, int page_number) {
    int frame_number;

#ifdef VERBOSE
    printf("    Fault on Page %4d: ", page_number);
#endif

    // Case 1: Free frame available (NFF > NFFMIN), allocate directly
    if (NFF > NFFMIN) {
        // Allocate the first available frame
        FreeFrame f = FFLIST[0];
        remove_frame_from_fflist(0);  // Remove from free list

#ifdef VERBOSE
        printf("Free frame %d found\n", f.frame_number);
#endif

        // Assign ownership
        f.last_owner = process->pid;
        f.page_number = page_number;

        // Update page table
        process->page_table.pages[page_number].pageTableEntry = (1 << 15) | (0 << 14) | f.frame_number;
        process->page_table.pages[page_number].history = 0xFFFF;

        process->num_frames_allocated++;
        NFF--;  // Frame allocated
    } 
    // Case 2: Page replacement needed (NFF == NFFMIN)
    else {
        int victim_page = victim_page_identification(process);
        if (victim_page == -1) {
            printf("Error: No victim page found!\n");
            return;
        }

        frame_number = process->page_table.pages[victim_page].pageTableEntry & 0x1FFF; // Get victim frame number

#ifdef VERBOSE
        printf("To replace Page %3d at Frame %d [history = %d]\n", 
               victim_page, frame_number, process->page_table.pages[victim_page].history);
#endif

        // Invalidate victim page entry
        process->page_table.pages[victim_page].pageTableEntry &= 0x7FFF; // Clear valid bit

        // Find the best free frame using victim_frame_identification
        int victim_frame_index = victim_frame_identification(process, page_number);
        if (victim_frame_index == -1) {
            printf("Error: No suitable victim frame found!\n");
            return;
        }

        FreeFrame f = FFLIST[victim_frame_index];
        remove_frame_from_fflist(victim_frame_index);  // Remove from free list

        // Assign ownership
        f.last_owner = process->pid;
        f.page_number = page_number;

        // Update page table
        process->page_table.pages[page_number].pageTableEntry = (1 << 15) | (0 << 14) | f.frame_number;
        process->page_table.pages[page_number].history = 0xFFFF;

        process->num_frames_allocated++;

        // Add freed frame to FFLIST
        FreeFrame freed_frame;
        freed_frame.frame_number = frame_number;
        freed_frame.last_owner = process->pid;
        freed_frame.page_number = victim_page;
        add_frame_to_fflist(freed_frame);
    }
}

void release_process_memory(int pid) {
    PCB *proc = &processes[pid];

    for (int i = 0; i < VM_SIZE; i++) {
        if (proc->page_table.pages[i].pageTableEntry & (1 << 15)) { // If page is valid
            int frame_num = proc->page_table.pages[i].pageTableEntry & 0x3FFF; // Extract frame number
            
            // Find the frame in FFLIST and mark it as free
            FreeFrame f;
            f.frame_number = frame_num;
            f.last_owner = -1;
            f.page_number = -1;
            add_frame_to_fflist(f); // Add back to free list

            // Invalidate the page table entry
            proc->page_table.pages[i].pageTableEntry = 0;
        }
    }

    // Increase available frame count
    NFF += proc->num_frames_allocated;
    proc->num_frames_allocated = 0;
}

void simulate_round_robin() {
    init_queue();
    
    // Initialize the queue with all processes
    for (int i = 0; i < n; i++) {
        enqueue(i);
    }

    while (!is_queue_empty()) {
        int pid = dequeue();
        PCB *proc = &processes[pid];

        if (proc->searches_completed >= m) {
            // Process has finished all searches, release its memory and continue
            release_process_memory(pid);
            continue;
        }

#ifdef VERBOSE
        printf("+++ Process %d: Search %d\n", pid, proc->current_search);
#endif

        int x = proc->search_keys[proc->current_search];  // Search key
        int L = 0, R = proc->array_size - 1;
        int M;

        while (L <= R) {
            M = (L + R) / 2;

            // Memory access at A[M] → calculate the page number
            int page_num = 10 + (M / (PAGE_SIZE / sizeof(int)));
            proc->num_page_accesses++;

            // Handle page fault if needed
            if (!(proc->page_table.pages[page_num].pageTableEntry & (1 << 15))) {
                handle_page_fault(proc, page_num);
                proc->num_page_faults++;
            }

            // Set the reference bit (bit 14)
            proc->page_table.pages[page_num].pageTableEntry |= (1 << 14);

            // Correct binary search logic
            // Since array A[i] = i, we're comparing x with M directly
            if (x < M) {
                R = M;  // If key is less than M, search left half
            } else if (x > M) {
                L = M + 1;  // If key is greater than M, search right half
            } else {
                break;  // Found the key, exit binary search
            }
        }

        // Search completed
        proc->searches_completed++;
        proc->current_search++;

        // History Update: Shift history for all valid pages
        for (int i = 10; i < 2048; i++) {  // Ignore pages 0-9
            if (proc->page_table.pages[i].pageTableEntry & (1 << 15)) { // If page is valid
                proc->page_table.pages[i].history >>= 1; // Shift history

                // Set MSB (bit 15) to reference bit (bit 14)
                if (proc->page_table.pages[i].pageTableEntry & (1 << 14)) {
                    proc->page_table.pages[i].history |= (1 << 15);
                }

                // Clear reference bit (bit 14)
                proc->page_table.pages[i].pageTableEntry &= ~(1 << 14);
            }
        }

        if (proc->searches_completed < m) {
            enqueue(pid); // Re-add to queue if searches remain
        } else {
            // Last search completed → Release memory
            release_process_memory(pid);
        }
    }
}

void print_stats() {
    printf("+++ Page access summary\n");
    printf("PID    Accesses        Faults      Replacements            Attempts\n");
    int total_accesses = 0;
    int total_page_faults = 0;
    int total_page_replacements = 0;
    int casewise_page_replacements[4] = {0, 0, 0, 0};
    int per_process_replacements;
    double faults_percent;
    double replacement_percent;
    double per_process_per_type_replacement_percent[4] = {0, 0, 0, 0};
    
    for(int i = 0; i < n; i++) {
        per_process_replacements = 0;
        for(int j = 0; j < 4; j++) {
            per_process_replacements += processes[i].num_page_replacements[j];
            casewise_page_replacements[j] += processes[i].num_page_replacements[j];
        }
        
        total_accesses += processes[i].num_page_accesses;
        total_page_faults += processes[i].num_page_faults;
        total_page_replacements += per_process_replacements;
        
        faults_percent = (processes[i].num_page_faults / (double)processes[i].num_page_accesses) * 100;
        replacement_percent = (per_process_replacements / (double)processes[i].num_page_accesses) * 100;
        
        for(int j = 0; j < 4; j++) {
            if (per_process_replacements > 0) {
                per_process_per_type_replacement_percent[j] = (processes[i].num_page_replacements[j] / (double)per_process_replacements) * 100;
            } else {
                per_process_per_type_replacement_percent[j] = 0.0;
            }
        }
        
        printf("%3d      %5d     %d (%.2f%%)      %d (%.2f%%)     %3d + %3d + %3d + %3d (%.2f%% + %.2f%% + %.2f%% + %.2f%%)\n",
               i, 
               processes[i].num_page_accesses,
               processes[i].num_page_faults, faults_percent,
               per_process_replacements, replacement_percent,
               processes[i].num_page_replacements[0],
               processes[i].num_page_replacements[1],
               processes[i].num_page_replacements[2],
               processes[i].num_page_replacements[3],
               per_process_per_type_replacement_percent[0],
               per_process_per_type_replacement_percent[1],
               per_process_per_type_replacement_percent[2],
               per_process_per_type_replacement_percent[3]);
    }
    
    // Print totals
    double total_faults_percent = (total_page_faults / (double)total_accesses) * 100;
    double total_replacement_percent = (total_page_replacements / (double)total_accesses) * 100;
    double total_per_type_percent[4];
    
    for(int j = 0; j < 4; j++) {
        total_per_type_percent[j] = (casewise_page_replacements[j] / (double)total_page_replacements) * 100;
    }
    
    printf("Total %d %d (%.2f%%) %d (%.2f%%) %d + %d + %d + %d (%.2f%% + %.2f%% + %.2f%% + %.2f%%)\n",
           total_accesses,
           total_page_faults, total_faults_percent,
           total_page_replacements, total_replacement_percent,
           casewise_page_replacements[0],
           casewise_page_replacements[1],
           casewise_page_replacements[2],
           casewise_page_replacements[3],
           total_per_type_percent[0],
           total_per_type_percent[1],
           total_per_type_percent[2],
           total_per_type_percent[3]);
}

int main() {
    // Seed the random number generator for Rule 4 in victim_frame_identification
    srand(time(NULL));

    read_input();
    initialize_page_tables();
    initialize_fflist();
    allocate_essential_pages();
    printf("simulation started\n");
    simulate_round_robin();
    printf("simulation ended\n");
    print_stats();
    
    // Free allocated memory
    free(FFLIST);
    
    return 0;
}