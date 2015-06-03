
#ifndef INTERNALS_TIMER_H
#define INTERNALS_TIMER_H

/** Forward declaration */
typedef struct Process Process;   

/** Timeout descriptor */
typedef struct Timeout Timeout;
typedef struct Timeout {
    Timeout *next;        // next timeout in timer's list
    Process *proc;        // process expecting timeout 
    Time time;            // expiration time
} Timeout;

/** Interval between elapsed-time interrupts */
#define TICK 1000000000ULL

/** Enables timeout guard for alternation,
 *  returning true if guard is ready */
_Bool enable_timeout(Timeout *timeout);

/** Disables timeout guard for alternation, 
 *  returning true if guard is ready */
_Bool disable_timeout(Timeout *timeout);

/** Initializes the timer module */
void timer_init();

#endif
