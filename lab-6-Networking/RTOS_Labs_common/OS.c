// *************os.c**************
// EE445M/EE380L.6 Labs 1, 2, 3, and 4 
// High-level OS functions
// Students will implement these functions as part of Lab
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 
// Jan 12, 2020, valvano@mail.utexas.edu


#include <stdint.h>
#include <stdio.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/PLL.h"
#include "../inc/LaunchPad.h"
#include "../inc/Timer4A.h"
#include "../inc/WTimer0A.h"
#include "../inc/WTimer1A.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../inc/Timer5A.h"
#include "../inc/EdgeInterruptPortF.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common/esp8266.h"

//Definitions for Lab 2
#define MAXTHREADS 10
#define MAXPROCESSES 5
#define STACKSIZE 256
#define FIFOSIZE 32
#define DEBOUNCEPERIOD 50
#define PRIORITIES 10

// Performance Measurements 
// int32_t MaxJitter;             // largest time jitter between interrupts in usec
#define JITTERSIZE 64
uint32_t const JitterSize=JITTERSIZE;
uint32_t JitterHistogram[JITTERSIZE]={0,};

// Global Variables Lab 1
uint32_t lab1Count = 0;
uint32_t lab2Count = 0;

// Counting Lab 1
void countMS(void){
	lab1Count++;
	lab2Count++;
}


// Lab 2 Globals
extern void ContextSwitch();
extern void StartOS();
void (*SW1Task)(void);   // user function
void (*SW2Task)(void);   // user function

// TCB* TempPt;
// int threadId; //stores amount of threads currently
TCB threads[MAXTHREADS]; //thread array
long stacks[MAXTHREADS][STACKSIZE]; //global stacks
PCB processes[MAXPROCESSES];
Sema4Type BoxFree;
Sema4Type DataValid;
uint32_t MailBox;
Sema4Type DataRoomLeft;
Sema4Type DataAvailable;
Sema4Type MutexFifo;
uint32_t Fifo[FIFOSIZE];
uint8_t FifoPutIdx = 0;
uint8_t FifoGetIdx = 0;
extern uint8_t ThreadCount;
uint8_t ThreadCounts[PRIORITIES];
uint8_t sleepCount = 0;
uint32_t lastPressed = 0;
uint8_t fifosize = 0;
uint32_t TimeSlice;

// Hardware Background Task Variables
uint8_t SW1Init = 0;
uint8_t SW2Init = 0;
uint32_t SW1Pri = 0;
uint32_t SW2Pri = 0;
uint8_t Timer5Used = 0;


// Linked List Variables
TCB* PriPointers[PRIORITIES];
TCB* RunPt; //points to current tcb
TCB* SleepPt = NULL;
TCB* LookPt;
TCB* Prev;
TCB* Next;
TCB* NextPt;

PCB* CurrPCB;
uint8_t ProcessCount;

void OS_setID(uint32_t id){
    RunPt->id = id;
}

void addThreadBack(TCB* addback){
	long status = StartCritical();
	if(PriPointers[addback->priority] == NULL){
		PriPointers[addback->priority] = addback;
		addback->next = addback;
		addback->prev = addback;
	}
	else{
		TCB* tempnext = PriPointers[addback->priority]->next;
		addback->next = PriPointers[addback->priority]->next;
		addback->prev = PriPointers[addback->priority];
		PriPointers[addback->priority]->next = addback;
		tempnext->prev = addback;
	}
	ThreadCounts[addback->priority]++;
	EndCritical(status);
//	if(ThreadCount == 1){
//			threads[i].next = &threads[i];
//			threads[i].prev = &threads[i];
//	}
//	else{
//			TCB* tempnext = RunPt->next;
//			threads[i].next = RunPt->next;
//			threads[i].prev = RunPt;
//			RunPt->next = &threads[i];
//			tempnext->prev = &threads[i];
//	}
	
}

void setAsBlocked(Sema4Type* sphore){
	long status = StartCritical();
	sphore->blockn++;
	RunPt->blocked = 1;
	ThreadCounts[RunPt->priority]--;
	if((RunPt->next == RunPt) && (RunPt->prev == RunPt)){
		PriPointers[RunPt->priority] = NULL;
	}
	else{
		RunPt->prev->next = RunPt->next;
		RunPt->next->prev = RunPt->prev;
	}
		
	TCB* temp = sphore->blockedList;
	TCB* previous = NULL;
	
	//check if beginning
	if(temp == NULL){
		sphore->blockedList = RunPt;
		sphore->blockedList->prev = NULL;
	}
	
	//if not beginning
	else{
		//cycle until priority reached or null
		while(temp != NULL && (temp->priority <= RunPt->priority)){ //temp either NULL or greater pri
			previous = temp;
			temp = temp->prev;
		}
		
		if(previous != NULL) previous->prev = RunPt; //set previous(next) of previous node
		else sphore->blockedList = RunPt; //if no previous exists, set as head
		
		RunPt->prev = temp;	//set previous(next) of RunPt
	}
	
	
	
	OS_Suspend();
}

/*------------------------------------------------------------------------------
  Systick Interrupt Handler
  SysTick interrupt happens every 10 ms
  used for preemptive thread switch
 *------------------------------------------------------------------------------*/
void SysTick_Handler(void) {
	
	// GPIO_PORTD_DATA_R ^= 0x08;
	

	//ContextSwitch();
	OS_Suspend();
	
	LookPt = SleepPt;
	Prev = NULL;
	Next = NULL;
	while(LookPt != NULL){
		if(LookPt->sleep <= 0){
			Next = LookPt->prev;
			sleepCount--;
			// LookPt->prev->next = LookPt->next;
      //LookPt->next->prev = LookPt->prev;
			LookPt->active = 1;
      LookPt->sleep = 0;
			
			// Add back to active list
			addThreadBack(LookPt);
			
			// Remove from sleeping list
			if(Prev == NULL){
				SleepPt = Next;
			}
			else{
				Prev->prev = Next;
			}
			LookPt = Next;
			
			
		}
		else{
			LookPt->sleep--;
			Prev = LookPt;
			LookPt = LookPt->prev;
		}
	}
	
	// GPIO_PORTD_DATA_R ^= 0x08;
	
	
	// go through sleep pointer to decrement counters
	
	
} // end SysTick_Handler

unsigned long OS_LockScheduler(void){
  // lab 4 might need this for disk formating
	unsigned long prev = NVIC_ST_CTRL_R;
	NVIC_ST_CTRL_R = NVIC_ST_CTRL_ENABLE + NVIC_ST_CTRL_CLK_SRC;
  return prev;// replace with solution
}
void OS_UnLockScheduler(unsigned long previous){
  // lab 4 might need this for disk formating
	NVIC_ST_CTRL_R = previous;
}


void SysTick_Init(unsigned long period){
  NVIC_ST_CTRL_R = 0;                   // disable SysTick during setup
  NVIC_ST_RELOAD_R = period;  					// maximum reload value
  NVIC_ST_CURRENT_R = 0;                // any write to current clears it
                                        // enable SysTick with core clock
  NVIC_ST_CTRL_R = NVIC_ST_CTRL_ENABLE+NVIC_ST_CTRL_INTEN+NVIC_ST_CTRL_CLK_SRC;
}

void inittask(void){
  while(1) {
    WaitForInterrupt();
  }
}


/**
 * @details  Initialize operating system, disable interrupts until OS_Launch.
 * Initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers.
 * Interrupts not yet enabled.
 * @param  none
 * @return none
 * @brief  Initialize OS
 */
int OS_AddProcess(void(*entry)(void), void *text, void *data, 
  unsigned long stackSize, unsigned long priority); 
void OS_Init(void){
  // put Lab 2 (and beyond) solution here
	DisableInterrupts();
	RunPt = &threads[0];
	ST7735_InitR(INITR_REDTAB);
	NVIC_SYS_PRI3_R &= ~0xF0000000; // Systick
	NVIC_SYS_PRI3_R |= 0x00E00000; // PendSV
	UART_Init(); 
	PFGeneralInit();
	WideTimer0A_Init(&(countMS), 80000, 0);
	PLL_Init(Bus80MHz);
	Heap_Init();
	//ESP8266_Init(1, 1);
	//OS_AddProcess(&inittask,Heap_Calloc(256),Heap_Calloc(256),128,9);
}; 

// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, int32_t value){
  // put Lab 2 (and beyond) solution here
	semaPt->Value = value;
	semaPt->blockedList = NULL;
}; 

// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	//DisableInterrupts();
	long status = StartCritical();
	semaPt->Value = semaPt->Value - 1;
	if(semaPt->Value < 0) setAsBlocked(semaPt);
	EndCritical(status);
	//EnableInterrupts();
}; 

//OS_Wait(Sema4Type *semaPt)
//1) Save the I bit and disable interrupts
//2) Decrement the semaphore counter, S=S-1
//(semaPt->Value)--;
//3) If the Value < 0 then this thread will be blockedset the status of this thread to blocked,
//specify this thread blocked on this semaphore,
//suspend thread
//4) Restore the I bit


// ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	//DisableInterrupts();
	long status = StartCritical();
	if(semaPt->blockedList != NULL){
		TCB* toExtract = semaPt->blockedList;
		semaPt->blockedList = semaPt->blockedList->prev;
		
		// add thread back to run list
		toExtract->blocked = 0;
		addThreadBack(toExtract);
		semaPt->blockn--;
		if(semaPt->blockn <= 0) semaPt->blockedList = NULL;
	}
	semaPt->Value = semaPt->Value + 1;
	EndCritical(status);
	//EnableInterrupts();
}; 

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	  // put Lab 2 (and beyond) solution here
	OS_Wait(semaPt);
}; 

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	OS_Signal(semaPt);
}; 



//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
int OS_AddThread(void(*task)(void), 
   uint32_t stackSize, uint32_t priority){
  // put Lab 2 (and beyond) solution here	
	long status = StartCritical();
	if (stackSize <= 8*STACKSIZE){
		//initialize fields
		// CHANGED TO DO A FOR LOOP TO CHECK EACH INDEX IN TCB ARRAY
		for(int i = 0; i < MAXTHREADS; i++){
			if(threads[i].used == 0){
				ThreadCount++;
				threads[i].sp = &stacks[i][STACKSIZE-1]; // We push 16 items here
				threads[i].active = 1;
				threads[i].id = i;
				threads[i].priority = priority;
				threads[i].used = 1;
				threads[i].sleep = 0;
				threads[i].pcb = CurrPCB;
				threads[i].pcb->numThreads++;
				addThreadBack(&threads[i]); //double check this works
				
				
				//initialize stack contents
				//CHANGED, PUSH R3-R0 FIRST, THEN R11-R4
				
				
				*threads[i].sp 		 = 0x01000000L; //psr 
				*(--threads[i].sp) = (long)task; // R15 (PC)
				*(--threads[i].sp) = 0x14141414L; // R14 (LR)
				*(--threads[i].sp) = 0x12121212L; // R12
				*(--threads[i].sp) = 0x03030303L; // R3
				*(--threads[i].sp) = 0x02020202L; // R2
				*(--threads[i].sp) = 0x01010101L; // R1
				*(--threads[i].sp) = 0x00000000L; // R0
				*(--threads[i].sp) = 0x11111111L; // R11
				*(--threads[i].sp) = 0x10101010L; // R10
				
				if(threads[i].pcb == NULL){
					*(--threads[i].sp) = 0x09090909L; // R9
				}
				else{
					*(--threads[i].sp) = (long)threads[i].pcb->data; // R9
				}
				*(--threads[i].sp) = 0x08080808L; // R8
				*(--threads[i].sp) = 0x07070707L; // R7
				*(--threads[i].sp) = 0x06060606L; // R6
				*(--threads[i].sp) = 0x05050505L; // R5
				*(--threads[i].sp) = 0x04040404L; // R4
				
				EndCritical(status);
				return 1;
			}
		}
	}
  EndCritical(status);
  return 0; // replace this line with solution
};
	 

//******** OS_AddProcess *************** 
// add a process with foregound thread to the scheduler
// Inputs: pointer to a void/void entry point
//         pointer to process text (code) segment
//         pointer to process data segment
//         number of bytes allocated for its stack
//         priority (0 is highest)
// Outputs: 1 if successful, 0 if this process can not be added
// This function will be needed for Lab 5
// In Labs 2-4, this function can be ignored
int OS_AddProcess(void(*entry)(void), void *text, void *data, 
  unsigned long stackSize, unsigned long priority){
    PCB* temp = CurrPCB;     
    long status = StartCritical();
    if (stackSize <= 8*STACKSIZE){
        //initialize fields
        // CHANGED TO DO A FOR LOOP TO CHECK EACH INDEX IN TCB ARRAY
        for(int i = 0; i < MAXPROCESSES; i++){
            if(processes[i].used == 0){
                ProcessCount++;
                processes[i].id = i;
                processes[i].used = 1;
                processes[i].code = text;
                processes[i].data = data;
                //addThreadBack(&threads[i]); //double check this works
                CurrPCB = &processes[i]; 
                OS_AddThread(entry, stackSize, priority);
                CurrPCB = temp;
                
                // add PCB to PCB list (PCBPriPointers)     
                EndCritical(status);
                return 1;
            }
        }
    }
  EndCritical(status);
    
  return 0; // replace this line with Lab 5 solution
}


//******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
uint32_t OS_Id(void){
  // put Lab 2 (and beyond) solution here
  return RunPt->id; // replace this line with solution
};


//******** OS_AddPeriodicThread *************** 
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units (12.5ns)
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// You are free to select the time resolution for this function
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In lab 1, this command will be called 1 time
// In lab 2, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, this command will be called 0 1 or 2 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddPeriodicThread(void(*task)(void), 
   uint32_t period, uint32_t priority){
  // put Lab 2 (and beyond) solution here
	long status = StartCritical();
	if(Timer5Used == 0){
		Timer5A_Init((*task), period, priority);
		Timer5Used = 1;
	}
	else{
		WideTimer1A_Init((*task), period, priority);
	}
  
  EndCritical(status);
  return 0; // replace this line with solution
};


/*----------------------------------------------------------------------------
  PF1 Interrupt Handler
 *----------------------------------------------------------------------------*/
void GPIOPortF_Handler(void){
	// GPIO_PORTF_ICR_R = 0x10;      // acknowledge flag4
	 // Check for PF4 interrupt
		//GPIO_PORTD_DATA_R ^= 0x08;
    if (GPIO_PORTF_RIS_R & 0x11) { // PF4 interrupt flag set
			GPIO_PORTF_ICR_R = 0x11;   // Clear PF4 interrupt flag immediately
			if ((GPIO_PORTF_DATA_R & 0x11) == 1) {
				(SW1Task)();              // Execute the task
			}
			else if ((GPIO_PORTF_DATA_R & 0x11) == 16) {
				(SW2Task)();              // Execute the task
			}
			else if ((GPIO_PORTF_DATA_R & 0x11) == 0) {
				if(SW2Pri < SW1Pri)  (SW2Task)();             // Execute the task
				else (SW1Task)();
			}
    }
		//GPIO_PORTD_DATA_R ^= 0x08;
		//GPIO_PORTD_DATA_R ^= 0x08;
}

//******** OS_AddSW1Task *************** 
// add a background task to run whenever the SW1 (PF4) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In labs 2 and 3, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW1Task(void(*task)(void), uint32_t priority){
  // put Lab 2 (and beyond) solution here
	if(SW1Init == 0){
		SW1Init = 1;
		SW1Pri = priority;
		long status = StartCritical();
		PF4Init(priority);
		SW1Task = task; 
		EndCritical(status);
		return 1; // replace this line with solution
	}
	return 0;
};

//******** OS_AddSW2Task *************** 
// add a background task to run whenever the SW2 (PF0) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is highest, 5 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed user task will run to completion and return
// This task can not spin block loop sleep or kill
// This task can call issue OS_Signal, it can call OS_AddThread
// This task does not have a Thread ID
// In lab 2, this function can be ignored
// In lab 3, this command will be called will be called 0 or 1 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW2Task(void(*task)(void), uint32_t priority){
  // put Lab 2 (and beyond) solution here
	if(SW2Init == 0){
		SW2Init = 1;
		SW2Pri = priority;
		long status = StartCritical();
		PF0Init(priority);
		SW2Task = task; 
		EndCritical(status);
		return 1; // replace this line with solution
	}
	return 0;
};


// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime){
  // put Lab 2 (and beyond) solution here
	//DisableInterrupts();
	long status = StartCritical();
	RunPt->sleep = sleepTime/TimeSlice;
	RunPt->active = 0;
	sleepCount++;
	ThreadCounts[RunPt->priority]--;
	if((RunPt->next == RunPt) && (RunPt->prev == RunPt)){
		PriPointers[RunPt->priority] = NULL;
	}
	else{
		RunPt->prev->next = RunPt->next;
		RunPt->next->prev = RunPt->prev;
	}
	
	if(SleepPt == NULL){
		RunPt->prev = NULL;
		SleepPt = RunPt;
	}
	else{
		TCB* temp = SleepPt;
		while(temp->prev != NULL){
		temp = temp->prev;
		}
		temp->prev = RunPt;
		RunPt->prev = NULL;
	}
	//SleepPt = RunPt;
	OS_Suspend();
	EndCritical(status);
	//EnableInterrupts();
};  

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
  // put Lab 2 (and beyond) solution here
	//DisableInterrupts();
	long status = StartCritical();
	ThreadCount--;
	RunPt->pcb->numThreads--;
	if(RunPt->pcb->numThreads <= 0){
		RunPt->pcb->used = 0;
		ProcessCount--;
		Heap_Free(RunPt->pcb->data);
		Heap_Free(RunPt->pcb->code);
	}
	RunPt->used = 0;
	RunPt->active = 0;
	
	ThreadCounts[RunPt->priority]--;
	if((RunPt->next == RunPt) && (RunPt->prev == RunPt)){
		PriPointers[RunPt->priority] = NULL;
	}
	else{
		RunPt->prev->next = RunPt->next;
		RunPt->next->prev = RunPt->prev;
	}
	
	
	OS_Suspend();
	EndCritical(status);
  //EnableInterrupts();   // end of atomic section 
    
}; 

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void){
  // put Lab 2 (and beyond) solution here
	//DisableInterrupts();
	long status = StartCritical();
	if(ThreadCounts[RunPt->priority] > 0) PriPointers[RunPt->priority] = RunPt->next;
	for(int i = 0; i < PRIORITIES; i++){ //reverse if priority order incorrect (0 is greatest, 9 is least)
		if(PriPointers[i] != NULL){
			NextPt = PriPointers[i];
			break;
		}
	}
	CurrPCB = NextPt->pcb;
	while(NextPt == NULL){}; // DEBUGGING
  ContextSwitch();
		EndCritical(status);
	//EnableInterrupts();
};
  
// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(uint32_t size){
  // put Lab 2 (and beyond) solution here
   DataRoomLeft.Value = size;
	 MutexFifo.Value = 1;
	 DataAvailable.Value = 0;
  
};

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers 
//  this function can not disable or enable interrupts
int OS_Fifo_Put(uint32_t data){
  // put Lab 2 (and beyond) solution here
		// OS_Wait(&DataRoomLeft);
		// OS_bWait(&MutexFifo);
		Fifo[FifoPutIdx] = data;
		FifoPutIdx = (FifoPutIdx + 1) % FIFOSIZE;
		// OS_bSignal(&MutexFifo);
		OS_Signal(&DataAvailable);
		if(fifosize >= FIFOSIZE){
			return 0;
		}
		fifosize++;
		
    return 1; // replace this line with solution
};  

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
uint32_t OS_Fifo_Get(void){
  // put Lab 2 (and beyond) solution here
  OS_Wait(&DataAvailable);
	// OS_bWait(&MutexFifo);
	uint32_t data = Fifo[FifoGetIdx];
	FifoGetIdx = (FifoGetIdx + 1) % FIFOSIZE;
	fifosize--;
	// OS_bSignal(&MutexFifo);
	// OS_Signal(&DataRoomLeft);
	
  return data; // replace this line with solution
};

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty 
//          zero or less than zero if a call to OS_Fifo_Get will spin or block
int32_t OS_Fifo_Size(void){
  // put Lab 2 (and beyond) solution here
   
  return DataAvailable.Value; // replace this line with solution
};


// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void){
  // put Lab 2 (and beyond) solution here
  BoxFree.Value = 1;
	DataValid.Value = 0;

  // put solution here
};

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(uint32_t data){
  // put Lab 2 (and beyond) solution here
  // put solution here
   OS_bWait(&BoxFree);
	 MailBox = data;
	 OS_bSignal(&DataValid);
};

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
uint32_t OS_MailBox_Recv(void){
  // put Lab 2 (and beyond) solution here
	OS_bWait(&DataValid);
	uint32_t temp = MailBox;
	OS_bSignal(&BoxFree);
	
  return temp; // replace this line with solution
};

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
uint32_t OS_Time(void){
  return lab2Count*80000 + WTIMER0_TAR_R; // replace this line with solution
};

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
uint32_t OS_TimeDifference(uint32_t start, uint32_t stop){
  // put Lab 2 (and beyond) solution here
  return stop - start;
};


// ******** OS_ClearMsTime ************
// sets the system time to zero (solve for Lab 1), and start a periodic interrupt
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void){
  // put Lab 1 solution here
	lab1Count = 0;
};

// ******** OS_MsTime ************
// reads the current time in msec (solve for Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// For Labs 2 and beyond, it is ok to make the resolution to match the first call to OS_AddPeriodicThread
uint32_t OS_MsTime(void){
  // put Lab 1 solution here
  return lab1Count; // replace this line with solution
};


//******** OS_Launch *************** 
// start the scheduler, enable interrupts
// Inputs: number of 12.5ns clock cycles for each time slice
//         you may select the units of this parameter
// Outputs: none (does not return)
// In Lab 2, you can ignore the theTimeSlice field
// In Lab 3, you should implement the user-defined TimeSlice field
// It is ok to limit the range of theTimeSlice to match the 24-bit SysTick
void OS_Launch(uint32_t theTimeSlice){
  // put Lab 2 (and beyond) solution here
  
	//set runPT to task 0
	//context switch(OSStart)
	//While loop
	TimeSlice = theTimeSlice/80000;
	while(1){
		SysTick_Init(theTimeSlice);
		StartOS();
	}
    
};

//************** I/O Redirection *************** 
// redirect terminal I/O to UART or file (Lab 4)

int StreamToDevice=0;                // 0=UART, 1=stream to file (Lab 4)

int fputc (int ch, FILE *f) { 
  if(StreamToDevice==1){  // Lab 4
    if(eFile_Write(ch)){          // close file on error
       OS_EndRedirectToFile(); // cannot write to file
       return 1;                  // failure
    }
    return 0; // success writing
  }
  
  // default UART output
  UART_OutChar(ch);
  return ch; 
}

int fgetc (FILE *f){
  char ch = UART_InChar();  // receive from keyboard
  UART_OutChar(ch);         // echo
  return ch;
}

int OS_RedirectToFile(const char *name){  // Lab 4
  eFile_Create(name);              // ignore error if file already exists
  if(eFile_WOpen(name)) return 1;  // cannot open file
  StreamToDevice = 1;
  return 0;
}

int OS_EndRedirectToFile(void){  // Lab 4
  StreamToDevice = 0;
  if(eFile_WClose()) return 1;    // cannot close file
  return 0;
}

int OS_RedirectToUART(void){
  StreamToDevice = 0;
  return 0;
}

int OS_RedirectToST7735(void){
  
  return 1;
}

