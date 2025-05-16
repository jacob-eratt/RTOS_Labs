// filename ************** eFile.c *****************************
// High-level routines to implement a solid-state disk 
// Students implement these functions in Lab 4
// Jonathan W. Valvano 1/12/20
#include <stdint.h>
#include <string.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../inc/CortexM.h"
#include <stdio.h>
#include "../RTOS_Labs_common/UART0int.h"

#define MAX_FILENAME 12
#define NUMBER_OF_FILES 16
#define BLOCK_SIZE 512

#define FILE_SIZE 20

#define OFFSET_NUM_FILES 0
#define OFFSET_OPEN_SLOTS 1
#define OFFSET_EMPTY_BLOCK_PT 17
#define OFFSET_FILES 19

#define FILE_OFFSET_NAME 0
#define FILE_OFFSET_DATA 12
#define FILE_OFFSET_SIZE 14
#define FILE_OFFSET_CURSOR 16
#define FILE_OFFSET_READ_CURSOR 18

#define littleEndian(highIndex,lowIndex) ((highIndex<<8) + lowIndex)
#define LOW8(x) (x&0xFF)
#define HIGH8(x) ((x>>8)&0xFF)
#define getFileLocation(num) (OFFSET_FILES + (num * FILE_SIZE))



//file directory struct
typedef struct fheader{//20 bytes
	uint8_t fileName[12]; //name of file as uint8_t, 20 char maximum
	uint16_t data; //pointer to first block of data.
	uint16_t num_blocks;
	uint16_t cursor;
	uint16_t read_cursor;
}eFile_header_t;

//software directory
typedef struct directory{//183
	uint8_t num_files;
	uint8_t open_file_slots[16];
	uint16_t empty_block_pt;
	eFile_header_t files[16];
}directory_t;

directory_t dir;

uint8_t file_index;
uint16_t open_block_id;
BYTE open_block[512];

Sema4Type directory_free;


//returns -1 on fail. and file index on success
int get_file_id(const char name[]){
    int file_id = -1;
    for(int i = 0; i < 16; i++){
        if(strcmp(name, (char*)dir.files[i].fileName) == 0){
            file_id = i;
            break;
        }
    }
    return file_id;
}

//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
int eFile_Init(void){ // initialize file system
	dir.num_files = 0;
	memset(dir.open_file_slots, 1, sizeof(dir.open_file_slots));
	memset(open_block, 1, sizeof(open_block));
	DSTATUS result = eDisk_Init(0);
	OS_InitSemaphore(&directory_free, 1);
  return result;   // replace
}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){ // erase disk, add format
	OS_Wait(&directory_free);
	uint8_t buff[BLOCK_SIZE];
	memset(buff, 0, sizeof(buff));
	
	for(uint16_t b = 1; b < 3900; b++){
		if(b == 1 ){
			//eDisk_WriteBlock(buff, 1);
			for(int i = 0; i < 16; i++){
				buff[OFFSET_OPEN_SLOTS+i] = 1;
			}
			buff[OFFSET_EMPTY_BLOCK_PT] = 2;
			eDisk_WriteBlock(buff, 1);
			continue;
	}
		buff[BLOCK_SIZE-1] = (b+1 &0xFF00)>>8;
		buff[BLOCK_SIZE-2] = b+1 &0x00FF;
		eDisk_Write(0, buff, b, 1);
	if(b%100 == 0) UART_OutChar('.');
	}
	UART_OutString("\n\r");
	//directory at top of disk, capable of holding info for 10 files.
		//each file header in directory will have a name and a pointer to the first block of data on the disk.
	
	OS_Signal(&directory_free);
  return 0;   // replace
}


//write open block for debugging


// Define constants for each item type
#define F  0x00  // num_files
#define S  0x01  // open_file_slots
#define E  0x03  // empty_block_pt
#define N  0x04  // file name (each file name is 12 bytes)
#define D  0x05  // data pointer (2 bytes)
#define B  0x06  // file size (2 bytes)
#define C  0x07  // cursor location (2 bytes)
#define R 0x08

uint8_t file_system[] = {
    // Directory struct (183 bytes)
    F,  // num_files (1 byte)
    S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, // open_file_slots (16 bytes)
    E, E,  // empty_block_pt (2 bytes)

    // Now for each file (16 files):
    // File 1
		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,

    // File 2
		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,

    // Repeat for all 16 files
		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,

		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,

		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,

		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,

		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,

		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,

		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,
		
		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,

		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,

		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,

		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,

		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,
		
		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R,
		
		N, N, N, N, N, N, N, N, N, N, N, N,
    D, D,
    B, B, C, C, R, R
};


//---------- eFile_Mount-----------------
// Mount the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure
int eFile_Mount(void){ // initialize file system
	OS_Wait(&directory_free);
	DRESULT result = eDisk_ReadBlock(open_block, 1);
	if (result != 0){
		OS_Signal(&directory_free);
		return result;
	}
	
	//memcpy(open_block, file_system, 512);
	
	dir.num_files = open_block[0];
	for(int i = 0; i < 16; i++){
		dir.open_file_slots[i] = open_block[i+OFFSET_OPEN_SLOTS];
	}
	dir.empty_block_pt = littleEndian(open_block[OFFSET_EMPTY_BLOCK_PT+1], open_block[OFFSET_EMPTY_BLOCK_PT]);
	for(int f = 0; f < 16; f++){
		for(int l = 0; l < 12; l++){
			char curLetter = open_block[getFileLocation(f)+ l];
			dir.files[f].fileName[l] = curLetter;
		}
		uint8_t highVal= open_block[getFileLocation(f) + FILE_OFFSET_DATA + 1];
		uint8_t lowVal = open_block[getFileLocation(f) + FILE_OFFSET_DATA];
		dir.files[f].data = (uint16_t)littleEndian(highVal,lowVal);
		
		highVal= open_block[getFileLocation(f) + FILE_OFFSET_SIZE + 1];
	  lowVal = open_block[getFileLocation(f) + FILE_OFFSET_SIZE];
		dir.files[f].num_blocks = (uint16_t)littleEndian(highVal,lowVal);
		
		highVal= open_block[getFileLocation(f) + FILE_OFFSET_CURSOR + 1];
	  lowVal = open_block[getFileLocation(f) + FILE_OFFSET_CURSOR];
		dir.files[f].cursor = (uint16_t)littleEndian(highVal,lowVal);
		
		highVal = open_block[getFileLocation(f) + FILE_OFFSET_READ_CURSOR + 1];
		lowVal = open_block[getFileLocation(f) + FILE_OFFSET_READ_CURSOR];
		dir.files[f].read_cursor = (uint16_t)littleEndian(highVal,lowVal);
		
	}
	OS_Signal(&directory_free);
  return 0;   // replace
}


//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( const char name[]){  // create new file, make it empty 
		//get first free file location in directory using open file slots
	OS_Wait(&directory_free);
	int file_id = get_file_id(name);
		if(file_id != -1){ //check if file name exist, if does, return 1
		OS_Signal(&directory_free);
	return 1;
	}
	int8_t avail_file = -1;
		for(int i = 0; i < 16; i++){
			if(dir.open_file_slots[i] == 1){
				avail_file = i;
				break;
			}
		}
		if(avail_file == -1){
			OS_Signal(&directory_free);
			return -1;
		}	
		//get first free block number
		uint16_t block = dir.empty_block_pt;
		DSTATUS result = eDisk_ReadBlock(open_block, block);
		if(result){
			OS_Signal(&directory_free);
			return result;
		}
		//update free block ptr.
		uint16_t next = littleEndian(open_block[BLOCK_SIZE-1], open_block[BLOCK_SIZE-2]);
		dir.empty_block_pt = next;
		//copy name into file name
		strncpy((char*)dir.files[avail_file].fileName, name, 11);
		dir.files[avail_file].fileName[11] = '\0';
		//set data pointer to block number
		dir.files[avail_file].data = block;
		//set pointer value at end of data equal to null ptr, or just clear entire block
		eDisk_ReadBlock(open_block, dir.files[avail_file].data);
		//open_block[BLOCK_SIZE-1 ] = 0;
		//open_block[BLOCK_SIZE-2] = 0;
		memset(open_block, 0, 512); 
		open_block[0] = 0xFF;
		eDisk_WriteBlock(open_block,dir.files[avail_file].data); 
		//set file size equal to one
		dir.files[avail_file].num_blocks = 1;
		//set cursor to 0
		dir.files[avail_file].cursor = 0;
		//set file location in slots to 0
		dir.open_file_slots[avail_file] = 0;
		//increase number of files counter
		dir.num_files++;
		OS_Signal(&directory_free);
		eFile_Unmount();
		eFile_Mount();
  return 0;   // replace
}


//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen( const char name[]){      // open a file for writing 
//find the file in the directory using strcmp. For now this is assuming that now file name is the same
		OS_Wait(&directory_free);
    //add semaphore for directory here
    int file_id = get_file_id(name);
    if(file_id == -1)
		{
			OS_Signal(&directory_free);
			return -1;
		}
    //set global file id
    file_index = file_id;
    //get the data block
    eFile_header_t* writeFile = &dir.files[file_id];
    uint16_t curr_block = writeFile->data;
    uint16_t next_block = 0;
    
    for(int i = 0; i < writeFile->num_blocks; i++){
        //open block
        DSTATUS result = eDisk_ReadBlock(open_block, curr_block);
        if(result) {
					OS_Signal(&directory_free);
					return result;
				}
        
        //get next block
        next_block = littleEndian(open_block[BLOCK_SIZE-1], open_block[BLOCK_SIZE-2]);
        if(next_block == 0){ //check end of file. Technically shouldnt need this. Should end properly in loop
            break;
        }
        else{
            curr_block = next_block;
        }
    }
    
    //set block id to open block
    open_block_id = curr_block;
    
  return 0;   // replace
}

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( const char data){

        //aquire semaphore
        eFile_header_t* writeFile = &dir.files[file_index];
       

        open_block[writeFile->cursor] = data;
        writeFile->cursor++;

        //check if cursor at end of block 
        if(writeFile->cursor >= 510){
            //get next block from free list
            uint16_t next_block = dir.empty_block_pt;
						

            //last two bytes of current block updated to next_block
            uint16_t low_val = next_block & 0x0FF;
            uint8_t high_val = next_block>>8 & 0x00FF;
            open_block[BLOCK_SIZE-2] = low_val;
            open_block[BLOCK_SIZE-1] = high_val;

        //store current block 
            DSTATUS result = eDisk_WriteBlock(open_block, open_block_id);
            if(result){ 
							return result;
						}

        //read that new block onto open_block
            result = eDisk_ReadBlock(open_block, next_block);
            if(result){ 
							return result;
						}

						dir.empty_block_pt = littleEndian(open_block[BLOCK_SIZE - 1], open_block[BLOCK_SIZE - 2]);
        //set global open_block_id
            open_block_id = next_block;

				//set null at end of write block
						open_block[BLOCK_SIZE-2] = 0;
						open_block[BLOCK_SIZE-1] = 0;
						
        //update write blocks contents
            writeFile->cursor = 0;
            //writeFile->data = next_block; dont think I need this. Changing pointer right?
            writeFile->num_blocks++;
        }
    return 0;
}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void){ // close the file for writing
  //save block at current id
		open_block[dir.files[file_index].cursor] = 0xFF;
    DSTATUS result = eDisk_WriteBlock(open_block, open_block_id);
    if(result){
			OS_Signal(&directory_free);
			return result;
		}
		//dir.files[file_index].cursor = 0;
    open_block_id = -1;
    file_index = -1;
		OS_Signal(&directory_free);
  return 0;
}


//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( const char name[]){      // open a file for reading 
    OS_Wait(&directory_free);
        //find file in directory based on name
    int file_id = get_file_id(name);
    if(file_id == -1){
			OS_Signal(&directory_free);
			return -1;
		}

    eFile_header_t* read_file = &dir.files[file_id];

    //open first block into open_block
    DSTATUS result = eDisk_ReadBlock(open_block, read_file->data);
    if(result){OS_Signal(&directory_free); return result;}

    //update globals with block information 
    open_block_id = read_file->data;
    file_index = file_id;
		dir.files[file_id].read_cursor = 0;

  return 0 ;   // replace
}
 
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt){       // get next byte /ask logan about parameter
	// set semaphore
  //get next pointer at end of current block
	if(dir.files[file_index].read_cursor == 510){
		uint8_t highVal = open_block[BLOCK_SIZE - 1];
		uint8_t lowVal = open_block[BLOCK_SIZE - 2];
		uint16_t next_block = littleEndian(highVal, lowVal);
		open_block_id = next_block;
		eDisk_ReadBlock(open_block, next_block);
		dir.files[file_index].read_cursor = 0;
	}
	
	*pt = open_block[dir.files[file_index].read_cursor];
	dir.files[file_index].read_cursor++;
	
	

	
  return 0;   // replace
}
    
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){ // close the file for writing
  //signal eFile semaphore
	dir.files[file_index].read_cursor = 0;
	open_block_id = -1;
	file_index = -1;
	
	memset(open_block, 0, sizeof(open_block));
	OS_Signal(&directory_free);
  return 0;   // replace
}


//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( const char name[]){  // remove this file 
	OS_Wait(&directory_free);
	//find file and save its index in the dir.
	uint8_t fileID = get_file_id(name);
	
	//set the empty pointer to the files first block
	uint16_t next = dir.files[fileID].data;
	uint16_t prev_empty_block_pt = dir.empty_block_pt;
	dir.empty_block_pt = next;
	//eDisk_ReadBlock(open_block, next);
	//open_block_id = next;
	//iterate to files last block
	while(next != 0){
		open_block_id = next;
		eDisk_ReadBlock(open_block, next);
		uint8_t highVal = open_block[BLOCK_SIZE-1];
		uint8_t lowVal = open_block[BLOCK_SIZE-2];
		next = littleEndian(highVal, lowVal);
		

	}
	//set next pointer of files last block to the empty pointer
	open_block[BLOCK_SIZE-2] = LOW8(prev_empty_block_pt);
	open_block[BLOCK_SIZE-1] = HIGH8(prev_empty_block_pt);
	DSTATUS result = eDisk_WriteBlock(open_block, open_block_id);
	if(result){
		OS_Signal(&directory_free);
		return result;
	}

	//clear file name to nulls
	memset(dir.files[fileID].fileName, 0, MAX_FILENAME);
	//open_slots of file location back to 1
	dir.open_file_slots[fileID] = 1;
	dir.num_files--;
	OS_Signal(&directory_free);
	eFile_Unmount();
		eFile_Mount();
  return 0;   // replace
}                             



//---------- eFile_DOpen-----------------
// Open a (sub)directory, read into RAM
// Input: directory name is an ASCII string up to seven characters
//        (empty/NULL for root directory)
// Output: 0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_DOpen( const char name[]){ // open directory
   
  return 0;   // replace
}
  
//---------- eFile_DirNext-----------------
// Retreive directory entry from open directory
// Input: none
// Output: return file name and size by reference
//         0 if successful and 1 on failure (e.g., end of directory)
int eFile_DirNext( char *name[], unsigned long *size, int num){  // get next entry 
	if(dir.open_file_slots[num] == 0){
		//strcpy(*name, (char*)dir.files[num].fileName);
		*name = (char*)dir.files[num].fileName;
		*size = dir.files[num].num_blocks;
	}
	else{return 1;}
	return 0;
}

//---------- eFile_DClose-----------------
// Close the directory
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_DClose(void){ // close the directory
   
  return 0;   // replace
}


//---------- eFile_Unmount-----------------
// Unmount and deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently mounted)
int eFile_Unmount(void){ 
   //save entire directory back to sd card
	OS_Wait(&directory_free);
	memset(open_block, 0, sizeof(open_block));
	open_block[OFFSET_NUM_FILES] = dir.num_files;
	for(int i = 0; i < 16; i++){
		open_block[i+OFFSET_OPEN_SLOTS] = dir.open_file_slots[i];
	}
	open_block[OFFSET_EMPTY_BLOCK_PT] = LOW8(dir.empty_block_pt);
	open_block[OFFSET_EMPTY_BLOCK_PT+1] = HIGH8(dir.empty_block_pt);
	for(int i = 0; i < NUMBER_OF_FILES; i++){
		eFile_header_t* curFile = &dir.files[i];
		strncpy((char*)&open_block[getFileLocation(i) + FILE_OFFSET_NAME], (char*)curFile->fileName, 12);
		open_block[getFileLocation(i)+FILE_OFFSET_DATA] = LOW8(curFile->data);
		open_block[getFileLocation(i)+FILE_OFFSET_DATA+1] = HIGH8(curFile->data);
		open_block[getFileLocation(i)+FILE_OFFSET_SIZE] = LOW8(curFile->num_blocks);
		open_block[getFileLocation(i)+FILE_OFFSET_SIZE+1] = HIGH8(curFile->num_blocks);
		open_block[getFileLocation(i)+FILE_OFFSET_CURSOR] = LOW8(curFile->cursor);
		open_block[getFileLocation(i)+FILE_OFFSET_CURSOR+1] = HIGH8(curFile->cursor);
		open_block[getFileLocation(i) + FILE_OFFSET_READ_CURSOR] = LOW8(curFile->read_cursor);
		open_block[getFileLocation(i) + FILE_OFFSET_READ_CURSOR + 1] = HIGH8(curFile->read_cursor);
	}
	DSTATUS result = eDisk_WriteBlock(open_block, 1);
	OS_Signal(&directory_free);
  return result;   // replace
}
