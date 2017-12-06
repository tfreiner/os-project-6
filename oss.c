/**
 * Author: Taylor Freiner
 * Date: December 6th, 2017
 * Log: Finishing daemon
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

//GLOBAL VARIABLES==================
struct sembuf sb;
int sharedmem[5];
int processIds[10000];
int globalProcessCount = 0;
int processCount = 0;
int clearProcess[MAX_PROCESSES];
int queue[MAX_PROCESSES][3];
int queueCount = 0;
int front = 0;
int back = -1;
bool verbose = false;
int lineCount = 0;
int lastRequest[3];
int memAccess = 0;
int pageFaultGlobal = 0;
int termProcs = 0;
int second = 0;
int daemonCount = 0;
//==================GLOBAL VARIABLES

//Semaphore union
union semun{
	int val;
};

//Cleans all shared memory on exit
void clean(int sig){
	int i;
	if(sig == 2)
		fprintf(stderr, "Interrupt signaled. Removing shared memory and killing processes.\n");
	else if(sig == 14)
		fprintf(stderr, "Program reached 2 seconds. Removing shared memory and killing processes.\n");	
	else if(sig == 11)
		fprintf(stderr, "Seg fault caught. Removing shared memory and killing processes. Please re-run program.\n");
	
	for(i = 0; i < globalProcessCount; i++){
		kill(processIds[i], SIGKILL);
	}
	shmctl(sharedmem[0], IPC_RMID, NULL);
	shmctl(sharedmem[1], IPC_RMID, NULL);
	shmctl(sharedmem[2], IPC_RMID, NULL);
	shmctl(sharedmem[3], IPC_RMID, NULL);
	semctl(sharedmem[4], 0, IPC_RMID);
	double avFault = pageFaultGlobal / (double)memAccess;
	double avMemAccess = memAccess / (double)2;
	double throughput = 0.0;
	if(second > 0)
		throughput = termProcs / (double)second;
	else
		throughput = termProcs;
	printf("The daemon ran %d times\n", daemonCount);
	printf("There was an average of %lf memory accesses per second.\n", avMemAccess);
	printf("There was an average of %lf page faults per memory request.\n", avFault);
	printf("Throughput: There was an average of %lf terminated processes per simulated second.\n", throughput);
	exit(1);
}

//Functions to handle bits====================================
void setBit(int bitArray[], int i){
	bitArray[i/BIT_COUNT] |= 1 << (i % BIT_COUNT);
}

void unsetBit(int bitArray[], int i){
	bitArray[i/BIT_COUNT] &= ~(1 << (i % BIT_COUNT));
}

bool checkBit(int bitArray[], int i){
	return ((bitArray[i/BIT_COUNT] & (1 << (i % BIT_COUNT))) != 0);
}
//===================================Functions to handle bits

void checkFrames(int (*)[2], int*, int*);

void daemonFunc(int (*)[2], int*, int*);

void checkMessages(int (*)[19], int*, FILE*, pStruct*, int(*)[2], int*, int*);

void printMemMap(FILE*, int*, int*, int(*)[2]);

int main(int argc, char* argv[]){

	//LOCAL VARIABLES======================
	union semun arg;
	arg.val = 1;
	int i, j, option;
	int bitProcessArray[1] = { 0 };
	int bitFrameArray[8] = { 0 };
	int bitFrameArray2[8] = { 0 };
	int sysMem[256][2];
	pageStruct* pageTable;
	pStruct* pBlock;
	bool tableFull = 0;
	int lastForkTime[2];
	int lastSecond;
	int processIndex = 0;
	int totalProcessNum = 0;
	for(i = 0; i < 256; i++){
		sysMem[i][0] = -1;
		sysMem[i][1] = -1;
	}

	for(i = 0; i < 18; i++){
		queue[i][0] = -1;
		queue[i][1] = -1;
		queue[i][2] = -1;
	}
	//=====================LOCAL VARIABLES

	lastRequest[0] = -1;
	lastRequest[1] = -1;
	lastRequest[2] = -1;

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

	//OPTIONS===========================
	if(argc != 1 && argc != 2){
		fprintf(stderr, "%s Error: Incorrect number of arguments\n", argv[0]);
		exit(1);
	}
	while((option = getopt(argc, argv, "hv")) != -1){
		switch (option){
			case 'h':
				printf("Usage: %s <-v>\n", argv[0]);
				printf("\t-v: turn verbose logging on\n");
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
	//================================OPTIONS
	
	//SHARED MEMORY========================
	key_t key = ftok("keygen", 1);
	key_t key2 = ftok("keygen2", 1);
	key_t key3 = ftok("keygen3", 1);
	key_t key4 = ftok("keygen4", 1);
	key_t key5 = ftok("keygen5", 1);

	int memid = shmget(key, sizeof(int*)*2, IPC_CREAT | 0644);
	int memid2 = shmget(key2, sizeof(struct pageStruct) * 30, IPC_CREAT | 0644);
	int memid3 = shmget(key3, sizeof(int*)*3*19, IPC_CREAT | 0644);
	int memid4 = shmget(key4, sizeof(struct pStruct) * MAX_PROCESSES, IPC_CREAT | 0644);
	int semid = semget(key5, 19, IPC_CREAT | 0644);

	if(memid == -1 || memid2 == -1 || memid3 == -1 || memid4 == -1 || semid == -1){
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
	int (*shmMsg)[19] = shmat(memid3, NULL, 0);
	pBlock = (struct pStruct *)shmat(memid4, NULL, 0);

	if(*clock == -1 || (int*)pageTable == (int*)-1 || *shmMsg == (int*)-1 || (int*)pBlock == (int*)-1){
		fprintf(stderr, "%s: ", argv[0]);
		perror("Error: \n");
		clean(1);
	}
	int clockVal = 0;
	int messageVal = -1;
	int queueFrontVal = 0;
	for(i = 0; i < 18; i++){
		if(i != 2){
			memcpy(&clock[i], &clockVal, 4);
		}
		memcpy(&shmMsg[i][0], &messageVal, 4);
		memcpy(&shmMsg[i][1], &messageVal, 4);
		memcpy(&shmMsg[i][2], &messageVal, 4);
	}
	memcpy(&shmMsg[18][0], &queueFrontVal, 4);
	memcpy(&shmMsg[18][1], &messageVal, 4);
	memcpy(&shmMsg[18][2], &queueFrontVal, 4);
	for(i = 0; i < 19; i++){
		semctl(semid, i, SETVAL, 1, arg);
	}

	sb.sem_op = 1;
	sb.sem_flg = 0;
	for(sb.sem_num = 0; sb.sem_num < 18; sb.sem_num++)
		semop(semid, &sb, 1);

	sb.sem_op = 1;
	sb.sem_num = 18;
	semop(semid, &sb, 1);

	//===========================SHARED MEMORY

	lastForkTime[0] = clock[0];
	lastForkTime[1] = clock[1];
	lastSecond = clock[0];
	pid_t childpid;
	srand(time(NULL));
	while(lineCount < 100000){
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

		if((clock[0] == 0 && clock[1] == 0) || (clock[0] - lastForkTime[0]) >= 1 || (clock[0] == lastForkTime[0] && clock[1] - lastForkTime[1] > 500000000)){
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

		checkFrames(sysMem, bitFrameArray, bitFrameArray2);
		checkMessages(shmMsg, clock, file, pBlock, sysMem, bitFrameArray, bitFrameArray2);
	
		if(clock[0] > lastSecond){
			lastSecond = clock[0];
			printMemMap(file, bitFrameArray, bitFrameArray2, sysMem);
		}
	}

	sleep(10);
	fclose(file);
	clean(1);
	
	return 0;
}

void checkFrames(int (*sysMem)[2], int *bitFrameArray, int *bitFrameArray2){
	int i, frameIndex;
	for(i = 0; i < 256; i++){
		if(sysMem[i][0] != -1 && sysMem[i][0] != -2){
			frameIndex = i + 1;
		}else{
			break;
		}
	}
	if(frameIndex > 100)
		daemonFunc(sysMem, bitFrameArray, bitFrameArray2);
	return;
}

void daemonFunc(int (*sysMem)[2], int *bitFrameArray, int *bitFrameArray2){
	int i;
	daemonCount++;
	for(i = 0; i < 256; i++){
		if(sysMem[i][0] == -2 && !checkBit(bitFrameArray2, i)){
			setBit(bitFrameArray, i);
			sysMem[i][0] = -1;
			sysMem[i][1] = -1;
		}else if(checkBit(bitFrameArray2, i)){
			unsetBit(bitFrameArray2, i);
			sysMem[i][0] = -2;
			sysMem[i][1] = -2;
		}
	}

	return;
}

void checkMessages(int (*shmMsg)[19], int *clock, FILE* file, pStruct *pBlock, int (*sysMem)[2], int *bitFrameArray, int *bitFrameArray2){
	int i;
	bool pageFault = false;
	int frameIndex = -1;

	if(shmMsg[shmMsg[18][0]][0] == -1 || shmMsg[shmMsg[18][0]][2] == -1 || shmMsg[18][0] == -1 || (lastRequest[0] == shmMsg[shmMsg[18][0]][0] && lastRequest[1] == shmMsg[shmMsg[18][0]][1] && lastRequest[2] == shmMsg[shmMsg[18][0]][2])){
		return;
	}

	for(i = 0; i < 256; i++){
		if(sysMem[i][0] == shmMsg[shmMsg[18][0]][0] && sysMem[i][1] == shmMsg[shmMsg[18][0]][2]){
			break;
		}
		if(frameIndex == -1 && sysMem[i][0] == -1){
			frameIndex = i;
		}
		pageFault = true;
	}

	lastRequest[0] = shmMsg[shmMsg[18][0]][0];
	lastRequest[1] = shmMsg[shmMsg[18][0]][1];
	lastRequest[2] = shmMsg[shmMsg[18][0]][2];
	
	if(shmMsg[shmMsg[18][0]][1] == 0){ //read
		memAccess++;
		if(!pageFault){
			if(clock[1] + 10 > 1000000000){
				clock[1] = (clock[1] + 10) % 1000000000;
				clock[0]++;
			}else{
				clock[1] += 10;
			}
			second = clock[0];
			if(verbose){
				fprintf(file, "P%d is requesting reading from page %d\n", shmMsg[shmMsg[18][0]][0], shmMsg[shmMsg[18][0]][2]);
				lineCount++;
				fprintf(file, "OSS did not detect a page fault and is granting P%d's request to read from page %d\n", shmMsg[shmMsg[18][0]][0], shmMsg[shmMsg[18][0]][2]);
				lineCount++;
			}
			sb.sem_num = shmMsg[shmMsg[18][0]][0];
			sb.sem_op = 1;
			semop(sharedmem[4], &sb, 1);
		}else{
			pageFaultGlobal++;
			if(clock[1] + 15000000 > 1000000000){
				clock[1] = (clock[1] + 15000000) % 1000000000;
				clock[0]++;
			}else{
				clock[1] += 15000000;
			}
			second = clock[0];
			sysMem[frameIndex][0] = shmMsg[shmMsg[18][0]][0];
			sysMem[frameIndex][1] = shmMsg[shmMsg[18][0]][2];
			unsetBit(bitFrameArray, frameIndex);
			setBit(bitFrameArray2, frameIndex);
			if(verbose){
				fprintf(file, "P%d is requesting reading from page %d\n", shmMsg[shmMsg[18][0]][0], shmMsg[shmMsg[18][0]][2]);
				lineCount++;
				fprintf(file, "OSS detected a page fault and is bringing the page into memory\n");
				lineCount++;
				fprintf(file, "OSS brought the page into memory and is granting P%d's request to read from page %d\n", shmMsg[shmMsg[18][0]][0], shmMsg[shmMsg[18][0]][2]);
				lineCount++;
			}
			sb.sem_num = shmMsg[shmMsg[18][0]][0];
			sb.sem_op = 1;
			semop(sharedmem[4], &sb, 1);
		}
		if(verbose)
			printMemMap(file, bitFrameArray, bitFrameArray2, sysMem);
	}else if(shmMsg[shmMsg[18][0]][1] == 1){ //write
		memAccess++;
		if(!pageFault){
			if(clock[1] + 10 > 1000000000){
				clock[1] = (clock[1] + 10) % 1000000000;
				clock[0]++;
			}else{
				clock[1] += 10;
			}
			second = clock[0];
			if(verbose){
				fprintf(file, "P%d is requesting writing to page %d\n", shmMsg[shmMsg[18][0]][0], shmMsg[shmMsg[18][0]][2]);
				lineCount++;
				fprintf(file, "OSS did not detect a page fault and is granting P%d's request to write to page %d\n", shmMsg[shmMsg[18][0]][0], shmMsg[shmMsg[18][0]][2]);
				lineCount++;
			}
			sb.sem_num = shmMsg[shmMsg[18][0]][0];
			sb.sem_op = 1;
			semop(sharedmem[4], &sb, 1);
		}else{
			pageFaultGlobal++;
			if(clock[1] + 15000000 > 1000000000){
				clock[1] = (clock[1] + 15000000) % 1000000000;
				clock[0]++;
			}else{
				clock[1] += 15000000;
			}
			second = clock[0];
			sysMem[frameIndex][0] = shmMsg[shmMsg[18][0]][0];
			sysMem[frameIndex][1] = shmMsg[shmMsg[18][0]][2];
			unsetBit(bitFrameArray, frameIndex);
			setBit(bitFrameArray2, frameIndex);
			if(verbose){
				fprintf(file, "P%d is requesting writing to page %d\n", shmMsg[shmMsg[18][0]][0], shmMsg[shmMsg[18][0]][2]);
				lineCount++;
				fprintf(file, "OSS detected a page fault and is bringing the page into memory\n");
				lineCount++;
				fprintf(file, "OSS brought the page into memory and is granting P%d's request to write to page %d\n", shmMsg[shmMsg[18][0]][0], shmMsg[shmMsg[18][0]][2]);
				lineCount++;
			}
			sb.sem_num = shmMsg[shmMsg[18][0]][0];
			sb.sem_op = 1;
			semop(sharedmem[4], &sb, 1);
		}
		if(verbose)
			printMemMap(file, bitFrameArray, bitFrameArray2, sysMem);
	}else if(shmMsg[shmMsg[18][0]][1] == 2){ //terminate
		termProcs++;
		fprintf(file, "Master has acknowledged P%d is terminating.\n", shmMsg[shmMsg[18][0]][0]);
		clearProcess[shmMsg[shmMsg[18][0]][0]] = 1;
		kill(pBlock[shmMsg[shmMsg[18][0]][0]].pid, SIGKILL);
		waitpid(pBlock[shmMsg[shmMsg[18][0]][0]].pid, NULL, 0);
		sb.sem_num = shmMsg[shmMsg[18][0]][0];
		sb.sem_op = 1;
		semop(sharedmem[4], &sb, 1);
		if(verbose)
			printMemMap(file, bitFrameArray, bitFrameArray2, sysMem);
	}
	return;
}

void printMemMap(FILE* file, int* bitFrameArray, int* bitFrameArray2, int (*sysMem)[2]){
	int i;
	for(i = 0; i < 256; i++){
		if(sysMem[i][0] == -1){
			if(checkBit(bitFrameArray, i))
				fprintf(file, "D");
			else
				fprintf(file, ".");
		}
		else if(!checkBit(bitFrameArray, i))
			fprintf(file, "U");
	}
	fprintf(file, "\n\n");
	for(i = 0; i < 256; i++){
		if(sysMem[i][0] == -1)
			fprintf(file, ".");
		else if(checkBit(bitFrameArray2, i))
			fprintf(file, "1");
		else
			fprintf(file, "0");	
	}
	fprintf(file, "\n\n");
	return;
}
