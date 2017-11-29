
#include "usloss.h"
#include "usyscall.h"
#include "phase5.h"
#include "phase1.h"

#define DEBUG 1
extern int debugflag;

/*
 * malloc the page table for the process. if VmInit hasn't been called, then
 * don't do anything.
 */
void
p1_fork(int pid)
{
	// change later lol
    if (DEBUG)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);

    // init proc in our proc table

    if (vmInitFlag) {
        // for simple1.c and incremental impl. purposes
        printf("1\n");
		processes[getpid() % MAXPROC].pid = getpid();
		printf("2\n");
		processes[getpid() % MAXPROC].pageTable = malloc(vmStats.pages * sizeof(struct PTE));
       	printf("3\n");
		processes[getpid() % MAXPROC].mboxID = mbox_create_real(0);
    	printf("4\n");
	}
} /* p1_fork */

void
p1_switch(int old, int new)
{
	// change later lol
    if (DEBUG)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
    if (vmInitFlag) {
        // unmap current proc stuff, map the new process
        int i, framePtr, protPtr, result;
        PTE *currPT = processes[old % MAXPROC].pageTable;
        for (i = 0; i < vmStats.pages; i++) {
            if (USLOSS_MmuGetMap(TAG, i, &framePtr, &protPtr) != USLOSS_MMU_ERR_NOMAP) {
                result = USLOSS_MmuUnmap(TAG, i);
                if (result) {
                    USLOSS_Console("p1_switch(): unmapping error, status code: %d\n", result);
                }
            }
        }
        // map stuff of new proc
        currPT = processes[new % MAXPROC].pageTable;
        for (i = 0; i < vmStats.pages; i++) {
            if (currPT[i].state == INCORE) {
                result = USLOSS_MmuMap(TAG, i, currPT[i].frame, USLOSS_MMU_PROT_RW);
                // for now, have this mapping.
                if (result) {
                    USLOSS_Console("p1_switch(): mapping error, status code: %d\n", result);
                }
            }
        }
    }
} /* p1_switch */

void
p1_quit(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */
