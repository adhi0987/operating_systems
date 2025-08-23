#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#define PAGE_SIZE 4096
#define PAGE_TABLE_ENTRIES 2048
#define OS_RESERVED_FRAMES 4096  // 16MB / 4KB
#define TOTAL_FRAMES 16384
#define USER_FRAMES (TOTAL_FRAMES - OS_RESERVED_FRAMES)
#define MAX_KEYS 1000  // Maximum number of keys per process

typedef struct {
    int pid;
    int s;  // Search space size
    int m;  // Number of searches
    int keys[MAX_KEYS];
    int current_search;
    uint16_t page_table[PAGE_TABLE_ENTRIES];
    int allocated_frames;
    bool is_swapped;
} Process;

// Global queues and lists
int free_frames[USER_FRAMES];
int free_frames_count = 0;
Process* ready_queue[MAX_KEYS];
int ready_queue_front = 0;
int ready_queue_rear = -1;
int ready_queue_size = 0;

Process* swapped_processes[MAX_KEYS];
int swapped_processes_count = 0;

// Global statistics
int page_accesses = 0;
int page_faults = 0;
int swap_count = 0;
int degree_min = INT_MAX;
int n_processes = 0;
int completed_count = 0;

// Queue helper functions
void enqueue(Process** queue, Process* process, int* front, int* rear, int* size) {
    if (*size == MAX_KEYS) {
        fprintf(stderr, "Queue is full\n");
        exit(1);
    }
    *rear = (*rear + 1) % MAX_KEYS;
    queue[*rear] = process;
    (*size)++;
}

Process* dequeue(Process** queue, int* front, int* rear, int* size) {
    if (*size == 0) {
        fprintf(stderr, "Queue is empty\n");
        exit(1);
    }
    Process* process = queue[*front];
    *front = (*front + 1) % MAX_KEYS;
    (*size)--;
    return process;
}

void initialize_free_frames() {
    for (int i = 0; i < USER_FRAMES; i++) {
        free_frames[free_frames_count++] = OS_RESERVED_FRAMES + i;
    }
}

int pop_free_frame() {
    if (free_frames_count == 0) {
        fprintf(stderr, "No free frames available\n");
        exit(1);
    }
    return free_frames[--free_frames_count];
}

void push_free_frame(int frame) {
    if (free_frames_count >= USER_FRAMES) {
        fprintf(stderr, "Free frames list is full\n");
        exit(1);
    }
    free_frames[free_frames_count++] = frame;
}

void load_processes(const char* filename, Process* processes) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error opening file\n");
        exit(1);
    }

    int n, m;
    if (fscanf(file, "%d %d", &n, &m) != 2) {
        fprintf(stderr, "Error reading process count and search count\n");
        fclose(file);
        exit(1);
    }
    n_processes = n;

    for (int i = 0; i < n; i++) {
        Process* p = &processes[i];
        p->pid = i;
        if (fscanf(file, "%d", &p->s) != 1) {
            fprintf(stderr, "Error reading search space size\n");
            fclose(file);
            exit(1);
        }
        p->m = m;
        p->current_search = 0;
        p->allocated_frames = 0;
        p->is_swapped = false;

        for (int j = 0; j < p->m; j++) {
            if (fscanf(file, "%d", &p->keys[j]) != 1) {
                fprintf(stderr, "Error reading key\n");
                fclose(file);
                exit(1);
            }
        }

        // Initialize page table
        for (int j = 0; j < PAGE_TABLE_ENTRIES; j++) {
            p->page_table[j] = 0;
        }

        // Allocate 10 essential frames
        for (int j = 0; j < 10; j++) {
            int frame = pop_free_frame();
            p->page_table[j] = 0x8000 | frame;
            p->allocated_frames++;
        }

        enqueue(ready_queue, p, &ready_queue_front, &ready_queue_rear, &ready_queue_size);
    }
    fclose(file);
}

void simulate(Process* processes) {
    while (true) {
        if (ready_queue_size == 0) {
            if (completed_count == n_processes) break;
            fprintf(stderr, "Deadlock: no ready processes but not all completed.\n");
            exit(1);
        }

        Process* p = dequeue(ready_queue, &ready_queue_front, &ready_queue_rear, &ready_queue_size);

        if (p->current_search >= p->m) continue;
        #ifdef VERBOSE
        printf("\tSearch %d by Process %d\n",p->current_search,p->pid);
        #endif
        int key = p->keys[p->current_search];
        int s = p->s;
        int L = 0, R = s - 1;
        bool swapped_out = false;

        while (L < R && !swapped_out) {
            int M = (L + R) / 2;
            int page_num = 10 + (M / 1024);

            // Count every access to A[M]
            page_accesses++;

            if ((p->page_table[page_num] & 0x8000) == 0) {
                page_faults++;
                if (free_frames_count > 0) {
                    int frame = pop_free_frame();
                    p->page_table[page_num] = 0x8000 | frame;
                    p->allocated_frames++;
                } else {
                    swap_count++;
                    // Free all frames allocated to this process
                    for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
                        if (p->page_table[i] & 0x8000) {
                            int frame = p->page_table[i] & 0x3FFF;
                            push_free_frame(frame);
                            p->page_table[i] = 0;
                        }
                    }
                    p->allocated_frames = 0;
                    p->is_swapped = true;
                    swapped_processes[swapped_processes_count++] = p;

                    int active = n_processes - swapped_processes_count - completed_count;
                    if (active < degree_min) degree_min = active;

                    printf("+++ Swapping out process   %3d  [%2d active processes]\n", p->pid, active);
                    swapped_out = true;
                    break;
                }
            }

            if (key <= M) R = M;
            else L = M + 1;
        }

        if (!swapped_out) {
            p->current_search++;

            if (p->current_search == p->m) {
                completed_count++;
                // Free all frames when process completes
                for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
                    if (p->page_table[i] & 0x8000) {
                        int frame = p->page_table[i] & 0x3FFF;
                        push_free_frame(frame);
                        p->page_table[i] = 0;
                    }
                }
                p->allocated_frames = 0;

                // Swap in a process if any are waiting
                if (swapped_processes_count > 0) {
                    Process* q = swapped_processes[0];
                    
                    // Remove first swapped process
                    for (int i = 0; i < swapped_processes_count - 1; i++) {
                        swapped_processes[i] = swapped_processes[i + 1];
                    }
                    swapped_processes_count--;
                    
                    // Allocate 10 essential frames
                    for (int i = 0; i < 10; i++) {
                        int frame = pop_free_frame();
                        q->page_table[i] = 0x8000 | frame;
                        q->allocated_frames++;
                    }
                    q->is_swapped = false;

                    int active = n_processes - swapped_processes_count - completed_count;
                    printf("+++ Swapping out process   %3d  [%2d active processes]\n", q->pid, active);

                    // Add to ready queue to restart its search immediately
                    enqueue(ready_queue, q, &ready_queue_front, &ready_queue_rear, &ready_queue_size);
                }
            } else {
                enqueue(ready_queue, p, &ready_queue_front, &ready_queue_rear, &ready_queue_size);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    Process* processes =(Process*) malloc(sizeof(Process) * MAX_KEYS);
    
    initialize_free_frames();
    load_processes("search.txt", processes);
    
    printf("+++ Simulation data read from file\n");
    printf("+++ Kernel data initialized\n");
    
    simulate(processes);
    
    printf("+++ Page access summary\n");
    printf("\tTotal number of page accesses  =  %d\n", page_accesses);
    printf("\tTotal number of page faults    =  %d\n", page_faults);
    printf("\tTotal number of swaps          =  %d\n", swap_count);
    printf("\tDegree of multiprogramming     =  %d\n", degree_min);

    free(processes);
    return 0;
}