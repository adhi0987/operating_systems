#include"common.h"

void cmain(int cook_id, int *M, int semid) {
    printTime(0);                                                           //initial time set to zero restuarent open at 11:00
    makeString(cook_id);                                                    //spaces for coressponding cooks 
    printf("Cook %c is ready\n", cook_id == 0 ? 'C' : 'D');
    int Time = 0, pending_orders = 0;

    while (1) {
        if (Time >= 240 && pending_orders == 0) {                           // check if cooks can leave 
            printTime(Time);
            makeString(cook_id);
            printf("Cook %c: Leaving\n", cook_id == 0 ? 'C' : 'D');
            sem_wait(semid, 0);                                             // unlock shared memory 

            if (M[4] == 1) {                                                // check if he is the last cook to leave 
                for (int i = 0; i < 5; i++) {                               // wake up all waiters 
                    sem_signal(semid, 2 + i);
                }
                sem_signal(semid, 0);                                       // Unlock shared memory 
                break;
            } else {
                M[4]--;                                                     
                //If Not Last Cook , There are No Pending Orders Wake-up other cook also before leaving
                // to avoid unbounded waiting  on the  cooks semaphores 
                sem_signal(semid, 1);
                //unlock shared memory also 
                sem_signal(semid, 0);   
                break;
            }
        }

        sem_wait(semid, 1);                                                 //Wait on Cook Semaphore 
        sem_wait(semid, 0);                                                 //Lock the shared Memory 

        if (M[3] > 0) { // pending_orders
            // printf("cook %c:",cook_id==0?'C':'D');
            // for (int i = M[1100]; i <M[1101]; i=i+3)
            // {
            //     printf("|%c %d %d |",'U'+M[1102+i],M[1102+i+1],M[1102+i+2]);
            // }
            // printf("\n");
            
            int front = M[1100]; // cook_queue_front
            int waiter_id = M[1102 + front];
            int customer_id = M[1102 + front + 1];
            int customer_count = M[1102 + front + 2];

            // Update cook queue front
            M[1100] += 3;
            if (M[1100] >= 2000) {
                M[1100] = 1102; // Wrap around for circular queue
            }
            M[3]--; // pending_orders--

            int curr_time = M[0];                                            //Note  time  before going to sleep
            sem_signal(semid, 0);                                            //Release Memory  Before  Sleep 

            // Simulate food preparation
            int preparation_time = 5 * customer_count;
            printTime(curr_time);
            makeString(cook_id);
            printf("Cook %c: Preparing order (Waiter %c, Customer %d, Count %d)\n",
                   cook_id == 0 ? 'C' : 'D', 'U' + waiter_id, customer_id, customer_count);
            usleep(preparation_time * 100000);                              //Simulation Of Preparing 5*c

            sem_wait(semid, 0);                                             //Lock Shared Memory 
            M[0] = curr_time + preparation_time;                            //Update Current Time
            printTime(curr_time + preparation_time);                
            makeString(cook_id);
            printf("Cook %c: Prepared order (Waiter %c, Customer %d, Count %d)\n",
                   cook_id == 0 ? 'C' : 'D', 'U' + waiter_id, customer_id, customer_count);
            M[100 + waiter_id*200] = customer_id;                           //Write To Waiter Food Read
            sem_signal(semid, 2 + waiter_id);                               //Wake-up Waiter 
            Time = M[0];
            pending_orders = M[3];
        } else {
            Time = M[0];
            pending_orders = M[3];
        }

        sem_signal(semid, 0);                                               //unlock Shared Memory 
    }
}

int main() {
    // Generate key
    key_t shm_key = ftok(SHM_KEY_PATH, SHM_KEY_PROJ_ID);
    key_t sem_key = ftok(SEM_KEY_PATH, SEM_KEY_PROJ_ID);
    if (shm_key == -1) {
        perror("ftok failed");
        exit(1);
    }

    // Create shared memory
    int shmid = shmget(shm_key, MEM_SIZE * sizeof(int), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }

    // Attach shared memory
    int *M = (int *)shmat(shmid, NULL, 0);
    if (M == (void *)-1) {
        perror("shmat failed");
        exit(1);
    }

    // Initialize shared memory
    M[0] = 0;      // time
    M[1] = 10;     // empty_tables
    M[2] = 0;      // next_waiter
    M[3] = 0;      // pending_orders
    M[4] = 2;      // no_of_cooks

   

    // Initialize waiter queues (5 waiters)
    for (int w = 0; w < 5; w++) {
        int base = 100 + w * 200;  // Start index for each waiter
        M[base] = 0;      // fr
        M[base + 1] = 0;  // po
        M[base + 2] = 0;  // front
        M[base + 3] = 0;  // back
        memset(&M[base + 4], 0, 196 * sizeof(int)); // Circular queue
    }

    // Initialize cooking queue
    M[1100] = 0; // cook_queue_front
    M[1101] = 0; // cook_queue_back
    memset(&M[1102], 0, (2000 - 1102) * sizeof(int)); // Remaining cells

    printf("Shared memory initialized successfully!\n");
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
        cmain(0, M, semid); // Cook C
        exit(0);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        cmain(1, M, semid); // Cook D
        exit(0);
    }

    // Wait for both cooks to terminate
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    // Detach and remove shared memory
    shmdt(M);
    shmctl(shmid, IPC_RMID, NULL);

    // Remove semaphores
    semctl(semid, 0, IPC_RMID);

    printf("Cook program terminated.\n");
    return 0;
}
