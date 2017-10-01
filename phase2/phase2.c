/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   Alex Yee & Katie Pan
   ------------------------------------------------------------------------ */

#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include "message.h"
#include <string.h>

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);
void disableInterrupts(void);
void enableInterrupts(void);
void check_kernel_mode(char *);
void freeSlot(slotPtr);
int receive(mailbox *, void *, int);
int send(mailbox *, void *, int);
int enqueue(mailbox *);
int dequeue(mailbox *);
int MboxRelease(int);
int check_io(void);
void nullsys(int, int);
void clockHandler2(int, int);
void diskHandler(int, int);
void termHandler(int, int);
void syscallHandler(int, int);

/* -------------------------- Globals ------------------------------------- */
int debugflag2 = 0;
// the mail boxes
mailbox MailBoxTable[MAXMBOX];
int currentMboxId = 0;
mailSlot MailSlotTable[MAXSLOTS];
int clockHandlerCycle = 0;
// also need array of mail slots, array of function ptrs to system call
// handlers, ...
void (*SyscallHandlers[MAXSYSCALLS])(int dev, int unit);
// the process table
//procStruct ProcTable[MAXPROC];
/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): at beginning\n");

    check_kernel_mode("start1");

    // Disable interrupts
    disableInterrupts();

    // Initialize the mail box table, slots, & other data structures.
    // Initialize USLOSS_IntVec and system call handlers,
    // allocate mailboxes for interrupt handlers.  Etc...
	USLOSS_IntVec[USLOSS_CLOCK_INT] = (void (*) (int, void *)) clockHandler2;
    USLOSS_IntVec[USLOSS_DISK_INT] = (void (*) (int, void *)) diskHandler;
	USLOSS_IntVec[USLOSS_TERM_INT] = (void (*) (int, void *)) termHandler;
	USLOSS_IntVec[USLOSS_ILLEGAL_INT] = (void (*) (int, void *)) nullsys;
	USLOSS_IntVec[USLOSS_SYSCALL_INT] = (void (*) (int, void *)) syscallHandler;
	// initialize the device i/o mailboxes to be 0 slot mailboxes
	int i;
	for (i = 0; i < 7; i++) {
		MboxCreate(0, sizeof(int));
	}

	// initalize the syscall handlers
	for (i = 0; i < MAXSYSCALLS; i++) {
		SyscallHandlers[i] = (void (*) (int, int)) nullsys;
	}

    enableInterrupts();

    // Create a process for start2, then block on a join until start2 quits
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): fork'ing start2 process\n");
	int kid_pid = fork1("start2", start2, 0, 4 * USLOSS_MIN_STACK, 1);
   	int status;
	if ( join(&status) != kid_pid ) {
        USLOSS_Console("start2(): join returned something other than ");
        USLOSS_Console("start2's pid\n");
    }

    return 0;
} /* start1 */


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array.
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
    check_kernel_mode("MboxCreate");
    int i = 0;
	while (MailBoxTable[currentMboxId % MAXMBOX].isUsed) {
		currentMboxId++;
	    i++;
		if (i >= MAXMBOX) {
	        return -1;
    	}
	}
	disableInterrupts();
	MailBoxTable[currentMboxId % MAXMBOX].isUsed = 1;
	MailBoxTable[currentMboxId % MAXMBOX].mboxID = currentMboxId;
    MailBoxTable[currentMboxId % MAXMBOX].numSlots = slots;
    MailBoxTable[currentMboxId % MAXMBOX].maxLength = slot_size;

	for(i = 0; i < MAXSLOTS; i++){
		//-1 will be our null. meaning that pid at that slot will be "empty"
		MailBoxTable[currentMboxId % MAXMBOX].sendQueuePids[i] = -1;
	}
    currentMboxId++;
	enableInterrupts();
    return currentMboxId - 1;

} /* MboxCreate */


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
    check_kernel_mode("MboxSend");
	if (!MailBoxTable[mbox_id % MAXMBOX].isUsed) {
        return -1;
    }
	disableInterrupts();
    mailbox *currentMbox = &MailBoxTable[mbox_id % MAXMBOX];
    // zero-slot mailbox case
    if (currentMbox->numSlots == 0) {
        int waitingPid = -1;
        // case 1: recv hasn't been called on the 0-slot mailbox yet, enqueue a
        // sendqueue message then block me
        if ((waitingPid = dequeue(currentMbox)) == -1) {
            enqueue(currentMbox);
			enableInterrupts();
            blockMe(11);
			disableInterrupts();
        // case 2: recv has been called on this 0-slot mailbox,
        } else {
            unblockProc(waitingPid);
        }
        enableInterrupts();
		if(!currentMbox->isUsed){
            return -3;
        }
        if(isZapped()){
    		return -3;
    	}
        return 0;
    }
    if (msg_size > currentMbox->maxLength) {
		enableInterrupts();
        return -1;
    }
    if (msg_ptr == NULL) {
		enableInterrupts();
        return -1;
    }

    // find a childslot in the mailbox to insert,
    // if none are available, send block and continue trying
	while (send(currentMbox, msg_ptr, msg_size) != 0) {
		enqueue(currentMbox);
		enableInterrupts();
		blockMe(11);
		if(!currentMbox->isUsed){
			return -3;
		}
	}
	enableInterrupts();
    //if it is zapped
	if(isZapped()){
		return -3;
	}
	return 0;

} /* MboxSend */


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
    check_kernel_mode("MboxReceive");
    if (!MailBoxTable[mbox_id % MAXMBOX].isUsed) {
        return -1;
    }
	disableInterrupts();
    mailbox *currentMbox = &MailBoxTable[mbox_id % MAXMBOX];
    // zero-slot mailbox case
    if (currentMbox->numSlots == 0) {
        // case 1: sender is blocked, receive the message and unblock sender
        // check if there is a message in sendqueue there
        int waitingPid = -1;
		if ((waitingPid = dequeue(currentMbox)) != -1) {
			unblockProc(waitingPid);
    		enableInterrupts();
	        if (isZapped()) return -3;
            return 0;
        // case 2: no message, reserve block receiver
		} else {
            enqueue(currentMbox);
			enableInterrupts();
            blockMe(12);
            if (isZapped()) return -3;
            return 0;
        }
    }

    if (msg_ptr == NULL) {
		enableInterrupts();
        return -1;
    }

    if (currentMbox->childSlots[0] != NULL && currentMbox->childSlots[0]->status == FULL) {
        int retval = receive(currentMbox, msg_ptr, msg_size);
        disableInterrupts(); // receive turns interrupts back on, disable
		if (retval == -1) {
         	enableInterrupts();
		    return -1;
		}
		int waitingPid = -1;
		if ((waitingPid = dequeue(currentMbox)) != -1) {
			unblockProc(waitingPid);
		}
		enableInterrupts();
		return retval;
    } else {
        // block receiver if there are no messages in this mailbox
		// reserve the mailbox slot
		// find a slot in the slot table to insert
        int j = 0;
        while (j < MAXSLOTS && (MailSlotTable[j].status == FULL || MailSlotTable[j].status == RSVD))
            j++;
        if (j == MAXSLOTS) {
			USLOSS_Halt(1);
        }
        // change its status to RSVD (reserved)
		currentMbox->childSlots[0] = &MailSlotTable[j];
        currentMbox->childSlots[0]->status = RSVD;
		currentMbox->childSlots[0]->reservedPid = getpid();
		enableInterrupts();
		blockMe(11);
		disableInterrupts();
		if(!currentMbox->isUsed){
			enableInterrupts();
			return -3;
		}
    }
	int status = receive(currentMbox, msg_ptr, msg_size);
    if (isZapped()) return -3;
    return status;

} /* MboxReceive */

/* ------------------------------------------------------------------------
   Name - MboxRelease
   Purpose - Resets the field of a mailbox and unblocks all processes blocked
              on this mailbox.
   Parameters - mailbox id
   Returns - -3 if zapped, 0 on success
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxRelease(int mailboxID)
{
    check_kernel_mode("MboxRelease");
	//if mailboxId isn't being used
	if(!MailBoxTable[mailboxID % MAXMBOX].isUsed)
		return -1;
	disableInterrupts();
	//unblock the procs in the rsvd slots
	int i = 0;
	mailbox *currentMbox = &MailBoxTable[mailboxID % MAXMBOX];
	currentMbox->mboxID = 0;
    currentMbox->isUsed = 0; // this lets a blocked process know it's released.
	for(i = 0; i < currentMbox->numSlots; i++){
		if(currentMbox->childSlots[i] != NULL && currentMbox->childSlots[i]->status == RSVD){
			unblockProc(currentMbox->childSlots[i]->reservedPid);
		}
	}

	//unblock the procs in sendQueues
	for(i = 0; i < MAXSLOTS; i++){
        if(currentMbox->sendQueuePids[i] != -1){
			unblockProc(currentMbox->sendQueuePids[i]);
        }
    }

	//reset rest of fields
	for(i = 0; i < currentMbox->numSlots; i++){
    	freeSlot(currentMbox->childSlots[i]);
		currentMbox->childSlots[i] = NULL;
	}

    //unblock the procs in sendQueues
    for(i = 0; i < MAXSLOTS; i++){
   		currentMbox->sendQueuePids[i] = -1;
	}

	currentMbox->numSlots = 0;
	currentMbox->maxLength = 0;
	enableInterrupts();
	//if it is zapped
	if(isZapped()){
		return -3;
	}

	return 0;

} /*MboxRelease*/

/*
 * MboxCondSend() --
 */
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size)
{
    check_kernel_mode("MboxSend");
    if (!MailBoxTable[mbox_id % MAXMBOX].isUsed) {
        return -1;
    }
    if (msg_size > MailBoxTable[mbox_id % MAXMBOX].maxLength) {
        return -1;
    }
	disableInterrupts();
    mailbox *currentMbox = &MailBoxTable[mbox_id % MAXMBOX];
    //if mailbox is full, message not sent, or no slots available in the system
    if (send(currentMbox, msg_ptr, msg_size) != 0) {
		return -2;
	}
	enableInterrupts();
    if(isZapped()){
        return -3;
    }

	return 0;

} /* MboxCondSend */

/*
 * MboxCondReceive() --
 */
int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_size)
{
    check_kernel_mode("MboxCondReceive");
    if (!MailBoxTable[mbox_id % MAXMBOX].isUsed) {
        return -1;
    }
    if (msg_ptr == NULL) {
        return -1;
    }
	disableInterrupts();
    mailbox *currentMbox = &MailBoxTable[mbox_id % MAXMBOX];
    if (currentMbox->childSlots[0] != NULL && currentMbox->childSlots[0]->status == FULL) {
        int retval = receive(currentMbox, msg_ptr, msg_size);
        int waitingPid = -1;
        if ((waitingPid = dequeue(currentMbox)) != -1) {
            unblockProc(waitingPid);
        }
        if (isZapped()) return -3;
        return retval;
    } else {
        if (isZapped()) return -3;
        return -2;
    }
} /* MboxCondReceive */

/*
 * waitDevice -- does a recieve operation on the mailbox with the given type and unit
 * 					of the device. 1 clock, then 2 disks, then 4 terminals
 */
int waitDevice(int type, int unit, int *status)
{
	check_kernel_mode("waitDevice");
	disableInterrupts();
	int mailboxId = 0; // default to clock
	if (type == USLOSS_DISK_DEV) {
		mailboxId = 1;
    } else if (type == USLOSS_TERM_DEV) {
		mailboxId = 3;
    }
	MboxCondReceive(mailboxId + unit, status, sizeof(int));
	if (isZapped())
		return -1;
	return 0;

}

/* an error method to handle invalid syscalls */
void nullsys(int dev, int unit)
{
    USLOSS_Console("nullsys(): Invalid syscall. Halting...\n");
    USLOSS_Halt(1);
} /* nullsys */

/*
 * The clock interrupt handler for our OS baby
 * conditionally sends to the clock i/o mailbox every 5th clock interrupt
 */
void clockHandler2(int dev, int unit)
{

    if (DEBUG2 && debugflag2)
      USLOSS_Console("clockHandler2(): called\n");
    check_kernel_mode("clockHandler");
	if (dev != USLOSS_CLOCK_DEV && unit != 0) {
		USLOSS_Console("clockHandler2(): incorrect device and/or unit number\n");
	}
    timeSlice();
	clockHandlerCycle++;
	if (clockHandlerCycle % 5 == 0) {
		int completionStatus;
		USLOSS_DeviceInput(dev, unit, &completionStatus);
		MboxCondSend(USLOSS_CLOCK_DEV, &completionStatus, sizeof(int));
	}

} /* clockHandler */


void diskHandler(int dev, int unit)
{

    if (DEBUG2 && debugflag2)
        USLOSS_Console("diskHandler(): called\n");
    check_kernel_mode("diskHandler");
    // error check: is device the correct device?
    // is unit number in correct range? 0-6
    // read device's status register using USLOSS_DeviceInput
    // condsend contents of status register to mailbox
	if (dev != USLOSS_DISK_DEV || unit < 0 || unit >= 2) {
        USLOSS_Console("diskHandler(): incorrect device and/or unit number\n");
    }

	int completionStatus;
    USLOSS_DeviceInput(dev, unit, &completionStatus);
    MboxCondSend(1 + unit, &completionStatus, sizeof(int));

} /* diskHandler */


void termHandler(int dev, int unit)
{

    if (DEBUG2 && debugflag2)
       USLOSS_Console("termHandler(): called\n");
    check_kernel_mode("termHandler");
	if (dev != USLOSS_DISK_DEV || unit < 0 || unit >= 4) {
        USLOSS_Console("termHandler(): incorrect device and/or unit number\n");
    }

    int completionStatus;
    USLOSS_DeviceInput(dev, unit, &completionStatus);
    MboxCondSend(3 + unit, &completionStatus, sizeof(int));
	//printf("termhandler: %d completion status\n", completionStatus);
} /* termHandler */


void syscallHandler(int dev, int unit)
{

   if (DEBUG2 && debugflag2)
      USLOSS_Console("syscallHandler(): called\n");
	// next phase stuff
	// error check
	if (dev != USLOSS_SYSCALL_INT || unit < 0 || unit >= MAXSYSCALLS) {
        USLOSS_Console("termHandler(): incorrect device and/or unit number\n");
    }	
	// call nullsys for now (initialized in start2)
	void (*syscallFunc) (int dev, int unit) = SyscallHandlers[unit];
	syscallFunc(dev, unit);
} /* syscallHandler */


/*
 *
 */
int check_io()
{
	int i = 0;
	for (i = 0; i < 7; i++) {
		if (MailBoxTable[i].sendQueuePids[0] != -1)
			return 1;
	}
	return 0;
}

/*
 * Enables the interrupts and also turns kernel mode on
 */
void enableInterrupts()
{
    // turn the interrupts ON iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS
    unsigned int previousPSRValue = USLOSS_PsrGet();

    if (!(previousPSRValue & 1)) {
        USLOSS_Console("enableInterrupts(): Not in kernel mode :-(\n");
        USLOSS_Halt(1);
    }

    unsigned int newPSRValue = ((previousPSRValue << 2) & 12) | 3;
    // left shifting by 2 to shift the previous psr bits to the left two bits
    // then logical and by 12 to mask all the other bits except the previous mode bits
    // then a logical or by 3 to set the first two bits to 11
    if (USLOSS_PsrSet(newPSRValue) == USLOSS_ERR_INVALID_PSR)
        USLOSS_Console("ERROR: Invalid PSR value set! was: %u\n", newPSRValue);
} /* enableInterrupts */

/*
 * Disables the interrupts and also turns kernel mode on
 */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS
    unsigned int previousPSRValue = USLOSS_PsrGet();

    if (!(previousPSRValue & 1)) {
        USLOSS_Console("disableInterrupts(): Not in kernel mode :-(\n");
        USLOSS_Halt(1);
    }

    unsigned int newPSRValue = ((previousPSRValue << 2) & 12) | 1;
    if (USLOSS_PsrSet(newPSRValue) == USLOSS_ERR_INVALID_PSR)
        USLOSS_Console("ERROR: Invalid PSR value set! was: %u\n", newPSRValue);
} /* disableInterrupts */
/*
 * checks if the OS is currently in kernel mode; halts if it isn't.
 */
void check_kernel_mode(char * funcName)
{
    if (!(USLOSS_PsrGet() & 1)) {
        USLOSS_Console(
             "%s(): called while in user mode, by process %d. Halting...\n", funcName, 69); // need to change this to current pid?
         USLOSS_Halt(1);
    }
}

/*
 * freeSlot - resets the fields of the mailbox slot
 */
 void freeSlot(slotPtr slot)
 {
     slot->mboxID = 0;
     slot->status = EMPTY;
     int i;
     slot->msgSize = 0;
     for (i = 0; i < MAX_MESSAGE; i++)
        slot->data[i] = '\0';
 }

/*
 * receive -- just a function that tries to receive a message at the mailbox currentMbox's
 * 				child slot at 0, then returns the size of the actual message.
 *              also returns -1 if the buffer is too small.
 */
int receive(mailbox *currentMbox, void *msg_ptr, int msg_size)
{
	disableInterrupts();
    int size = currentMbox->childSlots[0]->msgSize;
    if (size > msg_size) {
        enableInterrupts();
		return -1;
    }
    memcpy(msg_ptr, currentMbox->childSlots[0]->data, msg_size);
    freeSlot(currentMbox->childSlots[0]);
    int i = 0;
    while (i < currentMbox->numSlots - 1 && currentMbox->childSlots[i] != NULL) {
    	currentMbox->childSlots[i] = currentMbox->childSlots[i+1];
        i++;
    }
	currentMbox->childSlots[i] = NULL;
    enableInterrupts();
	return size;
}
/*
 * send -- just a function that tries to send a message to the mailbox currentMbox
 * 			returns 0 on success, 1 if no slots are available, halts if global mail
 * 			slot table is full.
 *
 */
int send(mailbox *currentMbox, void *msg_ptr, int msg_size)
{
	disableInterrupts();
	int i;
    for (i = 0; i < currentMbox->numSlots; i++) {
        if (currentMbox->childSlots[i] == NULL) {
            // find a slot in the slot table to insert
            int j=0;
            while (j < MAXSLOTS && MailSlotTable[j].status == FULL)
                j++;
            if (j == MAXSLOTS) {
            	return -2;
			}
            currentMbox->childSlots[i] = &MailSlotTable[j];
            currentMbox->childSlots[i]->status = FULL;
            currentMbox->childSlots[i]->msgSize = msg_size;
            memcpy(currentMbox->childSlots[i]->data, msg_ptr, msg_size);
            currentMbox->childSlots[i]->mboxID = currentMbox->mboxID;;
			enableInterrupts();
            return 0;
        } else if (currentMbox->childSlots[i]->status == RSVD) {
            currentMbox->childSlots[i]->status = FULL;
            currentMbox->childSlots[i]->msgSize = msg_size;
            memcpy(currentMbox->childSlots[i]->data, msg_ptr, msg_size);
            unblockProc(currentMbox->childSlots[i]->reservedPid);
            enableInterrupts();
			return 0;
        }
    }
	enableInterrupts();
	return 1;
}

/*
 * enqueue -- for sendqueue at mailbox currentMbox
 * returns 0 on success, -1 if full
 */
int enqueue(mailbox *currentMbox)
{
	int i = 0;
	while (i < MAXSLOTS && currentMbox->sendQueuePids[i] != -1) {
		i++;
		if (i == MAXSLOTS) {
			return -1; //error case if full
		}
	}
	currentMbox->sendQueuePids[i] = getpid();
	return 0;
}

/*
 * dequeue -- for sendqueue at mailbox currentMbox
 * returns pid if successful, -1 if unsuccessful
 */
int dequeue(mailbox *currentMbox)
{
	int i = 0;
	int retPid = currentMbox->sendQueuePids[i];

	while (i+1 < MAXSLOTS && currentMbox->sendQueuePids[i+1] != -1) {
		currentMbox->sendQueuePids[i] = currentMbox->sendQueuePids[i+1];
		i++;
	}

	return retPid;
}
