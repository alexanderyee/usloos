#include <stdio.h>
#include <phase1.h>
#include <phase2.h>
#include "message.h"

extern int debugflag2;

/* an error method to handle invalid syscalls */
void nullsys(sysargs *args)
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
    readtime(); // add to the current process
    timeSlice();


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


} /* diskHandler */


void termHandler(int dev, int unit)
{

   if (DEBUG2 && debugflag2)
      USLOSS_Console("termHandler(): called\n");
    check_kernel_mode("terminalHandler");
	printf("shouldn't be called");
} /* termHandler */


void syscallHandler(int dev, int unit)
{

   if (DEBUG2 && debugflag2)
      USLOSS_Console("syscallHandler(): called\n");


} /* syscallHandler */
