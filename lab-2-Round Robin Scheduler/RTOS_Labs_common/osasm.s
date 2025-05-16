;/*****************************************************************************/
;/* OSasm.s: low-level OS commands, written in assembly                       */
;/* derived from uCOS-II                                                      */
;/*****************************************************************************/
;Jonathan Valvano, OS Lab2/3/4/5, 1/12/20
;Students will implement these functions as part of EE445M/EE380L.12 Lab

        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8

        EXTERN  RunPt            ; currently running thread
		EXTERN main_stack_pointer ;where main stack pointer is saved
		
        EXPORT  StartOS
        EXPORT  ContextSwitch
        EXPORT  PendSV_Handler
        EXPORT  SVC_Handler
		EXPORT 	Thread_Stack_init

NVIC_INT_CTRL   EQU     0xE000ED04                              ; Interrupt control state register.
NVIC_SYSPRI14   EQU     0xE000ED22                              ; PendSV priority register (position 14).
NVIC_SYSPRI15   EQU     0xE000ED23                              ; Systick priority register (position 15).
NVIC_LEVEL14    EQU           0xEF                              ; Systick priority value (second lowest).
NVIC_LEVEL15    EQU           0xFF                              ; PendSV priority value (lowest).
NVIC_PENDSVSET  EQU     0x10000000                              ; Value to trigger PendSV exception.


StartOS
; put your code here
 LDR     R0, =RunPt ; 4) R0=pointer to RunPt, old thread
 LDR     R1, [R0]      ;    R1 = RunPt
 LDR     SP, [R1]      ; 7) new thread SP; SP = RunPt->sp;
 POP     {R4-R11}  
 POP {R0-R3, R12}    ; 8) restore regs r4-11
 POP {R0}
 POP {LR}
 POP {R0}
 MSR PSR, R0
 CPSIE   I             ; 9) run with interrupts enabled
 BX      LR            ; 10) restore R0-R3,R12,LR,PC,PSR



OSStartHang
    B       OSStartHang        ; Should never get here


;********************************************************************************************************
;                               PERFORM A CONTEXT SWITCH (From task level)
;                                           void ContextSwitch(void)
;
; Note(s) : 1) ContextSwitch() is called when OS wants to perform a task context switch.  This function
;              triggers the PendSV exception which is where the real work is done.
;********************************************************************************************************

ContextSwitch
; edit this code
;store current stuff
;store current stack pointer
;stores updated current stack pointer
;grab next stack pointer
;  LDR r0, [r0, #20] ;loads next TCB pointer into r0, same as next->stack_pointer
;  LDR r1, [r0]  ;loads the stack pointer from that
;  MOVS r1, SP  ;sets stack pointer
;pop stuff off stack
;----this code is altered from lecture 2 slides----
 CPSID   I             ; 2) Prevent interrupt during switch
 PUSH {R0-R3, R12, LR}
 PUSH    {R4-R11}      ; 3) Save remaining regs r4-11
 ;MRS R0, PSR		;save PSR to R0
 ;PUSH {R0}				;PUSH PSR onto stack
 LDR     R0, =RunPt ; 4) R0=pointer to RunPt, old thread
 LDR     R1, [R0]      ;    R1 = RunPt
 STR     SP, [R1]      ; 5) Save SP into TCB
 LDR     R1, [R1,#4]   ; 6) R1 = RunPt->next
 STR     R1, [R0]      ;    RunPt= R1
 LDR     SP, [R1]      ; 7) new thread SP; SP = RunPt->sp;
 ;POP {R0}
 ;MSR PSR, R0
 POP     {R4-R11}      ; 8) restore regs r4-11
 POP {R0-R3, R12, LR}
 CPSIE   I             ; 9) run with interrupts enabled
 BX      LR            ; 10) restore R0-R3,R12,LR,PC,PSR
    
    BX      LR
    

;********************************************************************************************************
;                                         HANDLE PendSV EXCEPTION
;                                     void OS_CPU_PendSVHandler(void)
;
; Note(s) : 1) PendSV is used to cause a context switch.  This is a recommended method for performing
;              context switches with Cortex-M.  This is because the Cortex-M3 auto-saves half of the
;              processor context on any exception, and restores same on return from exception.  So only
;              saving of R4-R11 is required and fixing up the stack pointers.  Using the PendSV exception
;              this way means that context saving and restoring is identical whether it is initiated from
;              a thread or occurs due to an interrupt or exception.
;
;           2) Pseudo-code is:
;              a) Get the process SP, if 0 then skip (goto d) the saving part (first context switch);
;              b) Save remaining regs r4-r11 on process stack;
;              c) Save the process SP in its TCB, OSTCBCur->OSTCBStkPtr = SP;
;              d) Call OSTaskSwHook();
;              e) Get current high priority, OSPrioCur = OSPrioHighRdy;
;              f) Get current ready thread TCB, OSTCBCur = OSTCBHighRdy;
;              g) Get new process SP from TCB, SP = OSTCBHighRdy->OSTCBStkPtr;
;              h) Restore R4-R11 from new process stack;
;              i) Perform exception return which will restore remaining context.
;
;           3) On entry into PendSV handler:
;              a) The following have been saved on the process stack (by processor):
;                 xPSR, PC, LR, R12, R0-R3
;              b) Processor mode is switched to Handler mode (from Thread mode)
;              c) Stack is Main stack (switched from Process stack)
;              d) OSTCBCur      points to the OS_TCB of the task to suspend
;                 OSTCBHighRdy  points to the OS_TCB of the task to resume
;
;           4) Since PendSV is set to lowest priority in the system (by OSStartHighRdy() above), we
;              know that it will only be run when no other exception or interrupt is active, and
;              therefore safe to assume that context being switched out was using the process stack (PSP).
;********************************************************************************************************

PendSV_Handler
; put your code here

    ;----this code is altered from lecture 2 slides----
 CPSID   I             ; 2) Prevent interrupt during switch
 PUSH    {R4-R11}      ; 3) Save remaining regs r4-11
 ;MRS R0, PSR		;save PSR to R0
 ;PUSH {R0}				;PUSH PSR onto stack
 LDR     R0, =RunPt ; 4) R0=pointer to RunPt, old thread
 LDR     R1, [R0]      ;    R1 = RunPt
 STR     SP, [R1]      ; 5) Save SP into TCB
 LDR     R1, [R1,#4]   ; 6) R1 = RunPt->next
 STR     R1, [R0]      ;    RunPt= R1
 LDR     SP, [R1]      ; 7) new thread SP; SP = RunPt->sp;
 ;POP {R0}
 ;MSR PSR, R0
 POP     {R4-R11}      ; 8) restore regs r4-11
 CPSIE   I             ; 9) run with interrupts enabled
 BX      LR            ; 10) restore R0-R3,R12,LR,PC,PSR
    
MainToThread
; PUSH {R0-R3, R12, LR}
; PUSH {R4-R11}
; MRS R0, PSR		;save PSR to R0
; PUSH {R0}				;PUSH PSR onto stack
; LDR R0, =main_stack_pointer
 ;STR SP, [R0] 		;store main stack pointer
 LDR     R0, =RunPt ; 4) R0=pointer to RunPt, old thread
 LDR     R1, [R0]      ;    R1 = RunPt
 LDR     SP, [R1]      ; 7) new thread SP; SP = RunPt->sp;
 POP     {R4-R11}  
 POP {R0-R3, R12}    ; 8) restore regs r4-11
 POP {R0}
 POP {LR}
 POP {R0}
 MSR PSR, R0
 CPSIE   I             ; 9) run with interrupts enabled
 BX      LR            ; 10) restore R0-R3,R12,LR,PC,PSR

    

;********************************************************************************************************
;                                         HANDLE SVC EXCEPTION
;                                     void OS_CPU_SVCHandler(void)
;
; Note(s) : SVC is a software-triggered exception to make OS kernel calls from user land. 
;           The function ID to call is encoded in the instruction itself, the location of which can be
;           found relative to the return address saved on the stack on exception entry.
;           Function-call paramters in R0..R3 are also auto-saved on stack on exception entry.
;********************************************************************************************************

        IMPORT    OS_Id
        IMPORT    OS_Kill
        IMPORT    OS_Sleep
        IMPORT    OS_Time
        IMPORT    OS_AddThread

SVC_Handler
; put your Lab 5 code here


    BX      LR                   ; Return from exception




Thread_Stack_init
	;R0 has function pointer address
	;PUSH {R0-R3, R12, LR}
    ;PUSH    {R4-R11}      ; 3) Save remaining regs r4-11
    ;MRS R0, PSR		;save PSR to R0
    ;PUSH {R0}				;PUSH PSR onto stack
	
	; R0 has function pointer address
	; R1 holds new stack pointer
	MOVS R2, 1<<24
	STR R2, [R1]
	SUBS R1, #4
	STR R0, [R1] 	;function pointer first into pc
	SUBS R1, #4
	MOVS R12, #12
	SUBS R1, #4
	STR R12, [R1] 	;will be R12 on pop
	SUBS R1, #48
	MOVS R0, R1
	
	BX LR

;	PUSH {R1}
;	MOVS R1, #1
;	PUSH {R1}
;	MOVS R2, #2
;	PUSH {R2}
;	MOVS R3, #3
;	PUSH {R3}
;	MOVS R4, #4
;	PUSH {R4}
;	MOVS R5, #5
;	PUSH {R5}
;	MOVS R6, #6
;	PUSH {R6}
;	MOVS R7, #7
;	PUSH {R7}
;	MOVS R8, #8
;	PUSH {R8}
;	MOVS R9, #9
;	PUSH {R9}
;	MOVS R10, #10
;	PUSH {R10}
;	MOVS R11, #11
;	PUSH {R11}
;	MOVS R12, #12
;	PUSH {R12}
;	PUSH {R0} ; pushes function pointer as Link Register

;	MRS R0, PSR
;	PUSH {R0}

    ALIGN
    END
		

