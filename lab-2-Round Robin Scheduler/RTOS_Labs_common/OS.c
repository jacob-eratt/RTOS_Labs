// *************os.c**************
// EE445M/EE380L.6 Labs 1, 2, 3, and 4 
// High-level OS functions
// Students will implement these functions as part of Lab
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 
// Jan 12, 2020, valvano@mail.utexas.edu


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/PLL.h"
#include "../inc/LaunchPad.h"

#include "../inc/Timer0A.h"
#include "../inc/Timer1A.h"
#include "../inc/Timer2A.h"
#include "../inc/Timer3A.h"
#include "../inc/Timer4A.h"
#include "../inc/Timer5A.h"
#include "../inc/WTimer0A.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eFile.h"




#define NUM_STACKS 10
#define STACK_SIZE 128  //STACK SIZE
#define NUM_MB	10
#define FIFO_SIZE 4

volatile uint32_t OS_timeMs;

extern int ContextSwitch();
extern uint32_t* Thread_Stack_init(void(*task), uint32_t* s_pointer);
extern void StartOS(void);

uint32_t num_ticks = 0;
uint32_t count;

//TCB Struct
typedef struct TCB{
  uint32_t* stack_pointer;
	struct TCB* next;
	struct TCB* previous;
	struct TCB* sleep_list_nextPtr;
	uint32_t id;
	uint32_t sleep;
	//uint32_t blocked;
	//uint32_t priority;
	

}TCB_t;

uint32_t thread_count = 0;
uint32_t sleep_count = 0;
uint32_t stacks[10][STACK_SIZE];
uint32_t stack_availability[10] = {0,0,0,0,0,0,0,0,0,0};

TCB_t* SleepPt;
TCB_t TCB_list[10];

TCB_t* RunPt;
uint32_t* main_stack_pointer;


// LINK LIST HEAD NODE
TCB_t* head_node = NULL;

//
typedef struct{
	Sema4Type send;
	Sema4Type ack;
	uint32_t value;
	uint8_t in_use;
}Mailbox;

//Mailbox mailboxes[NUM_MB];
//uint8_t open_mailboxes[NUM_MB] = {0,0,0,0,0,0,0,0,0,0};
//uint8_t num_mailboxes = 0;
Mailbox mailbox_1;


typedef struct{
	uint32_t array[FIFO_SIZE];
	Sema4Type CurrentSize; // 0 means FIFO empty
	Sema4Type RoomLeft; // 0 means FIFO full
	Sema4Type FIFOmutex; // exclusive access to FIFO
	uint32_t* putPt;
	uint32_t* getPt;
	uint32_t counter;
}FIFO;

FIFO fifo_1;


// Performance Measurements 
int32_t MaxJitter;             // largest time jitter between interrupts in usec
#define JITTERSIZE 64
uint32_t const JitterSize=JITTERSIZE;
uint32_t JitterHistogram[JITTERSIZE]={0,};


void ms_timer_increment(void){
	//OS_timeMs++;
	count++;
}

void sleep_to_run(TCB_t* thread_to_add){
		if(thread_count == 0){
			head_node = thread_to_add;
			head_node->next = thread_to_add;
			head_node->previous = thread_to_add;
		}
		else{
			TCB_t* last_node = head_node->previous;
			
			thread_to_add->next = head_node;
			thread_to_add->previous = last_node;
			
			last_node->next = thread_to_add;
			head_node->previous = thread_to_add;
		}
		thread_count++;
}

int OS_check_sleep_threads(){
  long status = StartCritical();
	TCB_t* iterator = SleepPt;
	TCB_t* prev = NULL;
	while(iterator != NULL){
		if(iterator->sleep <= num_ticks){
			sleep_to_run(iterator);
			sleep_count--;
			if(prev == NULL){
				SleepPt = iterator->sleep_list_nextPtr;
			}
			else{
			  prev->sleep_list_nextPtr = iterator->sleep_list_nextPtr;
			}
			
		}
		else{
			prev = iterator;
		}
		iterator = iterator->sleep_list_nextPtr;
	}
	
	EndCritical(status);
	return 1;
}



/*------------------------------------------------------------------------------
  Systick Interrupt Handler
  SysTick interrupt happens every 10 ms
  used for preemptive thread switch
 *------------------------------------------------------------------------------*/

#define PD0  (*((volatile uint32_t *)0x40007004))
#define PD1  (*((volatile uint32_t *)0x40007008))
#define PD2  (*((volatile uint32_t *)0x40007010))
#define PD3  (*((volatile uint32_t *)0x40007020))
void SysTick_Handler(void) {
	DisableInterrupts();
	num_ticks++;
	volatile int x = OS_check_sleep_threads();
  OS_Suspend();
	EnableInterrupts();
} // end SysTick_Handler

unsigned long OS_LockScheduler(void){
  // lab 4 might need this for disk formating
  return 0;// replace with solution
}
void OS_UnLockScheduler(unsigned long previous){
  // lab 4 might need this for disk formating
}

#define SYSTICK_LOAD (80000000/100)-1;
void SysTick_Init(unsigned long period){
	/******* Code altered from ValvanoWare Example *******/
  NVIC_ST_CTRL_R = 0;                   // disable SysTick during setup
  NVIC_ST_RELOAD_R = SYSTICK_LOAD;  // to run at 10 ms
  NVIC_ST_CURRENT_R = 0;                // any write to current clears it
                                        // enable SysTick with core clock
  NVIC_ST_CTRL_R = NVIC_ST_CTRL_ENABLE+NVIC_ST_CTRL_CLK_SRC+NVIC_ST_CTRL_INTEN;
	//PF1 ^= 0x02;
}




/**
 * @details  Initialize operating system, disable interrupts until OS_Launch.
 * Initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers.
 * Interrupts not yet enabled.
 * @param  none
 * @return none
 * @brief  Initialize OS
 */

void OS_Init(void){
  // put Lab 2 (and beyond) solution here
	DisableInterrupts();
	PLL_Init(Bus80MHz);
	LaunchPad_Init();
	
	//set pend sv priority to lowest
	NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0xFF1FFFFF)|(7<<21);
	NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0x9FFFFFFF)|(6<<28);
	
	//UART
	UART_Init();
	//LCD
	ST7735_InitR(INITR_REDTAB);
	ST7735_DrawFastHLine(0, 78, 128, 0xFFFF);
	//ADC
	
	//add periodic thread to account for OS timer

	
	count = 0;
	SysTick_Init(0);
}; 

// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, int32_t value){
  // put Lab 2 (and beyond) solution here
	semaPt->Value = value;
}; 

// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	DisableInterrupts();

	while(semaPt->Value <= 0){
		EnableInterrupts();
		DisableInterrupts();
	}
	semaPt->Value = semaPt->Value - 1;
	EnableInterrupts();
  
}; 

// ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution herePD
	long status;
  status = StartCritical();
	semaPt->Value = semaPt->Value + 1;
  EndCritical(status);

}; 

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	DisableInterrupts();
	while(semaPt->Value <= 0){
		EnableInterrupts();
		DisableInterrupts();
	}
	semaPt->Value = 0;
	EnableInterrupts();
  

}; 

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	long status;
  status = StartCritical();
	semaPt->Value = 1;
  EndCritical(status);
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
		 int avail_stack = -1;
		 //find new stack
		long status = StartCritical();
		 for(int i = 0; i < NUM_STACKS; i++){
				if(stack_availability[i] == 0){
				  avail_stack = i;
					stack_availability[i]=1;
					break;
				}
		 }
		 if(avail_stack == -1){
			return avail_stack;
		 }
			TCB_t* new_TCB = &TCB_list[avail_stack];
		//TCB_t* new_TCB = (TCB_t*)malloc(sizeof(TCB_t));
		new_TCB->id = avail_stack; //set id of TCB to stack space id
				//fill stack with dummy variables, and set it to the new TCB stack pointer
		new_TCB->stack_pointer = Thread_Stack_init((*task), &stacks[avail_stack][STACK_SIZE-1]);
		new_TCB->sleep = 0;
				
		if(thread_count == 0){
			head_node = new_TCB;
			head_node->next = new_TCB;
			head_node->previous = new_TCB;
		}
		else{
			TCB_t* last_node = head_node->previous;
			
			new_TCB->next = head_node;
			new_TCB->previous = last_node;
			
			last_node->next = new_TCB;
			head_node->previous = new_TCB;
		}
		 
		
		 thread_count++;
	EndCritical(status);
  return 1; // replace this line with solution
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
  // put Lab 5 solution here

     
  return 0; // replace this line with Lab 5 solution
}


//******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
uint32_t OS_Id(void){
  // put Lab 2 (and beyond) solution here
	return RunPt->id;
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
    //DisableInterrupts();
		 long status = StartCritical();
    SYSCTL_RCGCTIMER_R |= 0x3F;

    //if (!(TIMER0_CTL_R & 0x01)) {
       // Timer0A_Init(task, period, priority);
    if (!(TIMER1_CTL_R & 0x01)) {
        Timer1A_Init(task, period, priority);
    } else if (!(TIMER2_CTL_R & 0x01)) {
        Timer2A_Init(task, period, priority);
    } else if (!(TIMER3_CTL_R & 0x01)) {
        Timer3A_Init(task, period, priority);
    } else if (!(TIMER4_CTL_R & 0x01)) {
        Timer4A_Init(task, period, priority);
    } else if (!(TIMER5_CTL_R & 0x01)) {
        Timer5A_Init(task, period, priority);
    } else {
        //EnableInterrupts();
				EndCritical(status);
        return 0;
    }
    //thread_count++;
    EndCritical(status);
    return 1;
}


/*----------------------------------------------------------------------------
  PF1 Interrupt Handler
 *----------------------------------------------------------------------------*/
#define DEBOUNCE_PERIOD 10
void (*pf4_task)(void);
void (*pf0_task)(void);
volatile uint32_t FallingEdges;
void GPIOPortF_Handler(void){ 
	
	long status = StartCritical();
  FallingEdges = FallingEdges + 1;
	static uint32_t lastPressTimePF4 = 0;  // Keeps track of the last valid button press time
	static uint32_t lastPressTimePF0 = 0;
    	
	
    // Check for PF4 interrupt
    if (GPIO_PORTF_RIS_R & 0x10) { // PF4 interrupt flag set
        GPIO_PORTF_ICR_R = 0x10;   // Clear PF4 interrupt flag immediately
        if ((GPIO_PORTF_DATA_R & 0x10) == 0 && ((num_ticks - lastPressTimePF4) >= DEBOUNCE_PERIOD)) {
            lastPressTimePF4 = num_ticks; // Update the last valid press time
            (*pf4_task)();                // Execute the task
        }
    }

    // Check for PF0 interrupt
    if (GPIO_PORTF_RIS_R & 0x01) { // PF0 interrupt flag set
        GPIO_PORTF_ICR_R = 0x01;   // Clear PF0 interrupt flag immediately
        if ((GPIO_PORTF_DATA_R & 0x01) == 0 && ((num_ticks - lastPressTimePF0) >= DEBOUNCE_PERIOD)) {
            lastPressTimePF0 = num_ticks; // Update the last valid press time
            (*pf0_task)();                // Execute the task
        }
    }
	
  PF1 ^= 0x02; // profile
	EndCritical(status);
	
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
	//Copied from EdgeInterruptPortF.C
	long status = StartCritical();
	SYSCTL_RCGCGPIO_R |= 0x00000020; // (a) activate clock for port F
  FallingEdges = 0;             // (b) initialize counter
  GPIO_PORTF_LOCK_R = 0x4C4F434B;   // 2) unlock GPIO Port F
  GPIO_PORTF_CR_R = 0x1F;           // allow changes to PF4-0
  GPIO_PORTF_DIR_R |=  0x0E;    // output on PF3,2,1 
  GPIO_PORTF_DIR_R &= ~0x11;    // (c) make PF4,0 in (built-in button)
  GPIO_PORTF_AFSEL_R &= ~0x1F;  //     disable alt funct on PF4,0
  GPIO_PORTF_DEN_R |= 0x1F;     //     enable digital I/O on PF4   
  GPIO_PORTF_PCTL_R &= ~0x000FFFFF; // configure PF4 as GPIO
  GPIO_PORTF_AMSEL_R = 0;       //     disable analog functionality on PF
  GPIO_PORTF_PUR_R |= 0x11;     //     enable weak pull-up on PF4
  GPIO_PORTF_IS_R &= ~0x10;     // (d) PF4 is edge-sensitive
  GPIO_PORTF_IBE_R &= ~0x10;    //     PF4 is not both edges
  GPIO_PORTF_IEV_R &= ~0x10;    //     PF4 falling edge event
  GPIO_PORTF_ICR_R = 0x10;      // (e) clear flag4
  GPIO_PORTF_IM_R |= 0x10;      // (f) arm interrupt on PF4 *** No IME bit as mentioned in Book ***
  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|(priority <<21);//0x00A00000; // (g) priority 5
  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
	pf4_task = task;
	EndCritical(status);
  return 0; // replace this line with solution
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
//  SYSCTL_RCGCGPIO_R |= 0x00000020; // (a) activate clock for port F
//  FallingEdges = 0;             // (b) initialize counter
//  GPIO_PORTF_LOCK_R = 0x4C4F434B;   // 2) unlock GPIO Port F
//  GPIO_PORTF_CR_R = 0x1F;           // allow changes to PF4-0
//  GPIO_PORTF_DIR_R |=  0x0E;    // output on PF3,2,1 
//  GPIO_PORTF_DIR_R &= ~0x11;    // (c) make PF4,0 in (built-in button)
//  GPIO_PORTF_AFSEL_R &= ~0x1F;  //     disable alt funct on PF4,0
//  GPIO_PORTF_DEN_R |= 0x1F;     //     enable digital I/O on PF4   
//  GPIO_PORTF_PCTL_R &= ~0x000FFFFF; // configure PF4 as GPIO
//  GPIO_PORTF_AMSEL_R = 0;       //     disable analog functionality on PF
//  GPIO_PORTF_PUR_R |= 0x11;     //     enable weak pull-up on PF4
//  GPIO_PORTF_IS_R &= ~0x01;     // (d) PF4 is edge-sensitive
//  GPIO_PORTF_IBE_R &= ~0x01;    //     PF4 is not both edges
//  GPIO_PORTF_IEV_R &= ~0x01;    //     PF4 falling edge event
//  GPIO_PORTF_ICR_R = 0x01;      // (e) clear flag4
//  GPIO_PORTF_IM_R |= 0x01;      // (f) arm interrupt on PF4 *** No IME bit as mentioned in Book ***
//  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|(priority <<21);//0x00A00000; // (g) priority 5
//  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
//	pf0_task = task;
  return 0; // replace this line with solution
};


// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime){
  // put Lab 2 (and beyond) solution here
	long status  = StartCritical();
	TCB_t* sleep_TCB = RunPt;
	sleep_TCB->previous->next = sleep_TCB->next; // prev TCB gets set to  next TCB
	sleep_TCB->next->previous = sleep_TCB->previous; //next TCB's prev pointer gets updated 
	head_node =  sleep_TCB->next;

	sleep_TCB->sleep_list_nextPtr = NULL;
	sleep_TCB->sleep = num_ticks + sleepTime;
	

	if(sleep_count == 0){
		SleepPt = sleep_TCB;
		sleep_TCB->sleep_list_nextPtr = NULL;
	}
	else{
		sleep_TCB->sleep_list_nextPtr = SleepPt->sleep_list_nextPtr;
		SleepPt->sleep_list_nextPtr = sleep_TCB;
	}
	sleep_count++;
	thread_count--;

	OS_Suspend();
	//EndCritical(status);
	EnableInterrupts();

};  

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
  // put Lab 2 (and beyond) solution here
	DisableInterrupts();
	TCB_t* kill_TCB = RunPt;
	RunPt->previous->next = RunPt->next;
	RunPt->next->previous = RunPt->previous;
	
	if(kill_TCB == head_node){
		head_node = kill_TCB->next;
	}
	stack_availability[kill_TCB->id] = 0;
	thread_count--;
	//RunPt = kill_TCB->next;
	OS_Suspend();
  EnableInterrupts();   // end of atomic section 
  for(;;){};        // can not return
    
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
	NVIC_INT_CTRL_R = 0x10000000;
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
	//copied from textbook pg 220
   fifo_1.putPt = &fifo_1.array[0];
	 fifo_1.getPt = &fifo_1.array[0];
	 OS_InitSemaphore(&fifo_1.CurrentSize, 0);
	 OS_InitSemaphore(&fifo_1.RoomLeft, FIFO_SIZE);
	OS_InitSemaphore(&fifo_1.FIFOmutex, 1);
	fifo_1.counter = 0;
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
	//code copied from text book 220
	if(fifo_1.counter != 0 && fifo_1.putPt == fifo_1.getPt){
		return 0;
	}
  *(fifo_1.putPt) = data; // Put
  fifo_1.putPt++; // place to put next
  if(fifo_1.putPt == &fifo_1.array[FIFO_SIZE]){
   fifo_1.putPt = &fifo_1.array[0]; // wrap
  }

  OS_Signal(&fifo_1.CurrentSize);
	fifo_1.counter++;
  return 1; // replace this line with solution
};  

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
uint32_t OS_Fifo_Get(void){
  // put Lab 2 (and beyond) solution here
	//code copied from text book 220
	 uint32_t data;
   OS_Wait(&fifo_1.CurrentSize);

   data = *(fifo_1.getPt); // get data
   fifo_1.getPt++; // points to next data to get
   if(fifo_1.getPt == &fifo_1.array[FIFO_SIZE]){
     fifo_1.getPt = &fifo_1.array[0]; // wrap
   }
	 fifo_1.counter--;
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
   return fifo_1.CurrentSize.Value;
 };


// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void){
  // put Lab 2 (and beyond) solution here
		if (mailbox_1.in_use){
			return;
		}
		mailbox_1.in_use = 1;
		OS_InitSemaphore(&mailbox_1.send, 0);
		OS_InitSemaphore(&mailbox_1.ack, 0);
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
	mailbox_1.value = data;
	OS_Signal(&mailbox_1.send);
	OS_Wait(&mailbox_1.ack);
	
   

};

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
uint32_t OS_MailBox_Recv(void){
  // put Lab 2 (and beyond) solution here
	OS_Wait(&mailbox_1.send);
	uint32_t data = mailbox_1.value;
	OS_Signal(&mailbox_1.ack);
 
  return data; // replace this line with solution
};

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
uint32_t OS_Time(void){
  // put Lab 2 (and beyond) solution here
	return num_ticks;
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
	int milsec = stop - start;
	int mil_to_nano = milsec * 1000000;
	int time_units = mil_to_nano/12.5;
  return time_units; // replace this line with solution
};





// ******** OS_ClearMsTime ************
// sets the system time to zero (solve for Lab 1), and start a periodic interrupt
// Inputs:  none
// Outputs: none
// You are free to change how this works-----------------------------------------------------------------------------------------

//Timer0A_Init(&UserTask, F2HZ,2);
void OS_ClearMsTime(void){
  // put Lab 1 solution here
  OS_timeMs = 0;
};

// ******** OS_MsTime ************
// reads the current time in msec (solve for Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// For Labs 2 and beyond, it is ok to make the resolution to match the first call to OS_AddPeriodicThread
uint32_t OS_MsTime(void){
  // put Lab 1 solution here
  return OS_timeMs; // replace this line with solution
};


// ******** OS_IncrementTime ********
//
//
//
//
void OS_IncrementTime(){
  OS_timeMs++;
}
	


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
  RunPt = head_node;
	//systickTime = NVIC_ST_CURRENT_R;
	OS_ClearMsTime();
	StartOS();
	EnableInterrupts();
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

