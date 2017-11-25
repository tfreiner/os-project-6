/**
 * Author: Taylor Freiner
 * Date: November 25th, 2017
 * Log: Adding shared message queue
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

int queueCount = 0;
int front = 0;
int back = -1;

bool isQueueFull(){
	return queueCount == MAX_PROCESSES;
}
 
bool isQueueEmpty(){
	return queueCount == 0;
}
 
int* peek(int (*shmMsg)[19]){
	return shmMsg[shmMsg[18][0]];
}
 
void insert(int index, int request, int page, int (*shmMsg)[19]){
	if(!isQueueFull()){
		if(shmMsg[18][1] == 17)
			shmMsg[18][1] = -1;
		shmMsg[++shmMsg[18][1]][0] = index;
		shmMsg[shmMsg[18][1]][1] = request;
		shmMsg[shmMsg[18][1]][2] = page;
		shmMsg[18][2]++; //queue count
	}
}
 
void delete(int** shmMsg){
	if(!isQueueEmpty()){
		shmMsg[shmMsg[18][0]++][0] = -1;
		shmMsg[shmMsg[18][0]][1] = -1;
		shmMsg[shmMsg[18][0]][2] = -1;
		if(shmMsg[18][0] == 18)
			shmMsg[18][0] = 0;
		shmMsg[18][2]--; //queue count
	}
}

int main(int argc, char* argv[]){

	//LOCAL VARIABLES================================	
	FILE *file = fopen("logUser.txt", "a");
	if(file == NULL){
		fprintf(stderr, "%s: ", argv[0]);
		perror("Error: \n");
		exit(1);
	}

	struct sembuf sb;
	pStruct* pBlock;
	pageStruct* pageTable;
	srand(time(NULL) ^ (getpid()<<16));
	int index = atoi(argv[1]);
	int randFrameValue = rand() % 32;
	int frame = 0; 
	bool read = rand() % 2;
	int memoryReferences = 0;
	int randTerminate = rand() % (1100 + 1 - 900) + 900;
	
	sb.sem_op = 1;
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
	int memid3 = shmget(key3, sizeof(int*)*3*19, IPC_CREAT | 0644);
	int memid4 = shmget(key4, sizeof(struct pStruct) * MAX_PROCESSES, 0);
	int semid = semget(key5, 19, 0);
	
	if(memid == -1 || memid2 == -1 || memid3 == -1 || memid4 == -1 || semid == -1){
		fprintf(stderr, "%s: ", argv[0]);
		perror("Error: \n");
	}
	int *clock = (int *)shmat(memid, NULL, 0);
	pageTable = (struct pageStruct *)shmat(memid2, NULL, 0);	
	int (*shmMsg)[19] = shmat(memid3, NULL, 0);
	pBlock = (struct pStruct *)shmat(memid4, NULL, 0);
	//====================================SHARED MEMORY

	while(memoryReferences < randTerminate){
		sb.sem_op = -1;
		sb.sem_num = 19;
		read = rand() % 2;
		frame = index * randFrameValue * 1024;
		if(read){
			semop(semid, &sb, 1);
			insert(index, 1, frame, shmMsg);
			memoryReferences++;
			sb.sem_op = 0;
			sb.sem_num = index;
			semop(semid, &sb, 1);
			sb.sem_op = 1;
			sb.sem_num = 19;
			semop(semid, &sb, 1);
		}else{
			semop(semid, &sb, 1);
			insert(index, 0, frame, shmMsg);
			memoryReferences++;
			semop(semid, &sb, 1);
			sb.sem_op = 1;
			sb.sem_num = 19;
			semop(semid, &sb, 1);
		}
		exit(1);
	}

	semop(semid, &sb, 1);
	insert(index, 2, frame, shmMsg);
	exit(0);
}
