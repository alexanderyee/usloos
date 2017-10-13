/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   Alex Yee & Katie Pan
   ------------------------------------------------------------------------ */
#include <stdio.h>
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
void nullsys(systemArgs *);
void clockHandler2(int, void *);
void diskHandler(int, void *);
void termHandler(int, void *);
void syscallHandler(int, void *);

/* -------------------------- Globals ------------------------------------- */
int debugflag2 = 0;
// the mail boxes
mailbox MailBoxTable[MAXMBOX];
int currentMboxId = 0;
mailSlot MailSlotTable[MAXSLOTS];
int clockHandlerCycle = 0;
void (*SyscallHandlers[MAXSYSCALLS])(systemArgs *);
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
		int retval = MboxCreate(0, sizeof(int));
		if (retval < 0 || retval > 7)
			USLOSS_Console("Error with creating I/O mailboxes\n");
	}

	// initalize the syscall handlers
	for (i = 0; i < MAXSYSCALLS; i++) {
		SyscallHandlers[i] = (void (*) (systemArgs *)) nullsys;
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
int MboxCreate(int slots, int msg_size)
{
    check_kernel_mode("MboxCreate");
    int i = 0;
    if (msg_size > MAX_MESSAGE || msg_size < 0)
        return -1;
    else if (slots < 0)
        return -1;
	while (MailBoxTable[currentMboxId % MAXMBOX].isUsed) {
		currentMboxId++;
	    i++;
		if (i >= MAXMBOX) {
	        return -1;
    	}
	}
	disableInterrupts();
	MailBoxTable[currentMboxId % MAXMBOX].isUsed = 1;
	MailBoxTable[currentMboxId % MAXMBOX].mboxID = currentMboxId % MAXMBOX;
    MailBoxTable[currentMboxId % MAXMBOX].numSlots = slots;
    MailBoxTable[currentMboxId % MAXMBOX].maxLength = msg_size;

	enableInterrupts();
    return MailBoxTable[currentMboxId++ % MAXMBOX].mboxID;

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
	disableInterrupts();
    mailbox *currentMbox = &MailBoxTable[mbox_id % MAXMBOX];

    // invalid mailbox case
	if (!(currentMbox->isUsed)) {
        return -1;
    }
    // message size being too large case
    if (msg_size > currentMbox->maxLength) {
		enableInterrupts();
        return -1;
    }

    // find a childslot in the mailbox to insert,
    // if none are available, reserve a slot for send and send block
    // this also accounts for 0-slot mbox case (block sender until receive)
    // also need to unblock if it is a 0-slot RECV_RSVD
    int retval = send(currentMbox, msg_ptr, msg_size);
	if (retval == -2 || retval == 1) {
	    // no slots available in mbox, reserve a slot
        int i = 0;
        // iterate until we find an empty slot. any RECV_RSVD slots would've
        // been successfully sent to (and send should return 0 in that case)
        while (currentMbox->childSlots[i] != NULL && currentMbox->childSlots[i]->status != EMPTY) {
            // but, if it is a 0-slot mailbox and does find a RECV_RSVD, unblock
            if (currentMbox->childSlots[i]->status == RECV_RSVD) {
                currentMbox->childSlots[i]->status = FULL;
                currentMbox->childSlots[i]->msgSize = msg_size;
                memcpy(currentMbox->childSlots[i]->data, msg_ptr, msg_size);
                enableInterrupts();
                unblockProc(currentMbox->childSlots[i]->reservedPid);
                if (currentMbox->childSlots[i] != NULL && currentMbox->childSlots[i]->msgSize == -1)
                    return -1;
                if(isZapped()){
            		return -3;
            	}
            	return 0;
            }
            i++;
            // no slots in universal slot table case (all taken up by this mbox)
            if (i == MAXSLOTS) {
                USLOSS_Halt(1);
            }
        }
        // index i should be at an empty slot, copy message and reserve the slot.
        // find an empty slot in the universal slot table to insert
        int j = 0;
        while (j < MAXSLOTS && MailSlotTable[j].status != EMPTY)
            j++;
        if (j == MAXSLOTS) {
            USLOSS_Halt(1);
        }
        currentMbox->childSlots[i] = &MailSlotTable[j];
        currentMbox->childSlots[i]->status = SEND_RSVD;
        currentMbox->childSlots[i]->reservedPid = getpid();
        currentMbox->childSlots[i]->msgSize = msg_size;
        memcpy(currentMbox->childSlots[i]->data, msg_ptr, msg_size);
        currentMbox->childSlots[i]->mboxID = currentMbox->mboxID;
		enableInterrupts();
		blockMe(11);
		if(!currentMbox->isUsed){
			return -3;
		}
        if (currentMbox->childSlots[i] != NULL && currentMbox->childSlots[i]->msgSize == -1)
            return -1;
	}
    else if (retval == -1)
        return -1;
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
    disableInterrupts();
    mailbox *currentMbox = &MailBoxTable[mbox_id % MAXMBOX];
    // check if invalid mailbox
    if (!(currentMbox->isUsed)) {
		enableInterrupts();
        return -1;
    }
    // we will check msg_size later on if there is a sent message...
    int retval = receive(currentMbox, msg_ptr, msg_size);
    // return 0 on success, -2 if no slots left
    if (retval == -2) {
        // need to reserve a slot
        int i = 0, j = 0;
        while (i < MAXSLOTS && currentMbox->childSlots[i] != NULL)
            i++;
        while (j < MAXSLOTS && MailSlotTable[j].status != EMPTY)
            j++;
        if (j == MAXSLOTS) {
            USLOSS_Halt(1);
        }
        currentMbox->childSlots[i] = &MailSlotTable[j];
        currentMbox->childSlots[i]->status = RECV_RSVD;
        currentMbox->childSlots[i]->reservedPid = getpid();
        currentMbox->childSlots[i]->mboxID = currentMbox->mboxID;
		enableInterrupts();
		blockMe(11);
		if(!currentMbox->isUsed){
			return -3;
		}
        // we're unblocked now, try to copy from the slot ptr

        disableInterrupts();
        int size = MailSlotTable[j].msgSize;
        if (size > msg_size) {
            MailSlotTable[j].msgSize = -1;
            enableInterrupts();
	        return -1;
        }
        retval = size;
        memcpy(msg_ptr, MailSlotTable[j].data, size);
        // child slots might have shifted, need to account for this...
        // for 0 slots, use MAXSLOTS to check all slots.
        int numSlots = currentMbox->numSlots;
        if (!numSlots)
            numSlots = MAXSLOTS;

        for (i = 0; i < numSlots; i++) {
            if (currentMbox->childSlots[i] != NULL && currentMbox->childSlots[i]->reservedPid == getpid()) {
                // found slot, let's shift everything to the right left by 1.
                freeSlot(&MailSlotTable[j]);
                while (i < MAXSLOTS - 1 && currentMbox->childSlots[i] != NULL) {
                	currentMbox->childSlots[i] = currentMbox->childSlots[i+1];
                    i++;
                }
                break;
            }
        }
    }
    enableInterrupts();
    return retval;
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
	for(i = 0; i < MAXSLOTS; i++) {
		if(currentMbox->childSlots[i] != NULL && (currentMbox->childSlots[i]->status == RECV_RSVD || currentMbox->childSlots[i]->status == SEND_RSVD)) {
			unblockProc(currentMbox->childSlots[i]->reservedPid);
		}
	}

	//reset rest of fields
	for(i = 0; i < currentMbox->numSlots; i++) {
        if (currentMbox->childSlots[i] == NULL) break;
        freeSlot(currentMbox->childSlots[i]);
        currentMbox->childSlots[i] = NULL;
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
    check_kernel_mode("MboxCondSend");
    if (!MailBoxTable[mbox_id % MAXMBOX].isUsed) {
        return -1;
    }
    if (msg_size > MailBoxTable[mbox_id % MAXMBOX].maxLength) {
        return -1;
    }
    // if (msg_ptr == NULL) {
    //     return -1;
    // }
	disableInterrupts();
    mailbox *currentMbox = &MailBoxTable[mbox_id % MAXMBOX];
    //if mailbox is full, message not sent, or no slots available in the system
    int retval = send(currentMbox, msg_ptr, msg_size);
	if (retval == -2 || retval == 1) {
        // no slots available in mbox, reserve a slot
        int i = 0;
        // iterate until we find an empty slot. any RECV_RSVD slots would've
        // been successfully sent to (and send should return 0 in that case)
        while (currentMbox->childSlots[i] != NULL && currentMbox->childSlots[i]->status != EMPTY) {
            // but, if it is a 0-slot mailbox and does find a RECV_RSVD, unblock
            if (currentMbox->childSlots[i]->status == RECV_RSVD) {
                currentMbox->childSlots[i]->status = FULL;
                currentMbox->childSlots[i]->msgSize = msg_size;
                memcpy(currentMbox->childSlots[i]->data, msg_ptr, msg_size);
                enableInterrupts();
                unblockProc(currentMbox->childSlots[i]->reservedPid);
                if (currentMbox->childSlots[i] != NULL && currentMbox->childSlots[i]->msgSize == -1)
                    return -1; 
                if(isZapped()){
                    return -3; 
                }
                return 0;
            }
            i++;
            // no slots in universal slot table case (all taken up by this mbox)
            if (i == MAXSLOTS) {
            	return -2;
			}
        }
	}

	enableInterrupts();
    if(isZapped()){
        return -3;
    }

	return retval;

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
    int retval = receive(currentMbox, msg_ptr, msg_size);
    if (isZapped()) return -3;
    return retval;

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
	MboxReceive(mailboxId + unit, status, sizeof(int));
	enableInterrupts();
	if (isZapped())
		return -1;
	return 0;

}

/* an error method to handle invalid syscalls */
void nullsys(systemArgs *args)
{
    USLOSS_Console("nullsys(): Invalid syscall %d. Halting...\n", (*args).number);
    USLOSS_Halt(1);
} /* nullsys */

/*
 * The clock interrupt handler for our OS baby
 * conditionally sends to the clock i/o mailbox every 5th clock interrupt
 */
void clockHandler2(int dev, void *arg)
{
    int result, condsend, unit = 0;
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
		result = USLOSS_DeviceInput(dev, unit, &completionStatus);
		condsend = MboxCondSend(USLOSS_CLOCK_DEV, &completionStatus, sizeof(int));
	}

} /* clockHandler */


void diskHandler(int dev, void *arg)
{
    int result, unit = *((int *) arg);
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
    result = USLOSS_DeviceInput(dev, unit, &completionStatus);
    MboxCondSend(1 + unit, &completionStatus, sizeof(int));

} /* diskHandler */


void termHandler(int dev, void *arg)
{
    int result, unit = (int) arg;
    if (DEBUG2 && debugflag2)
       USLOSS_Console("termHandler(): called\n");
    check_kernel_mode("termHandler");
	if (dev != USLOSS_TERM_DEV || unit < 0 || unit >= 4) {
        USLOSS_Console("termHandler(): incorrect device and/or unit number\n");
    }

    int completionStatus;
    result = USLOSS_DeviceInput(dev, unit, &completionStatus);
    MboxCondSend(3 + unit, &completionStatus, sizeof(int));
} /* termHandler */


void syscallHandler(int dev, void *arg)
{
    int result, unit = (*((systemArgs *) arg)).number;
    if (DEBUG2 && debugflag2)
        USLOSS_Console("syscallHandler(): called\n");
	// next phase stuff
	// error check
	if (dev != USLOSS_SYSCALL_INT) {
        USLOSS_Console("syscallHandler(): incorrect device number %d\n", dev);
		USLOSS_Halt(1);
    } else if (unit < 0 || unit >= MAXSYSCALLS) {
		USLOSS_Console("syscallHandler(): sys number %d is wrong.  Halting...\n", unit);
		USLOSS_Halt(1);
	}
	// call nullsys for now (initialized in start2)
	void (*syscallFunc) (systemArgs *) = SyscallHandlers[unit];
	syscallFunc((systemArgs *) arg);
} /* syscallHandler */


/*
 *
 */
int check_io()
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("check_io(): called\n");
	
	int i = 0, j = 0;
	for (i = 0; i < 7; i++) {
        	for (j = 0; j < MAXSLOTS; j++) {
				if (MailBoxTable[i].childSlots[j] != NULL && (MailBoxTable[i].childSlots[j]->status == SEND_RSVD || MailBoxTable[i].childSlots[j]->status == RECV_RSVD))
					return 1;
			}
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
    // left shift 2 to store prev values
    // then and by 12 to mask all the other bits except the previous mode bits (bits 2-3)
    // then enable interrupts and kernel mode
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
             "%s(): called while in user mode, by process %d. Halting...\n", funcName, getpid());
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
     slot->reservedPid = 0;
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
    int size = 0;
    // check if there's something that is sent or to be sent (0-slot)
    if (currentMbox->childSlots[0] != NULL &&
        (currentMbox->childSlots[0]->status == FULL ||
            currentMbox->childSlots[0]->status == SEND_RSVD)) {
        // if there is, then memcpy and unblock if needed.
        size = currentMbox->childSlots[0]->msgSize;
        if (size > msg_size) {
            enableInterrupts();
	        return -1;
        }
        memcpy(msg_ptr, currentMbox->childSlots[0]->data, msg_size);
        // 0-slot unblock case
        if (currentMbox->childSlots[0]->status == SEND_RSVD) {
            unblockProc(currentMbox->childSlots[0]->reservedPid);
        }
        freeSlot(currentMbox->childSlots[0]);
        // shift mailbox slots left 1, this should also take care of any other
        // waiting sends
        int i = 0;
        while (i < MAXSLOTS - 1 && currentMbox->childSlots[i] != NULL) {
        	currentMbox->childSlots[i] = currentMbox->childSlots[i+1];
            i++;
        } // need to account for no more slots left case?
        // case where we need to unblock sender if their msg was placed in mbox
		if (currentMbox->childSlots[currentMbox->numSlots-1] != NULL &&
            currentMbox->childSlots[currentMbox->numSlots-1]->status == SEND_RSVD) {
            if (currentMbox->numSlots != 0) {
                unblockProc(currentMbox->childSlots[currentMbox->numSlots-1]->reservedPid);
            }
        }
        enableInterrupts();
    	return size;
    }
    // slot at 0 is either receive reserved or NULL, leave it to MboxSend
    // to reserve the slot.
    return -2;

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
            // find an empty slot in the slot table to insert
            int j = 0;
            while (j < MAXSLOTS && MailSlotTable[j].status != EMPTY)
                j++;
            if (j == MAXSLOTS) {
                return -2;
            }
            currentMbox->childSlots[i] = &MailSlotTable[j];
            currentMbox->childSlots[i]->status = FULL;
            currentMbox->childSlots[i]->msgSize = msg_size;
            memcpy(currentMbox->childSlots[i]->data, msg_ptr, msg_size);
            currentMbox->childSlots[i]->mboxID = currentMbox->mboxID;
			enableInterrupts();
            return 0;
        } else if (i < MAXSLOTS && currentMbox->childSlots[i]->status == RECV_RSVD) {
            // recieve reserved slot found, unblock and let the receiver
            // free the slot and shift child slots down 1
            currentMbox->childSlots[i]->status = FULL;
            currentMbox->childSlots[i]->msgSize = msg_size;
            memcpy(currentMbox->childSlots[i]->data, msg_ptr, msg_size);
            unblockProc(currentMbox->childSlots[i]->reservedPid);

            // check if it was received through msg size
            if (currentMbox->childSlots[i] != NULL && currentMbox->childSlots[i]->msgSize == -1)
                return -1;
            enableInterrupts();
			return 0;
        }
    }
	enableInterrupts();
	return -2;
}
