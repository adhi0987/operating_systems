#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

#define SHM_KEY_PATH "."
#define SHM_KEY_PROJ_ID 65
#define SEM_KEY_PATH "."
#define SEM_KEY_PROJ_ID 66
#define MEM_SIZE 2000 // Memory chunk size

// Semaphore operations
void sem_wait(int semid, int sem_num);
void sem_signal(int semid, int sem_num);

// Time printing function
void printTime(int arrival_time);

// String formatting functions
void makeString(int waiter_id);
void makeStringWaiter(int waiter_id);

#endif // IPC_UTILS_H
