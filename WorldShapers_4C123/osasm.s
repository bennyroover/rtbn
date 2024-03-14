;/*****************************************************************************/
; OSasm.s: low-level OS commands, written in assembly                       */
; Runs on LM4F120/TM4C123/MSP432
; Lab 4 starter file
; March 25, 2016

;


        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8

        EXTERN  RunPt            ; currently running thread
        EXPORT  StartOS
        EXPORT  SysTick_Handler
        IMPORT  Scheduler
        EXPORT  PendSV_Handler

SysTick_Handler                ; 1) Saves R0-R3,R12,LR,PC,PSR
    CPSID   I                  ; 2) Prevent interrupt during switch
    ;YOU IMPLEMENT THIS (same as Lab 3)
    
    PUSH    {R4-R11}           ; 3) Save status of current thread to stack
    LDR     R0, =RunPt         ; 4) R0 has the address of RunPtr. 
    LDR     R1, [R0]           ;    R1 has RunPtr
    STR     SP, [R1]           ; 5) Save current thread stack pointer into TCB
    
    ; 6) Update RunPtr here
    ; LDR     R1, [R1, #4]       ; R1 = RunPtr -> next   
    ; STR     R1, [R0]           ; Update RunPtr contents
    ; OR Update RunPtr in scheduler
    PUSH    {R0, LR}           ; Push our working register R0 and the LR 
    BL      Scheduler          ; Branch and execute Scheduler
    POP     {R0, LR}           ; After returning, pop our R0 and LR
    LDR     R1, [R0]           ; R1 gets new RunPtr
    
    LDR     SP, [R1]           ; 7) Update Stack Pointer to next thread sp   
    POP     {R4-R11}           ; 8) Restore R4-R11
    
    CPSIE   I                  ; 9) tasks run with interrupts enabled
    BX      LR                 ; 10) restore R0-R3,R12,LR,PC,PSR

StartOS
    ;YOU IMPLEMENT THIS (same as Lab 3)
    CPSIE   I                  ; Enable interrupts at processor level
    
    LDR     R0, =RunPt         ; 1) R0 has address of pointer to first tcb
    LDR     R1, [R0]           ; 2) R1 gets address of first tcb
    LDR     SP, [R1]           ; 3) Set initial stack pointer
    POP     {R4-R11}           ; 4) grab initial (meaningless) register values
    POP     {R0-R3}            ; 
    POP     {R12}              ;
    ADD     SP, SP, #4         ; 5) discard value of LR
    POP     {LR}               ; 6) pop the address of the first task
    ADD     SP, SP, #4         ; 7) discard PSR (has no real value) 
    
    BX      LR                 ; start first thread

PendSV_Handler
    LDR     R0, =RunPt         ; run this thread next
    LDR     R2, [R0]           ; R2 = value of RunPt
    LDR     SP, [R2]           ; new thread SP; SP = RunPt->stackPointer;
    POP     {R4-R11}           ; restore regs r4-11
    LDR     LR,=0xFFFFFFF9
    BX      LR                 ; start next thread
	
    ALIGN
    END
