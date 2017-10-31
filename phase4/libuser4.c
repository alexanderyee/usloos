#include <usyscall.h>
/*
 * interface for sleepReal.
 * int seconds -- # of seconds calling process should be asleep
 * arg1 = seconds
 * arg1 after syscall: return value, 0 if successful, -1 if seconds is invalid.
 */
int Sleep(int seconds)
{
    USLOSS_Sysargs sysArg;

    // TODO check for kernel mode
    sysArg.number = SYS_SLEEP;
    sysArg.arg1 = (void *) (long) seconds;
    USLOSS_Syscall(&sysArg);
    return (int) (long) sysArg.arg1;
}

int DiskRead(void *dbuff, int unit, int track, int first, int sectors,
                    int *status)
{
    USLOSS_Sysargs sysArg;

    // TODO check for kernel mode
    sysArg.number = SYS_DISKREAD;
    //sysArg.arg1 = (void *) (long) seconds;
    return 0;
}

int DiskWrite(void *dbuff, int unit, int track, int first,
                     int sectors,int *status)
{
    USLOSS_Sysargs sysArg;

    // TODO check for kernel mode
    sysArg.number = SYS_DISKWRITE;
    //sysArg.arg1 = (void *) (long) seconds;
    return 0;
}

int DiskSize(int unit, int *sector, int *track, int *disk)
{
    USLOSS_Sysargs sysArg;

    CHECKMODE;
    sysArg.number = SYS_DISKSIZE;
    sysArg.arg1 = (void *) (long) seconds;
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
