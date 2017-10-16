/*
 *  File:  libuser.c
 *
 *  Description:  This file contains the interface declarations
 *                to the OS kernel support package.
 *
 */

#include <usyscall.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <libuser.h>

void setUserMode();

#define CHECKMODE {    \
    if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) { \
        USLOSS_Console("Trying to invoke syscall from kernel\n"); \
        USLOSS_Halt(1);  \
    }  \
}

/*
 *  Routine:  Spawn
 *
 *  Description: This is the call entry to fork a new user process.
 *
 *  Arguments:    char *name    -- new process's name
 *                PFV func      -- pointer to the function to fork
 *                void *arg     -- argument to function
 *                int stacksize -- amount of stack to be allocated
 *                int priority  -- priority of forked process
 *                int  *pid     -- pointer to output value
 *                (output value: process id of the forked process)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
long Spawn(char *name, int (*func)(char *), char *arg, long stack_size,
    long priority, int *pid)
{
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_SPAWN;
    sysArg.arg1 = (void *) func;
    sysArg.arg2 = arg;
    sysArg.arg3 = (void *) stack_size;
    sysArg.arg4 = (void *) priority;
    sysArg.arg5 = name;
    USLOSS_Syscall(&sysArg);
    *pid = (long) sysArg.arg1;
    return (long) sysArg.arg4;
} /* end of Spawn */


/*
 *  Routine:  Wait
 *
 *  Description: This is the call entry to wait for a child completion
 *
 *  Arguments:    int *pid -- pointer to output value 1
 *                (output value 1: process id of the completing child)
 *                int *status -- pointer to output value 2
 *                (output value 2: status of the completing child)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Wait(int *pid, int *status)
{
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_WAIT;
	sysArg.arg1 = pid;
	sysArg.arg2 = status;
    USLOSS_Syscall(&sysArg);

    return 0;

} /* end of Wait */


/*
 *  Routine:  Terminate
 *
 *  Description: This is the call entry to terminate
 *               the invoking process and its children
 *
 *  Arguments:   int status -- the commpletion status of the process
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
void Terminate(int status)
{
    USLOSS_Sysargs sysArg;
    CHECKMODE;
    sysArg.number = SYS_TERMINATE;
    sysArg.arg1 = status;
    USLOSS_Syscall(&sysArg);
} /* end of Terminate */


/*
 *  Routine:  SemCreate
 *
 *  Description: Create a semaphore.
 *
 *  Arguments:
 *
 */
int SemCreate(int value, int *semaphore)
{
    USLOSS_Sysargs sysArg;
    CHECKMODE;
    if (value < 0)
        return -1;
    sysArg.number = SYS_SEMCREATE;
    sysArg.arg1 = value;
    sysArg.arg2 = semaphore;
    USLOSS_Syscall(&sysArg);
	*semaphore = sysArg.arg2;
    return sysArg.arg4;
} /* end of SemCreate */


/*
 *  Routine:  SemP
 *
 *  Description: "P" a semaphore.
 *
 *  Arguments:
 *
 */
int SemP(int semaphore)
{
    USLOSS_Sysargs sysArg;
    CHECKMODE;
    sysArg.number = SYS_SEMP;
    sysArg.arg1 = semaphore;
    USLOSS_Syscall(&sysArg);
    return sysArg.arg4;
} /* end of SemP */


/*
 *  Routine:  SemV
 *
 *  Description: "V" a semaphore.
 *
 *  Arguments:
 *
 */
int SemV(int semaphore)
{
    USLOSS_Sysargs sysArg;
    CHECKMODE;
    sysArg.number = SYS_SEMV;
    sysArg.arg1 = semaphore;
    USLOSS_Syscall(&sysArg);
    return sysArg.arg4;
} /* end of SemV */


/*
 *  Routine:  SemFree
 *
 *  Description: Free a semaphore.
 *
 *  Arguments:
    Input arg1: semaphore handle.
    Output: arg4: -1 if semaphore handle is invalid, 1 if there were
            processes blocked on the semaphore, 0 otherwise.
 */
int SemFree(int semaphore)
{
    USLOSS_Sysargs sysArg;
    CHECKMODE;
    sysArg.number = SYS_SEMFREE;
    sysArg.arg1 = semaphore;
    USLOSS_Syscall(&sysArg);
    return sysArg.arg4;
} /* end of SemFree */


/*
 *  Routine:  GetTimeofDay
 *
 *  Description: This is the call entry point for getting the time of day.
 *
 *  Arguments:
 *
 */
void GetTimeofDay(int *tod)
{
} /* end of GetTimeofDay */


/*
 *  Routine:  CPUTime
 *
 *  Description: This is the call entry point for the process' CPU time.
 *
 *  Arguments:
 *
 */
void CPUTime(int *cpu)
{
} /* end of CPUTime */


/*
 *  Routine:  GetPID
 *
 *  Description: This is the call entry point for the process' PID.
 *
 *  Arguments:
 *
 */
void GetPID(int *pid)
{
    USLOSS_Sysargs sysArg;
    CHECKMODE;
    sysArg.number = SYS_GETPID;
    USLOSS_Syscall(&sysArg);
    *pid = sysArg.arg1;
} /* end of GetPID */

/* end libuser.c */
