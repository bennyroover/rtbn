// os.c
// Runs on LM4F120/TM4C123/MSP432
// A priority/blocking real-time operating system 
// Lab 4 starter file.
// Daniel Valvano
// October 9, 2016
// Hint: Copy solutions from Lab 3 into Lab 4

#include <stdint.h>
#include "os.h"
#include "CortexM.h"
#include "BSP.h"
#include "../inc/tm4c123gh6pm.h"

// function definitions in osasm.s
void StartOS(void);

#define NUMTHREADS  20       // maximum number of threads
#define NUMPERIODIC 2        // maximum number of periodic threads
#define STACKSIZE   100      // number of 32-bit words in stack per thread
struct tcb{
  int32_t *sp;       // pointer to stack (valid for threads not running
  struct tcb *next;  // linked-list pointer
  uint32_t Id;       // 0 means TCB is free
  int32_t *BlockPt;  // nonzero if blocked on this semaphore
  uint32_t Sleep;    // nonzero if this thread is sleeping
  uint32_t Priority; // 0 is highest
};
typedef struct tcb tcbType;
tcbType tcbs[NUMTHREADS];
tcbType *RunPt;
int32_t Stacks[NUMTHREADS][STACKSIZE];
void static runperiodicevents(void);
uint32_t NumThread=0;  // number of threads
uint32_t static ThreadId=0;   // thread Ids are sequential from 1

// ******** OS_Init ************
// Initialize operating system, disable interrupts
// Initialize OS controlled I/O: periodic interrupt, bus clock as fast as possible
// Initialize OS global variables
// Inputs:  none
// Outputs: none
void OS_Init(void){
  DisableInterrupts();
  BSP_Clock_InitFastest();// set processor clock to fastest speed
  NumThread=0;  // number of threads
  ThreadId=0;   // thread Ids are sequential from 1
// perform any initializations needed, 
// set up periodic timer to run runperiodicevents to implement sleeping
  BSP_PeriodicTask_InitB(&runperiodicevents, 1000, 0);
}



//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground function
//         priority (0 is highest)
// Outputs: Thread ID if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
int OS_AddThread(void(*task)(void), uint32_t priority){ int status;
  int n;         // index to new thread TCB 
  tcbType *NewPt;  // Pointer to nex thread TCB
  tcbType *LastPt; // Pointer to last thread TCB
  int32_t *sp;      // stack pointer
  status = StartCritical();
//  NewPt = malloc(stackSize+10);    // 9 extra bytes needed
  for(n=0; n< NUMTHREADS; n++){
    if(tcbs[n].Id == 0) break; // found it 
    if(n == NUMTHREADS-1){  
      EndCritical(status);
      return 0;          // heap is full
    }
  }
  NewPt = &tcbs[n]; 
  if(NumThread==0){
    RunPt = NewPt;  // points to first thread created
  } else{
    LastPt = RunPt;
    while(LastPt->next != RunPt){
      LastPt = LastPt->next;
    }
    LastPt->next = NewPt; // Pointer to Next  
  }
  NewPt->Priority =  priority;
  NumThread++;
  ThreadId++;
  NewPt->Id = ThreadId;
  NewPt->BlockPt =  0;    // not blocked
  NewPt->Sleep =  0;      // not sleeping

  sp = &Stacks[n][STACKSIZE-1];      // last entry of stack


// derived from uCOS-II
                                            /* Registers stacked as if auto-saved on exception    */
  *(sp)    = (long)0x01000000L;             /* xPSR , T=1                                         */
  *(--sp)  = (long)task;                    /* Entry Point                                        */
  *(--sp)  = (long)0x14141414L;             /* R14 (LR) (init value does not matter)              */
  *(--sp)  = (long)0x12121212L;             /* R12                                                */
  *(--sp)  = (long)0x03030303L;             /* R3                                                 */
  *(--sp)  = (long)0x02020202L;             /* R2                                                 */
  *(--sp)  = (long)0x01010101L;             /* R1                                                 */
  *(--sp)  = (long)0x000000000;             /* R0                                                 */

                                            /* Remaining registers saved on process stack         */
  *(--sp)  = (long)0x11111111L;             /* R11                                                */
  *(--sp)  = (long)0x10101010L;             /* R10                                                */
  *(--sp)  = (long)0x09090909L;             /* R9                                                 */
  *(--sp)  = (long)0x08080808L;             /* R8                                                 */
  *(--sp)  = (long)0x07070707L;             /* R7                                                 */
  *(--sp)  = (long)0x06060606L;             /* R6                                                 */
  *(--sp)  = (long)0x05050505L;             /* R5                                                 */
  *(--sp)  = (long)0x04040404L;             /* R4                                                 */
  NewPt->sp = sp;        // make stack "look like it was previously suspended"
  NewPt->next = RunPt;   // Pointer to first, circular linked list 
  EndCritical(status);
  return 1;
}
// ****OS_Id**********
// returns the Id for the currently running thread
// Input:  none
// Output: Thread Id (1 to NUMTHREADS)
uint32_t OS_Id(void){
  return RunPt->Id;
}

void static runperiodicevents(void){
// ****IMPLEMENT THIS****
// **DECREMENT SLEEP COUNTERS
// In Lab 4, handle periodic events in RealTimeEvents
  for (int i = 0; i < NUMTHREADS; i++) {
    if (tcbs[i].Sleep != 0)
      (tcbs[i].Sleep)--;
  }  
}

//******** OS_Launch ***************
// Start the scheduler, enable interrupts
// Inputs: number of clock cycles for each time slice
// Outputs: none (does not return)
// Errors: theTimeSlice must be less than 16,777,216
void OS_Launch(uint32_t theTimeSlice){
  STCTRL = 0;                  // disable SysTick during setup
  STCURRENT = 0;               // any write to current clears it
  SYSPRI3 =(SYSPRI3&0x0000FFFF)|0xE0E00000; // priority 7, SysTick and PendSV
  STRELOAD = theTimeSlice - 1; // reload value
  STCTRL = 0x00000007;         // enable, core clock and interrupt arm
  StartOS();                   // start on the first task
}
// runs every ms
void Scheduler(void){      // every time slice
// ****IMPLEMENT THIS****
// look at all threads in TCB list choose
// highest priority thread not blocked and not sleeping 
// If there are multiple highest priority (not blocked, not sleeping) run these round robin
  uint32_t max = UINT32_MAX;
  tcbType *tpt = RunPt;
  tcbType *bestTpt;
  
  do {
    tpt = tpt->next;
    if (tpt->Sleep == 0 && tpt->BlockPt == 0 && tpt->Priority < max) {
      bestTpt = tpt;
      max = bestTpt->Priority;
    }
  } while (tpt != RunPt); // search the whole list for highest priority task
  
  RunPt = bestTpt;
  
}

//******** OS_Suspend ***************
// Called by main thread to cooperatively suspend operation
// Inputs: none
// Outputs: none
// Will be run again depending on sleep/block status
void OS_Suspend(void){
  STCURRENT = 0;        // any write to current clears it
  INTCTRL = 0x04000000; // trigger SysTick
// next thread gets a full time slice
}
// ******** OS_Kill ************
// kill the currently running thread, release its TCB memory
// input:  none
// output: none
tcbType *previousPt;   // Pointer to previous thread in list before current thread
tcbType *nextPt;       // Pointer to next thread in list after current thread
tcbType *killPt;       // Pointer to thread being killed
// RunPt will point to thread will be killed 
void OS_Kill(void){  // no local variables allowed
  DisableInterrupts();        // atomic
  NumThread--;
  if(NumThread==0){
    for(;;){};     // crash
  }
  RunPt->Sleep = 0xFFFFFFFF;  // can't rerun this thread, it will be dead
  killPt = RunPt;             // kill current thread
	Scheduler();                // RunPt points to thread to run next
//********initially RunPt points to thread to kill********
//               /----\           /----\          /----\
//               |    |  killPt-> |    |          |    |
//               |next----------> |next---------> |next--->
//               |    |           |    |          |    |
//               \----/           \----/          \----/
  killPt->Id = 0;         // mark as free
 
  previousPt = killPt;    // eventually, it will point to previous thread 
  nextPt = killPt->next;  // nextPt points to the thread after 
  while((previousPt->next)!=killPt){
    previousPt = previousPt->next;  
  }
//****previousPt -> one before, RunPt to thread to kill *********
//               /----\           /----\          /----\
// previousPt -> |    |  killPt-> |    | nextPt-> |    |
//               |next----------> |next---------> |next--->
//               |    |           |    |          |    |
//               \----/           \----/          \----/
  previousPt->next = nextPt; // remove from list
//****remove thread which we are killing *********
//               /----\                           /----\
// previousPt -> |    |                  nextPt-> |    |
//               |next--------------------------> |next--->
//               |    |                           |    |
//               \----/                           \----/
  STCURRENT = 0;        // next thread get full slice
  EnableInterrupts();
  INTCTRL = 0x10000000; // trigger pendSV to start next thread
  for(;;){};            // can not return
}
// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime){
// ****IMPLEMENT THIS****
// set sleep parameter in TCB, same as Lab 3
// suspend, stops running
  RunPt->Sleep = sleepTime;
  OS_Suspend();
}

// ******** OS_InitSemaphore ************
// Initialize counting semaphore
// Inputs:  pointer to a semaphore
//          initial value of semaphore
// Outputs: none
void OS_InitSemaphore(int32_t *semaPt, int32_t value){
// ****IMPLEMENT THIS****
// Same as Lab 3
  *semaPt = value; 
}

// ******** OS_Wait ************
// Decrement semaphore and block if less than zero
// Lab2 spinlock (does not suspend while spinning)
// Lab3 block if less than zero
// Inputs:  pointer to a counting semaphore
// Outputs: none
void OS_Wait(int32_t *semaPt){
// ****IMPLEMENT THIS****
// Same as Lab 3
  DisableInterrupts();
  (*semaPt)--;
  if (*semaPt < 0) { // we are blocked
    RunPt->BlockPt = semaPt;
    EnableInterrupts();
    OS_Suspend();
  }
  EnableInterrupts();  
}

// ******** OS_Signal ************
// Increment semaphore
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate
// Inputs:  pointer to a counting semaphore
// Outputs: none
void OS_Signal(int32_t *semaPt){
// ****IMPLEMENT THIS****
// Same as Lab 3
  tcbType *tpt;
  DisableInterrupts();
  (*semaPt)++;
  if (*semaPt <= 0) { // someone got unblocked
    tpt = RunPt->next; // we cannot be blocked
    while (tpt->BlockPt != semaPt) {
      tpt = tpt->next;
    }  
    tpt->BlockPt = 0;
  }
  EnableInterrupts();  
}

#define FSIZE 10    // can be any size
uint32_t PutI;      // index of where to put next
uint32_t GetI;      // index of where to get next
uint32_t Fifo[FSIZE];
int32_t CurrentSize;// 0 means FIFO empty, FSIZE means full
uint32_t LostData;  // number of lost pieces of data

// ******** OS_FIFO_Init ************
// Initialize FIFO.  The "put" and "get" indices initially
// are equal, which means that the FIFO is empty.  Also
// initialize semaphores to track properties of the FIFO
// such as size and busy status for Put and Get operations,
// which is important if there are multiple data producers
// or multiple data consumers.
// Inputs:  none
// Outputs: none
void OS_FIFO_Init(void){
// ****IMPLEMENT THIS****
// Same as Lab 3
  LostData = 0;
  PutI = 0;
  GetI = 0;
  OS_InitSemaphore(&CurrentSize, 0);  
}

// ******** OS_FIFO_Put ************
// Put an entry in the FIFO.  Consider using a unique
// semaphore to wait on busy status if more than one thread
// is putting data into the FIFO and there is a chance that
// this function may interrupt itself.
// Inputs:  data to be stored
// Outputs: 0 if successful, -1 if the FIFO is full
int OS_FIFO_Put(uint32_t data){
// ****IMPLEMENT THIS****
// Same as Lab 3
  if (CurrentSize == FSIZE) { // FIFO full
    LostData++;
    return -1; // full
  }
  
  Fifo[PutI] = data; // store data at Put index
  PutI = (PutI + 1) % FSIZE; // increment and wrap Put index
  OS_Signal(&CurrentSize);
  return 0;   // success  
}

// ******** OS_FIFO_Get ************
// Get an entry from the FIFO.  Consider using a unique
// semaphore to wait on busy status if more than one thread
// is getting data from the FIFO and there is a chance that
// this function may interrupt itself.
// Inputs:  none
// Outputs: data retrieved
uint32_t OS_FIFO_Get(void){uint32_t data;
// ****IMPLEMENT THIS****
// Same as Lab 3
  OS_Wait(&CurrentSize); // block if empty
  data = Fifo[GetI]; // get data from Get index
  GetI = (GetI + 1) % FSIZE; // increment and wrap Get index
  return data;  
}
// *****periodic events****************
int32_t *PeriodicSemaphore0;
uint32_t Period0; // time between signals
int32_t *PeriodicSemaphore1;
uint32_t Period1; // time between signals
void RealTimeEvents(void){int flag=0;
  static int32_t realCount = -10; // let all the threads execute once
  // Note to students: we had to let the system run for a time so all user threads ran at least one
  // before signalling the periodic tasks
  realCount++;
  if(realCount >= 0){
    if((realCount%Period0)==0){
      OS_Signal(PeriodicSemaphore0);
      flag = 1;
    }
    if((realCount%Period1)==0){
      OS_Signal(PeriodicSemaphore1);
      flag=1;
    }
    if(flag){
      OS_Suspend();
    }
  }
}
//****NOTE: this uses PeriodicTimer, not PeriodicTimerC***********
// ******** OS_PeriodTrigger0_Init ************
// Initialize periodic timer interrupt to signal 
// Inputs:  semaphore to signal
//          period in ms
// priority level at 0 (highest)
// Outputs: none
void OS_PeriodTrigger0_Init(int32_t *semaPt, uint32_t period){
  PeriodicSemaphore0 = semaPt;
  Period0 = period;
  BSP_PeriodicTask_Init(&RealTimeEvents,1000,0);
}
// ******** OS_PeriodTrigger1_Init ************
// Initialize periodic timer interrupt to signal 
// Inputs:  semaphore to signal
//          period in ms
// priority level at 0 (highest)
// Outputs: none
void OS_PeriodTrigger1_Init(int32_t *semaPt, uint32_t period){
  PeriodicSemaphore1 = semaPt;
  Period1 = period;
  BSP_PeriodicTask_Init(&RealTimeEvents,1000,0);
}

//****edge-triggered event************
int32_t *edgeSemaphore;
// ******** OS_EdgeTrigger_Init ************
// Initialize button1, PD6, to signal on a falling edge interrupt
// Inputs:  semaphore to signal
//          priority
// Outputs: none
void OS_EdgeTrigger_Init(int32_t *semaPt, uint8_t priority){
  edgeSemaphore = semaPt;
//***IMPLEMENT THIS***
// 1) activate clock for Port D
// allow time for clock to stabilize
// 2) no need to unlock PD6
// 3) disable analog on PD6
// 4) configure PD6 as GPIO
// 5) make PD6 input
// 6) disable alt funct on PD6
// disable pull-up on PD6
// 7) enable digital I/O on PD6  
// (d) PD6 is edge-sensitive 
//     PD6 is not both edges 
//     PD6 is falling edge event 
// (e) clear PD6 flag
// (f) arm interrupt on PD6
// priority on Port D edge trigger is NVIC_PRI0_R	31 � 29
// enable is bit 3 in NVIC_EN0_R
  SYSCTL_RCGCGPIO_R |= 0x08;  // 1) activate clock for Port D
// allow time for clock to stabilize
//GPIO_PORTF_LOCK_R = 0x4C4F434B; // 2) no need to unlock PD6
  GPIO_PORTD_CR_R |= 0x40;         // allow changes to PD6
  GPIO_PORTD_AMSEL_R &= ~0x40;     // 3) disable analog on PD6
  GPIO_PORTD_DIR_R &= ~0x40;      // 5) make PD6 input
  GPIO_PORTD_AFSEL_R &= ~0x40;    // 6) disable alt funct on PD6 (PD <- PD&(10111111)) 4) configure PD6 as GPIO
  GPIO_PORTD_PUR_R &= ~0x40;      // disable pull-up on PD6
  GPIO_PORTD_DEN_R |= 0x40;       // 7) enable digital I/O on PD6 
  GPIO_PORTD_IS_R &= ~0x40;       // (d) PD6 is edge-sensitive 
  GPIO_PORTD_IBE_R &= ~0x40;      //     PD6 is not both edges
  GPIO_PORTD_IEV_R &= ~0x40;      //     PD6 is falling edge event    
  GPIO_PORTD_ICR_R = 0x40;        // (e) clear PD6 flag
  GPIO_PORTD_IM_R |= 0x40;        // arm interrupt on PD6
  
  // extra: disable open drain mode
  GPIO_PORTD_ODR_R &= ~0x40;


// priority on Port D edge trigger is NVIC_PRI0_R	31   29
// enable is bit 3 in NVIC_EN0_R
  NVIC_PRI0_R = (NVIC_PRI0_R&~0xE0000000)|(priority << 29); 
  NVIC_EN0_R = 0x00000008; 
}

// ******** OS_EdgeTrigger_Restart ************
// restart button1 to signal on a falling edge interrupt
// rearm interrupt
// Inputs:  none
// Outputs: none
void OS_EdgeTrigger_Restart(void){
//***IMPLEMENT THIS***
// rearm interrupt 3 in NVIC
// clear flag6
  GPIO_PORTD_IM_R |= 0x40;
  GPIO_PORTD_ICR_R = 0x40;  // acknowledge flag6
}
void GPIOPortD_Handler(void){
//***IMPLEMENT THIS***
	// step 1 acknowledge by clearing flag
  // step 2 signal semaphore (no need to run scheduler)
  // step 3 disarm interrupt to prevent bouncing to create multiple signals
  if(GPIO_PORTD_RIS_R & 0x40){  // poll PD6
    GPIO_PORTD_ICR_R = 0x40;  // acknowledge flag6
    OS_Signal(edgeSemaphore); // signal edge occurred
    GPIO_PORTD_IM_R &= ~0x40; // disarm interrupt on PD6
  }
}


