/**
 * Author: Taylor Freiner
 * Date: November 21st, 2017
 * Log: Setting up shared memory
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdbool.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>
#include <math.h>
#include "pageStruct.h"
#include "pstruct.h"

#define BIT_COUNT 32
#define MAX_PROCESSES 18
#define ACTUAL_PROCESSES 12
#define MAX_MEMORY 256

struct sembuf sb;
int sharedmem[5];
int processIds[10000];
int globalProcessCount = 0;
int lineCount = 0;
int processCount = 0;
int clearProcess[18];

union semun{
	int val;
};

void clean(int sig){
	if(sig == 2)
		fprintf(stderr, "Interrupt signaled. Removing shared memory and killing processes.\n");
	else if(sig == 14)
		fprintf(stderr, "Program reached 2 seconds. Removing shared memory and killing processes.\n");	
	else if(sig == 11)
		fprintf(stderr, "Seg fault caught. Removing shared memory and killing processes. Please re-run program.\n");
	
	shmctl(sharedmem[0], IPC_RMID, NULL);
	shmctl(sharedmem[1], IPC_RMID, NULL);
	semctl(sharedmem[2], 0, IPC_RMID);
	shmctl(sharedmem[3], IPC_RMID, NULL);
	shmctl(sharedmem[4], IPC_RMID, NULL);
	
	exit(1);
}

void setBit(int bitArray[], int i){
	bitArray[i/BIT_COUNT] |= 1 << (i % BIT_COUNT);
}

void unsetBit(int bitArray[], int i){
	bitArray[i/BIT_COUNT] &= ~(1 << (i % BIT_COUNT));
}

bool checkBit(int bitArray[], int i){
	return ((bitArray[i/BIT_COUNT] & (1 << (i % BIT_COUNT))) != 0);
}

int main(int argc, char* argv[]){
	union semun arg;
	arg.val = 1;
	int i, j, option;
	int bitProcessArray[1] = { 0 };
	int bitFrameArray[8] = { 0 };
	pageStruct* pageTable;
	pStruct* pBlock;
	bool tableFull = 0;
	int frameArray[30][32]; // 30 page tables and 32 pages per page
	bool verbose = false;

	//SIGNAL HANDLING
	signal(SIGINT, clean);
	signal(SIGALRM, clean);
	signal(SIGSEGV, clean);
	alarm(2);

	//FILE MANAGEMENT
	FILE *file = fopen("log.txt", "w");
	
	if(file == NULL){
		fprintf(stderr, "%s: ", argv[0]);
		perror("Error: \n");
		exit(1);
	}

	//OPTIONS**********
	if (argc != 1 && argc != 2){
		fprintf(stderr, "%s Error: Incorrect number of arguments\n", argv[0]);
		exit(1);
	}
	while ((option = getopt(argc, argv, "hv")) != -1){
		switch (option){
			case 'h':
				printf("Usage: %s <-v>\n", argv[0]);
				printf("\t-v: verbose logging on\n");
				return 0;
				break;
			case 'v':
				verbose = true;
				break;
			case '?':
				fprintf(stderr, "%s Error: Usage: %s <-v>\n", argv[0], argv[0]);
				exit(1);
				break;
		}
	}
	//**********OPTIONS
	//SHARED MEMORY**********
	key_t key = ftok("keygen", 1);
	key_t key2 = ftok("keygen2", 1);
	key_t key3 = ftok("keygen3", 1);
	key_t key4 = ftok("keygen4", 1);
	key_t key5 = ftok("keygen5", 1);
	int memid = shmget(key, sizeof(int*)*2, IPC_CREAT | 0644);
	int memid2 = shmget(key2, sizeof(struct pageStruct) * 30, IPC_CREAT | 0644);
	int memid3 = shmget(key3, sizeof(int*)*3, IPC_CREAT | 0644);
	int memid4 = shmget(key4, sizeof(struct pStruct) * MAX_PROCESSES, IPC_CREAT | 0644);
	int semid = semget(key5, 1, IPC_CREAT | 0644);
	if(memid == -1 || memid2 == -1 || memid3 == -1 || memid4 == -1){
		fprintf(stderr, "%s: ", argv[0]);
		perror("Error: \n");
	}
	sharedmem[0] = memid;
	sharedmem[1] = memid2;
	sharedmem[2] = memid3;
	sharedmem[3] = memid4;
	sharedmem[4] = semid;
	int *clock = (int *)shmat(memid, NULL, 0);
	pageTable = (struct pageStruct *)shmat(memid2, NULL, 0);
	int *shmMsg = (int *)shmat(memid3, NULL, 0);
	pBlock = (struct pStruct *)shmat(memid4, NULL, 0);
	if(*clock == -1 || (int*)pageTable == (int*)-1 || (int*)pBlock == (int*)-1){
		fprintf(stderr, "%s: ", argv[0]);
		perror("Error: \n");
		clean(1);
	}
	int clockVal = 0;
	int messageVal = -1;
	for(i = 0; i < 3; i++){
		if(i != 2){
			memcpy(&clock[i], &clockVal, 4);
		}
		memcpy(&shmMsg[i], &messageVal, 4);
	}
	
	semctl(semid, 0, SETVAL, 1, arg);
	sb.sem_op = 1;
	sb.sem_num = 0;
	sb.sem_flg = 0;
	semop(semid, &sb, 1);
	if(errno){
		fprintf(stderr, "....%s\n", strerror(errno));
		clean(1);
	}
	//**********SHARED MEMORY
	//CREATING PROCESSES
	int incrementTime;
	int lastForkTime[2];
	int processIndex = 0;
	int totalProcessNum = 0;
	lastForkTime[0] = clock[0];
	lastForkTime[1] = clock[1];
	pid_t childpid;
	srand(time(NULL));

	while(1){
		incrementTime = rand() % 1000;
		clock[0] += 1;
		if(clock[1] + incrementTime >= 1000000000){
			clock[1] = (clock[1] + incrementTime) % 1000000000;
			clock[0]++;
		}
		else
			clock[1] += incrementTime;
		if(processCount == MAX_PROCESSES){
			for(i = 0; i < MAX_PROCESSES; i++){
				if(clearProcess[i] == 1){
					for(j = 0; j < MAX_PROCESSES; j++){
						clearProcess[j] = 0;
					}
					unsetBit(bitProcessArray, i);
					processCount--;
					processIndex = i;
					clearProcess[i] = 0;
					break;
				}
			}
		}
		if((clock[0] - lastForkTime[0]) >= 1 || (clock[0] == lastForkTime[0] && clock[1] - lastForkTime[1] > 500000000)){
			lastForkTime[0] = clock[0];
			lastForkTime[1] = clock[1];
			for(i = 0; i < MAX_PROCESSES; i++){
				if(checkBit(bitProcessArray, i) == 0){
					tableFull = 0;
					setBit(bitProcessArray, i);
					break;
				}
				tableFull = 1;
			}
			if(!tableFull){
				childpid = fork();
				
				if(errno){
					fprintf(stderr, "%s", strerror(errno));
					clean(1);
				}
				if(childpid == 0){
					char arg[12];
					sprintf(arg, "%d", processIndex);
					execl("./user", "user", arg, NULL);
				}else{
					processIds[globalProcessCount] = childpid;
					pBlock[processIndex].pid = childpid;
					processCount++;
					globalProcessCount++;
					processIndex++;
					totalProcessNum++;
				}
				if(errno){
					fprintf(stderr, "%s\n", strerror(errno));
					exit(1);
				}
			}
		}
	}

	sleep(5);
	fclose(file);
	clean(1);
	
	return 0;
}
