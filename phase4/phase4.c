/* ------------------------------------------------------------------------
   phase4.c

   University of Arizona
   Computer Science 452

   Katie Pan & Alex Yee
   ------------------------------------------------------------------------ */
#include <usloss.h>
#include <libuser.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <providedPrototypes.h>
#include <stdlib.h> /* needed for atoi() */
#include <stdio.h> /* needed for sprintf() */
#include <string.h> /* needed for bzero() */

/*data structures + globals*/
int     isDebug = 0;
int	 	running;
procStruct ProcTable[MAXPROC];
procPtr    sleepQueue[MAXPROC];
diskNode   disk0Queue[MAXPROC];
diskNode   disk1Queue[MAXPROC];
int        disk0Count = 0;
int        disk1Count = 0;
int        disk0Sem;
int        disk1Sem;
int        disk0QueueSem;
int        disk1QueueSem;
int        currDisk0Track = -1;
int        currDisk1Track = -1;
int        disk0Tracks = -1;
int        disk1Tracks = -1;
// Three processes per terminal: driver, reader, and writer respectively.
int        termPids[USLOSS_TERM_UNITS][3];
// one char-in, char-out, line-in, line-out mbox, and xmit_result per unit, respectively.
int        termMboxes[USLOSS_TERM_UNITS][5];
void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *);

//prototypes
static int ClockDriver(char *);
static int DiskDriver(char *);
static int TermDriver(char *);
static int TermWriter(char *);
static int TermReader(char *);
int sleepReal(USLOSS_Sysargs *);
void check_kernel_mode(char *);
int enqueue(procPtr);
procPtr pop(void);
procPtr popAtIndex(int);
int initProc(int);
int diskSizeReal(USLOSS_Sysargs *);
int diskEnqueue(void *, int, int, int, int, int);
int diskDequeue(int);
int diskReadReal(USLOSS_Sysargs *);
int diskWriteReal(USLOSS_Sysargs *);
int diskSizeRealActually(int, int *, int *, int *);
int termReadReal(USLOSS_Sysargs * );
int termWriteReal(USLOSS_Sysargs * );

/* start3: entry point for phase4 which spawns start4*/
void start3(void)
{
    char	name[128];
    //char    termbuf[10];
    int		i;
    int		clockPID, disk0PID, disk1PID;
    int		pid;
    int		status;
    /*
     * Check kernel mode here.
     */
    check_kernel_mode("start3");
    // initalize the syscall handlers
   systemCallVec[SYS_SLEEP] = (void (*) (USLOSS_Sysargs *)) sleepReal;
   systemCallVec[SYS_DISKSIZE] = (void (*) (USLOSS_Sysargs *)) diskSizeReal;
   systemCallVec[SYS_DISKREAD] = (void (*) (USLOSS_Sysargs *)) diskReadReal;
   systemCallVec[SYS_DISKWRITE] = (void (*) (USLOSS_Sysargs *)) diskWriteReal;
   systemCallVec[SYS_TERMREAD] = (void (*) (USLOSS_Sysargs *)) termReadReal;
   systemCallVec[SYS_TERMWRITE] = (void (*) (USLOSS_Sysargs *)) termWriteReal;

    /* init ProcTable */
    for (i = 0; i < MAXPROC; i++) {
        ProcTable[i].pid = -1;
    	sleepQueue[i] = NULL;
        disk0Queue[i].semID = -1;
        disk1Queue[i].semID = -1;
	}
	initProc(getpid());
    /*
     * Create clock device driver
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
    running = semcreateReal(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) {
	    USLOSS_Trace("start3(): Can't create clock driver\n");
        USLOSS_Halt(1);
    }
    // initialize the process entry in our ProcTable

    /*
     * Wait for the clock driver to start. The idea is that ClockDriver
     * will V the semaphore "running" once it is running.
     */

    sempReal(running);
	char buf[69];
    /*
     * Create the disk device drivers here.  You may need to increase
     * the stack size depending on the complexity of your
     * driver, and perhaps do something with the pid returned.
     */
	disk0Sem = semcreateReal(0);
    disk1Sem = semcreateReal(0);
    disk0QueueSem = semcreateReal(1);
    disk1QueueSem = semcreateReal(1);

    for (i = 0; i < USLOSS_DISK_UNITS; i++) {
        sprintf(buf, "%d", i);
        sprintf(name, "Disk %d", i);
        pid = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);

        if (pid < 0) {
            USLOSS_Trace("start3(): Can't create disk driver %d\n", i);
            USLOSS_Halt(1);
        }
        else if (i == 0) {
            disk0PID = pid;
        } else {
            disk1PID = pid;
        }

        sempReal(running);
    }

    /*
     * Create terminal device drivers.
     */

    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        // initialize the mboxes for this unit
        termMboxes[i][CHAR_IN] = MboxCreate(MAXSLOTS, 1);
        termMboxes[i][CHAR_OUT] = MboxCreate(0, 1);
        termMboxes[i][LINE_IN] = MboxCreate(10, MAXLINE+1); // +1 for the '\0'
        termMboxes[i][LINE_OUT] = MboxCreate(MAXSLOTS, MAXLINE+1);
        termMboxes[i][XMIT_RESULT] = MboxCreate(0, sizeof(int));

        sprintf(buf, "%d", i);
        sprintf(name, "Term Driver %d", i);
        pid = fork1(name, TermDriver, buf, USLOSS_MIN_STACK, 2);

        if (pid < 0) {
            USLOSS_Trace("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }
        else {
            termPids[i][0] = pid;
        }
        sempReal(running);

        // reader
        sprintf(name, "Term Reader %d", i);
        pid = fork1(name, TermReader, buf, USLOSS_MIN_STACK, 2);

        if (pid < 0) {
            USLOSS_Trace("start3(): Can't create term reader %d\n", i);
            USLOSS_Halt(1);
        }
        else {
            termPids[i][1] = pid;
        }
        sempReal(running);

        // writer
        sprintf(name, "Term Writer %d", i);
        pid = fork1(name, TermWriter, buf, USLOSS_MIN_STACK, 2);

        if (pid < 0) {
            USLOSS_Trace("start3(): Can't create term writer %d\n", i);
            USLOSS_Halt(1);
        }
        else {
            termPids[i][2] = pid;
        }
        // HOW MANY WRITTEN TERMINAL LINES TO BUFFER?
        sempReal(running);
    }

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case first letters, as shown in provided_prototypes.h
     */
    pid = spawnReal("start4", start4, NULL, 2 * USLOSS_MIN_STACK, 3);
    initProc(pid);
    pid = waitReal(&status);
    /*
     * Zap the device drivers
     */

    zap(clockPID);  // clock driver
	join(&status);

    semvReal(disk0Sem);
    zap(disk0PID); // disk 0
    join(&status);

    semvReal(disk1Sem);
    zap(disk1PID); // disk 1
    join(&status);

    //zap the terminal drivers
    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        MboxRelease(termMboxes[i][LINE_IN]);
        MboxRelease(termMboxes[i][CHAR_IN]);

        zap(termPids[i][1]);
        join(&status);

    }

    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        MboxRelease(termMboxes[i][LINE_OUT]);
        zap(termPids[i][2]);
        join(&status);

    }
    /*
     * Welp. Only workaround we've discovered is to just add more to the term files.
     * The term_.in files are running out too early to trigger an interrupt for
     * TermDriver's waitDevice.
     */
    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        MboxRelease(termMboxes[i][CHAR_OUT]);
        MboxRelease(termMboxes[i][XMIT_RESULT]);
        sprintf(buf, "term%d.in", i);
        FILE *file = fopen(buf, "a");
        fprintf(file, "q");
        fflush(file);
        fclose(file);
        zap(termPids[i][0]);
        join(&status);

    }

	semfreeReal(running);
	semfreeReal(disk0Sem);
	semfreeReal(disk1Sem);
    semfreeReal(disk0QueueSem);
    semfreeReal(disk1QueueSem);
    // eventually, at the end:
    quit(0);

}

/* ------------------------------------------------------------------------
   Name - TermDriver
   Purpose - Keeps track to when to Read and Write; We used
            sems to keep track of scheduling
   Parameters - char * arg
   Returns - an int. -1 if invalid. 0 if valid
   Side Effects - none.
   ----------------------------------------------------------------------- */
static int TermDriver(char *arg)
{
    int unit = atoi(arg), result, status, charsWritten = 0;
    // Let the parent know we are running and enable interrupts.

    semvReal(running);
    result = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    //USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, USLOSS_TERM_CTRL_RECV_INT(0));
    if (result == USLOSS_ERR_INVALID_PSR) {
        USLOSS_Console("oopsie daisie TermDriver invalid psr\n");
    }
    while (!isZapped()) {
        result = waitDevice(USLOSS_TERM_DEV, unit, &status);
        if (result != 0) {
            return 0;
        }
        if (status == USLOSS_DEV_INVALID) {
            USLOSS_Trace("Invalid params for TermDriver's DeviceInput\n");
            return -1;
        }

        // check recv
        int recvStatus = USLOSS_TERM_STAT_RECV(status);
        if (recvStatus == USLOSS_DEV_BUSY) {
            char charToRead = USLOSS_TERM_STAT_CHAR(status);
            // got a char, send to the mbox.
            result = MboxCondSend(termMboxes[unit][CHAR_IN], &charToRead, 1);

            // TODO check return val of above.
			if (result == -2) {
				break;
			}
        } else if (recvStatus == USLOSS_DEV_ERROR) {
            // something went wrong?
            USLOSS_Trace("Receive status register for terminal %d returned error\n", unit);
            return -1;
        }

        // check xmit
        int xmitStatus = USLOSS_TERM_STAT_XMIT(status);

        if (xmitStatus == USLOSS_DEV_READY) {
            char charToXmit;
            int condRecvStatus = MboxCondReceive(termMboxes[unit][CHAR_OUT], &charToXmit, 1);
			if (condRecvStatus >= 0) {
            	int ctrl = 0;
            	// basically set everything on
            	ctrl = USLOSS_TERM_CTRL_CHAR(ctrl, charToXmit);
				ctrl = USLOSS_TERM_CTRL_XMIT_CHAR(ctrl);
            	ctrl = USLOSS_TERM_CTRL_RECV_INT(ctrl);
            	ctrl = USLOSS_TERM_CTRL_XMIT_INT(ctrl);
				result = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void *) (long) ctrl);
                //result = waitDevice(USLOSS_TERM_DEV, unit, &status);
				//USLOSS_Console("wtf is the ctrl %d\n", ctrl);
            	if (result == USLOSS_DEV_OK) {
                	charsWritten++;
                	if (charToXmit == '\n') {
                    	//stahp here
                    	if (charsWritten == MAXLINE) {
                        	// mustve been a full line, no newline. just --
                        	charsWritten--;
                    	}
                    	MboxSend(termMboxes[unit][XMIT_RESULT], &charsWritten, sizeof(int));
                    	charsWritten = 0;
                	}

           	 	} else if (result == USLOSS_DEV_INVALID) {
                    USLOSS_Console("Invalid paramaters TermDriver\n");
                }
			} else if (condRecvStatus == -1) {
                USLOSS_Console("mbox id wtf\n");
            }
        }
    }
    quit(0);
    return 0;
}

/* ------------------------------------------------------------------------
   Name - TermWriter
   Purpose - write back onto the terminal
   Parameters - takes in an char * arg
   Returns - an int. return 0 if successful. however, it should since
            we parameter check prior. and we break.
   Side Effects - none.
   ----------------------------------------------------------------------- */
static int TermWriter(char *arg)
{

    const int ctrl = 0;
    int unit = atoi(arg), result;
    // Let the parent know we are running and enable interrupts.
    semvReal(running);
    result = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    if (result == USLOSS_ERR_INVALID_PSR) {
        USLOSS_Console("oopsie daisie TermWriter invalid psr\n");
    }
    char currLine[MAXLINE + 1];

    while (!isZapped()) {

        result = MboxReceive(termMboxes[unit][LINE_OUT], &currLine, MAXLINE + 1);
        if (result < 0)
            break;
        result = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void *) (long) USLOSS_TERM_CTRL_XMIT_INT(USLOSS_TERM_CTRL_RECV_INT(ctrl)));
        if (result == USLOSS_DEV_INVALID) {
            USLOSS_Console("oopsie daisie TermWriter invalid DEV/UNIT\n");
        }
        //result = waitDevice(USLOSS_TERM_DEV, unit, &status);
        int index = 0;
        while(index != MAXLINE && currLine[index] != '\0' && currLine[index] != '\n') {
            MboxSend(termMboxes[unit][CHAR_OUT], &currLine[index], 1);
			index++;
        }
        // send another message (\n) to indicate the end of the msg, as well as a null
        currLine[index-1] = '\n'; // recycle the memory <3
        MboxSend(termMboxes[unit][CHAR_OUT], &currLine[index-1], 1);

        // let termdriver send to termWriteReal the chars written.

        // disable everything but recv interrupts
        result = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void *) (long) USLOSS_TERM_CTRL_RECV_INT(ctrl));
        if (result == USLOSS_DEV_INVALID) {
            USLOSS_Console("oopsie daisie TermWriter invalid DEV/UNIT\n");
        }
        //result = waitDevice(USLOSS_TERM_DEV, unit, &status);

    }
    quit(0);
    return 0;

}

/* ------------------------------------------------------------------------
  Name - TermWriteReal
  Purpose - write back to the terminal
  Parameters - USLOSS_Sysargs * sysArg
  Returns - an int
  Side Effects - none.
  ----------------------------------------------------------------------- */
int termWriteReal(USLOSS_Sysargs * sysArg){
    int result;
    if (ProcTable[getpid() & MAXPROC].pid == -1) {
        // process hasn't been initialized yet, let's fix that
        initProc(getpid());
    }
    char * buff = sysArg->arg1;
    int bsize = (int) (long) sysArg->arg2;
    int unit_id = (int) (long) sysArg->arg3;

    // mbox send the line
    MboxSend(termMboxes[unit_id][LINE_OUT], buff, bsize);
    MboxReceive(termMboxes[unit_id][XMIT_RESULT], &result, sizeof(int));
    return 0;
}

/* ------------------------------------------------------------------------
  Name - TermReader
  Purpose - read from the terminal
  Parameters - char *arg
  Returns - an int. also if invalid, there will be a nice printout :)
  Side Effects - none.
  ----------------------------------------------------------------------- */
static int TermReader(char *arg)
{
    int unit = atoi(arg), result, currLineIndex = 0;
    char charRead;
    char currentLine[MAXLINE+1];
    // Let the parent know we are running and enable interrupts.

    semvReal(running);
    result = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    if (result == USLOSS_ERR_INVALID_PSR) {
        USLOSS_Console("oopsie daisie TermReader invalid psr\n");
    }
    // just enable recv interrupts for this unit for now. TermWriter will
    // enable both.
    result = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void *) (long) USLOSS_TERM_CTRL_RECV_INT(0));
    if (result == USLOSS_DEV_INVALID) {
        USLOSS_Console("oopsie daisie TermReader invalid DEV/UNIT\n");
    }
    //result = waitDevice(USLOSS_TERM_DEV, unit, &status);

    while (!isZapped()) {

        result = MboxReceive(termMboxes[unit][CHAR_IN], &charRead, 1);
        if (result < 0)
            break;
        currentLine[currLineIndex++] = charRead;

        if (charRead == '\n' || currLineIndex == MAXLINE) {
            // finished line, stick in a null and send it out
            currentLine[currLineIndex] = '\0';
            MboxCondSend(termMboxes[unit][LINE_IN], &currentLine, currLineIndex+1);
            // TODO check retval of above, discard line if it's full.
            bzero(currentLine, MAXLINE+1);
            currLineIndex = 0;
        }
    }
    quit(0);
    return 0;
}

/* ------------------------------------------------------------------------
   Name - diskReadReal
   Purpose - checks if there is invalid parameters and will add read requests
   Parameters - USLOSS_Sysargs * sysArg
   Returns - an int
   Side Effects - It will block until it finishes reading the data
   ----------------------------------------------------------------------- */
int termReadReal(USLOSS_Sysargs * sysArg){

    if (ProcTable[getpid() & MAXPROC].pid == -1) {
        // process hasn't been initialized yet, let's fix that
        initProc(getpid());
    }
    int charsRead = 0;
    char * buff = sysArg->arg1;
    int bsize = (int) (long) sysArg->arg2;
    int unit_id = (int) (long) sysArg->arg3;

    MboxReceive(termMboxes[unit_id][LINE_IN], buff, MAXLINE+1);

    while (charsRead < bsize && buff[charsRead] != '\0') {
        charsRead++;
    }

    sysArg->arg2 = (void *) (long) charsRead;

    return 0;
}

/* ------------------------------------------------------------------------
   Name - ClockDriver
   Purpose - Keep track of time to wake up user call.
   Parameters - char *arg
   Returns - again... an int
   Side Effects - none.
   ----------------------------------------------------------------------- */
static int ClockDriver(char *arg)
{
    int result;

    // Let the parent know we are running and enable interrupts.
    semvReal(running);
    result = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    if (result == USLOSS_ERR_INVALID_PSR) {
        USLOSS_Console("oopsie daisie ClockDriver invalid psr\n");
    }
    // Infinite loop until we are zap'd
    while(!isZapped()) {
        int status, i = 0;
		result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        if (result != 0) {
    	    return 0;
	    }

        // look through all the sleeping procs, subtract the time.
		while (i < MAXPROC && sleepQueue[i] != NULL) {
            if (sleepQueue[i]->lastSleepTime == 0) {
                // init the start time
                sleepQueue[i]->lastSleepTime = status;
            } else {
                sleepQueue[i]->sleepSecondsRemaining -= status - sleepQueue[i]->lastSleepTime;
                sleepQueue[i]->lastSleepTime = status;
            }
            if (sleepQueue[i]->sleepSecondsRemaining < 0) {
				procPtr p = popAtIndex(i);
	            semvReal(p->semID);
                continue;
            }
            i++;
        }
	/*
	 * Compute the current time and wake up any processes
	 * whose time has come.
	 */

    }
    quit(0);
    return 0;
}

/*
* Causes the calling process to become unrunnable for at least the
* specified number of seconds, and not significantly longer.
* The seconds must be non-negative.
*
* Return values:
*		-1: seconds is not valid
*		0: otherwise
*/
int sleepReal(USLOSS_Sysargs * args)
{
    if (args->arg1 < 0) {
        args->arg1 = (void *)(long) -1;
        return -1;
    }

    if (ProcTable[getpid() & MAXPROC].pid == -1) {
        // process hasn't been initialized yet, let's fix that
        initProc(getpid());
    }
    int result = enqueue(&ProcTable[getpid() % MAXPROC]);
    if (result == -1) {
        return -1;
    }
    ProcTable[getpid() % MAXPROC].sleepSecondsRemaining = (int) (long) args->arg1 * 1000000;
    sempReal(ProcTable[getpid() % MAXPROC].semID);
    args->arg1 = 0;
    return 0;
}

/* ------------------------------------------------------------------------
   Name - DiskDriver
   Purpose - handles calls to read and write to the disk. lots of the code checks
            which disk it is on. which is why we used ternary
   Parameters - char *arg
   Returns - again... an int
   Side Effects - none.
   ----------------------------------------------------------------------- */
static int DiskDriver(char *arg)
{
    int unit = atoi(arg);
    int sem = unit ? disk1Sem : disk0Sem;
    // initialize the number of tracks the disk has
    USLOSS_DeviceRequest deviceRequest;

    deviceRequest.opr = USLOSS_DISK_TRACKS;
    if (unit) {
        deviceRequest.reg1 = &disk1Tracks;
    } else {
        deviceRequest.reg1 = &disk0Tracks;
    }
	int result = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &deviceRequest);
    if (result == USLOSS_DEV_INVALID) {
        USLOSS_Console("oopsie daisie DiskDriver invalid DEV/UNIT\n");
    }
    waitDevice(USLOSS_DISK_DEV, unit, &result);
    semvReal(running);
    result = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    if (result == USLOSS_ERR_INVALID_PSR) {
        USLOSS_Console("oopsie daisie DiskDriver invalid psr\n");
    }
    while(!isZapped()) {
        sempReal(sem);
        diskNodePtr request = unit ? &disk1Queue[0] : &disk0Queue[0];
        if (request->semID == -1) // quit case
            break;
        int i, isNextTrack = 0;
        // seek to the track
        USLOSS_DeviceRequest deviceRequest;
        if (request->track != (unit ? currDisk1Track : currDisk0Track)) {
            if (unit) {
                currDisk1Track = request->track;
            } else {
                currDisk0Track = request->track;
            }
            deviceRequest.opr = USLOSS_DISK_SEEK;
            deviceRequest.reg1 = (void *) (long) request->track;
            int result = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &deviceRequest);
            waitDevice(USLOSS_DISK_DEV, unit, &result);
        }

        //check if it is a read or a write. put that as our opr
        if (request->op == READ) {
            deviceRequest.opr = USLOSS_DISK_READ;
        } else {
            deviceRequest.opr = USLOSS_DISK_WRITE;
        }

        //circular scheduling
        for (i = 0; i < request->sectors; i++) {

            deviceRequest.reg1 = (void *) (long) ((i + request->first) % USLOSS_DISK_TRACK_SIZE);
            if ((i + request->first) >= USLOSS_DISK_TRACK_SIZE && !isNextTrack) {
                // need to seek to next track
                USLOSS_DeviceRequest deviceRequestSeek;
                deviceRequestSeek.opr = USLOSS_DISK_SEEK;
                deviceRequestSeek.reg1 = (void *) (long) (request->track + 1);
                if (unit) {
                    currDisk1Track = request->track + 1;
                } else {
                    currDisk0Track = request->track + 1;
                }
                int result = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &deviceRequestSeek);
                waitDevice(USLOSS_DISK_DEV, unit, &result);
                isNextTrack = 1;
            }
            deviceRequest.reg2 = request->dbuff + (i * USLOSS_DISK_SECTOR_SIZE);

            int result = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &deviceRequest);
            waitDevice(USLOSS_DISK_DEV, unit, &result);
        }
        int reqSemID = diskDequeue(unit);
        // request fulfilled, dequeue it and unblock that proc
        semvReal(reqSemID);
    }

    quit(0);
    return 0;
}


/*
 * Reads sectors sectors from the disk indicated by unit, starting at track track and
 * sector first. The sectors are copied into buffer. Your driver must handle a range of
 * sectors specified by first and sectors that spans a track boundary (after reading the
 * last sector in a track it should read the first sector in the next track).
 * A file cannot wrap around the end of the disk.
 *   sysArg.arg1 = dbuff;
     sysArg.arg2 = unit;
     sysArg.arg3 = track;
     sysArg.arg4 = first;
     sysArg.arg5 = sectors;
 * Return values: (through arg1)
 *  -1: invalid parameters
 *  0: sectors were read successfully
 *  >0: disk’s status register
 */
int diskReadReal(USLOSS_Sysargs * args)
{
    if (ProcTable[getpid() & MAXPROC].pid == -1) {
        // process hasn't been initialized yet, let's fix that
        initProc(getpid());
    }
    int sectorSize, numSectors, numTracks;
    void *dbuff = args->arg1;
    int unit = (int) (long) args->arg2;
    int track = (int) (long) args->arg3;
    int first = (int) (long) args->arg4;
    int sectors = (int) (long) args->arg5;

    if(unit < 0 || unit >= USLOSS_DISK_UNITS){
        args->arg1 = (void *) (long) -1;
        return -1;
    }

    diskSizeRealActually(unit, &sectorSize, &numSectors, &numTracks);
    // check if first and sectors are > 0 and < numsectors; track > 0 and < numTracks
  	if(first < 0 || first >= numSectors){
		//USLOSS_Trace("diskReadReal() first is invalid\n");
        //USLOSS_Halt(1);
        args->arg1 = (void *) (long) -1;
        return -1;
	}

	if(sectors < 0 || sectors >= numSectors){
        //USLOSS_Trace("diskReadReal() sectors is invalid\n");
        //USLOSS_Halt(1);
        args->arg1 = (void *) (long) -1;
        return -1;
    }

	if(track < 0 || track >= numTracks){
        //USLOSS_Trace("diskReadReal() track is invalid\n");
        //USLOSS_Halt(1);
        args->arg1 = (void *) (long) -1;
        return -1;
    }

	diskEnqueue(dbuff, unit, track, first, sectors, READ);
    semvReal(unit ? disk1Sem : disk0Sem);
    sempReal(ProcTable[getpid() % MAXPROC].semID);
    args->arg1 = 0;
    return 0;
}


/*
 * Writes sectors sectors to the disk indicated by unit , starting at track track and sector
 * first . The contents of the sectors are read from buffer . Like diskRead , your driver
 * must handle a range of sectors specified by first and sectors that spans a track
 * boundary. A file cannot wrap around the end of the disk.
     sysArg.arg1 = dbuff;
     sysArg.arg2 = unit;
     sysArg.arg3 = track;
     sysArg.arg4 = first;
     sysArg.arg5 = sectors;
 * Return values:
 * -1: invalid parameters
 *  0: sectors were written successfully
 * >0: disk’s status register
 */
int diskWriteReal(USLOSS_Sysargs * args)
{
    if (ProcTable[getpid() & MAXPROC].pid == -1) {
        // process hasn't been initialized yet, let's fix that
        initProc(getpid());
    }
    int sectorSize, numSectors, numTracks;
    void *dbuff = args->arg1;
    int unit = (int) (long) args->arg2;
    int track = (int) (long) args->arg3;
    int first = (int) (long) args->arg4;
    int sectors = (int) (long) args->arg5;

    if(unit < 0 || unit >= USLOSS_DISK_UNITS){
        args->arg1 = (void *) (long) -1;
        return -1;
    }

    diskSizeRealActually(unit, &sectorSize, &numSectors, &numTracks);
    // check if first and sectors are > 0 and < numsectors; track > 0 and < numTracks
	if(first < 0 || first >= numSectors){
        //USLOSS_Trace("diskWriteReal() first is invalid\n");
        //USLOSS_Halt(1);
        args->arg1 = (void *) (long) -1;
        return -1;
    }

    if(sectors < 0 || sectors >= numSectors){
        //USLOSS_Trace("diskWriteReal() sectors is invalid\n");
        //USLOSS_Halt(1);
        args->arg1 = (void *) (long) -1;
        return -1;
    }

    if(track < 0 || track >= numTracks){
        //USLOSS_Trace("diskWriteReal() track is invalid\n");
        //USLOSS_Halt(1);
        args->arg1 = (void *) (long) -1;
        return -1;
    }


	diskEnqueue(dbuff, unit, track, first, sectors, WRITE);
    semvReal(unit ? disk1Sem : disk0Sem);
    sempReal(ProcTable[getpid() % MAXPROC].semID);
    args->arg1 = 0;
    return 0;


}


/* ------------------------------------------------------------------------
   Name - diskSizeReal
   Purpose - gets info from the a specific disk
   Parameters - USLOSS_Sysargs * sysArg
   Returns - an int, return 0 if successful, -1 if invalid parms
   Side Effects -none
   ----------------------------------------------------------------------- */
int diskSizeReal(USLOSS_Sysargs * sysArg)
{
    if (ProcTable[getpid() & MAXPROC].pid == -1) {
        // process hasn't been initialized yet, let's fix that
        initProc(getpid());
    }
	//check parameters are correct
	if( (int) (long) sysArg->arg1 < 0 || (int) (long) sysArg->arg1 > 1){
		USLOSS_Trace("diskSizeReal() sysArg->arg1 is invalid\n");
       	USLOSS_Halt(1);
		return -1;
	}
	diskSizeRealActually((int) (long) sysArg->arg1, (int *) sysArg->arg2,(int *) sysArg->arg3, (int *) sysArg->arg4);
    return 0;
}

/*
 * helper method to set the sector size, track size, and disk size
 * just returns 0 when done
 */
int diskSizeRealActually(int unit, int * sectorSize, int * trackSize, int * diskSize)
{
    *sectorSize = USLOSS_DISK_SECTOR_SIZE;
    *trackSize = USLOSS_DISK_TRACK_SIZE;
    *diskSize = unit ? disk1Tracks : disk0Tracks;
    return 0;

}

/* ------------------------------------------------------------------------
   Name - diskEnqueue
   Purpose - enqueues disk request
   Parameters - void *dbuff, int unit, int track, int first, int sectors, int op
   Returns - an int, return 0 if successful, -1 if invalid parms
   Side Effects -none
   ----------------------------------------------------------------------- */
int diskEnqueue(void *dbuff, int unit, int track, int first, int sectors, int op) {
    // block all access to queue
    sempReal(unit ? disk1QueueSem : disk0QueueSem);
    diskNodePtr queue = unit ? disk1Queue : disk0Queue;
    diskNodePtr insertedNode;
    // find where to insert. use first, then sectors to see if it can fit

    int i = 0, j, pivot = unit ? disk1Tracks : disk0Tracks, pivotIndex = MAXPROC, insertFlag = 0;
    // find the smallest. this takes O(n) lol
    while (queue[i].semID != -1) {
        if (queue[i].track < pivot) {
            pivot = queue[i].track;
            pivotIndex = i;
        }
        i++;
    }
    if (track < pivot && queue[0].semID != -1  && pivotIndex != 0 && track <= queue[0].track) {
        if (queue[MAXPROC - 1].semID != -1) {
            USLOSS_Trace("Too many r/w requests for disk %d\n", unit);
            return -1;
        }
        for (j = MAXPROC - 1; j > pivotIndex; j--) {
            queue[j] = queue[j-1];
        }
        insertedNode = &queue[pivotIndex];
    } else {
        // we know where our smallest is, this is the pivot.
        // we know to insert in the first half of the array if track > queue[0]track
        if (queue[0].semID == -1 || (track >= queue[0].track)) {
            insertFlag = 1;
        }

        for (i = (insertFlag ? 0 : pivotIndex); i < MAXPROC; i++) {
			if (queue[i].semID == -1) {
               // case where we reach an empty slot. just insert.
               insertedNode = &queue[i];
               break;
           } else if (i == MAXPROC - 1) {
               // error case for too many requests
               USLOSS_Trace("Too many r/w requests for disk %d\n", unit);
               return -1;
           } else if (i >= 1 && (queue[i].track > track || (i == pivotIndex && insertFlag))) {

			 if (queue[MAXPROC - 1].semID != -1) {
                   USLOSS_Trace("Too many r/w requests for disk %d\n", unit);
                   return -1;
               }
               for (j = MAXPROC - 1; j > i; j--) {
                   queue[j] = queue[j-1];
               }
               insertedNode = &queue[i];
               break;
           }
       }
    }

    //sets value with respective info
    insertedNode->semID = ProcTable[getpid() % MAXPROC].semID;
    insertedNode->dbuff = dbuff;
    insertedNode->unit = unit;
    insertedNode->track = track;
    insertedNode->first = first;
    insertedNode->sectors = sectors;
    insertedNode->op = op;

    for (i = 0; i < MAXPROC-1; i++) {
        if (queue[i].semID == -1)
            break;
    }
    semvReal(unit ? disk1QueueSem : disk0QueueSem);
    return 0;
}

/* ------------------------------------------------------------------------
   Name - diskDequeue
   Purpose - dequeues disk request
   Parameters - int unit
   Returns - an int, return 0 if successful, -1 if invalid
   Side Effects -none
   ----------------------------------------------------------------------- */
int diskDequeue(int unit) {

    sempReal(unit ? disk1QueueSem : disk0QueueSem);

    diskNodePtr queue = unit ? disk1Queue : disk0Queue;
    int resultSemID = -1;
    if (queue[0].semID == -1) {
        USLOSS_Trace("diskDequeue: Invalid dequeue\n");
        return -1;
    } else {
        resultSemID = queue[0].semID;
    }

    int i;
    for (i = 0; i < MAXPROC; i++) {
        if (i == MAXPROC - 1) {
            // reset the fields
            queue[i].semID = -1;
            queue[i].dbuff = NULL;
            queue[i].unit = 0;
            queue[i].track = 0;
            queue[i].first = 0;
            queue[i].sectors = 0;
            queue[i].op = 0;
        } else {
            queue[i] = queue[i+1];
        }
    }

    semvReal(unit ? disk1QueueSem : disk0QueueSem);
    return resultSemID;
}

/*
 * checks if the OS is currently in kernel mode; halts if it isn't.
 * to clarify, we are checking if the OS is in kernel mode. If it isn't,
 * we will terminate. Otherwise, we will continue, since it is a void
 * function. This function will take in a char* funcName, which is the function
 * name.
 */
void check_kernel_mode(char * funcName)
{
    if (!(USLOSS_PsrGet() & 1)) {
        USLOSS_Trace(
             "%s(): called while in user mode, by process %d. Halting...\n", funcName, getpid());
        USLOSS_Halt(1);
    }
} /* check_kernel_mode */

/*
 * initProc -- function that initializes the process entry in our phase4 ProcTable
 *              initializes pid and the private mbox.
 *      returns 0 on success, -1 if failure
 */
int initProc(int pid)
{
    ProcTable[pid % MAXPROC].pid = pid;
    ProcTable[pid % MAXPROC].semID = semcreateReal(0);
    if (ProcTable[pid % MAXPROC].semID < 0) {
        USLOSS_Trace("initProc(): Failure creating private mailbox for process %d", pid);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------------
   Name - enqueue
   Purpose - enqueue proc
   Parameters - procPtr p
   Returns - an int, return 0 if successful, 1 if invalid (full queue)
   Side Effects -none
   ----------------------------------------------------------------------- */
int enqueue(procPtr p){
    int i = 0;
    while (i < MAXPROC && sleepQueue[i] != NULL) {
    	i++;
		if (i == MAXPROC) {
      		USLOSS_Trace("insert(): no more roomsies ;_;\n");
     		return 1;
		}
   }
   sleepQueue[i] = p;
   return 0;
}

/* ------------------------------------------------------------------------
   Name - pop
   Purpose - pop proc
   Parameters - nada
   Returns - returns null if nothing, otherwise return the procPtr
   Side Effects -none
   ----------------------------------------------------------------------- */
procPtr pop()
{
    int i = 0;
    procPtr result = NULL;
    if (sleepQueue[0] != NULL) {
        result = sleepQueue[0];
    }
    while (i < (MAXPROC-1) && sleepQueue[i] != NULL) {
        sleepQueue[i] = sleepQueue[i+1];
        i++;
    }
    return result;
}

/*
 * pop at a certain index
 * takes in a certain index
 */
procPtr popAtIndex(int index)
{
    int i = index;
    procPtr result = sleepQueue[i];

    if(result != NULL){
        //shift everything else
        while (i < (MAXPROC-1) && sleepQueue[i] != NULL) {
            sleepQueue[i] = sleepQueue[i+1];
            i++;
        }
    }

    return result;
}

/*
 * sets the current mode to user mode.
 */
void setUserMode()
{
    check_kernel_mode("setUserMode");
    unsigned int previousPSRValue = USLOSS_PsrGet();
    // we need to store the current interrupt mode
    unsigned int tempPsrCurrentInterruptTemp = previousPSRValue & 2;
    // preserve the current interrupt
    unsigned int newPSRValue = (previousPSRValue << 2) | tempPsrCurrentInterruptTemp;
    // set user mode by making sure last bit is 0
    newPSRValue = newPSRValue & 14;
    if (USLOSS_PsrSet(newPSRValue) == USLOSS_ERR_INVALID_PSR)
        USLOSS_Trace("ERROR: Invalid PSR value set! was: %u\n", newPSRValue);
} /* setUserMode */
