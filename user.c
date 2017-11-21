/**
 * Author: Taylor Freiner
 * Date: November 21st, 2017
 * Log: Setting up shared memory
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/wait.h>
#include "pageStruct.h"
#include "pstruct.h"

#define MAX_PROCESSES 18

int main(int argc, char* argv[]){
	struct sembuf sb;
	pStruct* pBlock;
	pageStruct* pageTable;

	srand(time(NULL) ^ (getpid()<<16));
	int index = atoi(argv[1]);
	int i;

	//SHARED MEMORY
	key_t key = ftok("keygen", 1);
	key_t key2 = ftok("keygen2", 1);
	key_t key3 = ftok("keygen3", 1);
	key_t key4 = ftok("keygen4", 1);
	key_t key5 = ftok("keygen5", 1);
	int memid = shmget(key, sizeof(int*)*2, IPC_CREAT | 0644);
	int memid2 = shmget(key2, sizeof(struct pageStruct) * 30, IPC_CREAT | 0644);
	int memid3 = shmget(key3, sizeof(int*)*3, IPC_CREAT | 0644);
	int memid4 = shmget(key4, sizeof(struct pStruct) * MAX_PROCESSES, 0);
	int semid = semget(key5, 1, 0);

	int *clock = (int *)shmat(memid, NULL, 0);
	pageTable = (pStruct *)shmat(memid2, NULL, 0);	
	int *shmMsg = (int *)shmat(memid3, NULL, 0);
	pBlock = (pStruct *)shmat(memid4, NULL, 0);
	
	exit(0);
}
