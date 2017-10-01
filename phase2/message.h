
#define DEBUG2 1

typedef struct mailSlot  *slotPtr;
typedef struct mailbox   mailbox;
typedef struct mboxProc  *mboxProcPtr;
typedef struct mailSlot  mailSlot;
typedef struct queueSlot queueSlot;

struct mailbox {
    int       mboxID;
    int       isUsed;
    int       numSlots;
    slotPtr   childSlots[MAXSLOTS];
    int       maxLength;
    queueSlot sendQueue[MAXSLOTS];
    // other items as needed...

};

struct mailSlot {
    int       mboxID;
    int       status;
    char      data[MAX_MESSAGE];
    int       msgSize;
    int       reservedPid;
    // other items as needed...
};

struct queueSlot {
    int       pid;
    int       status;
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

#define FULL 1
#define EMPTY 0
#define RSVD 2
#define NULL 0
#define IO_BLOCKED 69
#define SENT 1
#define RECEIVED 2
