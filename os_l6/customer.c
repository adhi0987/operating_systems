#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

// Shared memory and semaphore keys
#define SHM_KEY_PATH "."
#define SEM_KEY_PATH "."
#define SHM_KEY_PROJ_ID 1234
#define SEM_KEY_PROJ_ID 5678

// Shared memory structure
typedef struct {
    int time;                   // Simulated time (minutes since 11:00 am)
    int empty_tables;           // Number of empty tables
    int next_waiter;            // Next waiter to serve a customer
    int pending_orders;         // Number of pending orders for cooks
    int food_ready[5];             // Customer ID for which food is ready 
    int no_of_cooks;                    // No of cooks to identify last leaving cooks
    int cook_queue[600];        // Cooking queue (200 orders max, 3 ints per order)
    int cook_queue_front;       // Front index of the cooking queue
    int cook_queue_back;        // Back index of the cooking queue
    int waiter_queues[5][200];  // Queues for each waiter (customer ID and count)
    int waiter_queue_front[5];  // Front index of each waiter's queue
    int waiter_queue_back[5];   // Back index of each waiter's queue
    int pending_waiter_orders[5];       // Pending Orders in Each Waiter Queue
} SharedData;

// Semaphore operations
void sem_wait(int semid, int sem_num) {
    struct sembuf sb = {sem_num, -1, 0};
    semop(semid, &sb, 1);
}

void sem_signal(int semid, int sem_num) {
    struct sembuf sb = {sem_num, 1, 0};
    semop(semid, &sb, 1);
}
void printTime(int arrival_time)
{
    printf("[%02d:%02d",(11+(arrival_time/60))%12==0?12:(11+(arrival_time/60))%12,arrival_time%60);
    if(11+(arrival_time/60)>=12) 
    {
        printf(" pm]");
    }else{
        printf(" am]");
    }
}
// Customer behavior
void cmain(int customer_id, int arrival_time, int count, SharedData *shared, int semid) {
    if(arrival_time<=240)
    {
        sem_wait(semid, 0); // Lock the shared memory
        if (shared->empty_tables == 0) {
            printTime(arrival_time);
            printf(" 						Customer %d leaves (no empty table)\n", customer_id);
            sem_signal(semid, 0); // Unlock the shared memory
            return;
        }
        
        int curr_time=shared->time=arrival_time;
        printTime(shared->time);
        printf(" Customer %d arrives (count = %d)\n",customer_id,count);
        // Wait until the simulated time reaches the arrival time
        // usleep(100000); // Sleep for 100 ms (1 minute in simulation time)
        // Check if there is an empty table
        // shared->time=curr_time+1;
        // Occupy a table
        shared->empty_tables--;
        // printf("Customer %d: Occupied a table. %d tables left.\n", customer_id, shared->empty_tables);

        // Find the next waiter
        int waiter_id = shared->next_waiter;
        shared->next_waiter = (shared->next_waiter + 1) % 5; // Circular fashion
        // printf("customer %d:waiter %c,waiter_queue_back:%d\n",customer_id,'U'+waiter_id,shared->waiter_queue_back[waiter_id]);
        // Write customer details to the waiter's queue
        shared->waiter_queues[waiter_id][shared->waiter_queue_back[waiter_id]] = customer_id;
        shared->waiter_queues[waiter_id][shared->waiter_queue_back[waiter_id] + 1] = count;
        shared->pending_waiter_orders[waiter_id]++;
        shared->waiter_queue_back[waiter_id] += 2;
        if (shared->waiter_queue_back[waiter_id] >= 200) {
            shared->waiter_queue_back[waiter_id] = 0; // Wrap around for circular queue
        }
        sem_signal(semid, 0); // Unlock the shared memory
        sem_signal(semid, 2 + waiter_id); // Signal the waiter (semaphore for waiter_id)
        sem_wait(semid,6+customer_id); // Waits For the waiter to take the order 
        sem_wait(semid,0);
        printTime(shared->time);
        printf(" \tCustomer %d: Order placed to Waiter %c\n", customer_id, 'U' + waiter_id);
        sem_signal(semid,0);
        // Wait for the waiter to take the order
        sem_wait(semid, 6 + customer_id); // Wait on the customer's semaphore from the waiter to serve the food


        // Wait for food to be served
        // printf("Customer %d: Waiting for food...\n", customer_id);
        // sem_wait(semid, 6 + customer_id); // Wait on the customer's semaphore again

        // Simulate eating (30 minutes)
        sem_wait(semid, 0); // Lock the shared memory
        printTime(shared->time);
        printf(" 		Customer %d gets food [Waiting time = %d]\n", customer_id,shared->time-arrival_time);
        curr_time=shared->time;
        sem_signal(semid,0); //releasing memory  before going to sleep 
        usleep(3000000); // Sleep for 3 seconds (30 minutes in simulation time)
        sem_wait(semid,0);  //acquiring resource after waking up from sleep 
        shared->time =curr_time+30; // Update the simulated time
        // Free the table 
        shared->empty_tables++;
        // printf("Customer %d: Finished eating. Freed the table. %d tables left.\n", customer_id, shared->empty_tables);
        printTime(shared->time);
        printf(" 			Customer %d finishes eating and leaves\n", customer_id);
        sem_signal(semid, 0); // Unlock the shared memory

    }else{
        printTime(arrival_time);
        printf(" 						Customer %d leaves (late arrival)\n",customer_id);
        return;
    }
}

int main() {
    // Generate keys using ftok()
    key_t shm_key = ftok(SHM_KEY_PATH, SHM_KEY_PROJ_ID);
    key_t sem_key = ftok(SEM_KEY_PATH, SEM_KEY_PROJ_ID);

    if (shm_key == -1 || sem_key == -1) {
        perror("ftok failed");
        exit(1);
    }

    // Attach shared memory
    int shmid = shmget(shm_key, sizeof(SharedData), 0666);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }

    SharedData *shared = (SharedData *) shmat(shmid, NULL, 0);
    if (shared == (void *) -1) {
        perror("shmat failed");
        exit(1);
    }

    // Get semaphores
    int semid = semget(sem_key, 7+200, 0666);
    if (semid == -1) {
        perror("semget failed");
        exit(1);
    }

    // Read customer information from customers.txt
    FILE *file = fopen("customers.txt", "r");
    if (file == NULL) {
        perror("Failed to open customers.txt");
        exit(1);
    }

    int customer_id, arrival_time, count,prev_time=0;
    while (fscanf(file, "%d %d %d", &customer_id, &arrival_time, &count) == 3) {
        if (customer_id == -1) {
            break; // End of file marker
        }
        sem_wait(semid,0); //lock the shared memory 
        // Wait for the interval between customer arrivals
        if (prev_time < arrival_time) {
            int curr_time=shared->time;
            int delay = arrival_time - prev_time;
            prev_time=arrival_time;
            sem_signal(semid,0);   //releasing memory before sleeping 
            usleep(delay * 100000); // Sleep for delay * 100 ms
            sem_wait(semid,0);      //locking memory to update the time 
            shared->time =curr_time+delay; // Update the simulated time
        }
        sem_signal(semid,0); // unlock the shared memory 
        // Fork a child process for each customer
        pid_t pid = fork();
        if (pid == 0) {
            cmain(customer_id, arrival_time, count, shared, semid); // Customer process
            exit(0);
        } else if (pid < 0) {
            perror("fork failed");
            exit(1);
        }

    }

    fclose(file);

    // Wait for all customer processes to terminate
    while (wait(NULL) > 0);

    // Detach shared memory
    shmdt(shared);

    printf("Customer program terminated.\n");
    return 0;
}