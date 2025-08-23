#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CHILDREN 100

int child_status[MAX_CHILDREN]; 
int n;                          
pid_t child_pids[MAX_CHILDREN]; 
pid_t dummy_pid;                
int current_child = 0;       

int flag = 0;

void handle_sigusr1(int sig) { flag = 1;}

void handle_sigusr2(int sig) {
    child_status[current_child] = 1; 
    flag = 1;
}

void create_dummy_process() {
    dummy_pid = fork();
    if (dummy_pid < 0) {
        perror("Fork failed for dummy process");
        exit(EXIT_FAILURE);
    }
    if (dummy_pid == 0) {
        execl("./dummy", "./dummy", NULL);
    } else {
        FILE *fp = fopen("dummycpid.txt", "w");
        if (fp == NULL) {
            perror("Error opening dummycpid.txt");
            exit(EXIT_FAILURE);
        }
        fprintf(fp, "%d\n", dummy_pid);
        fclose(fp);
    }
}

void setup_signal_handlers() {
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_children>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    n = atoi(argv[1]); 
    if (n <= 0 || n > MAX_CHILDREN) {
        fprintf(stderr, "Number of children must be between 1 and %d.\n", MAX_CHILDREN);
        exit(EXIT_FAILURE);
    }

    FILE *fp = fopen("childpid.txt", "w");
    if (fp == NULL) {
        perror("Error opening childpid.txt");
        exit(EXIT_FAILURE);
    }

    fprintf(fp, "%d\n", n);

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            fclose(fp);
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            execl("./child", "./child", NULL); 
            perror("execl failed");
            exit(EXIT_FAILURE);
        } else {
            child_pids[i] = pid;
            child_status[i] = 0; 
            fprintf(fp, "%d\n", pid); 
        }
    }

    fclose(fp);

    sleep(2);
    printf("++++Parent: Finished writing to childpid.txt and created all children.\n");
    fflush(stdout);

    setup_signal_handlers();

    printf("++++Parent:waiting for the child to read  the pids of other childs\n");
    fflush(stdout);
    //`sleep(5); 

    printf("++++Parent: Starting the game.\n");
    fflush(stdout);
    for (int i = 0; i < n; i++)
    {
        printf("---------");
        fflush(stdout);

    }
    printf("\n|");
    fflush(stdout);
    for (int i = 0; i < n; i++)
    {

        printf("|   %d   |", i + 1);
        fflush(stdout);
    }
    printf("\n");
    fflush(stdout);
    for (int i = 0; i < n; i++)
    {
        printf("---------");
        fflush(stdout);

    }
    
    printf("+\n");
    fflush(stdout);

    while (1) {
        while (child_status[current_child] == 1) {
            current_child = (current_child + 1) % n;
        }

        kill(child_pids[current_child], SIGUSR2);
        if(!flag)pause();


        flag = 0;
        printf("|");
        fflush(stdout);
        create_dummy_process();
        kill(child_pids[0], SIGUSR1);
        waitpid(dummy_pid, NULL, 0);
        printf("\n");
        fflush(stdout);

        int remaining_children = 0;
        // int last_child = -1;
        for (int i = 0; i < n; i++) {
            if (child_status[i] == 0) {
                remaining_children++;
                // last_child = i;
            }
        }

        if (remaining_children == 1) {
            // printf("Child %d is the winner!\n", last_child + 1);

            
            break;
        }
        current_child = (current_child + 1) % n;
    }
    for(int i=0;i<n;i++)
    {
         printf("---------");
         fflush(stdout);
    }
    printf("\n");
    fflush(stdout);
    for(int i=0;i<n;i++)
    {
        printf("    %d    ",i+1);
        fflush(stdout);
    }
    printf("+\n");
    fflush(stdout);

    for (int i = 0; i < n; i++) {
                kill(child_pids[i], SIGINT);
            }
    return 0;
}