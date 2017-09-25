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

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);
void disableInterrupts(void);
void enableInterrupts(void);
void check_kernel_mode(char *);
void freeSlot(slotPtr);
/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

// the mail boxes
mailbox MailBoxTable[MAXMBOX];
int currentMboxId;
mailSlot MailSlotTable[MAXSLOTS];
// also need array of mail slots, array of function ptrs to system call
// handlers, ...

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

	// @TODO: FOLLOWING FOR LOOP FOR DUMMY INIT OF INT HANDLERS
	int i;
	for (i = 0; i < 7; i++) {
		MailBoxTable[i].isUsed = 1;
	}

	currentMboxId = 7;

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
	MailBoxTable[currentMboxId % MAXMBOX].isUsed = 1;
	MailBoxTable[currentMboxId % MAXMBOX].mboxID = currentMboxId;
    MailBoxTable[currentMboxId % MAXMBOX].numSlots = slots;
    MailBoxTable[currentMboxId % MAXMBOX].maxLength = slot_size;
    currentMboxId++;
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
    if (msg_size > MailBoxTable[mbox_id % MAXMBOX].maxLength) {
        return -1;
    }
    if (msg_ptr == NULL) {
        return -1;
    }

    int i;
    mailbox *currentMbox = &MailBoxTable[mbox_id % MAXMBOX];
    // find a childslot in the mailbox to insert
    for (i = 0; i < currentMbox->numSlots; i++) {
        if (currentMbox->childSlots[i] == NULL) {
            // find a slot in the slot table to insert
            int j;
            while (j < MAXSLOTS && MailSlotTable[j].status == FULL)
                j++;
            if (j == MAXSLOTS) {
                USLOSS_Halt(1);
            }
            currentMbox->childSlots[i] = &MailSlotTable[j];
            currentMbox->childSlots[i]->status = FULL;
            memcpy(currentMbox->childSlots[i]->data, msg_ptr, msg_size);
            currentMbox->childSlots[i]->mboxID = mbox_id;
            return 0;
        }
    }
    // @TODO supposed to block, let's return -1 for now
    return -1;

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
    mailbox *currentMbox = &MailBoxTable[mbox_id % MAXMBOX];
    if (currentMbox->childSlots[0]->status == FULL) {
        memcpy(msg_ptr, currentMbox->childSlots[0]->data, msg_size);
        freeSlot(currentMbox->childSlots[0]);
        int i = 0;
        while (i < currentMbox->numSlots - 1 && currentMbox->childSlots[i] != NULL) {
            currentMbox->childSlots[i] = currentMbox->childSlots[i+1];
            i++;
        }
        if (isZapped()) return -3;
        printf("msg: %d   mboxmax: %d\n", msg_size, currentMbox->maxLength);
        return msg_size < currentMbox->maxLength ? msg_size : currentMbox->maxLength;
    } else {
    // TODO block receiver if there are no messages in this mailbox
        if (isZapped()) return -3;
        return -1;
    }
} /* MboxReceive */

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
     for (i = 0; i < MAX_MESSAGE; i++)
        slot->data[i] = '\0';
 }
