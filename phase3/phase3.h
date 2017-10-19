/*
 * These are the definitions for phase 3 of the project
 */

#ifndef _PHASE3_H
#define _PHASE3_H

#define MAXSEMS         200
#define BLOCKED 1
#define READY 0
typedef struct procStruct procStruct;
typedef struct procStruct * procPtr;
typedef struct procStruct
{
        int     pid;
        int     (* startFunc) (char *); /* function where process begins -- launch */
        int     mboxID;
        char    *startArg;       /* args passed to process */
        int     childPid;
        int     parentPid;
        int     nextPid;
        int     priority;
        int     status;
        int     termCode;
} procStruct;

typedef struct semaphore
{
        int     semId;
        int     mboxID;
        int     value;
        int     isBlocking; // true if any processes are blocked on this sem
} semaphore;

#endif /* _PHASE3_H */
