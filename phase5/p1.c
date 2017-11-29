
#include "usloss.h"
#include "usyscall.h"
#include "phase5.h"
#include "phase1.h"

#define DEBUG 0
extern int debugflag;


/*
 * malloc the page table for the process. if VmInit hasn't been called, then
 * don't do anything.
 */
void
p1_fork(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);

    // init proc in our proc table

    if (vmInitFlag) {
        // for simple1.c and incremental impl. purposes
        processes[getpid() % MAXPROC].pid = getpid();
		processes[getpid() % MAXPROC].pageTable = malloc(vmStats.pages * sizeof(struct PTE));
		processes[getpid() % MAXPROC].pageTable[0].frame = 0;
    }
} /* p1_fork */

void
p1_switch(int old, int new)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
    if (vmInitFlag) {
        // unmap current proc stuff, map the new process
        PTE *currPT = processes[getpid() % MAXPROC].pageTable;
        int i, framePtr, protPtr, result;
        for (i = 0; i < vmStats.pages; i++) {
            if (USLOSS_MmuGetMap(TAG, i, &framePtr, &protPtr) != USLOSS_MMU_ERR_NOMAP) {
                result = USLOSS_MmuUnmap(TAG, i);
                if (result) {
                    USLOSS_Console("p1_switch(): unmapping error, status code: %d\n", result);
                }
            }
        }
        result = USLOSS_MmuMap(0, 0, 0, USLOSS_MMU_PROT_RW);
        // for now, have this mapping.
        if (result) {
            USLOSS_Console("p1_switch(): mapping error, status code: %d\n", result);
        }
    }
} /* p1_switch */

void
p1_quit(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */
