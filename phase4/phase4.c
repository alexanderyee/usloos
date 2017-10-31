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

static int ClockDriver(char *);
static int DiskDriver(char *);
int sleepReal(USLOSS_Sysargs *);
void check_kernel_mode(char *);
int enqueue(procPtr);
procPtr pop(void);

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

    /* init ProcTable */
    for (i = 0; i < MAXPROC; i++) {
        ProcTable[i].pid = -1;
    }
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
    pid = spawnReal("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
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
    int status;

    // Let the parent know we are running and enable interrupts.
    semvReal(running);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

    // Infinite loop until we are zap'd
    while(! isZapped()) {
    	result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        printf("status for waitDevice: %d\n", status);
        if (result != 0) {
    	    return 0;
	    }
        result = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &status);
    	if (result == USLOSS_DEV_INVALID) {
    		USLOSS_Console("ClockDriver(): Device and unit invalid\n");
    		USLOSS_Halt(1);
    	}
        printf("status for DeviceInput: %d\n", status);


	/*
	 * Compute the current time and wake up any processes
	 * whose time has come.
	 */
    }
}

static int DiskDriver(char *arg)
{
    return 0;
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

    enqueue(ProcTable[getpid() % MAXPROC]);
    // TODO don't ignore the result of enqueue
    ProcTable[getpid() % MAXPROC].sleepSecondsRemaining = (int) (long) args->arg1;
    MboxSend(ProcTable[getpid() % MAXPROC].mboxID, NULL, 0);

    args->arg1 = 0;
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
    ProcTable[pid % MAXPROC].mboxID = MboxCreate(0, MAX_MESSAGE);
    if (ProcTable[pid % MAXPROC].mboxID < 0) {
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
