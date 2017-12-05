/**
 * Author: Taylor Freiner
 * Date: December 3rd, 2017
 * Log: Setting up daemon
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

bool isQueueFull(int (*shmMsg)[19]){
	return shmMsg[18][2] == MAX_PROCESSES;
}
 
bool isQueueEmpty(int (*shmMsg)[19]){
	return shmMsg[18][2] == 0;
}
 
int* peek(int (*shmMsg)[19]){
	return shmMsg[shmMsg[18][0]];
}
 
void insert(int index, int request, int page, int (*shmMsg)[19]){
	if(!isQueueFull(shmMsg)){
		if(shmMsg[18][1] == 17)
			shmMsg[18][1] = -1;
		shmMsg[18][1] += 1;
		shmMsg[shmMsg[18][1]][0] = index;
		shmMsg[shmMsg[18][1]][1] = request;
		shmMsg[shmMsg[18][1]][2] = page;
		shmMsg[18][2]++; //queue count
	}
}
 
void delete(int (*shmMsg)[19]){
	if(!isQueueEmpty(shmMsg)){
		shmMsg[18][0] += 1;
		if(shmMsg[18][0] == 18)
			shmMsg[18][0] = 0;
		shmMsg[18][2]--; //queue count
	}
}

int main(int argc, char* argv[]){

	//LOCAL VARIABLES================================	
	struct sembuf sb;
	srand(time(NULL) ^ (getpid()<<16));
	int index = atoi(argv[1]);
	int randFrameValue = rand() % 32;
	int frame = 0; 
	bool read = rand() % 2;
	bool terminate = false;
	int memoryReferences = 0;
	int randTerminate = rand() % (1100 + 1 - 900) + 900;
	//=================================LOCAL VARIABLES
	
	//SHARED MEMORY===================================
	key_t key = ftok("keygen", 1);
	key_t key2 = ftok("keygen2", 1);
	key_t key3 = ftok("keygen3", 1);
	key_t key4 = ftok("keygen4", 1);
	key_t key5 = ftok("keygen5", 1);
	
	int memid = shmget(key, sizeof(int*)*2, IPC_CREAT | 0644);
	int memid2 = shmget(key2, sizeof(struct pageStruct) * 30, IPC_CREAT | 0644);
	int memid3 = shmget(key3, sizeof(int*)*3*19, IPC_CREAT | 0644);
	int memid4 = shmget(key4, sizeof(struct pStruct) * MAX_PROCESSES, IPC_CREAT | 0644);
	int semid = semget(key5, 19, 0);
	
	if(memid == -1 || memid2 == -1 || memid3 == -1 || memid4 == -1 || semid == -1){
		fprintf(stderr, "%s: ", argv[0]);
		perror("Error: \n");
	}
	int (*shmMsg)[19] = shmat(memid3, NULL, 0);
	//====================================SHARED MEMORY
	
	while(memoryReferences < randTerminate || !terminate){
		if(memoryReferences >= randTerminate)
			memoryReferences = 0;
		read = rand() % 2;
		terminate = rand() % 2;
		randFrameValue = rand() % 32;
		frame = (index+1) * randFrameValue * 1024;
		if(read){
			sb.sem_op = -1;
			sb.sem_num = 18;
			semop(semid, &sb, 1);

			insert(index, 1, frame, shmMsg);
			
			sb.sem_op = 1;
			sb.sem_num = 18;
			semop(semid, &sb, 1);

			memoryReferences++;

			sb.sem_op = -1;
			sb.sem_num = index;
			sb.sem_flg = 0;
			semop(semid, &sb, 1);
			delete(shmMsg);
		}else{
			sb.sem_op = -1;
			sb.sem_num = 18;
			semop(semid, &sb, 1);
	
			insert(index, 0, frame, shmMsg);
			
			sb.sem_op = 1;
			sb.sem_num = 18;
			semop(semid, &sb, 1);
	
			memoryReferences++;
	
			sb.sem_op = -1;
			sb.sem_num = index;
			sb.sem_flg = 0;
			semop(semid, &sb, 1);
			delete(shmMsg);
		}
	}

	sb.sem_op = -1;
	sb.sem_num = 18;
	semop(semid, &sb, 1);
	
	insert(index, 2, frame, shmMsg);
	
	sb.sem_op = 1;
	sb.sem_num = 18;
	semop(semid, &sb, 1);

	sb.sem_op = -1;
	sb.sem_num = index;
	semop(semid, &sb, 1);
	delete(shmMsg);
	return 0;
}
