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
	  currentMboxId = 0;

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
			USLOSS_Console("no more free mailboxes :(");
      return -1;
    }
	}
	MailBoxTable[i].isUsed = 1;
	MailBoxTable[i].mboxID = currentMboxId;
  MailBoxTable[i].numSlots = slots;
  MailBoxTable[i].maxLength = slot_size;
  currentMboxId++;
  return currentMboxId-1;

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
