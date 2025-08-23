#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include<string.h>

#define SHM_KEY_PATH "."
#define SHM_KEY_PROJ_ID 65
#define SEM_KEY_PATH "."
#define SEM_KEY_PROJ_ID 66
#define MEM_SIZE 2000 // Memory chunk size
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

void makeStringWaiter(int waiter_id)
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