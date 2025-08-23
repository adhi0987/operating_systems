#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#define SHM_KEY_PATH "/"
#define SHM_PROJ_ID 65
#define MAX_FOLLOWERS 100

int main(int argc, char *argv[]) {
    int n = (argc > 1) ? atoi(argv[1]) : 10;
    if (n > MAX_FOLLOWERS) {
        printf("Max followers limit exceeded. Set to %d\n", MAX_FOLLOWERS);
        n = MAX_FOLLOWERS;
    }

    key_t key = ftok(SHM_KEY_PATH, SHM_PROJ_ID);
    int shmid = shmget(key, (4 + n) * sizeof(int), 0666 | IPC_CREAT | IPC_EXCL);
    if (shmid == -1) {
        perror("Shared memory creation failed. Is another leader running?");
        exit(1);
    }

    int *M = (int *)shmat(shmid, NULL, 0);
    if (M == (void *)-1) {
        perror("Shared memory attach failed");
        exit(1);
    }

    // Initialize shared memory
    M[0] = n;    // Number of followers
    M[1] = 0;    // Number of followers that have joined so far
    M[2] = 0;    // Turn of the process
    for (int i = 3; i < 4 + n; i++) {
        M[i] = 0;
    }
    while (M[1] < n);

    int hash_table[1000] = {0};
    srand(time(NULL));

    while (1) {
        M[3] = (rand() % 99) + 1;  // Leader writes a random number to M[3]
        M[2] = 1;  // Notify follower 1
        while (M[2] != 0);  // Busy wait until it's leader's turn again

        int sum = M[3];
        printf("%2d",sum);
        for (int i = 4; i < 4 + n; i++) {
            printf("+%d",M[i]);
            sum += M[i];
        }

        printf("= %3d\n", sum);

        if (hash_table[sum] > 0) {
            M[2] = -1;  // Termination signal
            break;
        }
        hash_table[sum] = 1;
    }

    // Wait for followers to terminate
    while (M[2] != 0) {
        sleep(1);
    }
    shmdt(M);
    shmctl(shmid, IPC_RMID, NULL);
    return 0;
}