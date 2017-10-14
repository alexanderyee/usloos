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
/* Data structures */
void (*systemCallVec[MAXSYSCALLS])(systemArgs *);

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

    systemCallVec[SYS_SPAWN] = (void (*) (systemArgs *)) Spawn;
    systemCallVec[SYS_WAIT] = (void (*) (systemArgs *)) Wait;
    systemCallVec[SYS_TERMINATE] = (void (*) (systemArgs *)) Terminate;
    systemCallVec[SYS_SEMCREATE] = (void (*) (systemArgs *)) SemCreate;
    systemCallVec[SYS_SEMP] = (void (*) (systemArgs *)) SemP;
    systemCallVec[SYS_SEMV] = (void (*) (systemArgs *)) SemV;
    systemCallVec[SYS_SEMFREE] = (void (*) (systemArgs *)) SemFree;
    systemCallVec[SYS_GETTIMEOFDAY] = (void (*) (systemArgs *)) GetTimeofDay;
    systemCallVec[SYS_CPUTIME] = (void (*) (systemArgs *)) CPUTime;
    systemCallVec[SYS_GETPID] = (void (*) (systemArgs *)) GetPID;


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
//    pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
//    pid = waitReal(&status); << LOSER METHOD LMAO

} /* start2 */



/* an error method to handle invalid syscalls
 * To clarify, we know it is an invalid syscall.
 * it is similar to the nullsys from phase 2
 * with the difference that it will terminate the offending process,
 * rather than halting USLOSS
 */
void nullsys3(systemArgs *args)
{
    USLOSS_Console("nullsys(): Invalid syscall %d. Halting...\n", (*args).number);
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
