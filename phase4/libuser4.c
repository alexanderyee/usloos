#include <usyscall.h>
#include <usloss.h>


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

int DiskWrite(void *dbuff, int unit, int track, int first,
                     int sectors,int *status)
{
    USLOSS_Sysargs sysArg;

    CHECKMODE;
	sysArg.number = SYS_DISKWRITE;
    //sysArg.arg1 = (void *) (long) seconds;
    return 0;
}

/*
 * Interface for diskSizeReal
 */
int DiskSize(int unit, int *sector, int *track, int *disk)
{
    USLOSS_Sysargs sysArg;
    if (sector == NULL || track == NULL || disk == NULL)
        return -1;
	CHECKMODE;
    sysArg.number = SYS_DISKSIZE;
    sysArg.arg1 = (void *) (long) unit;
    sysArg.arg2 = (void *) sector;
    sysArg.arg3 = (void *) track;
    sysArg.arg4 = (void *) disk;
    USLOSS_Syscall(&sysArg);
    return 0;
}

int TermRead(char *buff, int bsize, int unit_id, int *nread)
{
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_TERMREAD;
    //sysArg.arg1 = (void *) (long) seconds;
    return 0;
}

int TermWrite(char *buff, int bsize, int unit_id, int *nwrite)
{
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_TERMWRITE;
    //sysArg.arg1 = (void *) (long) seconds;
    return 0;
}
