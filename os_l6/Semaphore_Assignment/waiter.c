#include"common.h"
void wmain(int waiter_id, int *M, int semid) {
    printTime(0);                                       //initial time set to zero restuarent open at 11:00
    makeStringWaiter(waiter_id);                        // spaces printer for each waiter 
    printf("Waiter %c is ready\n", 'U' + waiter_id);    

    while (1) {                                         // starting loop 
        sem_wait(semid, 2 + waiter_id);                 // Wait on the waiter's semaphore
        sem_wait(semid, 0);                             // Lock the shared memory (mutex)
        int base = 100 + waiter_id * 200;               // calculating base for each waiter 
        if (M[base] != 0) {                             // Check if food is ready
            int customer_id = M[base];                 
            M[base] = 0;

            printTime(M[0]);
            makeStringWaiter(waiter_id);
            printf("Waiter %c: Serving food to Customer %d\n", 'U' + waiter_id, customer_id);
            sem_signal(semid, 6 + customer_id);         // Signal the customer that food is ready to serve

            if (M[0] >= 240) {                          //check if after serving last customer,can the waiter leave
                printTime(M[0]);
                makeStringWaiter(waiter_id);
                printf("Waiter %c leaving (no more customers to serve)\n", 'U' + waiter_id);
                sem_signal(semid, 0);                   // Unlock the shared memory
                break;
            }
        }

        if (M[base+1] > 0) {                            // Check pending orders
            // printf("Waiter %c:",'U'+waiter_id);      // for debbugging purpose 
            // for(int i=M[base+2];i<M[base+3];i=i+2)
            // {
            //     printf("|%d %d|",M[base+4+i],M[base+4+i+1]);
            // }
            // printf("\n");
            M[base+1]--;                                // start reading the pending orders 
            int customer_id = M[base + 4 + M[base + 2]];
            int customer_count = M[base + 4 + M[base + 2]+1];
            M[base + 2] += 2;
            if (M[base + 2] >= 200) {                   // wrap around if front exceed the queue index
                M[base + 2] = 0;
            }

            int curr_time = M[0];                       // note the current time before going to sleep 

            // Write the order to the cooking queue
            M[1102 + M[1101]] = waiter_id;
            M[1102 + M[1101]+1] = customer_id;
            M[1102 + M[1101]+2] = customer_count;
            M[1101] += 3;
            if (M[1101] >= 600) {
                M[1101] = 0;
            }
            M[3]++; // Increment pending orders

            sem_signal(semid, 0);
            usleep(100000);                             // Simulate order collection 1min sleep  (100 ms)
            sem_wait(semid, 0);

            M[0] = curr_time + 1;                       // Update time
            sem_signal(semid, 6 + customer_id);
            printTime(M[0]);
            makeStringWaiter(waiter_id);
            printf("Waiter %c: Placing order for Customer %d (count = %d)\n", 'U' + waiter_id, customer_id, customer_count);
            sem_signal(semid, 1); // Signal the cook
        } else {                                        // check if no order left to take and time is greater than 3:00pm 
            if (M[0] >= 240 && (M[base + 2] == M[base + 3])) {
                printTime(M[0]);
                makeStringWaiter(waiter_id);
                printf("Waiter %c leaving (no more customers to serve)\n", 'U' + waiter_id);
                sem_signal(semid, 0);
                break;
            }
        }

        sem_signal(semid, 0); // Unlock shared memory
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
    int shmid = shmget(shm_key, sizeof(int) *2000, 0666);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }

    int *M = (int *) shmat(shmid, NULL, 0);
    if (M == (void *) -1) {
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
            wmain(i, M, semid); // Waiter U, V, W, X, or Y
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
    shmdt(M);

    printf("Waiter program terminated.\n");
    return 0;
}