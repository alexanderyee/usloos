/*
 * phase5.c
 * TODO CHANGE HDR COMMENT
 * This is a skeleton for phase5 of the programming assignment. It
 * doesn't do much -- it is just intended to get you started.
 */

#include <usloss.h>
#include <usyscall.h>
#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <libuser.h>
#include <vm.h>
#include <string.h>

extern void mbox_create_real(USLOSS_Sysargs *args_ptr);
extern void mbox_release_real(USLOSS_Sysargs *args_ptr);
extern void mbox_send_real(USLOSS_Sysargs *args_ptr);
extern void mbox_receive_real(USLOSS_Sysargs *args_ptr);
extern void mbox_condsend_real(USLOSS_Sysargs *args_ptr);
extern void mbox_condreceive_real(USLOSS_Sysargs *args_ptr);

Process processes[MAXPROC];

FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
VmStats  vmStats;
void *vmRegion;
int isDebug = 1;
int vmInitFlag = 0;
Frame *frameTable;
static void FaultHandler(int type, void * offset);
void * vmInitReal(int, int, int, int, int *);
static void vmInit(USLOSS_Sysargs *USLOSS_SysargsPtr);
static void vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr);
/*
 *----------------------------------------------------------------------
 *
 * start4 --
 *
 * Initializes the VM system call handlers.
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int
start4(char *arg)
{
    int pid;
    int result;
    int status;

    /* to get user-process access to mailbox functions */
    systemCallVec[SYS_MBOXCREATE]      = (void (*) (USLOSS_Sysargs *)) mbox_create_real;
    systemCallVec[SYS_MBOXRELEASE]     = (void (*) (USLOSS_Sysargs *)) mbox_release_real;
    systemCallVec[SYS_MBOXSEND]        = (void (*) (USLOSS_Sysargs *)) mbox_send_real;
    systemCallVec[SYS_MBOXRECEIVE]     = (void (*) (USLOSS_Sysargs *)) mbox_receive_real;
    systemCallVec[SYS_MBOXCONDSEND]    = (void (*) (USLOSS_Sysargs *)) mbox_condsend_real;
    systemCallVec[SYS_MBOXCONDRECEIVE] = (void (*) (USLOSS_Sysargs *)) mbox_condreceive_real;

    /* user-process access to VM functions */
    systemCallVec[SYS_VMINIT]    = vmInit;
    systemCallVec[SYS_VMDESTROY] = vmDestroy;
    result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
    if (result != 0) {
        USLOSS_Console("start4(): Error spawning start5\n");
        Terminate(1);
    }
    result = Wait(&pid, &status);
    if (result != 0) {
        USLOSS_Console("start4(): Error waiting for start5\n");
        Terminate(1);
    }
    Terminate(0);
    return 0; // not reached

} /* start4 */

/*
 *----------------------------------------------------------------------
 *
 * VmInit -- interface method for vmInitReal
 *
 * Input
 * arg1: number of mappings the MMU should hold
 * arg2: number of virtual pages to use
 * arg3: number of physical page frames to use
 * arg4: number of pager daemons
 *
 * Results:
 *      arg1: address of the first byte in the VM region
 *      arg4: -1 if illegal values are given as input; -2 if the VM region has already been
 *              initialized; 0 otherwise..
 *
 * Side effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void
vmInit(USLOSS_Sysargs *args)
{
    CheckMode();
    int firstByteAddy;
    if (args->arg1 != args->arg2) {
        USLOSS_Console("vmInit(): Number of mappings does not equal number of virtual pages!\n");
        args->arg4 = (void *) (long) -1;
        return;
    }
    else if (vmInitFlag) {
        args->arg4 = (void *) (long) -2;
		return;
    }
    int status = vmInitReal((int) (long) args->arg1, (int) (long) args->arg2,
                    (int) (long) args->arg3, (int) (long) args->arg4, &firstByteAddy);
    args->arg1 = (void *) (long) firstByteAddy;
    args->arg4 = (void *) (long) 0;
    vmInitFlag = 1;
} /* vmInit */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void
vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr)
{
   CheckMode();
} /* vmDestroy */


/*
 *----------------------------------------------------------------------
 *
 * vmInitReal --
 *
 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 * Initialize the VM region. installs a handler for the MMU_Init interrupt,
 * creates the pager daemon(s), and calls MMU_Init to initialize the MMU.
 * When the MMU is initialized the current tag is set to 0. Do not change it.
 *
 * Results:
 *      Address of the VM region in firstByteAddy.
        returns: 0 if ok
                -2 if region already been init
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
void *
vmInitReal(int mappings, int pages, int frames, int pagers, int *firstByteAddy)
{
   int status, numPages;

   CheckMode();
   status = USLOSS_MmuInit(mappings, pages, frames, USLOSS_MMU_MODE_TLB);
   if (status != USLOSS_MMU_OK) {
      USLOSS_Console("vmInitReal: couldn't initialize MMU, status %d\n", status);
      abort();
   }
   USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;
   frameTable = malloc(frames * sizeof(Frame));
   /*
    * Initialize page tables.
    */

   /*
    * Create the fault mailbox.
    */

   /*
    * Fork the pagers.
    */

   /*
    * Zero out, then initialize, the vmStats structure
    */
   memset((char *) &vmStats, 0, sizeof(VmStats));
   vmStats.pages = pages;
   vmStats.frames = frames;
   /*
    * Initialize other vmStats fields.
    */
    *firstByteAddy = USLOSS_MmuRegion(&numPages);
    return;
} /* vmInitReal */


/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void
PrintStats(void)
{
     USLOSS_Console("VmStats\n");
     USLOSS_Console("pages:          %d\n", vmStats.pages);
     USLOSS_Console("frames:         %d\n", vmStats.frames);
     USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
     USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
     USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
     USLOSS_Console("switches:       %d\n", vmStats.switches);
     USLOSS_Console("faults:         %d\n", vmStats.faults);
     USLOSS_Console("new:            %d\n", vmStats.new);
     USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
     USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
     USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroyReal --
 *
 * Called by vmDestroy.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void
vmDestroyReal(void)
{

   CheckMode();
   int result = USLOSS_MmuDone();
   if (result != USLOSS_MMU_OK) {
       USLOSS_Console("vmDestroyReal(): TODO %d\n", result);
       abort();
   }
   /*
    * Kill the pagers here.
    */
   /*
    * Print vm statistics.
    */
   USLOSS_Console("vmStats:\n");
   USLOSS_Console("pages: %d\n", vmStats.pages);
   USLOSS_Console("frames: %d\n", vmStats.frames);
   USLOSS_Console("blocks: %d\n", vmStats.diskBlocks);
   /* and so on... */

} /* vmDestroyReal */

/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *
 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
static void
FaultHandler(int type /* MMU_INT */,
             void *offset  /* Offset within VM region */)
{
    if (isDebug)
        USLOSS_Console("FaultHandler() called");
    int cause;

    assert(type == USLOSS_MMU_INT);
    cause = USLOSS_MmuGetCause();
    assert(cause == USLOSS_MMU_FAULT);
    vmStats.faults++;
    /*
     * Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
     * reply.
     */
    faults[getpid() % MAXPROC].pid = getpid();
    faults[getpid() % MAXPROC].addr = offset;
} /* FaultHandler */


/*
 *----------------------------------------------------------------------
 *
 * Pager
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int
Pager(char *buf)
{
    while(1) {
        /* Wait for fault to occur (receive from mailbox) */
        /* Look for free frame */
        /* If there isn't one then use clock algorithm to
         * replace a page (perhaps write to disk) */
        /* Load page into frame from disk, if necessary */
        /* Unblock waiting (faulting) process */
    }
    return 0;
} /* Pager */

void mbox_create_real(USLOSS_Sysargs *args) {
    //check if args are correct
    int numSlot = (int) (long) args->arg1;
    int slotSize = (int) (long) args->arg2;

    if(numSlot < 0 || numSlot > MAXSLOTS){
        args->arg4 = (void *)(long) -1;
        return;
    }

    if(slotSize < 0 || slotSize > MAX_MESSAGE){
        args->arg4 = (void *)(long) -1;
        return;
    }

    args->arg1 = (void *)(long) MboxCreate(numSlot, slotSize);
    return;
}

void mbox_release_real(USLOSS_Sysargs *args) {
    int mboxID = (int) (long) args->arg1;

    //error case
    if(mboxID < 0 || mboxID > MAXMBOX){
        args->arg4 = (void *)(long) -1;
        return;
    }

    args->arg4 = (void *)(long) MboxRelease(mboxID);
}

void mbox_send_real(USLOSS_Sysargs *args) {
    int mboxID = (int) (long) args->arg1;
    void *msgPtr = (void*) (long) args->arg2;
    int msgSize = (int) (long) args->arg3;

    //error case
    if(mboxID < 0 || mboxID > MAXMBOX){
        args->arg4 = (void *)(long) -1;
        return;
    }

    if(msgPtr == NULL){
        args->arg4 = (void *)(long) -1;
        return;
    }

    if(msgSize < 0 || msgSize > MAX_MESSAGE){
        args->arg4 = (void *)(long) -1;
        return;
    }

    int result = MboxSend(mboxID, msgPtr, msgSize);

    if(result == -1){
        args->arg4 = (void *)(long) -1;
        return;
    }

    args->arg4 = (void *)(long) 0;
}

void mbox_receive_real(USLOSS_Sysargs *args) {
    int mboxID = (int) (long) args->arg1;
    void *msgPtr = (void*) (long) args->arg2;
    int msgSize = (int) (long) args->arg3;

    //error case
    if(mboxID < 0 || mboxID > MAXMBOX){
        args->arg4 = (void *)(long) -1;
        return;
    }

    if(msgPtr == NULL){
        args->arg4 = (void *)(long) -1;
        return;
    }

    if(msgSize < 0 || msgSize > MAX_MESSAGE){
        args->arg4 = (void *)(long) -1;
        return;
    }

    int result = MboxReceive(mboxID, msgPtr, msgSize);

    if(result == -1){
        args->arg4 = (void *)(long) -1;
        return;
    }

    args->arg4 = (void *)(long) 0;
}

void mbox_condsend_real(USLOSS_Sysargs *args) {
    int mboxID = (int) (long) args->arg1;
    void *msgPtr = (void*) (long) args->arg2;
    int msgSize = (int) (long) args->arg3;

    //error case
    if(mboxID < 0 || mboxID > MAXMBOX){
        args->arg4 = (void *)(long) -1;
        return;
    }

    if(msgPtr == NULL){
        args->arg4 = (void *)(long) -1;
        return;
    }

    if(msgSize < 0 || msgSize > MAX_MESSAGE){
        args->arg4 = (void *)(long) -1;
        return;
    }

    int result = MboxCondSend(mboxID, msgPtr, msgSize);

    //case whnen mailbox is invalid, return -1
    if(result == -1){
        args->arg4 = (void *)(long) -1;
        return;
    }

    //case whnen mailbox is full, return 1
    if(result == 1){
        args->arg4 = (void *)(long) 1;
        return;
    }

    args->arg4 = (void *)(long) 0;
}

void mbox_condreceive_real(USLOSS_Sysargs *args) {
    int mboxID = (int) (long) args->arg1;
    void *msgPtr = (void*) (long) args->arg2;
    int msgSize = (int) (long) args->arg3;

    //error case
    if(mboxID < 0 || mboxID > MAXMBOX){
        args->arg4 = (void *)(long) -1;
        return;
    }

    if(msgPtr == NULL){
        args->arg4 = (void *)(long) -1;
        return;
    }

    if(msgSize < 0 || msgSize > MAX_MESSAGE){
        args->arg4 = (void *)(long) -1;
        return;
    }

    int result = MboxCondReceive(mboxID, msgPtr, msgSize);

    //case whnen mailbox is invalid, return -1
    if(result == -1){
        args->arg4 = (void *)(long) -1;
        return;
    }

    //case whnen mailbox is not available
    if(result == 1){
        args->arg4 = (void *)(long) 1;
        return;
    }

    args->arg4 = (void *)(long) 0;
}
