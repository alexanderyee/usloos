/*
 * Definitions for Phase 5 of the project (virtual memory).
 */
#ifndef _PHASE5_H
#define _PHASE5_H

#include <usloss.h>
#include <stdio.h>
#include <stdlib.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <vm.h>


/*
 * Pager priority.
 */
#define PAGER_PRIORITY	2

/*
 * Maximum number of pagers.
 */
#define MAXPAGERS 4
/*
 * Paging statistics
 */
typedef struct VmStats {
    int pages;          // Size of VM region, in pages
    int frames;         // Size of physical memory, in frames
    int diskBlocks;     // Size of disk, in blocks (pages)
    int freeFrames;     // # of frames that are not in-use
    int freeDiskBlocks; // # of blocks that are not in-use
    int switches;       // # of context switches
    int faults;         // # of page faults
    int new;            // # faults caused by previously unused pages
    int pageIns;        // # faults that required reading page from disk
    int pageOuts;       // # faults that required writing a page to disk
    int replaced;	// # pages replaced; i.e., frame had a page and we
                        //   replaced that page in the frame with a different
                        //   page. */
} VmStats;

extern VmStats	vmStats;
extern int  start5(char *);
extern int vmInitFlag;
extern Process processes[MAXPROC];
extern int VmInit(int mappings, int pages, int frames, int pagers, void **region);
extern int VmDestroy(void);
extern int  SemCreate(int init_value, int *);
extern int  SemP(int semaphore);
extern int  SemV(int semaphore);
extern int  GetPID(int *pid);
//extern void PrintStats();

#endif /* _PHASE5_H */
