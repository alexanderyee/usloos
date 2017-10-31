/*
 * These are the definitions for phase 4 of the project (support level, part 2).
 */

#ifndef _PHASE4_H
#define _PHASE4_H

/*
 * Maximum line length
 */

#define MAXLINE         80

/*
 * Function prototypes for this phase.
 */

extern  int  Sleep(int seconds);

extern  int  DiskRead (void *diskBuffer, int unit, int track, int first,
                       int sectors, int *status);
extern  int  DiskWrite(void *diskBuffer, int unit, int track, int first,
                       int sectors, int *status);
extern  int  DiskSize (int unit, int *sector, int *track, int *disk);
extern  int  TermRead (char *buffer, int bufferSize, int unitID,
                       int *numCharsRead);
extern  int  TermWrite(char *buffer, int bufferSize, int unitID,
                       int *numCharsRead);

extern  int  start4(char *);

typedef struct procStruct procStruct;
typedef struct procStruct * procPtr;
typedef struct procStruct
{
    int     pid;
    int     mboxID;
    int     childPid;
    int     parentPid;
    int     nextPid;
    int     priority;
    int     sleepSecondsRemaining; // number of seconds left to sleep. if > 0, then this process is asleep.
} procStruct;

typedef struct sleepNode sleepNode;
typedef struct sleepNodePtr * sleepNode;
typedef struct sleepNode
{
    procPtr proc;
    sleepNodePtr next;

} sleepNode;

#define ERR_INVALID             -1
#define ERR_OK                  0

#endif /* _PHASE4_H */
