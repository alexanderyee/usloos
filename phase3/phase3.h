/*
 * These are the definitions for phase 3 of the project
 */

#ifndef _PHASE3_H
#define _PHASE3_H

#define MAXSEMS         200

typedef struct procStruct procStruct;
typedef struct procStruct * procPtr;
typedef struct procStruct
{
        int     pid;
        int     (* startFunc) (char *); /* function where process begins -- launch */
        int     mboxID;
        char    *startArg;       /* args passed to process */
        int     childPid;
        int     nextPid;
} procStruct;

typedef struct semaphore
{
        int     semPid;
        int     mboxID;

} semaphore;

#endif /* _PHASE3_H */
