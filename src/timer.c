
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

#include "timer.h"
#include "hardware.h"
#include <stdio.h>
#include <stdlib.h>

// user timer 0 for elapsed time, timer 1 for timeouts
#define TIMER_ELAPSED TIMER_0
#define TIMER_TIMEOUT TIMER_1

// time units (nsec) per tick
static Time Tick = 1000000000ULL;
//static Time Tick = 100000000000ULL;   // for testing  100 sec.

/** Timer queue descriptor */
typedef struct TimerQ {
    Timeout *head;
} TimerQ;

/** the timer queue */
static TimerQ timerQ;

/** Number of ticks since beginning of run */
static Time currentTime = 0;

/** 
 *  Returns elapsed time since beginning of run.
 */
Time Now()
{
    Time t, c, cc;
    c = currentTime;
    do {
        t = read_timer(TIMER_ELAPSED);
        cc = c;
        c = currentTime;
    } while (c != cc);
    return (Tick - t) + c;
}

/**
 *  Inserts timeout in timeout queue in time order.
 */
static void insertInQueue(Timeout *timeout)
{
    // INTERRUPTS MUST BE DISABLED

    // find timeout's place in the queue
    Timeout *prev = NULL;
    Timeout *curr = timerQ.head;
    while (curr != NULL && timeout->time >= curr->time) {
        prev = curr;
        curr = curr->next;
    }

    // insert timeout into the queue
    if (prev == NULL && curr == NULL) {
        // only entry in queue
        timerQ.head = timeout;
        timeout->next = NULL;
        Time now = Now();
        Time diff = timeout->time - now;
        set_timer_single(TIMER_TIMEOUT, diff);

    } else if (prev == NULL && curr != NULL) {
        // entry is first in queue but has a successor
        timerQ.head = timeout;
        timeout->next = curr;
        Time now = Now();
        Time diff = timeout->time - now;
        set_timer_single(TIMER_TIMEOUT, diff);

    } else if (prev != NULL && curr != NULL) {
        // entry has a predecessor and a successor in the queue
        prev->next = timeout;
        timeout->next = curr;

    } else /* prev != NULL && curr == NULL */ {
        // entry is last but has a predecessor
        prev->next = timeout;
        timeout->next = NULL;
    }
}

/** 
 * Removes a designated entry from the timeout queue and returns it.
 */
static Timeout *removeFromQueue(Time time, Process *proc)
{
    // INTERRUPTS MUST BE DISABLED
    // find entry in queue
    Timeout *prev = NULL;
    Timeout *curr = timerQ.head;            
    while (curr != NULL && curr->time < time) {
        prev = curr;
        curr = curr->next;    
    }
    // curr == NULL || curr->time >= time
    while (curr != NULL && curr->time == time && curr->proc != proc) {
        prev = curr;
        curr = curr->next;
    }
    // curr == NULL || curr->time > time || curr->proc == proc

    // remove entry from queue
    if (curr != NULL && curr->time == time && curr->proc == proc) 
    {
        if (prev != NULL)
            prev->next = curr->next;
        if (curr == timerQ.head) 
            timerQ.head = curr->next;
        // could restart timer if remove head entry..
    } 

    // return removed item (null if none)
    return curr;
}

/** 
 *  Removes and returns the head item in the timeout queue.
 *  Called only when queue is nonempty.
 */
static Timeout *removeHead()
{
    // INTERRUPTS MUST BE DISABLED
    Timeout *timeout = timerQ.head;
    timerQ.head = timerQ.head->next;
    return timeout;
}


/** Enables timeout guard for alternation,
 *  returning true if guard is ready */
_Bool enable_timeout(Timeout *timeout)
{
    // INTERRUPTS MUST BE DISABLED
    _Bool ready = (Now() >= timeout->time);
    if (!ready) {
        insertInQueue(timeout);
    }
    return ready;
}

/** Disables timeout guard for alternation, 
 *  returning true if guard is ready */
_Bool disable_timeout(Timeout *timeout)
{  
    // INTERRUPTS MUST BE DISABLED
    _Bool ready = (Now() >= timeout->time);
    removeFromQueue(timeout->time, timeout->proc);
    return ready;
}

static void handle_elapsed_time_interrupt()
{
    // INTERRUPTS OF PRIORITY <= THAT OF ELAPSED TIME INTERRUPT ARE DISABLED
    currentTime += Tick;      // increment tick count
}

static void handle_timeout_interrupt()
{
    // INTERRUPTS OF PRIORITY <= THAT OF TIMEOUT INTERRUPT ARE DISABLED
    
    // get current time
    Time now = Now();

    // ready processes whose timeouts are due
    Timeout *head = timerQ.head; 
    while (head != NULL && head->time <= now) {

        // this one is due, so remove it from queue
        removeHead(timerQ);

        // make process waiting on timeout ready if it isn't already
        readyProcessIfNecessary(head->proc);

        // resume at head of queue
        head = timerQ.head;
    }

    // set time for next interrupt if any
    if (timerQ.head != NULL) {
        Time diff = timerQ.head->time - now;
        set_timer_single(TIMER_TIMEOUT, diff);
    }
}

/** Initializes the timer module */
void timer_init()
{
    // initialize elapsed time timer and define an interrupt handler for it
    define_interrupt_handler(INTR_ELAPSED, handle_elapsed_time_interrupt);
    init_timer(TIMER_ELAPSED, INTR_ELAPSED);

    // start the elapsed time timer
    set_timer_repeating(TIMER_ELAPSED, Tick);

    // initialize timeout timer and define an interrupt handler for it
    define_interrupt_handler(INTR_TIMEOUT, handle_timeout_interrupt);
    init_timer(TIMER_TIMEOUT, INTR_TIMEOUT); 
}
