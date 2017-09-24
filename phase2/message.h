
#define DEBUG2 1

typedef struct mailSlot *slotPtr;
typedef struct mailbox   mailbox;
typedef struct mboxProc *mboxProcPtr;
typedef struct mailSlot  mailSlot;

struct mailbox {
    int       mboxID;
    int       isUsed;
    int       numSlots;
    slotPtr   childSlots[MAXSLOTS];
    int       maxLength;
    // other items as needed...

};

struct mailSlot {
    int       mboxID;
    int       status;
    char      data[MAX_MESSAGE];
    // other items as needed...
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
#define NULL 0