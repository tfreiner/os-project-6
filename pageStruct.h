/**
 * Author: Taylor Freiner
 * Date: November 21st, 2017
 * Log: Setting up page table
 */
typedef struct pageStruct{
	int size;
	int pages[32];  //1 or 0 if page is in system memory
}pageStruct;
