/* ------------------------------------------------------------------------
   phase3.c

   University of Arizona
   Computer Science 452

   Katie Pan & Alex Yee
   ------------------------------------------------------------------------ */
#include <stdio.h>
#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <libuser.h>

/* Prototypes */
void check_kernel_mode(char *);
void nullsys3(systemArgs *);
extern int start3 (char *);
int spawn(systemArgs *);
int wait(systemArgs *);
int spawnReal(char *, int (*)(char *), char *, long , long);
int waitReal(int *);
void setUserMode();
void spawnLaunch();
void terminate(systemArgs *);
void terminateReal(int);
void semCreate(systemArgs *);
void semP(systemArgs *);
void semV(systemArgs *);
void getPID(systemArgs *);
void semFree(systemArgs *);
void getTimeofDay(systemArgs *);
void cpuTime(systemArgs *);

/* Data structures */
void (*systemCallVec[MAXSYSCALLS])(systemArgs *);
procStruct ProcTable[MAXPROC];
semaphore SemsTable[MAXSEMS];

/* start2: entry point for phase3 which spawns start3 */
int start2(char *arg)
{
    int pid, i, status;
    /*
     * Check kernel mode here.
     */
     check_kernel_mode("start2");
    /*
     * Data structure initialization as needed...
     */
     // initalize the syscall handlers
 	for (i = 0; i < MAXSYSCALLS; i++) {
 		systemCallVec[i] = (void (*) (systemArgs *)) nullsys3;
 	}

    systemCallVec[SYS_SPAWN] = (void (*) (systemArgs *)) spawn;
    systemCallVec[SYS_WAIT] = (void (*) (systemArgs *)) wait;
    systemCallVec[SYS_TERMINATE] = (void (*) (systemArgs *)) terminate;
    systemCallVec[SYS_SEMCREATE] = (void (*) (systemArgs *)) semCreate;
    systemCallVec[SYS_SEMP] = (void (*) (systemArgs *)) semP;
    systemCallVec[SYS_SEMV] = (void (*) (systemArgs *)) semV;
    systemCallVec[SYS_SEMFREE] = (void (*) (systemArgs *)) semFree;
    systemCallVec[SYS_GETTIMEOFDAY] = (void (*) (systemArgs *)) getTimeofDay;
    systemCallVec[SYS_CPUTIME] = (void (*) (systemArgs *)) cpuTime;
    systemCallVec[SYS_GETPID] = (void (*) (systemArgs *)) getPID;

    for (i = 0; i < MAXPROC; i++) {
        ProcTable[i].mboxID = -1;
        ProcTable[i].childPid = -1;
		ProcTable[i].nextPid = -1;

    }

    for(i = 0; i < MAXSEMS; i++){
        SemsTable[i].semId = -1;
        SemsTable[i].mboxID = -1;
    }

    pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);
    pid = waitReal(&status);
    return pid;
} /* start2 */

/*
    spawnReal -- forks a process using given parameters. coordinates with
                    spawnLaunch to initialize the process table fields using
                    a created mutex mbox or spawnLaunch's created mutex mbox
 */
int spawnReal(char *name, int (*func)(char *), char *arg, long stack_size, long priority)
{
    procStruct *currentProc = &ProcTable[getpid() % MAXPROC];
    int pid = fork1(name, (int (*)(char *))spawnLaunch, NULL, stack_size, priority);
    if (pid < 0)
        return pid;
    ProcTable[pid % MAXPROC].pid = pid;
	if (ProcTable[pid % MAXPROC].mboxID == -1){
         ProcTable[pid % MAXPROC].mboxID = MboxCreate(0, 50);
	}
    ProcTable[pid % MAXPROC].startFunc = func;
    ProcTable[pid % MAXPROC].startArg = arg;
    ProcTable[pid % MAXPROC].parentPid = getpid();
    ProcTable[pid % MAXPROC].priority = priority;

    // insert child into our child list
    if (currentProc->childPid == -1) {
        currentProc->childPid = pid;
    } else {
        procStruct *childPtr = &ProcTable[currentProc->childPid % MAXPROC];
        while (childPtr->nextPid != -1) {
            childPtr = &ProcTable[childPtr->nextPid % MAXPROC];
        }
        ProcTable[childPtr->pid % MAXPROC].nextPid = pid;
    }

    // if our child has a higher priority, then let's block ourselves on their mbox
    if (ProcTable[pid % MAXPROC].pid != 0 && priority < currentProc->priority) {
        MboxSend(ProcTable[pid % MAXPROC].mboxID, NULL, 0);
    }
    if (isZapped()) {
        Terminate(69);
    }
    return pid;
} /* spawnReal */

/*
 * Spawn
 * Create a user-level process. Use fork1 to create the process, then change
 * it over to user-mode. If thespawned function returns it should have the same
 * effect as calling terminate.
 * Input: arg1= address of the function to spawn,
          arg2: parameter passed to spawned function.
          arg3: stack size (in bytes).
          arg4: priority.
          arg5: character string containing process’s name.
   Output: arg1: the PID of the newly created process; -1 if a process could not be created.
           arg4: -1 if illegal values are given as input; 0 otherwise.
 */
int spawn(systemArgs *args)
{
    int ans = spawnReal(args->arg5, args->arg1, args->arg2, (long) args->arg3, (long) args->arg4);
    args->arg1 = (void *) (long) ans;
    return ans;
}

/* ------------------------------------------------------------------------
   Name - spawnLaunch
   Purpose - calls the func of the process, coordinates with spawnReal to
                initialize process table fields.
   Parameters - none
   Returns - nothing
   ------------------------------------------------------------------------ */
void spawnLaunch()
{
    procStruct *currentProc = &ProcTable[getpid() % MAXPROC];

    // if we are running before our parent is done initalizing, block
	if (currentProc->mboxID == -1) {
		currentProc->mboxID = MboxCreate(0, 50);
        MboxReceive(currentProc->mboxID, NULL, MAX_MESSAGE);
	}

    int result;
    if (isZapped()) {
        terminateReal(69);
    }
	setUserMode();
    // Call the function passed to fork1, and capture its return value
    result = currentProc->startFunc(currentProc->startArg);
	Terminate(result);
} /* spawnLaunch */

/*
    waitReal -- calls join to retrieve the quitting child's pid and termination
                status. Once a child quits, then the parent's child list is
                modified to have the child deleted from the list.
*/
int waitReal(int *status)
{
	int pid = join(status);
    if(pid < 0)
    {
        USLOSS_Console("waitReal(): Invalid join %d. Halting...\n", pid);
        USLOSS_Halt(1);
    }
    // search through children to delete
    if (ProcTable[getpid() % MAXPROC].childPid == pid) {
        ProcTable[getpid() % MAXPROC].childPid = ProcTable[ProcTable[getpid() % MAXPROC].childPid % MAXPROC].nextPid;
    } else { // child is at middle or end of list
        procStruct *childPtr2 = &ProcTable[ProcTable[getpid() % MAXPROC].childPid % MAXPROC];
        // we know the child is terminated through mboxID being -1
        while (ProcTable[childPtr2->nextPid % MAXPROC].mboxID != -1) {
            childPtr2 = &ProcTable[childPtr2->nextPid % MAXPROC];
        }
        childPtr2->nextPid = ProcTable[pid % MAXPROC].nextPid;
    }
    ProcTable[pid % MAXPROC].pid = 0;
    ProcTable[pid % MAXPROC].nextPid = -1;
	return pid;
} /* waitReal */

/* wait
    Wait for a child process to terminate.
    Output
        arg1: process id of the terminating child.
        arg2: the termination code of the child.
*/
int wait(systemArgs *args)
{
    int pid = waitReal(args->arg2);
    args->arg1 = (void *)(long) pid;
    return pid;
}

/*
    Terminates the invoking process and all of its children, and synchronizes
    with its parent’s Wait system call. Processes are terminated by zap’ing them.
    When all user processes have terminated, your operating system should shut
    down. Thus, after start3 terminates (or returns) all user processes should
    have terminated. Since there should then be no runnable or blocked
    processes, the kernel will halt.

    Input
    arg1: termination code for the process.
*/
void terminate(systemArgs *args)
{
    terminateReal((long) args->arg1);
} /* terminate */

/*
    terminateReal -- resets the fields of the quitting process in our process
                        table. Zaps any remaining children and quits with param
                        status.
 */
void terminateReal(int status)
{
    procStruct *currentProc = &ProcTable[getpid() % MAXPROC];
    int childPid = currentProc->childPid;
    currentProc->childPid = -1;
    MboxRelease(currentProc->mboxID);
    currentProc->mboxID = -1;
    currentProc->priority = 0;
    currentProc->parentPid = -1;
    currentProc->termCode = status;

    // zap any remaining children
    if (childPid != -1) {
        procStruct *childPtr = &ProcTable[childPid % MAXPROC];
        while (childPtr != NULL && childPtr->pid != 0 && childPtr->termCode == 0){
            zap(childPtr->pid);
            if (childPtr->nextPid == -1)
                break;
            childPtr = &ProcTable[childPtr->nextPid % MAXPROC];
        }
    }
    quit(status);
} /* terminateReal */

/*
  Creates a user-level semaphore.
    Input
        arg1: initial semaphore value.
    Output
        arg1: semaphore handle to be used in subsequent semaphore system calls.
        arg4: -1 if initial value is negative or no semaphores are available; 0 otherwise.
 */
void semCreate(systemArgs *args)
{
    // find an empty slot to insert
    int i;
    for (i = 0; i < MAXSEMS; i++){
        if(SemsTable[i].semId == -1)
            break;
    }

    // no open slots were found, return -1 in arg4
    if (i == MAXSEMS){
        args->arg4 = (void *) -1;
        return;
    }

    int result = MboxCreate((long) args->arg1, 50);
    if (result == -1) {
        args->arg4 = (void *) -1;
        return;
    }
	args->arg4 = 0;
	args->arg2 = (void *) (long) i;
    SemsTable[i].mboxID = result;
    SemsTable[i].semId = i;
    SemsTable[i].value = (long) args->arg1;
}

/*
 * Performs a “P” operation on a semaphore.
   Input: arg1: semaphore handle
   Output:  arg4: -1 if semaphore handle is invalid, 0 otherwise
 */
void semP(systemArgs *args)
{
    if (args->arg1 < 0 || (long) args->arg1 >= MAXSEMS) {
        args->arg4 = (void *) -1;
        return;
    }
    // MboxSend could potentially block, set isBlocking flag just in case we are
    // freeing a Sem that has blocked processes later on
    SemsTable[(long) args->arg1].isBlocking = 1;
    int result = MboxSend(SemsTable[(long) args->arg1].mboxID, NULL, 0);
    SemsTable[(long) args->arg1].isBlocking = 0;

    if (SemsTable[(long) args->arg1].semId == -1) {
        systemArgs args;
        args.arg1 = (void *) 1;
        terminate(&args);
    }
    if (result == -1)
        USLOSS_Console("semP(): Invalid params for MboxSend\n");
    args->arg4 = 0;
} /* semP */

/*
 * Performs a “V” operation on a semaphore.
   Input: arg1: semaphore handle
   Output:  arg4: -1 if semaphore handle is invalid, 0 otherwise
 */
void semV(systemArgs *args)
{
    if (args->arg1 < 0 || (long) args->arg1 >= MAXSEMS) {
        args->arg4 = (void *) -1;
        return;
    }

    int result = MboxCondReceive(SemsTable[(long) args->arg1].mboxID, NULL, MAX_MESSAGE);
    if (result == -1)
        USLOSS_Console("semV(): Invalid params for MboxCondReceive\n");
    args->arg4 = 0;
} /* semV */

/*
 *Frees a semaphore.
Input
    arg1: semaphore handle.
Output
    arg4: -1 if semaphore handle is invalid, 1 if there were processes blocked on the semaphore, 0 otherwise.
Any process waiting on a semaphore when it is freed should be terminated using the equivalent of the Terminate system call.
 */
void semFree(systemArgs *args)
{
    if (args->arg1 < 0 || (long) args->arg1 >= MAXSEMS) {
        args->arg4 = (void *) -1;
        return;
    }
    SemsTable[(long) args->arg1].semId = -1;

    //1 if there were processes blocked on the semaphore
    //check using a MboxCondSend if the mailbox is full

    if (SemsTable[(long) args->arg1].isBlocking) {
        args->arg4 = (void *) 1;
    } else {
        //0 otherwise
        args->arg4 = (void *) 0;
    }

    int result = MboxRelease(SemsTable[(long) args->arg1].mboxID);

    if (result == -1)
        USLOSS_Console("semFree(): Invalid params for MboxRelease\n");

    SemsTable[(long) args->arg1].mboxID = -1;
} /* semFree */

/*
 * Returns the process ID of the currently running process.
   Output:  arg1: the process ID.
 */
void getPID(systemArgs *args)
{
    args->arg1 = (void *) (long) getpid();
} /* getPID */

/*
 * Returns the value of USLOSS time-of-day clock.
Output
    arg1: the time of day.
 */
void getTimeofDay(systemArgs *args)
{
	int errCode = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, (int *) &(args->arg1));
	if (errCode == USLOSS_DEV_INVALID) {
		USLOSS_Console("readCurStartTime(): Device and unit invalid\n");
		USLOSS_Halt(1);
	}
} /* getTimeofDay */

/*
 *  Routine:  cpuTime
 *
 *  Description: Returns the CPU time of the process (this is
                the actual CPU time used, not just the time since
                the current time slice started).
 *
 *  Arguments: Output: arg1: the CPU time used by the currently running process.
 *
 */
 void cpuTime(systemArgs *args)
 {
    args->arg1 = (void *) (long) readtime();
} /* cpuTime */

/* an error method to handle invalid syscalls
 * To clarify, we know it is an invalid syscall.
 * it is similar to the nullsys from phase 2
 * with the difference that it will terminate the offending process,
 * rather than halting USLOSS
 */
void nullsys3(systemArgs *args)
{
    USLOSS_Console("nullsys(): Invalid syscall %d. Halting...\n", args->number);
    terminateReal(69);
} /* nullsys3 */

/*
 * checks if the OS is currently in kernel mode; halts if it isn't.
 * to clarify, we are checking if the OS is in kernel mode. If it isn't,
 * we will terminate. Otherwise, we will continue, since it is a void
 * function. This function will take in a char* funcName, which is the function
 * name.
 */
void check_kernel_mode(char * funcName)
{
    if (!(USLOSS_PsrGet() & 1)) {
        USLOSS_Console(
             "%s(): called while in user mode, by process %d. Halting...\n", funcName, getpid());
         USLOSS_Halt(1);
    }
} /* check_kernel_mode */

/*
 * sets the current mode to user mode.
 */
void setUserMode()
{
    check_kernel_mode("setUserMode");
    unsigned int previousPSRValue = USLOSS_PsrGet();
    // we need to store the current interrupt mode
    unsigned int tempPsrCurrentInterruptTemp = previousPSRValue & 2;
    // preserve the current interrupt
    unsigned int newPSRValue = (previousPSRValue << 2) | tempPsrCurrentInterruptTemp;
    // set user mode by making sure last bit is 0
    newPSRValue = newPSRValue & 14;
    if (USLOSS_PsrSet(newPSRValue) == USLOSS_ERR_INVALID_PSR)
        USLOSS_Console("ERROR: Invalid PSR value set! was: %u\n", newPSRValue);
} /* setUserMode */
