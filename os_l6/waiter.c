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
void makeString(int waiter_id)
{
    switch (waiter_id)
    {
    case 0:
        printf(" ");
        break;
    case 1:
        printf(" \t");
        break;
    case 2:
        printf(" \t\t");
        break;
    case 3:
        printf(" \t\t\t");
        break;
    case 4:
        printf(" \t\t\t\t");
    default:
        break;
    }
}

// Waiter behavior
void wmain(int waiter_id, SharedData *shared, int semid) {
    printTime(0);
    makeString(waiter_id);
    printf("Waiter %c is ready\n", 'U' + waiter_id);
    while (1) {
        // printf("waiter %c:Time:%d,pending order =%d,food_ready=%d\n",'U'+waiter_id,Time,pending_orders,food_ready);
        
        // Wait for a signal from a cook or a new customer
        sem_wait(semid, 2 + waiter_id); // Wait on the waiter's semaphore
        // if(waiter_id==2)
        // {
        //     printf("waiter %c:Crossed this \n\n",'U'+waiter_id);
        // }
        
        sem_wait(semid, 0); // Lock the shared memory (mutex)
        // printf("Waiter %c:Time now :%d,Pending Orders :%d Food Ready:%d\n\n",'U'+waiter_id,shared->time,shared->pending_waiter_orders[waiter_id],shared->food_ready);
        // Check if the signal is from a cook (food is ready)

        
        
        if (shared->food_ready[waiter_id]!=0) 
        {
            // Food is ready for a customer
            int customer_id = shared->food_ready[waiter_id];
            // shared->waiter_queue_front[waiter_id] += 2; // Move the front of the queue
            // if (shared->waiter_queue_front[waiter_id] >= 200) {
            //     shared->waiter_queue_front[waiter_id] = 0; // Wrap around for circular queue
            // }
            shared->food_ready[waiter_id]=0;
            printTime(shared->time);
            makeString(waiter_id);
            printf("Waiter %c: Serving food to Customer %d\n", 'U' + waiter_id, customer_id);
            sem_signal(semid, 6 + customer_id); // Signal the customer (semaphore for customer_id)
            // printf("waiter %c :calling customer at : %d \n",'U'+waiter_id ,shared->time);
            if(shared->time>=240)
            {
                printTime(shared->time);
                makeString(waiter_id);
                printf("Waiter %c leaving (no more customer to serve)\n", 'U' + waiter_id);
                sem_signal(semid, 0); // Unlock the shared memory
                break;
            }
        }
        if(shared->pending_waiter_orders[waiter_id]>0)
        {
            shared->pending_waiter_orders[waiter_id]--;
            // Signal is from a new customer (order to be taken)
            // printf("waiter %c: front index to read %d\n",'U'+waiter_id,shared->waiter_queue_front[waiter_id]);
            int customer_id = shared->waiter_queues[waiter_id][shared->waiter_queue_front[waiter_id]];
            int customer_count = shared->waiter_queues[waiter_id][shared->waiter_queue_front[waiter_id] + 1];
            // printf("waiter %c:customer id:%d,customer_count:%d\n",'U'+waiter_id,customer_id,customer_count);
            shared->waiter_queue_front[waiter_id] += 2; // Move the front of the queue
            if (shared->waiter_queue_front[waiter_id] >= 200) {
                shared->waiter_queue_front[waiter_id] = 0; // Wrap around for circular queue
            }
            int curr_time=shared->time;
            // Write the order to the cooking queue
            shared->cook_queue[shared->cook_queue_back] = waiter_id;
            shared->cook_queue[shared->cook_queue_back + 1] = customer_id;
            shared->cook_queue[shared->cook_queue_back + 2] = customer_count;
            shared->cook_queue_back += 3;
            if (shared->cook_queue_back >= 600) {
                shared->cook_queue_back = 0; // Wrap around for circular queue
            }
            shared->pending_orders++;
            // Simulate order collection (1 minute)
            // printf("Waiter %c:Releasing Memory before Sleeping\n",'U'+waiter_id);
            sem_signal(semid,0);
            usleep(100000); // Sleep for 100 ms (1 minute in simulation time)
            sem_wait(semid,0);  //lock the memory 
            shared->time =curr_time+1; // Update the simulated time
            sem_signal(semid,6+customer_id); //notify the customer that waiter has taken the order 
            printTime(shared->time);
            makeString(waiter_id);
            printf("Waiter %c: Placing order for Customer %d (count = %d)\n", 'U' + waiter_id, customer_id, customer_count);
            // printTime(shared->time);
            // makeString(waiter_id);
            // printf(" Waiter %c: Order from Customer %d placed in cooking queue.\n", 'U' + waiter_id, customer_id);
            sem_signal(semid, 1); // Signal the cook semaphore
        }
        else
        {
            if(shared->time>=240 &&(shared->waiter_queue_front[waiter_id]==shared->waiter_queue_back[waiter_id]))
            {
                printTime(shared->time);
                makeString(waiter_id);
                printf("Waiter %c leaving (no more customer to serve)\n", 'U' + waiter_id);
                sem_signal(semid, 0); // Unlock the shared memory
                break;
            }
            // printf("Waiter %c:No Order to add to cook queue  Right Now \n",'U'+waiter_id);
        }
        // Time=shared->time;
        // pending_orders=shared->pending_waiter_orders[waiter_id];
        // food_ready=shared->food_ready;
        sem_signal(semid, 0); // Unlock the shared memory
        // printf("waiter %c: memory released \n\n",'U'+waiter_id);
    }
    // makeString(waiter_id);
    // printf(" Waiter %c leaving (no more customer to serve)\n", 'U' + waiter_id);
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
    int semid = semget(sem_key, 7, 0666);
    if (semid == -1) {
        perror("semget failed");
        exit(1);
    }

    // Fork five waiter processes
    for (int i = 0; i < 5; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            wmain(i, shared, semid); // Waiter U, V, W, X, or Y
            exit(0);
        } else if (pid < 0) {
            perror("fork failed");
            exit(1);
        }
    }

    // Wait for all waiter processes to terminate
    for (int i = 0; i < 5; i++) {
        wait(NULL);
    }

    // Detach shared memory
    shmdt(shared);

    printf("Waiter program terminated.\n");
    return 0;
}