
/**
 *  Microcsp 
 *  Copyright (c) 2015 Michael E. Goldsby
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "internals/hardware.h"
#include "atomic.h"
#include "sched.h"
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


// number of signal handlers presently active
static int activeHandlers;

// map from interrupt source to interrupt handler
static INTERRUPT_HANDLER interrupt_handler[NINTR_SOURCES];

// signal set containing all the RT signals
static sigset_t all_signals;

// signals blocked at current signal level
//    Note that the currently-blocked set does not reflect
//    the signals blocked and unblocked by DISABLE and ENABLE
//    but only the blocking and unblocking done as the result 
//    of signal handling
static sigset_t currently_blocked;

// map from our timer id to Linux timer id
static timer_t sys_timer_id[NTIMERS];

// nanoseconds per second
#define NS_PER_SEC 1000000000ULL

/** Display according to format string */
int Printf(char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt); 
    vprintf(fmt, argp);
    va_end(argp);
}

// for testing
static void display_sigmask(char *descrip, sigset_t *mask)
{
    Printf("%s: ", descrip);
    int i;
    for (i = SIGRTMIN; i <= SIGRTMAX; i++) {
        if (sigismember(mask, i)) {
            Printf(" %d", i);
        }
    } 
    Printf("\n");
}

/*
 *  Produces a signal set containing all RT signals with
 *  values >= given signo.
 */
static void greater_than_or_equal_to(int signo, sigset_t *result)
{
    // signals of lesser value have higher priority
    *result = all_signals;
    int i;
    for (i = SIGRTMIN; i < signo; i++) {
        int r = sigdelset(result, i);
        if (r) error("greater_than_or_equal_to sigdelset");
    }
}

/**
 *  The one and only signal handler.
 */
static void handle_signal(int signo, siginfo_t *info, void *ucontext)
{
    /** arrive with all RT signals blocked */

    // protect against unwatned compiler optimizations
    SIGFENCE;

    // atomically incr number of active handlers
    activeHandlers += 1;   

    // save currently blocked signals 
    sigset_t previously_blocked = currently_blocked;
     
    // set currently blocked signals to all those with values >= signo
    greater_than_or_equal_to(signo, &currently_blocked);
    int r = sigprocmask(SIG_BLOCK, &currently_blocked, NULL);
    if (r) error("handle_signal sigprocmask-1");

    // get interrupt handler and call it
    int intrsrc = signo - SIGRTMIN;    
    INTERRUPT_HANDLER handler = interrupt_handler[intrsrc];
    if (handler != NULL) {
        handler(intrsrc);
    }

    // block all signals
    r = sigprocmask(SIG_BLOCK, &all_signals, NULL);
    if (r) error("handle_signal sigprocmask-2");

    // atomically decr number of active handlers
    activeHandlers -= 1;

    // if no handlers are active, schedule processes if necessary
    if (activeHandlers == 0) {
        // Note 'currently_blocked' contains no signals,
        // since no handlers are active.
        // All signals are blocked now, but scheduler will
        // eventually enable all signals
        schedule(currentPriority());
    }
    
    // Restore previously blocked signals and install them as current.
    //    If are returning to interrupted higher-priority signal handler,
    //    this will restore the blocked signals proper to that handler.
    //    If are returning to interrupted application, there are no
    //    active handlers, so 'currently_blocked' contains no signals.
    //    (Note that handler for signal s must return before handler
    //    for signal s' > s can return.)
    currently_blocked = previously_blocked;
    r = sigprocmask(SIG_SETMASK, &currently_blocked, NULL);
    if (r) error("handle_signal sigprocmask-2");

    // protect against unwatned compiler optimizations
    SIGFENCE;
}

/**
 *  Defines an interrupt handler for the given interrupt source.
 */
void define_interrupt_handler(int intrsrc, INTERRUPT_HANDLER handler)
{
    interrupt_handler[intrsrc] = handler;
}

/**
 *  Defines a signal handler for a given signal number.
 */
void define_signal_handler(int signo) 
{ 
    struct sigaction action;
    action.sa_handler = NULL;          // call sigaction(), not signal()
    action.sa_sigaction = handle_signal;                  // the handler
    action.sa_mask = all_signals;  // arrive with all RT signals blocked
    action.sa_flags = SA_SIGINFO;                    // call sigaction()
    int r = sigaction(signo, &action, NULL);           // install driver
    if (r) error("define_signal_handler sigaction");
}

/**
 *  Defines a signal handler for each simulated interrupt.
 */
void define_signal_handlers()
{
    // start with  the currently-blocked set containing
    // no signals (that we are interested in)
    sigset_t no_signals;
    int r = sigfillset(&no_signals);
    if (r) error("define_signal_handlers sigfillset");
    int intr;
    for (intr = 0; intr < NINTR_SOURCES; intr++) {
        int signo = SIGRTMIN + intr;
        r = sigdelset(&no_signals, signo);
        if (r) error("define_signal_handlers sigdelset");
    }
    currently_blocked = no_signals;

    // define the handlers
    for (intr = 0; intr < NINTR_SOURCES; intr++) {
        int signo = SIGRTMIN + intr;
        define_signal_handler(signo);
    }
}

/** Enable interrupts */
void enable_interrupts()
{
    // enable all RT signals
    int r = sigprocmask(SIG_UNBLOCK, &all_signals, NULL);
    if (r) error("enable_interrupts sigprocmask");
}

/** Disable interrupts */
void disable_interrupts()
{
    // disable all RT signals
    int r = sigprocmask(SIG_BLOCK, &all_signals, NULL);
    if (r) error("disable_interrupts sigprocmask");
}

/** Set interval timer for a single interval. */
void set_timer_single(int timerId, Time interval)
{
    // if interval is negative, jsut use zero
    interval = (interval > 0 ? interval : 0);

    // break time value into seconds and nanoseconds
    struct timespec ts;
    ts.tv_sec = interval / NS_PER_SEC;
    ts.tv_nsec = interval % NS_PER_SEC;

    // set value to given interval and interval to zero
    struct itimerspec spec;
    spec.it_value = ts;
    spec.it_interval.tv_sec = 0;
    spec.it_interval.tv_nsec = 0;

    // retrieve system timer id
    timer_t id = sys_timer_id[timerId];

    // set the timer
    int r = timer_settime(id, 0, &spec, NULL);
    if (r) error("set_timer_single timer_settime");
}

/** Set interval timer for a repeating interval. */
void set_timer_repeating(int timerId, Time interval)
{
    // make sure interval is nonzero
    if (interval == 0) error("set_timer_repeating interval");

    // break interval value into seconds and nanoseconds
    struct timespec ts;
    ts.tv_sec = interval / NS_PER_SEC;
    ts.tv_nsec = interval % NS_PER_SEC;

    // set value and interval to same quantity
    struct itimerspec spec;
    spec.it_value = ts;
    spec.it_interval = ts;

    // retrieve system timer id
    timer_t id = sys_timer_id[timerId];

    // set the timer
    int r = timer_settime(id, 0, &spec, NULL);
    if (r) error("set_timer_repeating timer_settime");
}

/** Reads designated timer and returns time */
Time read_timer(int timerId)
{  
    // retrieve system timer id
    timer_t id = sys_timer_id[timerId];

    // read the timer
    struct itimerspec time;
    int r = timer_gettime(id, &time);
    if (r) error("read_timer timer_gettime");

    // extract time as nanoseconds
    struct timespec *ts = &time.it_value;
    Time t = ((Time)ts->tv_sec * NS_PER_SEC) + ts->tv_nsec;
 
    // return time
    return t;
}

/** Creates a timer */
void init_timer(int timerId, int intrsrc)
{
    // check for valid timer id
    if (!(0 <= timerId && timerId < NTIMERS)) error("init_timer timerId");

    // set up timer to notify by signal
    // using signal # derived from given interrupt number
    struct sigevent se;
    se.sigev_notify = SIGEV_SIGNAL;
    se.sigev_signo = SIGRTMIN + intrsrc;

    // create timer and save its system id in map
    int r = timer_create(CLOCK_REALTIME, &se, &sys_timer_id[timerId]);
    if (r) error("init_timer timer_create");
}

/** Sends user interrupt via software. */
void send_user_interrupt(int intrsrc)
{
    // get signal number corresponding to interrupt
    int signo = SIGRTMIN + intrsrc;

    // send signal to this process
    int r = kill(getpid(), signo);
    if (r) error("send_software_interrupt kill");
}

/** Should not happen */
void error(char *why)
{
    Printf("%s errno=%d\n", why, errno);
    exit(1);
}

/** Copies byte from src to dest */
inline void *Memcpy(void *dest, const void *src, size_t n)
{
    memcpy(dest, src, n);
}

/** Allocates memory */
inline void *Malloc(size_t size)
{
    return malloc(size);
}

/** Initializes this module. */
void hardware_init()
{
    // make a signal set containing all the RT signals
    int r = sigemptyset(&all_signals);
    if (r) error("hardware_init sigemptyset");
    int i;
    for (i = SIGRTMIN; i <= SIGRTMAX; i++) {
        int r = sigaddset(&all_signals, i);
        if (r) error("hardware_init sigaddset");
    }

    // disable interrupts
    DISABLE;    

    // define signal handlers for each simulated interrupt
    define_signal_handlers();
}

