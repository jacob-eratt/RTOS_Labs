// filename ************** eFile.c *****************************
// High-level routines to implement a solid-state disk 
// Students implement these functions in Lab 4
// Jonathan W. Valvano 1/12/20
#include <stdint.h>
#include <string.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/CortexM.h"
#include <stdio.h>

#define DIRSECTOR 1
#define FATSECTOR 16
#define MAXSECTOR 2048
#define MAXFILES	11

Sector FAT[MAXSECTOR];
File Directory[MAXFILES];
uint8_t initialized = 0;
uint8_t isMounted = 0;
uint8_t writeBuffer[512];
uint16_t currentWrite;
uint16_t currentRead;
int16_t cursor;
uint8_t readBuffer[512];
uint8_t currDir;
uint8_t tempBuf[512*DIRSECTOR];
extern TCB* RunPt;

extern Sema4Type LCDFree; 


//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
int eFile_Init(void){ // initialize file system
	OS_Wait(&LCDFree);
  eDisk_Init(0);
	OS_Signal(&LCDFree);
	return 0;
}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){ // erase disk, add format
	unsigned long prev = OS_LockScheduler();
	// Clears directory
	for(int i = 0; i < MAXFILES; i++){
		Directory[i].entrySector = -1;
	}
	
	// Sets empty list at the top of directory
	Directory[0].entrySector = DIRSECTOR + FATSECTOR;
	Directory[0].numSector = 1;
	Directory[0].name[0] = '*';
	Directory[0].name[1] = '\0';
	
	// Has empty space point to eachother
	for(int i = DIRSECTOR + FATSECTOR; i < MAXSECTOR - 1; i++){
		FAT[i].nextSector = i+1;
		FAT[i].bytesRemaining = 512;
	}
	FAT[MAXSECTOR-1].nextSector = -1;
	FAT[MAXSECTOR-1].bytesRemaining = 512;
	
	// Writes to disk
	// memcpy(tempBuf, Directory, sizeof(Directory)); // Assumes that the directory fits in one sector
	for(int i = 0; i < MAXFILES; i++){
//		tempBuf[i*16 + 1] = Directory[i].name[1];
//		tempBuf[i*16 + 2] = Directory[i].name[2];
//		tempBuf[i*16 + 3] = Directory[i].name[3];
//		tempBuf[i*16 + 4] = Directory[i].name[4];
//		tempBuf[i*16 + 5] = Directory[i].name[5];
//		tempBuf[i*16 + 6] = Directory[i].name[6];
//		tempBuf[i*16 + 7] = Directory[i].name[7];
//		tempBuf[i*16 + 8] = Directory[i].name[8];
//		tempBuf[i*16 + 9] = Directory[i].name[9];
//		tempBuf[i*16 + 10] = Directory[i].name[10];
//		tempBuf[i*16 + 11] = Directory[i].name[11];

//		tempBuf[i*16 + 12] = Directory[i].numSector & 0xFF;
//		tempBuf[i*16 + 13] = (Directory[i].numSector >> 8) & 0xFF; //TEST TO MAKE SURE WORKS 

//		tempBuf[i*16 + 14] = Directory[i].entrySector & 0xFF;
//		tempBuf[i*16 + 15] = (Directory[i].entrySector >> 8) & 0xFF;
		memcpy(&tempBuf[i*16], Directory[i].name, sizeof(char[12]));
		memcpy(&tempBuf[i*16+12], &Directory[i].numSector, sizeof(uint16_t));
		memcpy(&tempBuf[i*16+14], &Directory[i].entrySector, sizeof(uint16_t));
	}
	
	if(eDisk_Write(0, tempBuf, 0, DIRSECTOR)){
		OS_UnLockScheduler(prev);
		return 1; // fail write
	}
	
	for(int i = DIRSECTOR; i < FATSECTOR + DIRSECTOR; i++){ // 16 sectors in fat
		// memcpy(tempBuf, (void*)(((int)FAT + (i - DIRSECTOR)*512)), 512);
		for(int j = 0; j < MAXSECTOR/FATSECTOR; j++){ // 128 structs per fat sector
//			tempBuf[j*4] = FAT[i*128 + j].nextSector & 0xFF;
//      tempBuf[j*4 + 1] = (FAT[i*128 + j].nextSector >> 8) & 0xFF;
//        
//      tempBuf[j*4 + 2] = FAT[i*128 + j].bytesRemaining & 0xFF;
//      tempBuf[j*4 + 3] = (FAT[i*128 + j].bytesRemaining >> 8) & 0xFF;
			memcpy(&tempBuf[j*4], &FAT[(i-1)*128 + j].nextSector, sizeof(uint16_t));
			memcpy(&tempBuf[j*4+2], &FAT[(i-1)*128 + j].bytesRemaining, sizeof(uint16_t));
		}
		if(eDisk_Write(0, tempBuf, i, 1)){
			OS_UnLockScheduler(prev);
			return 1; // fail write
		}
	}
	
	OS_UnLockScheduler(prev);
  return 0;   // replace
}

//---------- eFile_Mount-----------------
// Mount the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure
int eFile_Mount(void){ // initialize file system
	OS_Wait(&LCDFree);
	
	if(isMounted){
		OS_Signal(&LCDFree);
		return 0; // fail read
	}
	
	if(eDisk_Read(0, tempBuf, 0, DIRSECTOR)){
		OS_Signal(&LCDFree);
		return 1; // fail read
	}
//	while(sizeof(Directory) > 512){}; // DEBUGGUING, hangs if directory is larger than a sector somehow
//	memcpy(Directory, tempBuf, sizeof(Directory)); // Assumes that the directory fits in one sector
	
	for(int i = 0; i < MAXFILES; i++){
//		Directory[i].name[0] = tempBuf[i*16];
//		Directory[i].name[1] = tempBuf[i*16 + 1];
//		Directory[i].name[2] = tempBuf[i*16 + 2];
//		Directory[i].name[3] = tempBuf[i*16 + 3];
//		Directory[i].name[4] = tempBuf[i*16 + 4];
//		Directory[i].name[5] = tempBuf[i*16 + 5];
//		Directory[i].name[6] = tempBuf[i*16 + 6];
//		Directory[i].name[7] = tempBuf[i*16 + 7];
//		Directory[i].name[8] = tempBuf[i*16 + 8];
//		Directory[i].name[9] = tempBuf[i*16 + 9];
//		Directory[i].name[10] = tempBuf[i*16 + 10];
//		Directory[i].name[11] = tempBuf[i*16 + 11];
		memcpy(Directory[i].name, &tempBuf[i*16], sizeof(char[12]));
		memcpy(&Directory[i].numSector, &tempBuf[i*16+12], sizeof(uint16_t));
		memcpy(&Directory[i].entrySector, &tempBuf[i*16+14], sizeof(uint16_t));

		
//		Directory[i].numSector = tempBuf[i*16 + 12] + (tempBuf[i*16 + 13]<<8); //TEST TO MAKE SURE WORKS 
//		Directory[i].entrySector = tempBuf[i*16 + 14] + (tempBuf[i*16 + 15]<<8);
  	}
	TCB* pt = RunPt;
	for(int i = DIRSECTOR; i < FATSECTOR + DIRSECTOR; i++){ // 16 sectors in fat
		if(eDisk_Read(0, tempBuf, i, 1)){
			OS_Signal(&LCDFree);
			return 1; // fail read
		}
		//memcpy((void*)(((int)FAT + (i - DIRSECTOR)*512)), tempBuf, 512);
		for(int j = 0; j < 128; j++){ // 128 structs per fat sector
			//FAT[i*128 + j].nextSector = tempBuf[j*4] + (tempBuf[j*4 + 1]<<8);
			//FAT[i*128 + j].bytesRemaining = tempBuf[j*4 + 2] + (tempBuf[j*4 + 3]<<8);
			memcpy(&FAT[(i-1)*128 + j].nextSector, &tempBuf[j*4], sizeof(uint16_t));
			if (RunPt != pt){
				volatile int stop = 1;
			}
			memcpy(&FAT[(i-1)*128 + j].bytesRemaining, &tempBuf[j*4+2], sizeof(uint16_t));
			if (RunPt != pt){
				volatile int stop = 1;
			}
		}	
	}
	isMounted = 1;
	OS_Signal(&LCDFree);
  return 0;   // replace
}


//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( const char name[]){  // create new file, make it empty 
	if(!isMounted)
		return 1;
	
	OS_Wait(&LCDFree);
	
	// Checks if any free file
	uint8_t nextIdx = 0;
	for(int i = 1; i < MAXFILES; i++){
		if(strcmp(Directory[i].name, name) == 0 && Directory[i].entrySector >= 0){ // duplicate file name
			OS_Signal(&LCDFree);
			return 1;
		}
		else if(Directory[i].entrySector < 0) {
			nextIdx = i;
			break;
		}
	}
	// Fails if can't find a free file slot
	if(nextIdx < 1){
		OS_Signal(&LCDFree);
		return 1;
	}
	
	strcpy(Directory[nextIdx].name, name); // Sets file name
	Directory[nextIdx].entrySector = Directory[0].entrySector; // Current file's entry sector becomes first sector in empty list
	FAT[Directory[nextIdx].entrySector].bytesRemaining = 512; // Current file's entry sector should have all 512 bytes
	Directory[0].entrySector = FAT[Directory[0].entrySector].nextSector; // Empty list's entry sector gets changed to the next sector in empty list
	FAT[Directory[nextIdx].entrySector].nextSector = -1;
	
	OS_Signal(&LCDFree);
  return 0;   // replace
}


//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen( const char name[]){      // open a file for writing 
	if(!isMounted)
		return 1;
	
	OS_Wait(&LCDFree);
	
	// eFile_Mount();
	
	// Find file in directory
	uint8_t fileIdx = 0;
	for(int i = 0; i < MAXFILES; i++){
		if(strcmp(name, Directory[i].name) == 0 && Directory[i].entrySector >= DIRSECTOR + FATSECTOR){
			fileIdx = i;
			break;
		}
	}
	
	if(fileIdx == 0){
		return 1; // fails to find file in dir
	}
	
	currentWrite = Directory[fileIdx].entrySector;
	while(FAT[currentWrite].nextSector > 0){ // Gets the last sector/block in file
		currentWrite = FAT[currentWrite].nextSector;
	}
	
	eDisk_Read(0, writeBuffer, currentWrite, 1);
	
	
  return 0;   // replace  
}

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( const char data){
	
		if(((FAT[currentWrite].bytesRemaining) - 1) < 0){ // No more bytes remaining in sector
			FAT[currentWrite].nextSector = Directory[0].entrySector; // File next sector gets first sector from empty list
			Directory[0].entrySector = FAT[Directory[0].entrySector].nextSector; // First sector in empty list gets change to the next sector in empty list
			FAT[FAT[currentWrite].nextSector].bytesRemaining = 512; // Newly added last sector gets 512 bytes remaining
			FAT[FAT[currentWrite].nextSector].nextSector = -1; // Newly added sector's next sector is -1 (end of file)
			currentWrite = FAT[currentWrite].nextSector; // Current write index sector gets changed to newly added sector
		}
		
		writeBuffer[512 - (FAT[currentWrite].bytesRemaining)] = data;
		FAT[currentWrite].bytesRemaining--;
		if(eDisk_Write(0, writeBuffer, currentWrite, 1))
			return 1; // fails
	
    return 0;   // replace
}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void){ // close the file for writing
	// WRITE UPDATED DIR AND FAT TO DISK TO DO
	// Writes to disk

	// memcpy(tempBuf, Directory, sizeof(Directory));
	for(int i = 0; i < MAXFILES; i++){
//		tempBuf[i*16 + 1] = Directory[i].name[1];
//		tempBuf[i*16 + 2] = Directory[i].name[2];
//		tempBuf[i*16 + 3] = Directory[i].name[3];
//		tempBuf[i*16 + 4] = Directory[i].name[4];
//		tempBuf[i*16 + 5] = Directory[i].name[5];
//		tempBuf[i*16 + 6] = Directory[i].name[6];
//		tempBuf[i*16 + 7] = Directory[i].name[7];
//		tempBuf[i*16 + 8] = Directory[i].name[8];
//		tempBuf[i*16 + 9] = Directory[i].name[9];
//		tempBuf[i*16 + 10] = Directory[i].name[10];
//		tempBuf[i*16 + 11] = Directory[i].name[11];

//		tempBuf[i*16 + 12] = Directory[i].numSector & 0xFF;
//		tempBuf[i*16 + 13] = (Directory[i].numSector >> 8) & 0xFF; //TEST TO MAKE SURE WORKS 

//		tempBuf[i*16 + 14] = Directory[i].entrySector & 0xFF;
//		tempBuf[i*16 + 15] = (Directory[i].entrySector >> 8) & 0xFF;
		memcpy(&tempBuf[i*16], Directory[i].name, sizeof(char[12]));
		memcpy(&tempBuf[i*16+12], &Directory[i].numSector, sizeof(uint16_t));
		memcpy(&tempBuf[i*16+14], &Directory[i].entrySector, sizeof(uint16_t));
	}
	
	if(eDisk_Write(0, tempBuf, 0, DIRSECTOR))
		return 1; // fail write
	
	for(int i = DIRSECTOR; i < FATSECTOR + DIRSECTOR; i++){ // 16 sectors in fat
		// memcpy(tempBuf, (void*)(((int)FAT + (i - DIRSECTOR)*512)), 512);
		for(int j = 0; j < MAXSECTOR/FATSECTOR; j++){ // 128 structs per fat sector
//			tempBuf[j*4] = FAT[i*128 + j].nextSector & 0xFF;
//      tempBuf[j*4 + 1] = (FAT[i*128 + j].nextSector >> 8) & 0xFF;
//        
//      tempBuf[j*4 + 2] = FAT[i*128 + j].bytesRemaining & 0xFF;
//      tempBuf[j*4 + 3] = (FAT[i*128 + j].bytesRemaining >> 8) & 0xFF;
			memcpy(&tempBuf[j*4], &FAT[(i-1)*128 + j].nextSector, sizeof(uint16_t));
			memcpy(&tempBuf[j*4+2], &FAT[(i-1)*128 + j].bytesRemaining, sizeof(uint16_t));
			
		}
		if(eDisk_Write(0, tempBuf, i, 1))
			return 1; // fail write
	}
  OS_Signal(&LCDFree);
  return 0;   // replace
}


//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( const char name[]){      // open a file for reading 
  cursor = 0;
	if(!isMounted)
		return 1;
	
	OS_Wait(&LCDFree);
	
	// Find file in directory
	uint8_t fileIdx = 0;
	for(int i = 0; i < MAXFILES; i++){
		if(strcmp(name, Directory[i].name) == 0 && Directory[i].entrySector >= DIRSECTOR + FATSECTOR){
			fileIdx = i;
			break;
		}
	}
	
	if(fileIdx == 0){
		return 1; // fails to find file in dir
	}
	
	currentRead = Directory[fileIdx].entrySector; // Gets first sector/block in file
	eDisk_Read(0, readBuffer, currentRead, 1);
	
  return 0;   // replace   
}
 
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt){       // get next byte 
  
	if(cursor >= 512){ // needs to move to next sector
		cursor = 0;
		currentRead = FAT[currentRead].nextSector;
		eDisk_Read(0, readBuffer, currentRead, 1);
	}
	
	if(cursor >= (512 - FAT[currentRead].bytesRemaining)){ // Checks if no more data left
		return 1;
	}
		
	*pt = readBuffer[cursor];
	cursor++;
	
  return 0;   // replace
}
    
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){ // close the file for writing
  OS_Signal(&LCDFree);
  return 0;   // replace
}


//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( const char name[]){  // remove this file 
	if(!isMounted)
		return 1;
		
	OS_Wait(&LCDFree);
	
	uint8_t fileIdx = 0;
	for(int i = 0; i < MAXFILES; i++){ // looks for file
		if(strcmp(name, Directory[i].name) == 0 && Directory[i].entrySector >= DIRSECTOR + FATSECTOR){
			fileIdx = i;
			break;
		}
	}
	
	if(fileIdx == 0){
		return 1; // fails to find file in dir
	}
	
	uint16_t free = Directory[fileIdx].entrySector; // head of current file
	uint16_t prevEmptyHead = Directory[0].entrySector; // old file list head
	while(FAT[free].nextSector > 0){ // Gets the last sector/block in file
		free = FAT[free].nextSector;
	}
	Directory[0].entrySector = Directory[fileIdx].entrySector; 
	FAT[free].nextSector = prevEmptyHead;
	Directory[fileIdx].entrySector = -1;
	
	OS_Signal(&LCDFree);
	
  return 0;   // replace
}                             


//---------- eFile_DOpen-----------------
// Open a (sub)directory, read into RAM
// Input: directory name is an ASCII string up to seven characters
//        (empty/NULL for root directory)
// Output: 0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_DOpen( const char name[]){ // open directory
	if(!isMounted)
		return 1;
	
  OS_Wait(&LCDFree);
	currDir = 0;
	
  return 0;   // replace
}
  
//---------- eFile_DirNext-----------------
// Retreive directory entry from open directory
// Input: none
// Output: return file name and size by reference
//         0 if successful and 1 on failure (e.g., end of directory)
int eFile_DirNext( char *name[], unsigned long *size){  // get next entry 
  
	for(int i = currDir + 1; i < MAXFILES; i++){
		if(Directory[i].entrySector > 0){
			*name = Directory[i].name;
			currDir = i;
			uint16_t sectorCount = 0;
			uint16_t current = Directory[i].entrySector;
			while(FAT[current].nextSector > 0){
				sectorCount++;
				current = FAT[current].nextSector;
			}
			*size = sectorCount*512 + (512 - FAT[current].bytesRemaining);
			return 0;
		}
	}
	
	
  return 1;   // replace
}

//---------- eFile_DClose-----------------
// Close the directory
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_DClose(void){ // close the directory
  
	OS_Signal(&LCDFree);
  return 0;   // replace
}


//---------- eFile_Unmount-----------------
// Unmount and deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently mounted)
int eFile_Unmount(void){ 
	if(!isMounted)
		return 0;
	
	//memcpy(tempBuf, Directory, sizeof(Directory));
	
	for(int i = 0; i < MAXFILES; i++){
//		tempBuf[i*16 + 1] = Directory[i].name[1];
//		tempBuf[i*16 + 2] = Directory[i].name[2];
//		tempBuf[i*16 + 3] = Directory[i].name[3];
//		tempBuf[i*16 + 4] = Directory[i].name[4];
//		tempBuf[i*16 + 5] = Directory[i].name[5];
//		tempBuf[i*16 + 6] = Directory[i].name[6];
//		tempBuf[i*16 + 7] = Directory[i].name[7];
//		tempBuf[i*16 + 8] = Directory[i].name[8];
//		tempBuf[i*16 + 9] = Directory[i].name[9];
//		tempBuf[i*16 + 10] = Directory[i].name[10];
//		tempBuf[i*16 + 11] = Directory[i].name[11];

//		tempBuf[i*16 + 12] = Directory[i].numSector & 0xFF;
//		tempBuf[i*16 + 13] = (Directory[i].numSector >> 8) & 0xFF; //TEST TO MAKE SURE WORKS 

//		tempBuf[i*16 + 14] = Directory[i].entrySector & 0xFF;
//		tempBuf[i*16 + 15] = (Directory[i].entrySector >> 8) & 0xFF;
		memcpy(&tempBuf[i*16], Directory[i].name, sizeof(char[12]));
		memcpy(&tempBuf[i*16+12], &Directory[i].numSector, sizeof(uint16_t));
		memcpy(&tempBuf[i*16+14], &Directory[i].entrySector, sizeof(uint16_t));
	}
	
	if(eDisk_Write(0, tempBuf, 0, DIRSECTOR))
		return 1; // fail write
	
	for(int i = DIRSECTOR; i < FATSECTOR + DIRSECTOR; i++){ // 16 sectors in fat
	//memcpy(tempBuf, (void*)(((int)FAT + (i - DIRSECTOR)*512)), 512);
		for(int j = 0; j < MAXSECTOR/FATSECTOR; j++){ // 128 structs per fat sector
//			tempBuf[j*4] = FAT[i*128 + j].nextSector & 0xFF;
//      tempBuf[j*4 + 1] = (FAT[i*128 + j].nextSector >> 8) & 0xFF;
//        
//      tempBuf[j*4 + 2] = FAT[i*128 + j].bytesRemaining & 0xFF;
//      tempBuf[j*4 + 3] = (FAT[i*128 + j].bytesRemaining >> 8) & 0xFF;
			memcpy(&tempBuf[j*4], &FAT[(i-1)*128 + j].nextSector, sizeof(uint16_t));
			memcpy(&tempBuf[j*4+2], &FAT[(i-1)*128 + j].bytesRemaining, sizeof(uint16_t));
		}
		if(eDisk_Write(0, tempBuf, i, 1))
			return 1; // fail write
	}
	isMounted = 0;
   
  return 0;   // replace
}
