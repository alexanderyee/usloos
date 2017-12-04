/*
 * vm.h
 */

#ifndef _VM_H
#define _VM_H
/*
 * All processes use the same tag.
 */
#define TAG 0

/*
 * Different states for a page.
 */
#define UNUSED 500
#define INCORE 501
/* You'll probably want more states */

/*
 * Different states for a frame.
 */
#define EMPTY 69
#define IN_MEM 70
#define ON_DISK 71
#define CLEAN 72
#define DIRTY 73
#define REFD 74
#define UNREFD 75

/*
 * Page table entry.
 */
typedef struct PTE {
    int  state;      // See above.
    int  frame;      // Frame that stores the page (if any). -1 if none.
    int  diskBlock;  // Disk block that stores the page (if any). -1 if none.
    // Add more stuff here
} PTE;

/*
 * Per-process information.
 */
typedef struct Process {
    int  pid;
    int  numPages;   // Size of the page table.
    PTE  *pageTable; // The page table for the process.
    int  mboxID;     // private mbox id
    int  lastRef;    // the PTE that was last referenced
    // Add more stuff here */
} Process;

/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int  pid;        // Process with the problem.
    void *addr;      // Address that caused the fault.
    int  replyMbox;  // Mailbox to send reply.
    // Add more stuff here.
} FaultMsg;

typedef struct Frame {
    int status; // whether or not is in on disk or empty
    int pid;
    int page;
} Frame;
#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)
#endif
