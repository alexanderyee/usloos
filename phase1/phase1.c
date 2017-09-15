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
void illegalHandler(int, int);
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
void unZap(void);
void checkIfChildrenQuit(void);
void checkForKernelMode(char *, int);
void dumpReadyList(void);
/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 0;

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
	for (i = 0; i < MAXPROC; i++) {
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
	USLOSS_IntVec[USLOSS_ILLEGAL_INT] = (void (*) (int, void *)) illegalHandler;
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
    
//`    dispatcher(); // need to delete other dispatcher. (dispatcher should be here? from lect on thurs) should also be at end of quit. should still be at fork1 tho
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
	// test if in kernel mode; halt if in user mode
   	if(Current == NULL){
		checkForKernelMode("fork1", nextPid-1);
	} else{
		checkForKernelMode("fork1", Current->pid);
	}
    /* disable interrupts and check for kernel mode */
    disableInterrupts();
    // Return if stack size is too small
    if (stacksize < USLOSS_MIN_STACK) {
        USLOSS_Console("yo i don't think the stack size is enuf dood");
    }
    ProcTable[procSlot].stack = malloc(stacksize);
	
 
    // Return if stack size is too small (again :^) )
    if(ProcTable[procSlot].stack == NULL){
    	USLOSS_Console("yo dude. u got a null for your ProcTable[procSlot].stack. that malloc aint kool");
    	USLOSS_Halt(1);
	}
	ProcTable[procSlot].isNull = 0;
    ProcTable[procSlot].stackSize = stacksize;
    ProcTable[procSlot].priority = priority;
    ProcTable[procSlot].pid = nextPid-1;

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
        procPtr temp = Current->childProcPtr;
		// case where parent has no child
		if (temp == NULL) {
			Current->childProcPtr = &ProcTable[procSlot];
			Current->childProcPtr->parentProcPtr = Current;
		} else {
       		while (temp->nextProcPtr != NULL) // iterate through children if there are any, place child ptr there
            	temp = temp->nextProcPtr;
        	temp->nextProcPtr = &ProcTable[procSlot];
        	temp->nextProcPtr->parentProcPtr = Current;
        //USLOSS_Console("link between parent %d and child %d estab.\n", Current->pid, ProcTable[procSlot].pid);
    	}
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
			if (temp->status == QUIT) {// a child has quit case
				*status = temp->terminationCode;
				int deadChildPid = temp->pid;
				Current->childProcPtr = Current->childProcPtr->nextProcPtr;
				// reset all the fields
				clearProcess(deadChildPid);
				return deadChildPid;
			}
			temp = temp->nextProcPtr;
		}
		// no child has quit, change status to blockedOnJoin,  run dispatcher
		Current->status = JOIN_BLOCKED;
		popPriority(Current->priority);
		readtime();
		dispatcher();
		*status = Current->childProcPtr->terminationCode;
	}
	int deadChildPid = Current->childProcPtr->pid;
	Current->childProcPtr = Current->childProcPtr->nextProcPtr;
	clearProcess(deadChildPid);
	if (isZapped())
		return -1;
	return deadChildPid;  // -1 is not correct! Here to prevent warning.
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
	if(Current == NULL){
        checkForKernelMode("quit", nextPid-1);
    } else {
        checkForKernelMode("quit", Current->pid);
    }
// from lecture on thurs. do i have a parent? is it blocked on join? if yes, then change parent status to unblock, put em bakc on rdylist, dispatcher should run parent eventually, then free proctable for child's slot (this current process slot)
	disableInterrupts();
	// check if all children quit
    checkIfChildrenQuit();
	// check if it was zapped, if so unblock and enqueue those who zapped it
	if (Current->isZapped)
		unZap();
	// now check if any parents are stuck on a join
	if (Current->parentProcPtr != NULL) { 
		//the code to return to the sad, grieving parental guardian :^(
		Current->terminationCode = status;
		Current->status = QUIT; //quit code
		popPriority(Current->priority);
		readtime();
		// @TODO adjust child and parent pointers 
		if (Current->parentProcPtr->status == JOIN_BLOCKED) {
			Current->parentProcPtr->status = READY;
			insert(Current->parentProcPtr); 
		} 
	}
	int cPid = Current->pid; 
	if (Current->parentProcPtr == NULL) {
		popPriority(Current->priority);
		clearProcess(cPid);
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
	enableInterrupts();
	// @TODO implement time-related cases, check if higher priority exists, etc.
	if (tempCurrent != NULL) {
	/*	if (tempCurrent->status <= 4 && tempCurrent->status >= 1) 
		{ // remove the current proc from the readylist
			popPriority(tempCurrent->priority);
		}
	*/
		//dumpReadyList();
		ReadyList = peek();
        Current = ReadyList;
		if (tempCurrent->pid != Current->pid) {
			Current->status = RUNNING;
			if (Current->priority == 6){ // sentinel case 
				p1_switch(tempCurrent->pid, Current->pid);
            	readtime();
            	launch();
            	USLOSS_ContextSwitch(&(tempCurrent->state), &(ProcTable[Current->pid % MAXPROC].state));
       		} else {
				p1_switch(tempCurrent->pid, Current->pid);
				readtime();
				USLOSS_ContextSwitch(&(tempCurrent->state), &(Current->state));
			}
		}
	} else {
		ReadyList = peek();
        Current = ReadyList;
		if (Current->priority == 6){ // sentinel case 
			p1_switch(-1, Current->pid);
			readtime();
			launch();
			USLOSS_ContextSwitch(NULL, &(ProcTable[Current->pid % MAXPROC].state));
		}
		else {
			Current->status = RUNNING;
			p1_switch(-1, Current->pid);
			readtime();
			USLOSS_ContextSwitch(NULL, &(Current->state));	
		}
	}
	// need to check if tempCurrent has a higher priority? should higher priority run over the peekd process priority
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
	//USLOSS_Console("checkDeadlock(): hullo");
	int i;
	int numProc = 0;
	int isNoMoreProcs = 1;
	int isAllBlocked = 1;
	for (i = 0; i < MAXPROC; i++) {
		if (!ProcTable[i].isNull && ProcTable[i].priority != 6) {
			isNoMoreProcs = 0;
			if (DEBUG && debugflag)
				USLOSS_Console("checkDeadlock(): hullo %d \n", i);
			// check if there are any processes that aren't quit or blocked
			if (!((ProcTable[i].status >= 1 && ProcTable[i].status <= 4) || !(ProcTable[i].status >= 10))) {
				isAllBlocked = 0;
			} else if (ProcTable[i].status != QUIT) // else if it hasn't quit, increment the counter
				numProc++;
		// check if blocked (probs in later phases)
		} else {
		}
	}
	if (isNoMoreProcs == 1) {
		USLOSS_Console("All processes completed.\n");
		USLOSS_Halt(0);
	} else if (isAllBlocked) {
		USLOSS_Console("checkDeadlock(): numProc = %d. Only Sentinel should be left. Halting...\n", numProc + 1);
        USLOSS_Halt(1);
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
	if (isZapped()) {
		return -1;
	} else if (Current->pid == pid || ProcTable[pid % MAXPROC].isNull) { 
		USLOSS_Console("zap error d(^-^)b");
		USLOSS_Halt(1);
		return -1;
	} else {
		ProcTable[pid % MAXPROC].isZapped = 1;
		zapAdd(&ProcTable[pid % MAXPROC]);	
		Current->status = ZAP_BLOCKED;
		popPriority(Current->priority);
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
 * something something illegal handler function that doesn't do anything
 */ 
void illegalHandler(int dev, int unit)
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
	pid = pid % MAXPROC; // where it should be ProcTable-slot wise
	ProcTable[pid].nextProcPtr = NULL;
    ProcTable[pid].childProcPtr = NULL;
    int i;
    for (i=0; i<MAXNAME; i++) {
        ProcTable[pid].name[i] = '\0';
    }
    for (i=0; i<MAXARG; i++) {
         ProcTable[pid].startArg[i] = '\0';
    }
    // what do with state when quit huh???????????
	//Current->state = NULL; // check valgrind l8r
    ProcTable[pid].priority = 0;
    ProcTable[pid].startFunc = NULL;
    ProcTable[pid].stack = NULL;
    ProcTable[pid].stackSize = 0;
    ProcTable[pid].status = 0;
    ProcTable[pid].isZapped = 0;
	ProcTable[pid].terminationCode = 0;
	ProcTable[pid].isNull = 1;
	ProcTable[pid].zapHead = NULL;
	ProcTable[pid].timeMaster5000Start = 0;
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
   return result;
}


int insert(procPtr p){
    int priority = (*p).priority;
    int i = 0;
    while (i < MAXPROC && pQueues[priority-1][i] != NULL) {
    	i++;
		if (i == 50) {
      		USLOSS_Console("insert(): no more roomsies ;_;\n");
     		USLOSS_Halt(1);
      		return 1;
		}
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

/*
 *
 */
void unZap(void)
{
	Current->isZapped = 0;
	zapNode * zapNodePtr = Current->zapHead;
	while (zapNodePtr->zapper != NULL) {
		zapNodePtr->zapper->status = READY;
		insert(zapNodePtr->zapper);
		zapNodePtr = zapNodePtr->next;
	}

}

/*
 * dumps all the procs everywhere (nasty)
 */
void dumpProcesses(void)
{
	USLOSS_Console("~                Process Dump (nasty)                ~\n");
	USLOSS_Console("%10s%10s%10s%10s%15s%10s%10s\n", "Name", "PID", "parentPID", "Priority", "Status", "#Children", "CPU Time");
	int i, numProcs = 0;
	for (i = 0; i < MAXPROC; i++) {
		if (!ProcTable[i].isNull) {
			numProcs++;
			procStruct p = ProcTable[i];
			procPtr pChild = p.childProcPtr;
			int pChildCount = 0;
			while(pChild != NULL) {
				pChildCount++;
				pChild = pChild->nextProcPtr;
			}
			char * stateus;
			if (p.status == READY)
				stateus = "READY";
			else if (p.status == BLOCKED)
                stateus = "BLOCKED";
			else if (p.status == QUIT)
                 stateus = "QUIT";
			else if (p.status == JOIN_BLOCKED)
                 stateus = "JOIN_BLOCKED";
			else if (p.status == ZAP_BLOCKED)
                 stateus = "ZAP_BLOCKED";
			else if (p.status == RUNNING)
				stateus = "RUNNING";
			else stateus = "BLOCKED AF";
			USLOSS_Console("%10s%10d%10d%10d%15s%10d%10d\n", p.name, p.pid, ((p.parentProcPtr == NULL) ? -2 : p.parentProcPtr->pid), p.priority, stateus, pChildCount, -2); 
		} else { // null proctable case
			 USLOSS_Console("%10s%10d%10d%10d%15s%10d%10d\n", "", -1, -1, -1, "EMPTY", 0, -1);
		}
	} 	
}

/*
 *
 */
void dumpReadyList(void)
{
	USLOSS_Console("<|o.o|>        RDYLIS        <|o.o|>\n");
	USLOSS_Console("%10s%10s%15s\n", "Priority", "PID", "Name");
	int i, j;
	for (i = 0; i < 6; i++) {
		for (j = 0; j < MAXPROC; j++) {	
			if (pQueues[i][j] != NULL) {
				procPtr p = pQueues[i][j];
				USLOSS_Console("%10d%10d%15s\n", p->priority, p->pid, p->name);  
			}
		}
	}	
}

/*
 *
 */
int getpid(void) 
{
	return Current->pid;
}

/*
 *
 */
int blockMe (int newStatus) 
{
	if (newStatus <= 10) {
		USLOSS_Console("blockMe(): newStatus param must be greater than 10");
		USLOSS_Halt(1);
	} else if (Current->isZapped) {
		return -1;
	}
	Current->status = newStatus;
	popPriority(Current->priority);
	return 0;
}

/*
 *
 */
int unblockProc(int pid)
{
	procStruct proc = ProcTable[pid % MAXPROC];
	if (proc.status <= 10 || proc.isNull || Current->pid == pid)
		return -2;
	else if (proc.isZapped)
		return -1;
	
	ProcTable[pid % MAXPROC].status = READY;
	insert(&(ProcTable[pid % MAXPROC]));
	dispatcher();
	return 0;
}
/*
 *
 */
int readtime(void)
{
	if (Current->timeMaster5000Start == 0) { // @TODO fix this? should we start time slice clock start here?
		Current->timeMaster5000Start = readCurStartTime()/1000;
	} else if (Current->status == RUNNING) { // still running case
		(Current->timeMaster5000) += ((readCurStartTime()/1000) - Current->timeMaster5000Start);
		Current->timeMaster5000Start = readCurStartTime()/1000;
		timeSlice();
	} else { // became blocked/quit, just pause the timer
		(Current->timeMaster5000) += ((readCurStartTime()/1000) - Current->timeMaster5000Start);
		Current->timeMaster5000Start = 0;
	}
	return Current->timeMaster5000; 
}
/*
 *
 */
void timeSlice(void) 
{
	if (Current->timeMaster5000 >= TIME_SLICE) {
		Current->timeMaster5000 = 0;
		Current->timeMaster5000Start = 0;
		popPriority(Current->priority);
		insert(Current);
		dispatcher(); 
	}
}

/*
 * returns the time (in microseconds, 1000us = 1ms
 */
int readCurStartTime(void)
{
	int status;
	int errCode = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &status);
	if (errCode == USLOSS_DEV_INVALID) {
		USLOSS_Console("readCurStartTime(): Device and unit invalid\n");
		USLOSS_Halt(1);
	}
	return status;
}

/*
 *
 */
void checkIfChildrenQuit(void)
{
	procPtr childPtr = Current->childProcPtr;
	while (childPtr != NULL) {
		if (childPtr->status != QUIT) {
			USLOSS_Console("quit(): process %d, '%s', has active children. Halting...\n", Current->pid, Current->name);
			USLOSS_Halt(1);
		}
		childPtr = childPtr->nextProcPtr;
	}

}

void  checkForKernelMode(char * funcName, int pid)
{
	if (!(USLOSS_PsrGet() & 1)) {
		USLOSS_Console(
             "%s(): called while in user mode, by process %d. Halting...\n", funcName, pid);
         USLOSS_Halt(1);
    }
}
