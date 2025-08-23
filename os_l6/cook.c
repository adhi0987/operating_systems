#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include<string.h>

// Shared memory and semaphore keys
#define SHM_KEY_PATH "."
#define SEM_KEY_PATH "."
#define SHM_KEY_PROJ_ID 1234
#define SEM_KEY_PROJ_ID 5678

// Shared memory structure
typedef struct {
    int time;                           // Simulated time (minutes since 11:00 am)
    int empty_tables;                   // Number of empty tables
    int next_waiter;                    // Next waiter to serve a customer
    int pending_orders;                 // Number of pending orders for cooks
    int food_ready[5];                     // Customer ID for which food is ready 
    int no_of_cooks;                    // No of cooks to identify last leaving cooks
    int cook_queue[600];                // Cooking queue (200 orders max, 3 ints per order)
    int cook_queue_front;               // Front index of the cooking queue
    int cook_queue_back;                // Back index of the cooking queue
    int waiter_queues[5][200];          // Queues for each waiter (customer ID and count)
    int waiter_queue_front[5];          // Front index of each waiter's queue
    int waiter_queue_back[5];           // Back index of each waiter's queue
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
    default:
        break;
    }
}
// Cook behavior
void cmain(int cook_id, SharedData *shared, int semid) {
    printTime(0);
    makeString(cook_id);
    printf("Cook %c is ready\n", cook_id == 0 ? 'C' : 'D');
    int Time=0,pending_orders=0;
    while (1) {
        // Wait for a cooking request
        // Check if the session is over
        // printf("Cook %c:Time :%d,Pending Orders:%d\n",cook_id==0?'C':'D',Time,pending_orders);
        if (Time>= 240 && pending_orders == 0) { // 240 minutes = 4 hours (11:00 am to 3:00 pm)
            printTime(Time);
            makeString(cook_id);
            printf("Cook %c: Leaving\n", cook_id == 0 ? 'C' : 'D');
            sem_wait(semid,0);
            if(shared->no_of_cooks==1)
            {
                // printf("Cook %c: Waking Up all Waiters\n",cook_id==0?'C':'D');
                for(int i=0;i<5;i++)
                {
                    // printf("cook %c:Waking up Waiter %c\n",cook_id==0?'C':'D','U'+i);
                    sem_signal(semid,2+i);
                }
                sem_signal(semid,0);    //unlock shared memory 
                break;
            }else
            {
                shared->no_of_cooks--;
                sem_signal(semid,1);    //wakeup other cook also 
                sem_signal(semid, 0);   // Unlock the shared memory
                break;
            }
        }
        sem_wait(semid, 1); // Wait on the cook semaphore
        sem_wait(semid, 0); // Lock the shared memory (mutex)
        // printf("cook %c:Time now :%d,Pending Orders:%d\n\n",cook_id==0?'C':'D',shared->time,shared->pending_orders); 
        if(shared->pending_orders>0)
        {
            int waiter_id = shared->cook_queue[shared->cook_queue_front];
            int customer_id = shared->cook_queue[shared->cook_queue_front + 1];
            int customer_count = shared->cook_queue[shared->cook_queue_front + 2];

            // Update the queue
            shared->cook_queue_front += 3;
            if (shared->cook_queue_front >= 600) {
                shared->cook_queue_front = 0; // Wrap around for circular queue
            }
            shared->pending_orders--;

            int curr_time=shared->time;
            sem_signal(semid, 0); // Unlock the shared memory

            // Simulate food preparation (5 minutes per person)
            int preparation_time = 5 * customer_count;
            printTime(curr_time);
            makeString(cook_id);
            printf("Cook %c: Preparing order (Waiter %c, Customer %d, Count %d)\n",
                cook_id == 0 ? 'C' : 'D','U'+waiter_id,customer_id, customer_count);
            usleep(preparation_time * 100000); // Sleep for preparation_time * 100 ms

            // Notify the waiter that food is ready
            sem_wait(semid, 0); // Lock the shared memory
            shared->time=curr_time+ preparation_time; // Update the simulated time
            printTime(curr_time+preparation_time);
            makeString(cook_id);
            printf("Cook %c: Prepared order (Waiter %c, Customer %d, Count %d)\n",
                   cook_id == 0 ? 'C' : 'D', 'U'+waiter_id,customer_id,customer_count);
            shared->food_ready[waiter_id]=customer_id;
            sem_signal(semid, 2 + waiter_id); // Signal the waiter (semaphore for waiter_id)
            Time=shared->time;
            pending_orders=shared->pending_orders;
        }
        else{
            // printf("Cook %c:No Pending Orders Right Now \n",cook_id==0?'C':'D');
            Time=shared->time;
            pending_orders=shared->pending_orders;
        }
        sem_signal(semid, 0); // Unlock the shared memory
        // printf("Cook %c:Memory Released\n",cook_id==0?'C':'D');
    }
    // printf("Cook %c: Terminated\n", cook_id == 0 ? 'C' : 'D');
}

int main() {
    // Generate keys using ftok()
    key_t shm_key = ftok(SHM_KEY_PATH, SHM_KEY_PROJ_ID);
    key_t sem_key = ftok(SEM_KEY_PATH, SEM_KEY_PROJ_ID);

    if (shm_key == -1 || sem_key == -1) {
        perror("ftok failed");
        exit(1);
    }

    // Create shared memory segment
    int shmid = shmget(shm_key, sizeof(SharedData), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }

    // Attach shared memory
    SharedData *shared = (SharedData *) shmat(shmid, NULL, 0);
    if (shared == (void *) -1) {
        perror("shmat failed");
        exit(1);
    }

    // Initialize shared memory
    shared->time = 0;
    shared->empty_tables = 10;
    shared->next_waiter = 0;
    shared->pending_orders = 0;
    shared->cook_queue_front = 0;
    shared->cook_queue_back = 0;
    // shared->food_ready=0;
    shared->no_of_cooks=2;
    // Initialize cooking queue
    memset(shared->cook_queue, 0, sizeof(shared->cook_queue));

    // Initialize waiter queues
    for (int i = 0; i < 5; i++) {
        memset(shared->waiter_queues[i], 0, sizeof(shared->waiter_queues[i]));
        shared->waiter_queue_front[i] = 0;
        shared->waiter_queue_back[i] = 0;
        shared->pending_waiter_orders[i]=0;
        shared->food_ready[i]=0;
    }

    // Create semaphores
    int semid = semget(sem_key, 207, 0666 | IPC_CREAT); // 1 mutex + 1 cook + 5 waiters
    if (semid == -1) {
        perror("semget failed");
        exit(1);
    }

    // Initialize semaphores
    semctl(semid, 0, SETVAL, 1); // mutex = 1
    semctl(semid, 1, SETVAL, 0); // cook = 0
    for (int i = 2; i < 207; i++) {
        semctl(semid, i, SETVAL, 0); // waiters = 0,customers = 0
    }

    // Fork two cook processes
    pid_t pid1 = fork();
    if (pid1 == 0) {
        cmain(0, shared, semid); // Cook C
        exit(0);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        cmain(1, shared, semid); // Cook D
        exit(0);
    }

    // Wait for both cooks to terminate
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    // Detach and remove shared memory
    shmdt(shared);
    shmctl(shmid, IPC_RMID, NULL);

    // Remove semaphores
    semctl(semid, 0, IPC_RMID);

    printf("Cook program terminated.\n");
    return 0;
}