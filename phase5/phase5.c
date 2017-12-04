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
#include <providedPrototypes.h>

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
int *pagerPids;
int numPagers = 0;
int faultMboxID;
int runningSem;
Frame *frameTable;

// disk variables
int currentBlock = 0;
int TRACKS;
int SECTORS;
int SECTORS_PER_PAGE;
static void FaultHandler(int type, void * offset);
static int Pager(char *buf);
void * vmInitReal(int, int, int, int, int *);
static void vmInit(USLOSS_Sysargs *USLOSS_SysargsPtr);
static void vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr);
void vmDestroyReal(void);
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
	else if (((int) (long) args->arg4) >= MAXPAGERS) {
        USLOSS_Console("vmInit(): Too many pagers!\n");
        args->arg4 = (void *) (long) -1;
        return;
    }
    else if (vmInitFlag) {
        args->arg4 = (void *) (long) -2;
		return;
    }
	else {
    	int status = (int) (long) (vmInitReal((int) (long) args->arg1, (int) (long) args->arg2,
                    (int) (long) args->arg3, (int) (long) args->arg4, &firstByteAddy));
    	args->arg1 = (void *) (long) firstByteAddy;
    	args->arg4 = (void *) (long) 0;
	}
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
	vmDestroyReal();
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
   int status, numPages, i;

   CheckMode();
   status = USLOSS_MmuInit(mappings, pages, frames, USLOSS_MMU_MODE_TLB);
   if (status != USLOSS_MMU_OK) {
      USLOSS_Console("vmInitReal: couldn't initialize MMU, status %d\n", status);
      abort();
   }
   USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;
   frameTable = malloc(frames * sizeof(Frame));
   vmInitFlag = 1;
	for (i = 0; i < frames; i++) {
       frameTable[i].status = EMPTY;
       frameTable[i].pid = -1;
       frameTable[i].page = -1;
   }
   /*
    * Initialize page tables.
    */
	// this is done in p1_fork
   /*
    * Create the fault mailbox.
    */
	faultMboxID = MboxCreate(0, sizeof(int));
   /*
    * Fork the pagers.
    */
	runningSem = MboxCreate(0, sizeof(int));
	int dummyMsg;
	pagerPids = malloc(pagers * sizeof(int));
	for (i = 0; i < pagers; i++) {
        char arg[1], procName[6];
        sprintf(arg, "%d", i);
        sprintf(procName, "Pager%d", i);
		// TODO how much stack space is required for pager??
		status = fork1(procName, Pager, arg, 2 * USLOSS_MIN_STACK, PAGER_PRIORITY);
		pagerPids[i] = status;
		if (isDebug) {
			USLOSS_Console("Forked pager %d, pid = %d\n", i, status);
		}
		status = MboxReceive(runningSem, &dummyMsg, sizeof(int));
    }
	/*
    * Zero out, then initialize, the vmStats structure
    */
    int sectorSize, numSectors, numTracks;
    status = diskSizeReal(1, &sectorSize, &numSectors, &numTracks);
    if (isDebug) {
        USLOSS_Console("Disk info:\n");
        USLOSS_Console("\tSector size: %d\n", sectorSize);
        USLOSS_Console("\t# of sectors per track: %d\n", numSectors);
        USLOSS_Console("\t# of tracks: %d\n", numTracks);
		USLOSS_Console("\tPage Size: %d\n", USLOSS_MmuPageSize());

    }
    TRACKS = numTracks;
    SECTORS = numSectors;
    SECTORS_PER_PAGE = (int) USLOSS_MmuPageSize() / sectorSize;
    memset((char *) &vmStats, 0, sizeof(VmStats));
    vmStats.pages = pages;
    vmStats.frames = frames;
    vmStats.diskBlocks = (int) ((sectorSize * numSectors * numTracks) / USLOSS_MmuPageSize());
    vmStats.freeFrames = frames;
    vmStats.freeDiskBlocks = vmStats.diskBlocks;
	numPagers = pagers;
    *firstByteAddy = (int) (long) USLOSS_MmuRegion(&numPages);
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
    if (isDebug) {
        USLOSS_Console("vmDestroyReal() called\n");
    }
    CheckMode();
    int i, status;
	int result = USLOSS_MmuDone();
    if (result != USLOSS_MMU_OK) {
        USLOSS_Console("vmDestroyReal(): TODO %d\n", result);
        abort();
    }
	free(frameTable);
    // find all procs that use the vmRegion, free their page tables
	for (i = 0; i < MAXPROC; i++) {
		PTE *currentPT = processes[i].pageTable;
		if (currentPT != NULL) {
			free(currentPT);
		}
	}

	/*
     * Kill the pagers here.
     */
	int quitMsg = -1;

	for (i = 0; i < numPagers; i++) {
        if (isDebug)
            USLOSS_Console("Quitting pager %d\n", i);
		//mbox send to pagers to unblock them
        status = MboxSend(faultMboxID, &quitMsg, sizeof(int));
		zap(pagerPids[i]);
	}
	free(pagerPids);
    /*
     * Print vm statistics.
     */
    PrintStats();
    /* and so on... */
    vmInitFlag = 0;
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
        USLOSS_Console("FaultHandler() called\n");
    int cause, result;

    assert(type == USLOSS_MMU_INT);
    cause = USLOSS_MmuGetCause();
    assert(cause == USLOSS_MMU_FAULT);
    vmStats.faults++;
    printf("vmStats.fault increased %d\n", vmStats.faults);
	vmStats.new++;
    /*
     * Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
     * reply.
     */
    faults[getpid() % MAXPROC].pid = getpid();
    faults[getpid() % MAXPROC].addr = offset;
    int tempMbox = MboxCreate(1, sizeof(int));
    faults[getpid() % MAXPROC].replyMbox = tempMbox;
    int pidMsg = getpid();

    MboxSend(faultMboxID, &pidMsg, sizeof(int));
    MboxReceive(tempMbox, (void *) &pidMsg, sizeof(int));
    int pageToMap = (int) ((long) offset / USLOSS_MmuPageSize());
    processes[getpid() % MAXPROC].pageTable[pageToMap].frame = pidMsg;
    processes[getpid() % MAXPROC].pageTable[pageToMap].state = INCORE;
    processes[getpid() % MAXPROC].pageTable[pageToMap].diskBlock = -1;
    frameTable[pidMsg].pid = getpid();
    frameTable[pidMsg].page = pageToMap;
    result = USLOSS_MmuMap(TAG, pageToMap, pidMsg, USLOSS_MMU_PROT_RW);
    if (isDebug) {
        USLOSS_Console("Mapping %d to frame %d\n", pageToMap, pidMsg);
    }
    //mbox_receive_real(mboxID, 0, 0);
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
	int result, mappedFlag;
	void *msgPtr = malloc(sizeof(int));
	MboxSend(runningSem, msgPtr, sizeof(int));
    while(1) {
        mappedFlag = 0;
		/* Wait for fault to occur (receive from mailbox) */
		MboxReceive(faultMboxID, msgPtr, sizeof(int));
		if (isDebug) {
			USLOSS_Console("Pager%s received from faultMbox\n", buf);
		}
		// check to quit
		if (*((int *) msgPtr) == -1) {
			if (isDebug) {
            	USLOSS_Console("Pager%s quitting...\n", buf);
        	}
			break;
		}
        int faultedPid = *((int *) msgPtr);
        PTE *currentPT = processes[faultedPid % MAXPROC].pageTable;
        /* Look for free frame */
        int i;
        for (i = 0; i < vmStats.frames; i++) {
            if (frameTable[i].status == EMPTY) {
                //currentPT[faults[faultedPid % MAXPROC]].frame = i;
                // set the frame, state and map later, for now just unblock
                USLOSS_MmuMap(TAG, 0, i, USLOSS_MMU_PROT_RW);
                void *region = USLOSS_MmuRegion(&result);
                memset(region, 0, USLOSS_MmuPageSize());
                USLOSS_MmuUnmap(TAG, 0);
                frameTable[i].status = IN_MEM;
                MboxSend(faults[faultedPid % MAXPROC].replyMbox,
                        &i, sizeof(int));
                processes[faultedPid % MAXPROC].lastRef = i + 1 % vmStats.frames;
                mappedFlag = 1;
				break;
            }
        }
        /* if page was already mapped to an empty frame, then skip
            doing other stuff and wait for next fault*/
        if (mappedFlag)
            continue;
        if (isDebug) {
            USLOSS_Console("Checking for unreferenced and clean frames...\n");
        }
        /* If there isn't a free frame then use clock algorithm to
         * replace a page (perhaps write to disk) */
        int access, lastReferenced = processes[faultedPid % MAXPROC].lastRef;
        // first check for unreferenced and clean (00)
        for (i = 0; i < vmStats.frames; i++) {
            int frameIndex = (i + lastReferenced) % vmStats.frames;
            result = USLOSS_MmuGetAccess(frameIndex, &access);
            if (access == 0) {
                // don't have to write to disk
                USLOSS_MmuMap(TAG, 0, frameIndex, USLOSS_MMU_PROT_RW);
                void *region = USLOSS_MmuRegion(&result);
                memset(region, 0, USLOSS_MmuPageSize());
                USLOSS_MmuUnmap(TAG, 0);
                frameTable[frameIndex].status = IN_MEM;
                MboxSend(faults[faultedPid % MAXPROC].replyMbox,
                        &frameIndex, sizeof(int));
                mappedFlag = 1;
				break;
            }
        }
        if (mappedFlag)
            continue;

            if (isDebug) {
            USLOSS_Console("Checking for unreferenced and dirty frames...\n");
            }
        /* now check for unreferenced and dirty, set the access
           bits to unreferenced along the way? */
        for (i = 0; i < vmStats.frames; i++) {
            int frameIndex = (i + lastReferenced) % vmStats.frames;
            result = USLOSS_MmuGetAccess(frameIndex, &access);
            if (access == 2) {
                // write to disk. have to coordinate with diskDriver

                USLOSS_MmuMap(TAG, 0, frameIndex, USLOSS_MMU_PROT_RW);
                char buf[USLOSS_MmuPageSize()];
                void *region = USLOSS_MmuRegion(&result);
                memcpy(buf, region, USLOSS_MmuPageSize());
                diskWriteReal(1, (int) (currentBlock / SECTORS),
                        currentBlock % SECTORS, SECTORS_PER_PAGE, buf);
                // the next line is kinda disgusting, but it's setting the disk
                // block of the page that is being saved to disk to currentBlock
                processes[frameTable[frameIndex].pid % MAXPROC]
                    .pageTable[frameTable[frameIndex].page]
                    .diskBlock = currentBlock;
                currentBlock += SECTORS_PER_PAGE;
                memset(region, 0, USLOSS_MmuPageSize());
                USLOSS_MmuUnmap(TAG, 0);
                frameTable[frameIndex].status = IN_MEM;
                MboxSend(faults[faultedPid % MAXPROC].replyMbox,
                        &frameIndex, sizeof(int));
                mappedFlag = 1;
    		    break;
            }

        }
        if (mappedFlag)
           continue;
        /* Load page into frame from disk, if necessary */
        /* Unblock waiting (faulting) process */
    }
	free(msgPtr);
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
