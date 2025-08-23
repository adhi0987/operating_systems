#include "common.h"
// Customer behavior
void cmain(int customer_id, int arrival_time, int count, int *M, int semid) {
    if (arrival_time <= 240) {
        sem_wait(semid, 0); // Lock the shared memory
        if (M[1] == 0) { // M[1] stores empty tables count
            printTime(arrival_time);
            printf(" \t\t\t\tCustomer %d leaves (no empty table)\n", customer_id);
            sem_signal(semid, 0); // Unlock the shared memory
            return;
        }
        
        int curr_time = M[0] = arrival_time; // M[0] stores simulated time
        printTime(M[0]);
        printf(" Customer %d arrives (count = %d)\n", customer_id, count);
        
        M[1]--; // Occupy a table
        
        int waiter_id = M[2]; // M[2] stores next waiter ID
        M[2] = (M[2] + 1) % 5; // Circular fashion
        
        int base=100+200*waiter_id; // Queue back index for the waiter
        int queue_back_index=M[base+3];
        M[base+4+queue_back_index] = customer_id;
        M[base+4+queue_back_index+ 1] = count;
        M[base+1]++; // Pending orders count for waiter
        M[base+3] = (queue_back_index + 2) % 200;
        // printf("customer %d: writing to waiter %c queue at index %d\n",customer_id,'U'+waiter_id,queue_back_index);
        
        sem_signal(semid, 0); // Unlock the shared memory
        sem_signal(semid, 2 + waiter_id); // Signal the waiter
        sem_wait(semid, 6 + customer_id); // Wait for order to be taken
        
        sem_wait(semid, 0);
        printTime(M[0]);
        printf(" \tCustomer %d: Order placed to Waiter %c\n", customer_id, 'U' + waiter_id);
        sem_signal(semid, 0);
        
        sem_wait(semid, 6 + customer_id); // Wait for food to be served
        
        sem_wait(semid, 0); // Lock the shared memory
        printTime(M[0]);
        printf(" \t\tCustomer %d gets food [Waiting time = %d]\n", customer_id, M[0] - arrival_time);
        curr_time = M[0];
        sem_signal(semid, 0);
        
        usleep(3000000); // Simulate eating for 30 minutes
        
        sem_wait(semid, 0);
        M[0] = curr_time + 30; // Update time
        M[1]++; // Free the table
        printTime(M[0]);
        printf(" \t\t\tCustomer %d finishes eating and leaves\n", customer_id);
        sem_signal(semid, 0);
    } else {
        printTime(arrival_time);
        printf(" \t\t\t\tCustomer %d leaves (late arrival)\n", customer_id);
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
    int shmid = shmget(shm_key, 2000 * sizeof(int), 0666);
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
    int semid = semget(sem_key, 7 + 200, 0666);
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

    int customer_id, arrival_time, count, prev_time = 0;
    while (fscanf(file, "%d %d %d", &customer_id, &arrival_time, &count) == 3) {
        if (customer_id == -1) {
            break; // End of file marker
        }
        sem_wait(semid, 0); // Lock the shared memory
        // Wait for the interval between customer arrivals
        if (prev_time < arrival_time) {
            int curr_time = M[0];
            int delay = arrival_time - prev_time;
            prev_time = arrival_time;
            sem_signal(semid, 0); // Releasing memory before sleeping
            usleep(delay * 100000); // Sleep for delay * 100 ms
            sem_wait(semid, 0); // Locking memory to update the time
            M[0] = curr_time + delay; // Update the simulated time
        }
        sem_signal(semid, 0); // Unlock the shared memory

        // Fork a child process for each customer
        pid_t pid = fork();
        if (pid == 0) {
            cmain(customer_id, arrival_time, count, M, semid); // Customer process
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
    shmdt(M);

    printf("Customer program terminated.\n");
    return 0;
}
