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
		EXTERN 	NextPt

        EXPORT  StartOS
        EXPORT  ContextSwitch
        EXPORT  PendSV_Handler
        EXPORT  SVC_Handler


NVIC_INT_CTRL   EQU     0xE000ED04                              ; Interrupt control state register.
NVIC_SYSPRI14   EQU     0xE000ED22                              ; PendSV priority register (position 14).
NVIC_SYSPRI15   EQU     0xE000ED23                              ; Systick priority register (position 15).
NVIC_LEVEL14    EQU           0xEF                              ; Systick priority value (second lowest).
NVIC_LEVEL15    EQU           0xFF                              ; PendSV priority value (lowest).
NVIC_PENDSVSET  EQU     0x10000000                              ; Value to trigger PendSV exception.


StartOS
	;CHANGED POP IN THE SAME WAY AS REGS WERE PUSHED WHEN CONTEXT SWITCHING
	CPSID	I
	LDR R0, =RunPt ; currently running thread
	LDR R1, [R0] ; R1 = value of RunPt
	LDR SP, [R1] ; new thread SP; SP = RunPt->sp;
	POP {R4-R11} ; restore regs r4-11
	POP {R0-R3} ; restore regs r0-3
	POP {R12}
	ADD SP, SP, #4 ; discard LR from initial stack
	POP {LR} ; start location (PC)
	ADD SP, SP, #4 ; discard PSR
	CPSIE	I
    BX      LR                 ; start first thread

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
	LDR R0, =NVIC_INT_CTRL
	LDR R1, =NVIC_PENDSVSET
	STR R1, [R0]
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
	CPSID	I
	PUSH {R4-R11} ; 3) Save remaining regs r4-11
	LDR R0, =RunPt ; 4) R0=pointer to RunPt, old thread
	LDR R1, [R0] ; R1 = RunPt
	STR SP, [R1] ; 5) Save SP into TCB
	LDR R1, =NextPt ; 6) R1 = address of NextPt
	LDR R1, [R1] ;6.5) R1 = NextPt
	STR R1, [R0] ; RunPt = R1 = NextPt
	LDR SP, [R1] ; 7) new thread SP; SP = RunPt->sp;
	POP {R4-R11} ; 8) restore regs r4-11
	CPSIE	I
    BX      LR                 ; Exception return will restore remaining context
	
;	CPSID	I
;	PUSH {R4-R11} ; 3) Save remaining regs r4-11
;	LDR R0, =RunPt ; 4) R0=pointer to RunPt, old thread
;	LDR R1, [R0] ; R1 = RunPt
;	STR SP, [R1] ; 5) Save SP into TCB
;	LDR R1, [R1,#4] ; 6) R1 = RunPt->next
;	STR R1, [R0] ; RunPt = R1
;	LDR SP, [R1] ; 7) new thread SP; SP = RunPt->sp;
;	POP {R4-R11} ; 8) restore regs r4-11
;	CPSIE	I
;    BX      LR                 ; Exception return will restore remaining context
;    

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
    ;CPSID   I
    LDR R12,[SP,#24] ; Return address

    LDRH R12,[R12,#-2] ; SVC instruction is 2 bytes
    BIC R12,#0xFF00 ; Extract ID in R12
    LDM SP,{R0-R3} ; Get any parameters

    PUSH{LR}
    CMP R12, #0
    BLEQ OS_Id ; Call OS routine by ID�

    CMP R12, #1
    BLEQ OS_Kill

    CMP R12, #2
    BLEQ OS_Sleep

    CMP R12, #3
    BLEQ OS_Time

    CMP R12, #4
    BLEQ OS_AddThread


    POP{LR}
    STR R0,[SP] ; Store return value
    ;CPSIE   I 
    BX LR ; Return from exception  

    ALIGN
    END
