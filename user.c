/**
 * Author: Taylor Freiner
 * Date: November 24th, 2017
 * Log: Sending info to oss through message array
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
	
	//LOCAL VARIABLES================================
	struct sembuf sb;
	pStruct* pBlock;
	pageStruct* pageTable;
	srand(time(NULL) ^ (getpid()<<16));
	int index = atoi(argv[1]);
	int pid = atoi(argv[2]);
	int i;
	int randFrameValue = rand() % 32;
	int frame = 0; 
	bool read = rand() % 2;
	int memoryReferences = 0;
	int randTerminate = rand() % (1100 + 1 - 900) + 900;
	sb.sem_op = -1;
	sb.sem_num = index;
	sb.sem_flg = 0;
	//=================================LOCAL VARIABLES
	
	//SHARED MEMORY===================================
	key_t key = ftok("keygen", 1);
	key_t key2 = ftok("keygen2", 1);
	key_t key3 = ftok("keygen3", 1);
	key_t key4 = ftok("keygen4", 1);
	key_t key5 = ftok("keygen5", 1);
	
	int memid = shmget(key, sizeof(int*)*2, IPC_CREAT | 0644);
	int memid2 = shmget(key2, sizeof(struct pageStruct) * 30, IPC_CREAT | 0644);
	int memid3 = shmget(key3, sizeof(int*)*3, IPC_CREAT | 0644);
	int memid4 = shmget(key4, sizeof(struct pStruct) * MAX_PROCESSES, 0);
	int semid = semget(key5, 17, 0);

	int *clock = (int *)shmat(memid, NULL, 0);
	pageTable = (struct pageStruct *)shmat(memid2, NULL, 0);	
	int *shmMsg = (int *)shmat(memid3, NULL, 0);
	pBlock = (pStruct *)shmat(memid4, NULL, 0);
	//====================================SHARED MEMORY
	
	while(memoryReferences < randTerminate){
		frame = index * randFrameValue * 1024;
		if(read){
			semop(semid, &sb, 1);
			shmMsg[0] = index;
			shmMsg[1] = 0; //read
			shmMsg[2] = frame;
		}else{
			semop(semid, &sb, 1);
			shmMsg[0] = index;
			shmMsg[1] = 1; //write
			shmMsg[2] = frame;
		}
	}
	semop(semid, &sb, 1);
	shmMsg[0] = index;
	shmMsg[1] = 2; //terminate
	shmMsg[2] = frame;
	
	exit(0);
}
