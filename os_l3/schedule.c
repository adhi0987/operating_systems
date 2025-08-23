#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAX_PROCESSES 100
#define MAX_BURSTS 50
#define INF 1000000000

typedef struct {
    int id;
    int arrival_time;
    int bursts[MAX_BURSTS];
    int burst_count;
    int current_burst;
    int remaining_time;
    int wait_time;
    int turnaround_time;
    int state; // 0 = Not arrived, 1 = Ready, 2 = Running, 3 = Waiting, 4 = Finished
} Process;

typedef struct {
    int data[MAX_PROCESSES];
    int front, rear;
} Queue;

typedef struct {
    int time;
    int process_id;
    int event_type; // 0 = Arrival, 1 = CPU completion, 2 = IO completion, 3 = Timeout
} Event;

typedef struct {
    Event data[MAX_PROCESSES * 10];
    int size;
} MinHeap;

Process processes[MAX_PROCESSES];
Queue ready_queue;
MinHeap event_queue;
int num_processes;

void initialize_processes(const char* filename);
void initialize_ready_queue();
void enqueue(Queue* q, int process_id);
int dequeue(Queue* q);
int is_empty(Queue* q);
void initialize_event_queue();
void add_event(int time, int process_id, int event_type);
Event pop_event();
int is_event_queue_empty();
void simulate(int time_quantum);
void handle_event(Event event, int time_quantum, int* current_time, int* cpu_busy, int* idle_time);
void print_metrics(int time_quantum);

int main() {
    initialize_processes("proc.txt");

    printf("Simulating FCFS scheduling...\n");
    simulate(INF);

    printf("\nSimulating Round Robin scheduling (q = 10)...\n");
    simulate(10);

    printf("\nSimulating Round Robin scheduling (q = 5)...\n");
    simulate(5);

    return 0;
}

void initialize_processes(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(1);
    }

    fscanf(file, "%d", &num_processes);

    for (int i = 0; i < num_processes; i++) {
        int burst;
        fscanf(file, "%d %d", &processes[i].id, &processes[i].arrival_time);
        processes[i].burst_count = 0;
        processes[i].current_burst = 0;
        processes[i].wait_time = 0;
        processes[i].turnaround_time = 0;
        processes[i].state = 0;

        while (fscanf(file, "%d", &burst) && burst != -1) {
            processes[i].bursts[processes[i].burst_count++] = burst;
        }
    }

    fclose(file);
}

void initialize_ready_queue() {
    ready_queue.front = ready_queue.rear = 0;
}

void enqueue(Queue* q, int process_id) {
    q->data[q->rear++] = process_id;
}

int dequeue(Queue* q) {
    if (is_empty(q)) return -1;
    return q->data[q->front++];
}

int is_empty(Queue* q) {
    return q->front == q->rear;
}

void initialize_event_queue() {
    event_queue.size = 0;
}

void add_event(int time, int process_id, int event_type) {
    Event new_event = {time, process_id, event_type};
    int i = event_queue.size++;
    while (i > 0 && event_queue.data[(i - 1) / 2].time > time) {
        event_queue.data[i] = event_queue.data[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    event_queue.data[i] = new_event;
}

Event pop_event() {
    Event min_event = event_queue.data[0];
    Event last_event = event_queue.data[--event_queue.size];
    int i = 0, child;
    while (2 * i + 1 < event_queue.size) {
        child = 2 * i + 1;
        if (child + 1 < event_queue.size && event_queue.data[child].time > event_queue.data[child + 1].time) {
            child++;
        }
        if (last_event.time <= event_queue.data[child].time) break;
        event_queue.data[i] = event_queue.data[child];
        i = child;
    }
    event_queue.data[i] = last_event;
    return min_event;
}

int is_event_queue_empty() {
    return event_queue.size == 0;
}

void simulate(int time_quantum) {
    initialize_ready_queue();
    initialize_event_queue();

    for (int i = 0; i < num_processes; i++) {
        add_event(processes[i].arrival_time, i, 0);
    }

    int current_time = 0, cpu_busy = 0, idle_time = 0;

    while (!is_event_queue_empty()) {
        Event event = pop_event();
        current_time = event.time;

        handle_event(event, time_quantum, &current_time, &cpu_busy, &idle_time);
    }

    print_metrics(time_quantum);
}

void handle_event(Event event, int time_quantum, int* current_time, int* cpu_busy, int* idle_time) {
    Process* proc = &processes[event.process_id];

    switch (event.event_type) {
    case 0: // Arrival
        enqueue(&ready_queue, event.process_id);
        proc->state = 1;
        break;

    case 1: // CPU completion
        proc->current_burst++;
        if (proc->current_burst < proc->burst_count) {
            add_event(*current_time + proc->bursts[proc->current_burst], event.process_id, 2);
            proc->state = 3;
        } else {
            proc->state = 4;
            proc->turnaround_time = *current_time - proc->arrival_time;
        }
        break;

    case 2: // IO completion
        enqueue(&ready_queue, event.process_id);
        proc->state = 1;
        break;

    case 3: // Timeout
        enqueue(&ready_queue, event.process_id);
        proc->state = 1;
        break;
    }

    while (!is_empty(&ready_queue)) {
        int next_process_id = dequeue(&ready_queue);
        Process* next_proc = &processes[next_process_id];
        next_proc->state = 2;
        if (time_quantum < next_proc->bursts[next_proc->current_burst]) {
            add_event(*current_time + time_quantum, next_process_id, 3);
            next_proc->bursts[next_proc->current_burst] -= time_quantum;
            *current_time += time_quantum;
        } else {
            add_event(*current_time + next_proc->bursts[next_proc->current_burst], next_process_id, 1);
            *current_time += next_proc->bursts[next_proc->current_burst];
        }
    }
}

void print_metrics(int time_quantum) {
    int total_turnaround_time = 0, total_wait_time = 0, total_processes = 0;

    printf("Time Quantum: %d\n", time_quantum);
    printf("ID\tArrival\tTurnaround\tWait\n");

    for (int i = 0; i < num_processes; i++) {
        total_turnaround_time += processes[i].turnaround_time;
        total_wait_time += processes[i].wait_time;
        total_processes++;

        printf("%d\t%d\t%d\t\t%d\n", processes[i].id, processes[i].arrival_time, processes[i].turnaround_time, processes[i].wait_time);
    }

    printf("Average Turnaround Time: %.2f\n", (double)total_turnaround_time / total_processes);
    printf("Average Wait Time: %.2f\n", (double)total_wait_time / total_processes);
}
