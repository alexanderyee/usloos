/*------------------------------------------------------------------------
   phase1.c
   Katie Pan & Alex Yee
   University of Arizona
   Computer Science 452
   Fall 2017
   Dr. Homer
   ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();
void clockHandler(int, int);
void clearProcess(int);
void enableInterrupts();
void disableInterrupts();
int isEmpty();
procPtr pop();
procPtr popPriority(int);
procPtr peek();
int insert(procPtr);
int zap(int);
int isZapped(void);
void zapAdd(procPtr);
/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static procPtr ReadyList;
static procPtr pQueues[6][MAXPROC];
// current process ID
procPtr Current;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;


/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - argc and argv passed in by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup(int argc, char *argv[])
{
    int result; /* value returned by call to fork1() */
	int i;
	for (i = 0; i < 50; i++) {
		ProcTable[i].isNull = 1;
	}    
    /* initialize the process table */
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");

    /* disable interrupts and check for kernel mode */
	disableInterrupts(); 
    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");
    
    // Initialize the clock interrupt handler
    USLOSS_IntVec[USLOSS_CLOCK_INT] = (void (*) (int, void *)) clockHandler;
	// startup a sentinel process
	if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                    SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }
  
    // start the test process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }
    
    dispatcher(); // need to delete other dispatcher. (dispatcher should be here? from lect on thurs) should also be at end of quit. should still be at fork1 tho
    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");

    return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish(int argc, char *argv[])
{
    if (DEBUG && debugflag)
        USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stacksize, int priority)
{
    // test if in kernel mode; halt if in user mode
    /* disable interrupts and check for kernel mode */
    disableInterrupts(); 
    // Return if stack size is too small
    if (stacksize < USLOSS_MIN_STACK) {
        printf("yo i don't think the stack size is enuf dood");
    }
  
    // Is there room in the process table? What is the next PID?
    int procSlot = nextPid % MAXPROC;
	nextPid++;
	int count = 0;
	while (!ProcTable[procSlot].isNull) {
		procSlot = nextPid % MAXPROC; 
		nextPid++;
		if (count++ == MAXPROC) {
			USLOSS_Console("Too many processes! Halting... >:D");
			USLOSS_Halt(1);
		}
	}

    if (!procSlot) // change this later to account for more than 50 procs.
    {
        USLOSS_Console("No more room in the process table. Halting...\n");
        USLOSS_Halt(1);
    } 

    ProcTable[procSlot].stack = malloc(stacksize);
   
    // Return if stack size is too small (again :^) )
    if(ProcTable[procSlot].stack == NULL){
    	printf("yo dude. u got a null for your ProcTable[procSlot].stack. that malloc aint kool");
    }
	ProcTable[procSlot].isNull = 0;
    ProcTable[procSlot].stackSize = stacksize;
    ProcTable[procSlot].priority = priority;
    ProcTable[procSlot].pid = procSlot;

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);

    // fill-in entry in process table */
    if ( strlen(name) >= (MAXNAME - 1) ) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }

    strcpy(ProcTable[procSlot].name, name);
    ProcTable[procSlot].startFunc = startFunc;

    if ( arg == NULL )
        ProcTable[procSlot].startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[procSlot].startArg, arg);
    
    insert(&ProcTable[procSlot]); // inserts into the readylist
    //check is parent pointer isn't null and the priority of parent isn't 6 (sential process)
    if (Current != NULL && Current->priority != 6) {
        procPtr temp = Current;
        while (temp->nextProcPtr != NULL) // iterate through children if there are any, place child ptr there
            temp = temp->nextProcPtr;
        temp->childProcPtr = &ProcTable[procSlot];
        temp->childProcPtr->parentProcPtr = Current;
        USLOSS_Console("link between parent %d and child %d estab.\n", Current->pid, ProcTable[procSlot].pid);
    }
    // PSR. Saved previousPSRValue; set new PSR value withh current mode =1 && current 
    // interrupt mode = 1; 
    enableInterrupts();
	// Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)
    
    USLOSS_ContextInit(&(ProcTable[procSlot].state),
                       ProcTable[procSlot].stack,
                       ProcTable[procSlot].stackSize,
                       NULL,
                       launch);
    // for future phase(s)
    p1_fork(ProcTable[procSlot].pid);
    // More stuff to do here...
    if (ProcTable[procSlot].priority != 6) 
		dispatcher();
	else
		Current = &ProcTable[procSlot];	
    printf("%d\n",  ProcTable[procSlot].pid);
    
    return ProcTable[procSlot].pid;  // -1 is not correct! Here to prevent warning.
} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
    int result;

    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");

    // Enable interrupts
	enableInterrupts();

	printf("launch(): calling startFunc of pid %d\n", Current->pid); 
    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
        USLOSS_Console("Process %d returned to launch\n", Current->pid);
    quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
             -1 if the process was zapped in the join
             -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{
	// check if in kernel mode and disable interrupts
	disableInterrupts();
    if (Current->childProcPtr == NULL) {
        return -2;  
    } 
    else if (Current->isZapped) {
        return -1;
    }
	else {
		procPtr temp = Current->childProcPtr;
		while (temp != NULL) {
			if (temp->status == 2) {// a child has quit case
				*status = temp->terminationCode;
				int deadChildPid = temp->pid;
				// reset all the fields
				clearProcess(deadChildPid);
				return deadChildPid;
			}
			temp = temp->nextProcPtr;
		}
		// no child has quit, change status to blockedOnJoin,  run dispatcher
		Current->status = 3;
		dispatcher();
		*status = Current->childProcPtr->terminationCode;
	}
    return Current->pid;  // -1 is not correct! Here to prevent warning.
} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int status)
{

// from lecture on thurs. do i have a parent? is it blocked on join? if yes, then change parent status to unblock, put em bakc on rdylist, dispatcher should run parent eventually, then free proctable for child's slot (this current process slot)
	disableInterrupts();
	// check if parent is stuck on join
    if (Current->parentProcPtr != NULL) { 
		//the code to return to the sad, grieving parental guardian :^(
		Current->terminationCode = status;
		Current->status = 2; //quit code
		// @TODO adjust child and parent pointers 
		if (Current->parentProcPtr->status == 3) {
			Current->parentProcPtr->status = 0;
			insert(Current->parentProcPtr); 
		} else if (Current->isZapped) {
			// to be implemented in the future @TODO
		}
	}
	int cPid = Current->pid; 
	if (Current->parentProcPtr == NULL) {
		popPriority(Current->priority);
		clearProcess(cPid);
		printf("quit(): Process cleared\n");
	}
    p1_quit(cPid);
	dispatcher();
} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
    disableInterrupts();
    procPtr tempCurrent = Current;
	printf("dispatcher() tempCurrent isNull: %d\n", tempCurrent->isNull);
	enableInterrupts();
	// if current process' status has changed to blocked/quit, remove it from RL
	if (!tempCurrent->isNull) {
		if (tempCurrent->status <= 4 && tempCurrent->status >= 1) 
		{ // remove the current proc from the readylist
			popPriority(tempCurrent->priority);
		}
		ReadyList = peek();
        Current = ReadyList;
		printf("dispatcher switch: %d\n", tempCurrent->pid);
		printf("dispatcher switch2: %d\n", Current->pid);
		USLOSS_ContextSwitch(&(tempCurrent->state), &(Current->state));
		
	} else {
		ReadyList = peek();
        Current = ReadyList;
		printf("dispatcher(): current priority is %d\n", Current->priority);
		printf("dispatcher(): current is %d\n", Current->pid);
		if (Current->priority == 6) 
			USLOSS_ContextSwitch(NULL, &(Current->state));
		else 
			USLOSS_ContextSwitch(&(tempCurrent->state), &(Current->state));
	}
	// need to check if tempCurrent has a higher priority? should higher priority run over the peekd process priority
	p1_switch(tempCurrent->pid, Current->pid);
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */

int sentinel (char *dummy)
{
    if (DEBUG && debugflag)
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{ 
	USLOSS_Console("checkDeadlock(): hullo");
	int i;
	int isNoMoreProcs = 1;
	for (i = 0; i <= 50; i++) {
		if (!ProcTable[i].isNull) {
			isNoMoreProcs = 0;
			// check if blocked (probs in later phases)
		} else {
		}
	}
	if (isNoMoreProcs) {
		USLOSS_Console("All processes completed");
		USLOSS_Halt(0);
	}
} /* checkDeadlock */

/*
 * Enables the interrupts.
 */
void enableInterrupts()
{
    // turn the interrupts ON iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS
    unsigned int previousPSRValue = USLOSS_PsrGet();
    
	if (!(previousPSRValue & 1)) {
        USLOSS_Console("enableInterrupts(): Not in kernel mode :-(\n");
        USLOSS_Halt(1);
    }   
    
	unsigned int newPSRValue = ((previousPSRValue << 2) & 12) | 3;
	// left shifting by 2 to shift the previous psr bits to the left two bits
	// then logical and by 12 to mask all the other bits except the previous mode bits
	// then a logical or by 3 to set the first two bits to 11 
	if (USLOSS_PsrSet(newPSRValue) == USLOSS_ERR_INVALID_PSR)
		USLOSS_Console("ERROR: Invalid PSR value set! was: %u\n", newPSRValue);
} /* enableInterrupts */

/*
 * Disables the interrupts and also turns kernel mode on
 */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS
	unsigned int previousPSRValue = USLOSS_PsrGet();
	
    if (!(previousPSRValue & 1)) {
		USLOSS_Console("disableInterrupts(): Not in kernel mode :-(\n");
		USLOSS_Halt(1);
	}
	
    unsigned int newPSRValue = ((previousPSRValue << 2) & 12) | 1;
	if (USLOSS_PsrSet(newPSRValue) == USLOSS_ERR_INVALID_PSR)
		USLOSS_Console("ERROR: Invalid PSR value set! was: %u\n", newPSRValue);
} /* disableInterrupts */

/*
 *
 */ 
int zap(int pid) 
{
	if (Current->isZapped) {
		return -1;
	} else if (Current->pid == pid || ProcTable[pid % MAXPROC].isNull) { 
		USLOSS_Console("zap error d(^-^)b");
		USLOSS_Halt(1);
		return -1;
	} else {
		ProcTable[pid].isZapped = 1;
		zapAdd(&ProcTable[pid]);	
		Current->status = ZAP_BLOCKED;
		dispatcher(); 
	}
	return 0;
}

/*
 *
 */
int isZapped(void)
{
	return Current->isZapped;
}

/*
 * The clock interrupt handler for our OS baby
 */
void clockHandler(int dev, int unit)
{
}

/*
 * Clears the fields for the slot of the process at ProcTable[pid]
 * This should execute after the parent joined with its dead child,
 * or when a process with no parent quits. 
 * @param int pid -- the process id of the process to clear or reset
 * 
 * @return void
 */
void clearProcess(int pid)
{
	Current->nextProcPtr = NULL;
    Current->childProcPtr = NULL;
    Current->nextSiblingPtr = NULL;
    Current->parentProcPtr = NULL;
    int i;
    for (i=0; i<MAXNAME; i++) {
        Current->name[i] = '\0';
    }
    for (i=0; i<MAXARG; i++) {
         Current->startArg[i] = '\0';
    }
    // what do with state when quit huh???????????
	//Current->state = NULL; // check valgrind l8r
    Current->priority = 0;
    Current->startFunc = NULL;
    Current->stack = NULL;
    Current->stackSize = 0;
    Current->status = 0;
    Current->isZapped = 0;
	Current->terminationCode = 0;
	Current->isNull = 1;
	Current->zapHead = NULL;
	
	return;
}

/*
 * 
 */
procPtr pop()
{
   int i, j;
   procPtr result;
   for (i=0; i < 5; i++) {
      if (pQueues[i][0] != NULL){
         result = pQueues[i][0];
         break;
      }
   }
   // begin shift
   j=0;
   while (j < (MAXPROC-1) && pQueues[i][j] != NULL) {
      pQueues[i][j] = pQueues[i][j+1];
      j++; 
   }
   printf("popPriority(): popped %d\n", result->pid);
   return result;
}

/*
 * 
 */
procPtr popPriority(int priority)
{
   int j;
   priority--;
   procPtr result;
   if (pQueues[priority][0] != NULL){
       result = pQueues[priority][0];
   }
   // begin shift
   j=0;
   while (j < (MAXPROC-1) && pQueues[priority][j] != NULL) {
      pQueues[priority][j] = pQueues[priority][j+1];
      j++;
   }
   printf("popPriority(): popped %d\n", result->pid);
   return result;
}


int insert(procPtr p){
   int priority = (*p).priority;
   int i = 0;
   while(i < MAXPROC && pQueues[priority-1][i] != NULL){
      i++;
   }
   if(i == 50){
      USLOSS_Console("pop(): no more roomsies ;_;\n");
      USLOSS_Halt(1);
      return 1;
   }
   pQueues[priority-1][i] = p;
   return 0;
}

/*
 *
 */
procPtr peek()
{
    int i;
    procPtr result;
    for (i=0; i < 6; i++) {
       if (pQueues[i][0] != NULL){
          result = pQueues[i][0];
          break;
       }
    }
    return result;
}

/*
 *
 */
int isEmpty()
{
   int flag = 1;
   int i;
   for(i = 0; i < 5; i++){
   	if(pQueues[i][0] != NULL){
 		flag = 0;
		break;
        } 
   }

   return flag;
}

/*
 * pronounced: zuh-PAD
 */
void zapAdd(procPtr zapped)
{
	zapNode tempNode;
	tempNode.zapper = Current;
	tempNode.next = zapped->zapHead;
	zapped->zapHead = &tempNode;
}
