#include <usyscall.h>
#include <usloss.h>
//for null
#include <stdlib.h>
#include <phase2.h> /* for MAXLINE */

#define CHECKMODE {    \
    if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) { \
        USLOSS_Console("Trying to invoke syscall from kernel\n"); \
        USLOSS_Halt(1);  \
    }  \
}

/*
 * interface for sleepReal.
 * int seconds -- # of seconds calling process should be asleep
 * arg1 = seconds
 * arg1 after syscall: return value, 0 if successful, -1 if seconds is invalid.
 */
int Sleep(int seconds)
{
    USLOSS_Sysargs sysArg;

	CHECKMODE;
    sysArg.number = SYS_SLEEP;
    sysArg.arg1 = (void *) (long) seconds;
    USLOSS_Syscall(&sysArg);
    return (int) (long) sysArg.arg1;
}

/*
 * Interface for diskReadReal
 */
int DiskRead(void *dbuff, int unit, int track, int first, int sectors, int *status)
{
    USLOSS_Sysargs sysArg;

	CHECKMODE;
    sysArg.number = SYS_DISKREAD;
    sysArg.arg1 = dbuff;
    sysArg.arg2 = (void *) (long) unit;
    sysArg.arg3 = (void *) (long) track;
    sysArg.arg4 = (void *) (long) first;
    sysArg.arg5 = (void *) (long) sectors;
    USLOSS_Syscall(&sysArg);
    *status = (int) (long) sysArg.arg1;
    return *status;
}

/*
 * Interface for diskWriteReal
 */
int DiskWrite(void *dbuff, int unit, int track, int first,
                     int sectors,int *status)
{
    USLOSS_Sysargs sysArg;

    CHECKMODE;
	sysArg.number = SYS_DISKWRITE;
    sysArg.arg1 = dbuff;
    sysArg.arg2 = (void *) (long) unit;
    sysArg.arg3 = (void *) (long) track;
    sysArg.arg4 = (void *) (long) first;
    sysArg.arg5 = (void *) (long) sectors;
    USLOSS_Syscall(&sysArg);
    *status = (int) (long) sysArg.arg1;
    return *status;

}

/*
 * Interface for diskSizeReal
 * check invalid parms. -1 if invalid/null
 */
int DiskSize(int unit, int *sector, int *track, int *disk)
{
    USLOSS_Sysargs sysArg;
    //check invalid parms. -1 if invalid/null
    if (sector == NULL || track == NULL || disk == NULL)
        return -1;
	CHECKMODE;
    sysArg.number = SYS_DISKSIZE;
    sysArg.arg1 = (void *) (long) unit;
    sysArg.arg2 = sector;
    sysArg.arg3 = track;
    sysArg.arg4 = disk;
    USLOSS_Syscall(&sysArg);
    return 0;
}

/*
 * Interface for termReadReal
 * check invalid parms. -1 if invalid/null
 */
int TermRead(char *buff, int bsize, int unit_id, int *nread)
{
    USLOSS_Sysargs sysArg;

	//check if parameters are invalid, if so return -1
	if(buff == NULL || nread == NULL)
		return -1;
    if(unit_id < 0 || unit_id >= USLOSS_TERM_UNITS || bsize <= 0 || bsize > MAXLINE){
        return -1;
    }
    CHECKMODE;
    sysArg.number = SYS_TERMREAD;
    sysArg.arg1 = buff;
	sysArg.arg2 = (void *) (long) bsize;
	sysArg.arg3 = (void *) (long) unit_id;
	sysArg.arg4 = nread;
	USLOSS_Syscall(&sysArg);
    *nread = (int) (long) sysArg.arg2;
	return (long) *nread;
}

/*
 * Interface for termWriteReal
 * check invalid parms. -1 if invalid/null
 */
int TermWrite(char *buff, int bsize, int unit_id, int *nwrite)
{
    USLOSS_Sysargs sysArg;

	//check if parameters are invalid, if so return -1
	if(buff == NULL || nwrite == NULL)
		return -1;
    if(unit_id < 0 || unit_id >= USLOSS_TERM_UNITS || bsize < 0 || bsize > MAXLINE){
        return -1;
    }
    CHECKMODE;
    sysArg.number = SYS_TERMWRITE;
    sysArg.arg1 = buff;
    sysArg.arg2 = (void *) (long) bsize;
    sysArg.arg3 = (void *) (long) unit_id;
    USLOSS_Syscall(&sysArg);
    *nwrite = (int) (long) sysArg.arg2;
	return (long) sysArg.arg4;


}
