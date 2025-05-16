// filename *************************heap.c ************************
// Implements memory heap for dynamic memory allocation.
// Follows standard malloc/calloc/realloc/free interface
// for allocating/unallocating memory.

// Jacob Egner 2008-07-31
// modified 8/31/08 Jonathan Valvano for style
// modified 12/16/11 Jonathan Valvano for 32-bit machine
// modified August 10, 2014 for C99 syntax

/* This example accompanies the book
   "Embedded Systems: Real Time Operating Systems for ARM Cortex M Microcontrollers",
   ISBN: 978-1466468863, Jonathan Valvano, copyright (c) 2015

 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains

 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */


#include <stdint.h>
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common/OS.h"

#define HEAPSIZE 512
int32_t heap[HEAPSIZE];
Sema4Type heapSema;

void ClearBlock(int32_t headerindex){
	int32_t size = heap[headerindex];
	if(size < 0) size *= -1;
	for(int i = headerindex + 1; i < headerindex + 1 + size; i++){
		heap[i] = 0;
	}
}


//******** Heap_Init *************** 
// Initialize the Heap
// input: none
// output: always 0
// notes: Initializes/resets the heap to a clean state where no memory
//  is allocated.
int32_t Heap_Init(void){
	heap[0] = -(HEAPSIZE - 2);
	heap[HEAPSIZE-1] = -(HEAPSIZE - 2);
	OS_InitSemaphore(&heapSema, 1);
  return 0;
}


//******** Heap_Malloc *************** 
// Allocate memory, data not initialized
// input: 
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory or will return NULL
//   if there isn't sufficient space to satisfy allocation request
void* Heap_Malloc(int32_t desiredBytes){
	OS_Wait(&heapSema);
	int index = 0;
	void* ret = 0;
	int currSize;
	int footerIndex;
	
	// byte align and divide by 4 to get amount of int32_t's in request instead of chars/int8_t's
	desiredBytes = (desiredBytes + 3) & 0xFFFFFFFC;
	desiredBytes /= 4;
	
	while(index < HEAPSIZE){
		currSize = -heap[index]; // obtaining size of current free block from header
		if(currSize < 0) footerIndex = index - currSize + 1;
		else footerIndex = index + currSize + 1; // footer of current block

		if(currSize > 0 && currSize >= desiredBytes){

			
			// if the remaining free space in block can fit headers
			if(currSize > desiredBytes + 2){
			
				// set header and footer of NEW block
				heap[index] = desiredBytes; // start of block
				heap[index + 1 + desiredBytes] = desiredBytes; // end of block
				
				// update header and footer of free space
				int newFreeSize = currSize - desiredBytes;
				heap[index + 2 + desiredBytes] = -(newFreeSize - 2); // start of free block
				heap[footerIndex] = -(newFreeSize - 2); // end of free block
				
				OS_Signal(&heapSema);
				return &heap[index+1]; // return start of block(NOT HEADER)
			}
			
			// if the remaining free space in block CANNOT fit headers
			else{
				heap[index] = currSize; // consume entire block
				heap[footerIndex] = currSize; // consume entire block
				
				OS_Signal(&heapSema);
				return &heap[index+1]; // return start of block(NOT HEADER)
			}
		}
		index = footerIndex + 1; // go to next block
	}
	OS_Signal(&heapSema);
  return 0;   // NULL
}


//******** Heap_Calloc *************** 
// Allocate memory, data are initialized to 0
// input:
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory block or will return NULL
//   if there isn't sufficient space to satisfy allocation request
//notes: the allocated memory block will be zeroed out
void* Heap_Calloc(int32_t desiredBytes){
	OS_Wait(&heapSema);  
	int index = 0;
	void* ret = 0;
	int currSize;
	int footerIndex;
	
	// byte align and divide by 4 to get amount of int32_t's in request instead of chars/int8_t's
	desiredBytes = (desiredBytes + 3) & 0xFFFFFFFC;
	desiredBytes /= 4;
	
	while(index < HEAPSIZE){
		currSize = -heap[index]; // obtaining size of current free block from header
		if(currSize < 0) footerIndex = index - currSize + 1;
		else footerIndex = index + currSize + 1; // footer of current block		
		
		if(currSize > 0 && currSize >= desiredBytes){
			
			
			// if the remaining free space in block can fit headers
			if(currSize > desiredBytes + 2){
			
				// set header and footer of NEW block
				heap[index] = desiredBytes; // start of block
				heap[index + 1 + desiredBytes] = desiredBytes; // end of block
				
				// update header and footer of free space
				int newFreeSize = currSize - desiredBytes;
				heap[index + 2 + desiredBytes] = -(newFreeSize - 2); // start of free block
				heap[footerIndex] = -(newFreeSize - 2); // end of free block
				
				ClearBlock(index); // wipe block
				
				OS_Signal(&heapSema);
				return &heap[index+1]; // return start of block(NOT HEADER)
			}
			
			// if the remaining free space in block CANNOT fit headers
			else{
				heap[index] = currSize; // consume entire block
				heap[footerIndex] = currSize; // consume entire block
				
				ClearBlock(index); // wipe block
				
				OS_Signal(&heapSema);
				return &heap[index+1]; // return start of block(NOT HEADER)
			}
		}
		
		index = footerIndex + 1; // go to next block
	}
	OS_Signal(&heapSema);
  return 0;   // NULL
}


//******** Heap_Realloc *************** 
// Reallocate buffer to a new size
//input: 
//  oldBlock: pointer to a block
//  desiredBytes: a desired number of bytes for a new block
// output: void* pointing to the new block or will return NULL
//   if there is any reason the reallocation can't be completed
// notes: the given block may be unallocated and its contents
//   are copied to a new block if growing/shrinking not possible
void* Heap_Realloc(void* oldBlock, int32_t desiredBytes){
	OS_Wait(&heapSema); 
 
  int index = 0;
	void* ret = 0;
	int currSize;
	int footerIndex;
	
	// byte align and divide by 4 to get amount of int32_t's in request instead of chars/int8_t's
	desiredBytes = (desiredBytes + 3) & 0xFFFFFFFC;
	desiredBytes /= 4;
	
	while(index < HEAPSIZE){
		currSize = -heap[index]; // obtaining size of current free block from header
		if(currSize < 0) footerIndex = index - currSize + 1;
		else footerIndex = index + currSize + 1; // footer of current block		
		
		if(currSize > 0 && currSize >= desiredBytes){
			
			
			// if the remaining free space in block can fit headers
			if(currSize > desiredBytes + 2){
			
				// set header and footer of NEW block
				heap[index] = desiredBytes; // start of block
				heap[index + 1 + desiredBytes] = desiredBytes; // end of block
				
				// update header and footer of free space
				int newFreeSize = currSize - desiredBytes;
				heap[index + 2 + desiredBytes] = -(newFreeSize - 2); // start of free block
				heap[footerIndex] = -(newFreeSize - 2); // end of free block
				
				// obtain header address as int32_t* and size from old block
				int32_t* oldHeader = ((int32_t*)oldBlock) - 1;
				int32_t oldSize = *oldHeader;
				if (oldSize < 0) oldSize *= -1;
				
				// copy over data
				for(int i = 0; i < oldSize; i++){
					heap[index + 1 + i] = *(oldHeader + 1 + i);
				}
				
				OS_Signal(&heapSema);
				Heap_Free(oldBlock);
				return &heap[index+1]; // return start of block(NOT HEADER)
			}
			
			// if the remaining free space in block CANNOT fit headers
			else{
				heap[index] = currSize; // consume entire block
				heap[footerIndex] = currSize; // consume entire block
				
				// obtain header address as int32_t* and size from old block
				int32_t* oldHeader = ((int32_t*)oldBlock) - 1;
				int32_t oldSize = *oldHeader;
				if (oldSize < 0) oldSize *= -1;
				
				// copy over data
				for(int i = 0; i < oldSize; i++){
					heap[index + 1 + i] = *(oldHeader + 1 + i);
				}
				OS_Signal(&heapSema);
				Heap_Free(oldBlock);
				return &heap[index+1]; // return start of block(NOT HEADER)
			}
		}
		index = footerIndex + 1; // go to next block
	}
	OS_Signal(&heapSema);
  return 0;   // NULL
	
}


//******** Heap_Free *************** 
// return a block to the heap
// input: pointer to memory to unallocate
// output: 0 if everything is ok, non-zero in case of error (e.g. invalid pointer
//     or trying to unallocate memory that has already been unallocated
int32_t Heap_Free(void* pointer){
	OS_Wait(&heapSema); 
	
	int32_t* header = ((int32_t*)pointer) - 1;
	int size = *header;
	if(size < 0){
		OS_Signal(&heapSema);
		return 1;
	}
	int32_t* footer = header + size + 1;
	
	int32_t* aboveHeader = 0;
	int aboveSize = 0;
	int32_t* aboveFooter = 0;
	
	int32_t* belowHeader = 0;
	int belowSize = 0;
	int32_t* belowFooter = 0;
	
	
	if(header != &heap[0] && footer != &heap[HEAPSIZE-1]){ // island in heap
		
		belowHeader = footer + 1;
		belowSize = *belowHeader;
		belowFooter = belowHeader + (-1*belowSize) + 1;
		
		aboveFooter = header - 1;
		aboveSize = *aboveFooter;
		aboveHeader = aboveFooter - (-1*aboveSize) - 1;
		
		// if above block and below block are free space, combine all three blocks
		if(aboveSize < 0 && belowSize < 0){
			int totalSize = (-aboveSize) + (-belowSize) + size + 2 + 2;
			*aboveHeader = -totalSize;
			*belowFooter = -totalSize;
			OS_Signal(&heapSema);
			return 0;
		}
		
		// if above block is free space, combine blocks
		if(aboveSize < 0){
			int totalSize = (-aboveSize) + size + 2;
			*aboveHeader = -totalSize;
			*footer = -totalSize;
			OS_Signal(&heapSema);
			return 0;
		}
		
		// if below block is free space, combine blocks
		if(belowSize < 0){
			int totalSize = (-belowSize) + size + 2;
			*header = -totalSize;
			*belowFooter = -totalSize;
			OS_Signal(&heapSema);
			return 0;
		}
		
		// else just free this block
		else{
			*header = -size;
			*footer = -size;
			OS_Signal(&heapSema);
			return 0;
		}
		
	}
	
	else if(header == &heap[0] && footer != &heap[HEAPSIZE-1]){ // touches top of heap
		
		belowHeader = footer + 1;
		belowSize = *belowHeader;
		belowFooter = belowHeader + (-1*belowSize) + 1; // changed
		
		// if below block is free space, combine blocks
		if(belowSize < 0){
			int totalSize = (-belowSize) + size + 2;
			*header = -totalSize;
			*belowFooter = -totalSize;
			OS_Signal(&heapSema);
			return 0;
		}
		
		// else just free this block
		else{
			*header = -size;
			*footer = -size;
			OS_Signal(&heapSema);
			return 0;
		}
		
	}
	
	else if(header != &heap[0] && footer == &heap[HEAPSIZE-1]){ // touches bottom of heap
		aboveFooter = header - 1;
		aboveSize = *aboveFooter;
		aboveHeader = aboveFooter - (-1*aboveSize) - 1; // changed
		
		// if above block is free space, combine blocks
		if(aboveSize < 0){
			int totalSize = (-aboveSize) + size + 2;
			*aboveHeader = -totalSize;
			*footer = -totalSize;
			OS_Signal(&heapSema);
			return 0;
		}
		
		// else just free this block
		else{
			*header = -size;
			*footer = -size;
			OS_Signal(&heapSema);
			return 0;
		}
		
	}
	
	else if(header == &heap[0] && footer == &heap[HEAPSIZE-1]){ // spans entire heap
		*header = -size;
		*footer = -size;
		OS_Signal(&heapSema);
		return 0;
	}
	
	OS_Signal(&heapSema);
  return 1;
}


//******** Heap_Stats *************** 
// return the current status of the heap
// input: reference to a heap_stats_t that returns the current usage of the heap
// output: 0 in case of success, non-zeror in case of error (e.g. corrupted heap)

//typedef struct heap_stats {
//  uint32_t size;   // heap size (in bytes)
//  uint32_t used;   // number of bytes used/allocated
//  uint32_t free;   // number of bytes available to allocate
//} heap_stats_t;


int32_t Heap_Stats(heap_stats_t *stats){
	stats->size = HEAPSIZE * 4;
	int index = 0;
	void* ret = 0;
	volatile int32_t freeSpace = 0;
	volatile int32_t reservedSpace = 0;
	int footerIndex;
	
	while(index < HEAPSIZE){
		int currSize = heap[index]; // obtaining size of current free block from header
		if(currSize < 0) footerIndex = index - currSize + 1;
		else footerIndex = index + currSize + 1; // footer of current block		
		
		if (currSize < 0){
			freeSpace += currSize * -1;
		}
		else{
			reservedSpace += currSize;
		}
		reservedSpace += 2;
		index = footerIndex + 1; // go to next block
	}
	
	stats->free = freeSpace * 4;
	stats->used = reservedSpace * 4;
  return 0;   // replace
}
