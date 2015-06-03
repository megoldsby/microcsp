
#ifndef INTERNALS_SCHED_H
#define INTERNALS_SCHED_H

typedef struct Channel {
    Process *waiting;     
    void *src;          
} Channel;

typedef struct ChanIn {
    Process *waiting;
    void *src;
} ChanIn;

typedef struct ChanOut {
    Process *waiting;
    void *src;
} ChanOut;

typedef struct InterruptChannel {
    Process *waiting;
    int count;
} InterruptChannel;

typedef struct Guard {
    union {
        struct {
            Channel *channel;
            void *dest;
            unsigned int len;
        } chanin;
        struct {
            Channel *channel;
            void *src;
        } chanout;
        Timeout *timeout;
        struct {
            InterruptChannel *channel;
            void *dest;
        } interrupt;
    };
    int8_t type;
    _Bool active;
} Guard;
// guard type      
#define GUARD_CHANIN     0
#define GUARD_CHANOUT    1
#define GUARD_SKIP       2
#define GUARD_TIMEOUT    3
#define GUARD_INTERRUPT  4

/** Returns priority of current process. */
int currentPriority();

/** Makes given process ready if it isn't already. */
void readyProcessIfNecessary(Process *proc);

/** Schedules the highest priority ready process. */
void schedule(int prev_priority);

#endif
