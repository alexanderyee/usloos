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
    int fid;    // frame id
    int status; // whether or not is in on disk or empty
    int isRef;  
} Frame;
#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)
#endif
