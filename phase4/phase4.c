/* ------------------------------------------------------------------------
   phase4.c

   University of Arizona
   Computer Science 452

   Katie Pan & Alex Yee
   ------------------------------------------------------------------------ */
#include <usloss.h>
#include <libuser.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <providedPrototypes.h>
#include <stdlib.h> /* needed for atoi() */

int	 	running;
procStruct ProcTable[MAXPROC];
procPtr    sleepQueue[MAXPROC];
void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *);

static int ClockDriver(char *);
static int DiskDriver(char *);
int sleepReal(USLOSS_Sysargs *);
void check_kernel_mode(char *);
int enqueue(procPtr);
procPtr pop(void);
procPtr popAtIndex(int);
int diskSizeReal(USLOSS_Sysargs * );

void start3(void)
{
    char	name[128];
    //char    termbuf[10];
    int		i;
    int		clockPID;
    int		pid;
    int		status;
    /*
     * Check kernel mode here.
     */
    check_kernel_mode("start3");
    // initalize the syscall handlers
   systemCallVec[SYS_SLEEP] = (void (*) (USLOSS_Sysargs *)) sleepReal;
   systemCallVec[SYS_DISKSIZE] = (void (*) (USLOSS_Sysargs *)) diskSizeReal;

    /* init ProcTable */
    for (i = 0; i < MAXPROC; i++) {
        ProcTable[i].pid = -1;
    	sleepQueue[i] = NULL;
	}
	initProc(getpid());
    /*
     * Create clock device driver
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
    running = semcreateReal(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) {
	    USLOSS_Console("start3(): Can't create clock driver\n");
        USLOSS_Halt(1);
    }
    // initialize the process entry in our ProcTable

    /*
     * Wait for the clock driver to start. The idea is that ClockDriver
     * will V the semaphore "running" once it is running.
     */

    sempReal(running);
	char buf[69];
    /*
     * Create the disk device drivers here.  You may need to increase
     * the stack size depending on the complexity of your
     * driver, and perhaps do something with the pid returned.
     */


    for (i = 0; i < USLOSS_DISK_UNITS; i++) {
        sprintf(buf, "%d", i);
        pid = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }
    }

    // May be other stuff to do here before going on to terminal drivers

    /*
     * Create terminal device drivers.
     */


    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case first letters, as shown in provided_prototypes.h
     */
    //setUserMode();
    pid = spawnReal("start4", start4, NULL, 2 * USLOSS_MIN_STACK, 3);
    initProc(pid);
    pid = waitReal(&status);

    /*
     * Zap the device drivers
     */
    zap(clockPID);  // clock driver

    // eventually, at the end:
    quit(0);

}

static int ClockDriver(char *arg)
{
    int result;

    // Let the parent know we are running and enable interrupts.
    semvReal(running);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

    // Infinite loop until we are zap'd
    while(!isZapped()) {
        int status, i = 0;
		result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        if (result != 0) {
    	    return 0;
	    }

        // look through all the sleeping procs, subtract the time.
		while (i < MAXPROC && sleepQueue[i] != NULL) {
            if (sleepQueue[i]->lastSleepTime == 0) {
                // init the start time
                sleepQueue[i]->lastSleepTime = status;
            } else {
                sleepQueue[i]->sleepSecondsRemaining -= status - sleepQueue[i]->lastSleepTime;
                sleepQueue[i]->lastSleepTime = status;
            }
            if (sleepQueue[i]->sleepSecondsRemaining < 0) {
				procPtr p = popAtIndex(i);
	            semvReal(p->semID);
                continue;
            }
            i++;
        }
	/*
	 * Compute the current time and wake up any processes
	 * whose time has come.
	 */

    }
}

/*
* Causes the calling process to become unrunnable for at least the
* specified number of seconds, and not significantly longer.
* The seconds must be non-negative.
*
* Return values:
*		-1: seconds is not valid
*		0: otherwise
*/
int sleepReal(USLOSS_Sysargs * args)
{
    if (args->arg1 < 0) {
        args->arg1 = (void *)(long) -1;
        return -1;
    }

    if (ProcTable[getpid() & MAXPROC].pid == -1) {
        // process hasn't been initialized yet, let's fix that
        initProc(getpid());
    }
    int result = enqueue(&ProcTable[getpid() % MAXPROC]);
    long time;
    // TODO don't ignore the result of enqueue
    ProcTable[getpid() % MAXPROC].sleepSecondsRemaining = (int) (long) args->arg1 * 1000000;
    sempReal(ProcTable[getpid() % MAXPROC].semID);
    args->arg1 = 0;
    return 0;
}

/*
 *
 */
static int DiskDriver(char *arg)
{
    return 0;
}

/*
 * Reads sectors sectors from the disk indicated by unit, starting at track track and
 * sector first. The sectors are copied into buffer. Your driver must handle a range of
 * sectors specified by first and sectors that spans a track boundary (after reading the
 * last sector in a track it should read the first sector in the next track).
 * A file cannot wrap around the end of the disk.
 *   sysArg.arg1 = dbuff;
     sysArg.arg2 = unit;
     sysArg.arg3 = track;
     sysArg.arg4 = first;
     sysArg.arg5 = sectors;
 * Return values: (through arg1)
 *  -1: invalid parameters
 *  0: sectors were read successfully
 *  >0: disk’s status register
 */
int diskReadReal(USLOSS_Sysargs * args)
{

}

/*
 *
 */
int diskWriteReal(USLOSS_Sysargs * args)
{

}

/*
 *
 */
int diskSizeReal(USLOSS_Sysargs * sysArg)
{
    USLOSS_DeviceRequest deviceRequest;
    deviceRequest.opr = USLOSS_DISK_TRACKS;
    deviceRequest.reg1 = sysArg->arg4;

    USLOSS_DeviceOutput(USLOSS_DISK_DEV, sysArg->arg1, &deviceRequest);
    return 0;
}

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
 * initProc -- function that initializes the process entry in our phase4 ProcTable
 *              initializes pid and the private mbox.
 *      returns 0 on success, -1 if failure
 */
int initProc(int pid)
{
    ProcTable[pid % MAXPROC].pid = pid;
    ProcTable[pid % MAXPROC].semID = semcreateReal(0);
    if (ProcTable[pid % MAXPROC].semID < 0) {
        USLOSS_Console("initProc(): Failure creating private mailbox for process %d", pid);
        return -1;
    }
    return 0;
}

/*
 *
 */
int enqueue(procPtr p){
    int i = 0;
    while (i < MAXPROC && sleepQueue[i] != NULL) {
    	i++;
		if (i == MAXPROC) {
      		USLOSS_Console("insert(): no more roomsies ;_;\n");
     		return 1;
		}
   }
   sleepQueue[i] = p;
   return 0;
}

/*
 *
 */
procPtr pop()
{
    int i = 0;
    procPtr result = NULL;
    if (sleepQueue[0] != NULL) {
        result = sleepQueue[0];
    }
    while (i < (MAXPROC-1) && sleepQueue[i] != NULL) {
        sleepQueue[i] = sleepQueue[i+1];
        i++;
    }
    return result;
}

/*
 * pop at a certain index
 */
procPtr popAtIndex(int index)
{
    int i = index;
    procPtr result = sleepQueue[i];

    if(result != NULL){
        //shift everything else
        while (i < (MAXPROC-1) && sleepQueue[i] != NULL) {
            sleepQueue[i] = sleepQueue[i+1];
            i++;
        }
    }

    return result;
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
} /* setUserMode */
