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
    }

    for(i = 0; i < MAXSEMS; i++){
        SemsTable[i].semId = -1;
        SemsTable[i].mboxID = -1;
    }

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * Assumes kernel-mode versions of the system calls
     * with lower-case names.  I.e., Spawn is the user-mode function
     * called by the test cases; spawn is the kernel-mode function that
     * is called by the syscallHandler; spawnReal is the function that
     * contains the implementation and is called by spawn.
     *
     * Spawn() is in libuser.c.  It invokes USLOSS_Syscall()
     * The system call handler calls a function named spawn() -- note lower
     * case -- that extracts the arguments from the sysargs pointer, and
     * checks them for possible errors.  This function then calls spawnReal().
     *
     * Here, we only call spawnReal(), since we are already in kernel mode.
     *
     * spawnReal() will create the process by using a call to fork1 to
     * create a process executing the code in spawnLaunch().  spawnReal()
     * and spawnLaunch() then coordinate the completion of the phase 3
     * process table entries needed for the new process.  spawnReal() will
     * return to the original caller of Spawn, while spawnLaunch() will
     * begin executing the function passed to Spawn. spawnLaunch() will
     * need to switch to user-mode before allowing user code to execute.
     * spawnReal() will return to spawn(), which will put the return
     * values back into the sysargs pointer, switch to user-mode, and
     * return to the user code that called Spawn.
     */
    pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
    pid = waitReal(&status);
    return pid;
} /* start2 */

int spawnReal(char *name, int (*func)(char *), char *arg, long stack_size, long priority)
{
	int pid = fork1(name,(int (*)(char *))spawnLaunch, NULL, stack_size, priority);
    ProcTable[pid % MAXPROC].pid = pid;
	if (ProcTable[pid % MAXPROC].mboxID == -1){
         ProcTable[pid % MAXPROC].mboxID = MboxCreate(0, 50);
	}
    ProcTable[pid % MAXPROC].startFunc = func;
    ProcTable[pid % MAXPROC].startArg = arg;
    // TODO: implement multiple children
    ProcTable[getpid() % MAXPROC].childPid = pid;
    // block
    MboxSend(ProcTable[pid % MAXPROC].mboxID, NULL, 0);
    return pid;
}

int spawn(systemArgs *args)
{
    int ans = spawnReal(args->arg5, args->arg1, args->arg2, (long) args->arg3, args->arg4);
    args->arg1 = (long) ans;
    return ans;
}

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void spawnLaunch()
{
	if (ProcTable[getpid() % MAXPROC].mboxID == -1){
		ProcTable[getpid() % MAXPROC].mboxID = MboxCreate(0, 50);
	}
    MboxReceive(ProcTable[getpid() % MAXPROC].mboxID, NULL, MAX_MESSAGE);
    int result, currPid = getpid();

    // Enable interrupts
	//enableInterrupts();
	setUserMode();
    // Call the function passed to fork1, and capture its return value
    result = ProcTable[currPid % MAXPROC].startFunc(ProcTable[currPid % MAXPROC].startArg);
	Terminate(result);
} /* spawnLaunch */

int waitReal(int *status)
{

    int pid = join(status);
    if(pid < 0)
    {
        USLOSS_Console("waitReal(): Invalid join %d. Halting...\n", pid);
        USLOSS_Halt(1);
    }

    return pid;
}

int wait(systemArgs *args)
{

    int pid = waitReal(args->arg2);
    args->arg1 = (long) pid;
    return pid;
}

void terminate(systemArgs *args)
{
    int childPid = ProcTable[getpid() % MAXPROC].childPid;
	ProcTable[getpid() % MAXPROC].childPid = 0;
	MboxRelease(ProcTable[getpid() % MAXPROC].mboxID);
	ProcTable[getpid() % MAXPROC].mboxID = 0;
	ProcTable[getpid() % MAXPROC].pid = 0;
    //if there is a child zap em
    if (childPid != 0 && ProcTable[childPid % MAXPROC].pid != 0)
    {
        zap(childPid);
    }
    quit((int) args->arg1);
}
/*
 *Creates a user-level semaphore.
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
    if (i == MAXSEMS){
        args->arg4 = (void *) -1;
        return;
    }

    int result = MboxCreate(args->arg1, 50);
    if (result == -1) {
        args->arg4 = -1;
        return;
    }
	args->arg4 = 0;
	args->arg2 = (long) i;
    SemsTable[i].mboxID = result;
    SemsTable[i].semId = i;
}

/*
 * Performs a “P” operation on a semaphore.
   Input: arg1: semaphore handle
   Output:  arg4: -1 if semaphore handle is invalid, 0 otherwise
 */
void semP(systemArgs *args)
{
    if (args->arg1 < 0 || (int) args->arg1 > MAXSEMS) {
        args->arg4 = -1;
        return;
    }
    int result = MboxSend(SemsTable[(int) args->arg1].mboxID, NULL, 0);
    if (SemsTable[(int) args->arg1].semId == -1) {
        systemArgs args;
        args.arg1 = 420;
        terminate(&args);
    }
    if (result == -1)
        USLOSS_Console("semP(): Invalid params for MboxSend\n");
    args->arg4 = 0;
}

/*
 * Performs a “V” operation on a semaphore.
   Input: arg1: semaphore handle
   Output:  arg4: -1 if semaphore handle is invalid, 0 otherwise
 */
void semV(systemArgs *args)
{
    if (args->arg1 < 0 || args->arg1 > MAXSEMS) {
        args->arg4 = -1;
        return;
    }

    int result = MboxCondReceive(SemsTable[(int) args->arg1].mboxID, NULL, MAX_MESSAGE);
    if (result == -1)
        USLOSS_Console("semV(): Invalid params for MboxCondReceive\n");
    args->arg4 = 0;
}
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
    if (args->arg1 < 0 || args->arg1 > MAXSEMS) {
        args->arg4 = -1;
        return;
    }
    SemsTable[(int) args->arg1].semId = -1;

    //1 if there were processes blocked on the semaphore
    //check using a MboxCondSend if the mailbox is full
    if(MboxCondSend(SemsTable[(int) args->arg1].mboxID, NULL, 0) == 1 ||
        MboxCondSend(SemsTable[(int) args->arg1].mboxID, NULL, 0) == -1){
        printf("ey ;)\n");
        args->arg4 = 1;
    } else{
        //0 otherwise
        args->arg4 = 0;
    }

    int result = MboxRelease(SemsTable[(int) args->arg1].mboxID);

    if (result == -1)
        USLOSS_Console("semFree(): Invalid params for MboxRelease\n");

    SemsTable[(int) args->arg1].mboxID = -1;
}
/*
 * Returns the process ID of the currently running process.
   Output:  arg1: the process ID.
 */
void getPID(systemArgs *args)
{
    args->arg1 = getpid();
}

void getTimeofDay(systemArgs *args)
{
	int errCode = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &(args->arg1));
	if (errCode == USLOSS_DEV_INVALID) {
		USLOSS_Console("readCurStartTime(): Device and unit invalid\n");
		USLOSS_Halt(1);
	}
}

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
    args->arg1 = readtime();
 }

/* an error method to handle invalid syscalls
 * To clarify, we know it is an invalid syscall.
 * it is similar to the nullsys from phase 2
 * with the difference that it will terminate the offending process,
 * rather than halting USLOSS
 */
void nullsys3(systemArgs *args)
{
    USLOSS_Console("nullsys(): Invalid syscall %d. Halting...\n", args->number);
    USLOSS_Halt(1);
} /* nullsys */

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
}
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

}
