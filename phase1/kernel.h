/* Patrick's DEBUG printing constant... */
#define DEBUG 1

typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
   procPtr         nextProcPtr;
   procPtr         childProcPtr;
   procPtr         nextSiblingPtr;
   procPtr         parentProcPtr;     /* the process pointer to this process' parent */
   char            name[MAXNAME];     /* process's name */
   char            startArg[MAXARG];  /* args passed to process */
   USLOSS_Context  state;             /* current context for process */
   short           pid;               /* process id */
   int             priority;
   int (* startFunc) (char *);        /* function where process begins -- launch */
   char           *stack;
   unsigned int    stackSize;
   int             status;            /* READY=0, BLOCKED=1, QUIT=2, JOIN_BLOCKED=3, ZAP_BLOCKED=4 etc. maybe have statuses for why it's blocked?*/
   int             isZapped;          /* is it zapped or is it not zapped. that is the question teehee :^) */
   /* other fields as needed... */
   int	     	   terminationCode;   /* termination code of when the child quit */	
   int 		   isNull;	      /* determines whether or not there is a process in this slot. */
   zapNode *       zapHead;           /* head of the linked list of zappers */
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
   struct psrBits bits;
   unsigned int integerPart;
};

struct zapNode {
    procPtr    zapper;
    zapNode *  next;
};
/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)
#define READY 0
#define BLOCKED 1
#define QUIT 2
#define JOIN_BLOCKED 3
#define ZAP_BLOCKED 4
