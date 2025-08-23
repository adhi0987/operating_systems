#include <iostream>
#include <fstream>
#include <cstdint>
#include <queue>
#include <list>
#include <climits>

using namespace std;

const int PAGE_SIZE = 4096;
const int PAGE_TABLE_ENTRIES = 2048;
const int OS_RESERVED_FRAMES = 4096;  // 16MB / 4KB
const int TOTAL_FRAMES = 16384;
const int USER_FRAMES = TOTAL_FRAMES - OS_RESERVED_FRAMES;

struct Process {
    int pid;
    int s;
    int m;
    vector<int> keys;
    int current_search;
    vector<uint16_t> page_table;
    int allocated_frames;
    bool is_swapped;
};

queue<int> free_frames;
queue<Process*> ready_queue;
list<Process*> swapped_processes;

int page_accesses = 0;
int page_faults = 0;
int swap_count = 0;
int degree_min = INT_MAX;
int n_processes;
int completed_count = 0;

void initialize_free_frames() {
    for (int i = 0; i < USER_FRAMES; i++) {
        free_frames.push(OS_RESERVED_FRAMES + i);
    }
}

void load_processes(const string& filename, vector<Process>& processes) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error opening file" << endl;
        exit(1);
    }

    int n, m;
    file >> n >> m;
    n_processes = n;
    processes.resize(n);

    for (int i = 0; i < n; i++) {
        processes[i].pid = i;
        file >> processes[i].s;
        processes[i].m = m;
        processes[i].keys.resize(m);
        for (int j = 0; j < m; j++) {
            file >> processes[i].keys[j];
        }
        processes[i].current_search = 0;
        processes[i].page_table.resize(PAGE_TABLE_ENTRIES, 0);
        processes[i].allocated_frames = 0;
        processes[i].is_swapped = false;

        // Allocate 10 essential frames
        for (int j = 0; j < 10; j++) {
            if (free_frames.empty()) {
                cerr << "Not enough free frames during init" << endl;
                exit(1);
            }
            int frame = free_frames.front();
            free_frames.pop();
            processes[i].page_table[j] = 0x8000 | frame;
            processes[i].allocated_frames++;
        }
        ready_queue.push(&processes[i]);
    }
    file.close();
}

void simulate() {
    while (true) {
        if (ready_queue.empty()) {
            if (completed_count == n_processes) break;
            cerr << "Deadlock: no ready processes but not all completed." << endl;
            exit(1);
        }

        Process* p = ready_queue.front();
        ready_queue.pop();

        if (p->current_search >= p->m) continue;

#ifdef VERBOSE
        cout << "\tSearch " << p->current_search + 1 << " by Process " << p->pid << endl;
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
                if (!free_frames.empty()) {
                    int frame = free_frames.front();
                    free_frames.pop();
                    p->page_table[page_num] = 0x8000 | frame;
                    p->allocated_frames++;
                } else {
                    swap_count++;
                    // Free all frames allocated to this process
                    for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
                        if (p->page_table[i] & 0x8000) {
                            int frame = p->page_table[i] & 0x3FFF;
                            free_frames.push(frame);
                            p->page_table[i] = 0;
                        }
                    }
                    p->allocated_frames = 0;
                    p->is_swapped = true;
                    swapped_processes.push_back(p);

                    int active = n_processes - swapped_processes.size() - completed_count;
                    if (active < degree_min) degree_min = active;

                    cout << "+++ Swapping out process " << p->pid << " [" << active << " active processes]" << endl;
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
                        free_frames.push(frame);
                        p->page_table[i] = 0;
                    }
                }
                p->allocated_frames = 0;

                // Swap in a process if any are waiting
                if (!swapped_processes.empty()) {
                    Process* q = swapped_processes.front();
                    swapped_processes.pop_front();
                    
                    // Allocate 10 essential frames
                    for (int i = 0; i < 10; i++) {
                        if (free_frames.empty()) {
                            cerr << "No free frames when swapping in." << endl;
                            exit(1);
                        }
                        int frame = free_frames.front();
                        free_frames.pop();
                        q->page_table[i] = 0x8000 | frame;
                        q->allocated_frames++;
                    }
                    q->is_swapped = false;

                    int active = n_processes - swapped_processes.size() - completed_count;
                    cout << "+++ Swapping in process " << q->pid << " [" << active << " active processes]" << endl;

                    // Add to front of ready queue to restart its search immediately
                    ready_queue.push(q);
                }
            } else {
                ready_queue.push(p);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    vector<Process> processes;
    
    initialize_free_frames();
    load_processes("search.txt", processes);
    
    cout << "+++ Simulation data read from file" << endl;
    cout << "+++ Kernel data initialized" << endl;
    
    simulate();
    
    cout << "+++ Page access summary" << endl;
    cout << "Total number of page accesses = " << page_accesses << endl;
    cout << "Total number of page faults = " << page_faults << endl;
    cout << "Total number of swaps = " << swap_count << endl;
    cout << "Degree of multiprogramming = " << degree_min << endl;

    return 0;
}