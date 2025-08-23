#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CHILDREN 100

pid_t parent_pid;
pid_t child_pids[MAX_CHILDREN];
int n; 
int my_index; 
int status; 

void handle_sigusr2(int sig) {
    if (status == 3) {
        return;
    }

    float rand_prob = (float)rand() / RAND_MAX;
    if (rand_prob <= 0.8) {
        status = 1; 
        kill(parent_pid, SIGUSR1);
    } else {
        status = 2; 
        kill(parent_pid, SIGUSR2);
    }
}

void handle_sigusr1(int sig) {
    switch (status) {
        case 0:
            printf("| ..... |");
            fflush(stdout);
            break;
        case 1:
            printf("| CATCH |");
            fflush(stdout);
            status = 0; 
            break;
        case 2:
            printf("| MISS  |");
            fflush(stdout);
            status = 3; 
            break;
        case 3:
            printf("|       |");
            fflush(stdout);
            break;
    }
    

    if (my_index < n - 1) {
        kill(child_pids[my_index + 1], SIGUSR1);
    } else {
        FILE *fp = fopen("dummycpid.txt", "r");
        if (fp == NULL) {
            perror("Error opening dummycpid.txt");
            exit(EXIT_FAILURE);
        }
        pid_t dummy_pid;
        fscanf(fp, "%d", &dummy_pid);
        fclose(fp);
        kill(dummy_pid, SIGINT);
    }
}

void handle_sigint(int sig) {
    if (status == 0) {
        for (int i = 0; i < n; i++){
            printf("---------");
            fflush(stdout);
        }
            
        printf("\n+++Child %d: Yay! I am the winner! \n", my_index + 1);
        fflush(stdout);
    }
    exit(0);
}

int main() {
    srand(time(NULL) ^ getpid());
    sleep(1);
    FILE *fp = fopen("childpid.txt", "r");
    if (fp == NULL) {
        perror("Error opening childpid.txt");
        exit(EXIT_FAILURE);
    }

    fscanf(fp, "%d", &n); 
    for (int i = 0; i < n; i++) {
        fscanf(fp, "%d", &child_pids[i]);
        if (child_pids[i] == getpid()) {
            my_index = i;
        }
    }
    fclose(fp);

    parent_pid = getppid(); 
    status = 0;             

    signal(SIGUSR2, handle_sigusr2);
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGINT, handle_sigint);

    while (1) {
        pause();
    }

    return 0;
}