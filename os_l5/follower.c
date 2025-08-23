#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <time.h>
#include<sys/types.h>
#include<sys/wait.h>
#define SHM_KEY_PATH "/"
#define SHM_PROJ_ID 65

void follower_process(int follower_id, int *M) {
    srand(time(NULL) + getpid());

    while (1) {
        if (M[2] < 0) break;
        if (M[2] == follower_id) {
            M[3 + follower_id] = (rand() % 9) + 1;  // Follower writes a random number to its designated cell
            M[2] = (follower_id == M[0]) ? 0 : follower_id + 1;  // Update turn
        }
    }

    // Termination round
    while (1) {
        if (M[2] == -M[0]) {
            M[2] = 0;
            break;
        } else if (M[2] == -follower_id) {
            M[2] = -(follower_id + 1);
            break;
        } else {
            usleep(1);
        }
    }

    printf("Follower %d leaves\n", follower_id);
    shmdt(M);
}

int main(int argc, char *argv[]) {
    key_t key = ftok(SHM_KEY_PATH, SHM_PROJ_ID);
    int shmid = shmget(key, 0, 0666);
    if (shmid == -1) {
        perror("Leader not running or shared memory error");
        exit(1);
    }

    int *M = (int *)shmat(shmid, NULL, 0);
    if (M == (void *)-1) {
        perror("Shared memory attach failed");
        exit(1);
    }

    int num_followers = (argc > 1) ? atoi(argv[1]) : 1;
    int i, joined = 0;

    for (int j = 0; j < num_followers; j++) {
        i = ++M[1];
        if (i > M[0]) {
            printf("follower error:%dFollowers already joined\n",M[0]);
            break;
        }
        printf("Follower %d joins\n", i);

        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        } else if (pid == 0) {
            follower_process(i, M);
            exit(0);
        } else {
            joined++;
        }
    }

    if (joined == 0) {
        shmdt(M);
        exit(0);
    }

    // Parent waits for all child processes to terminate
    for (int j = 0; j < joined; j++) {
        wait(NULL);
    }

    shmdt(M);
    return 0;
}