
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

#include "sched.h"
#include "memory.h"
#include "hardware.h"
#include <stdbool.h>
#include <stddef.h>
//#include <stdio.h>
//#include <string.h>


/** Alternation descriptor */
typedef struct Alternation {
    Guard *guards;    // guards
    uint8_t nrGuards; // # of guards
    uint8_t index;    // current or selected ready branch
    uint8_t count;    // running branch count
    _Bool alt_pri;    // true if alt pri
} Alternation;

/** Process record */
typedef struct Process Process;
typedef struct Process {
    Process *next;           // next process in ready queue
    void (*rtc)(void *);     // function called each time process executes
    Alternation alt;         // the process's ALT record
    uint8_t index;           // memory class of this process
    int8_t pri;              // priority of this process
    int8_t state;            // scheduling state
} Process;
// process state
#define PROC_INITIAL   0    // new process
#define PROC_QUIESCENT 1    // no alternation in progress
#define PROC_ENABLING  2    // enabling the ALT
#define PROC_WAITING   3    // waiting for a ready branch
#define PROC_READY     4    // a branch is ready
#define PROC_DONE      5    // process has terminated

/** Develops pointer to process's local variables given process record */
#define LOCAL(proc)  ((char *)proc + proc_offset)

/** currently executing process */
static Process *current;

/** bit set of priority levels with processes in ready queue */
static uint8_t priority_mask;

/** priority-to-mask translation table */
static uint8_t mask[] = { 1, 2, 4, 8, 16, 32, 64, 128 };

/** ready queue descriptor */
typedef struct ReadyQ {
    Process *head;
    Process *tail;
} ReadyQ;

/** the ready queues, one per priority level */
static ReadyQ readyQ[PRI_MAX+1];

/** offset of process's local variables from start of allocation */
static size_t proc_offset;

/** map from user interrupt number to channel */
static InterruptChannel *channel_for_interrupt[NINTR_SOURCES-INTR_USER0];

/** Returns highest priority that has a ready process 
 *  (Returns zero if none on ready queues) */
static int highest_ready()
{
    /* -------------------------- *
     | constant-time, low-storage |
     | assumes 8 priority levels  |
     * -------------------------- */ 
    // INTERRUPTS MUST BE DISABLED 
    int level;
    unsigned register int ready = priority_mask;
    if (ready & 0b11110000) {
        if (ready & 0b11000000) {
            if (ready & 0b10000000) {
                level = 7;
            } else {
                level = 6;
            }
        } else {
            if (ready & 0b00100000) {
                level = 5;
            } else {
                level = 4;
            }
        }
    } else {
        if (ready & 0b00001100) {
            if (ready & 0b00001000) {
                level = 3;
            } else {
                level = 2;
            }
        } else {
            if (ready & 0b00000010) {
                level = 1;
            } else {
                level = 0;
            }
        }
    }
    
    /*****
    int level = 0;
    if (mask & 0xF0) {
        mask >>= 4;
        level += 4; 
    } 
    if (mask & 0x0C) {
        mask >>= 2;
        level += 2;
    }
    if (mask & 0x02) {
        level += 1;
    }
    *****/

    return level;
    // level 0 (the idle process) is always ready
}

/**
 *  Removes and returns head process from ready queue of
 *  given priority level (returns nil if no such).
 */
static Process *take(int pri)
{
    // INTERRUPTS MUST BE DISABLED
    ReadyQ *queue = &readyQ[pri];
    Process *proc = queue->head;
    if (proc != NULL) {
        queue->head = proc->next;
        if (queue->head == NULL) {
            queue->tail = NULL;
            priority_mask &= ~mask[pri];  // show no ready process at level pri
        }
    }
    return proc;
}

/**
 *  Appends given process to its priority's ready queue.
 */
static void append(Process *proc)
{
    // INTERRUPTS MUST BE DISABLED
    ReadyQ *queue = &readyQ[proc->pri];
    if (queue->tail != NULL) {
        queue->tail->next = proc;
    }
    queue->tail = proc;
    proc->next = NULL;
    if (queue->head == NULL) {
        queue->head = proc;
        priority_mask |= mask[proc->pri];  // show ready process at level pri
    }
/****
    if (queue->head != NULL) {
        proc->next = queue->head;
    } else {
        proc->next = NULL;
        priority_mask |= mask[proc->pri];  // show ready process at level pri
    }    
    queue->head = proc;
****/
}

/**
 *  Returns priority of currently executing process.
 */
int currentPriority()
{
    // there is always a current process (idle process if no other)
    return current->pri;
}

/** 
 *  Starts a process.
 *  rtc is the function called each time the process executes
 *  local_struct is a struct containing all of the process's local variables
 *  local_size is the size of lcoal_struct in bytes
 *  pri is the priority of the new process
 */
void start(void(*rtc)(void *), void *local_struct, unsigned int local_size, int pri)
{
    // Check for valid priority.
    if (pri < 1 || pri > PRI_MAX) error("Invalid priority");

    // Compute size or process record + user's local variables.
    unsigned int total_size = proc_offset + local_size;

    // Allocate memory for combined process record and local vars.
    int index = find_mem_index(total_size);    
    char *record = allocate_mem(index);

    // Copy initial values of user's local vars into record.
    Memcpy(record + proc_offset, local_struct, local_size); 
    
    // Get pointer to process record part of allocation. */    
    Process *proc = (Process *)record;
    
    // Now initialize the process record.
    proc->rtc = rtc;
    proc->next = 0;
    proc->index = index;
    proc->pri = pri;
    proc->state = PROC_INITIAL;
    proc->alt.guards = NULL;
    proc->alt.nrGuards = 0;

    // put new process on its ready queue
    append(proc);
}

/** 
 *  Starts the idle process.
 *  rtc is the function called when the process executes
 */
static void start_idle(void(*rtc)(void *))
{
    // Allocate memory for process record 
    int index = find_mem_index(sizeof(struct Process));    
    char *mem = allocate_mem(index);
     
    // Now initialize the process record.
    Process *proc = (Process *)mem;
    proc->rtc = rtc;
    proc->next = 0;
    proc->index = index;
    proc->pri = PRI_MIN;
    proc->state = PROC_INITIAL;
    proc->alt.guards = NULL;
    proc->alt.nrGuards = 0;

    // make idle process initially the current process
    current = proc;

    // put new process on the ready queue
    append(proc);
}

/**
 *  Terminates a process when (called by the terminating process).
 */
void terminate()
{
    // mark process terminated (scheduler will do the rest)
    current->state = PROC_DONE; 
}

/** Returns pointer to process's Alternation record. */
static inline Alternation *alternation(Process *proc)
{
    return &(proc->alt);
}

/** Returns selected branch of ALT. */
int selected() 
{
    return alternation(current)->index;
}

/** Initializes channel */
inline void init_channel(Channel *chan)
{
    chan->waiting = NULL;
    chan->src = NULL;
}

/** Returns input end of channel. */
inline ChanIn *in(Channel *chan)
{
    return (ChanIn *)chan;
}

/** Returns output end of channel. */
inline ChanOut *out(Channel *chan) 
{
    return (ChanOut *)chan;
}

/** Initializes alternation for fair selection */
void init_alt(Guard *guards, int size) 
{
    Alternation *alt = alternation(current);
    alt->guards = guards;
    alt->nrGuards = size;
    alt->alt_pri = false;
    alt->index = size-1;
}

/** Initializes alternation for priority selection */
void init_alt_pri(Guard *guards, int size) 
{
    Alternation *alt = alternation(current);
    alt->guards = guards;
    alt->nrGuards = size;
    alt->alt_pri = true;
}

/** Initializes channel input guard */
inline void init_chanin_guard(
    Guard *guard, ChanIn *chan, void *dest, unsigned int len)
{
    guard->type = GUARD_CHANIN;    
    guard->chanin.channel = (Channel *)chan;
    guard->chanin.dest = dest;
    guard->chanin.len = len;
}

/** Initializes channel input guard for interrupt */
inline void init_chanin_guard_for_interrupt(
                      Guard *guard, ChanIn *chan, void *dest)
{
    guard->type = GUARD_INTERRUPT;
    guard->interrupt.channel = (InterruptChannel *)chan;
    guard->interrupt.dest = dest;
} 

/** Initializes channel output guard */
inline void init_chanout_guard(Guard *guard, ChanOut *chan, void *src)
{
    guard->type = GUARD_CHANOUT;    
    guard->chanout.channel = (Channel *)chan;
    guard->chanout.src = src;
}

/** Initializes skip guard */
inline void init_skip_guard(Guard *guard)
{
    guard->type = GUARD_SKIP;
}

/** Initializes timeout guard */
inline void init_timeout_guard(Guard *guard, Timeout *timeout, Time time)
{
    guard->type = GUARD_TIMEOUT;
    guard->timeout = timeout;
    timeout->time = time;
    timeout->proc = current;
}

/** Activates a guard */
inline void activate(Guard *guard)
{
   guard->active = true;
}

/** Decctivates a guard */
inline void deactivate(Guard *guard) 
{
    guard->active = false;
}

/** Returns true if guard is active */
inline _Bool is_active(Guard *guard)
{
    return guard->active;
}

/** Makes a guard active or inactive */
inline void set_active(Guard *guard, _Bool active)
{
    guard->active = active;
}

/** Connects user interrupt to channel */
void connect_interrupt_to_channel(ChanIn *chan, int intrno)
{
    // develop user interrupt number from interrupt number
    int intrsrc = intrno + INTR_USER0;

    // check for valid user interrupt numbber
    if (!(INTR_USER0 <= intrsrc && intrsrc < NINTR_SOURCES)) error(
        "connect_interrupt_to_channel: Invalid user interrupt source number");

    // initialize channel for interrupt
    InterruptChannel *ichan = (InterruptChannel *)chan;
    ichan->waiting = NULL;
    ichan->count = 0;                                // reset interrupt count
    channel_for_interrupt[intrsrc-INTR_USER0] = ichan;    // map intr to chan
}

/** Disconnects user interrupt from channel */
void disconnect_interrupt_from_channel(int intrno)
{
    // develop user interrupt number from interrupt number
    int intrsrc = intrno + INTR_USER0;

    // check for valid user interrupt numbber
    if (!(INTR_USER0 <= intrsrc && intrsrc < NINTR_SOURCES)) error(
        "Invalid user interrupt source number");

    channel_for_interrupt[intrsrc-INTR_USER0] = NULL;    // unmap intr 
}

/**
 *  Enables a channel for input.
 */
static _Bool enable_channel_input(Channel *chan, Process **partner)
{
    // INTERRUPTS DISABLED
    if (chan->waiting != NULL) {
        if (chan->waiting == current) {
            return false;               // just us, not ready
        } else {
            *partner = chan->waiting;   // return waiting process
            return true;                // ready
        }
    } else {                          
        chan->waiting = current;        // wait in this channel
        return false;                   // not ready
    }
}

/**
 *  Disables a channel for input.
 */
static _Bool disable_channel_input(Channel *chan, Process **partner)
{
    // INTERRUPTS DISABLED
    if (chan->waiting != NULL) {
        if (chan->waiting != current) {
            *partner = chan->waiting;    // return waiting partner
            return true;                 // ready
        } else {
            chan->waiting = NULL;        // just us, disable channel
            return false;                // not ready
        }
    } else {                             // channel not enabled
        return false;                    // not ready
    } 
}

/**
 *  Enables a channel for output.
 */
static _Bool enable_channel_output(Channel *chan, void *src, Process **partner)
{
    // INTERRUPTS DISABLED
    //ASSERT(chan->waiting != NULL);
    *partner = chan->waiting;        // return partner (inputter)
    chan->waiting = current;         // wait in this channel
    chan->src = src;                 // leave source addr in channel
    return false;                    // not quite ready yet
}

/**
 *  Enables an interrupt channel to receive an interrupt.
 */
static _Bool enable_interrupt_channel(InterruptChannel *chan)
{
    // INTERRUPTS DISABLED
    if (chan->count == 0) {
        chan->waiting = current;  // no interrupt yet, wait in channel
        return false;             // not ready 
    } else {
        return true;              // ready
    }
}

/**
 *  Disables an interrupt channel.
 */
static _Bool disable_interrupt_channel(InterruptChannel *chan)
{
    // INTERRUPTS DISABLED
    chan->waiting = NULL;             // we're not waiting now
    return (chan->count > 0);         // ready if interrupt has occurred
}

/** Adds 1 modulo given modulus */
static inline int plus1_mod(int i, int m) { 
    return (i + 1) % m; 
}

/** Subtracts 1 modulo given modulus */
static inline int minus1_mod(int i, int m) {
    // assume 0 <= i < m
    return (i - 1 + m) % m;
}

/**
 *  Enables given process's ALT and returns true if
 *  it found a ready branch.
 */
static _Bool enable_alt(Process *proc, Process **partner)
{
    // INTERRUPTS ENABLED

    // get pointer to Alternation record in Process record
    Alternation *alt = alternation(proc);

    // initialize ready indicator and returned i/o partner
    _Bool ready = false;
    *partner = NULL;

    // make sure output guard restriction holds
    int nrGuards = alt->nrGuards;
    int i; 
    _Bool activeOutput;
    int nrActive;
    for (i = 0, activeOutput = false, nrActive = 0;
         i < nrGuards;
         i++) {
        Guard *g = &alt->guards[i]; 
        if (g->active) {
            nrActive++;
            if (g->type == GUARD_CHANOUT) {
                activeOutput = true;
            }
        }
    }
    if (activeOutput && nrActive > 1) {
        error("Chanout guard must be only active guard");
    }
        
    // if ALT PRI start at 0th branch, if fair ALT
    // start at next branch past branch last selected 
    int k;
    if (alt->alt_pri) {
        i = 0;
     } else {
        i = plus1_mod(alt->index, nrGuards);
     }
     k = 0;

    // for each guard, in ascending order..
    for (; k < nrGuards; 
           k++, i = plus1_mod(i, nrGuards)) {
        Guard *g  = &alt->guards[i];

        // provided guard is active..
        if (g->active) {
            switch(g->type) {

            case GUARD_CHANIN:
                // enable channel input, leave loop if channel ready
                DISABLE;
                ready = enable_channel_input(g->chanin.channel, partner);
                if (ready) goto Ready;
                ENABLE;
                break;

            case GUARD_CHANOUT:
                // enable channel output, leave loop if channel ready
                DISABLE;
                ready = enable_channel_output(
                    g->chanout.channel, g->chanout.src, partner);
                if (ready) goto Ready;
                ENABLE;
                break;

            case GUARD_SKIP:
                // skip guards are always ready
                DISABLE;
                ready = true;
                goto Ready;
                break;

            case GUARD_TIMEOUT:
                // enable timeout, leave loop if ready
                DISABLE;
                ready = enable_timeout(g->timeout); 
                if (ready) goto Ready;
                ENABLE;
                break;

            case GUARD_INTERRUPT:
                // enable interrupt channel, leave loop if ready
                DISABLE;
                ready = enable_interrupt_channel(g->interrupt.channel);
                if (ready) goto Ready;
                break;
            }//switch
        }//if active
    }//for 
    // no ready branch

    // step back to last guard processed
    i = minus1_mod(i, nrGuards); 
    k--;
    DISABLE;

Ready:
    // INTERRUPTS DISABLED

    // preserve index and count for disable_alt 
    alt->index = i;
    alt->count = k;

    // return ready indicator
    return ready;
}

/**
 *  Disables given process's ALT, selecting a ready branch and 
 *  returning the i/o partner if the selected branch is an input.
 */
static Process *disable_alt(Process *proc)
{
    // INTERRUPTS ENABLED, proc->state = Ready

    // initialize returned i/o partner
    Process *partner = NULL;

    // get pointer to Alternation record in Process record
    Alternation *alt = alternation(proc);

    // start with last guard processed by enable_alt
    int i = alt->index;
    int k = alt->count;
    int nrGuards = alt->nrGuards;

    // for each guard, in descending order..
    for (; k >= 0;
           k--, i = minus1_mod(i, nrGuards)) {
        Guard *g = &alt->guards[i];
        if (g->active) {
            _Bool ready;
            switch(g->type) {

            case GUARD_CHANIN:
                DISABLE;
                ready = disable_channel_input(g->chanin.channel, &partner);
                ENABLE;
                if (ready) alt->index = i;
                break;

            case GUARD_CHANOUT:
                alt->index = i;
                break;

            case GUARD_SKIP:
                alt->index = i;
                break;

            case GUARD_TIMEOUT:
                DISABLE;
                ready = disable_timeout(g->timeout);
                ENABLE;
                if (ready) alt->index = i;
                break;

            case GUARD_INTERRUPT:
                DISABLE;
                ready = disable_interrupt_channel(g->interrupt.channel);
                ENABLE; 
                if (ready) alt->index = i;
            }//switch
        }//if
    }//for
    // alt->index contains index of selected branch

    // INTERRUPTS DISABLED

    // If selected branch is an input, transfer the data
    Guard *g = &alt->guards[alt->index];
    if (g->type == GUARD_CHANIN) {
        // Note partner is not executing yet, so don't need to disable.
        Channel *chan = g->chanin.channel;
        Memcpy(g->chanin.dest, chan->src, g->chanin.len);  // xfr data
        chan->waiting = NULL;	                  // set channel empty

    // If selected branch is an interrupt, clear the interrupt
    // count, first tranferring it if it is wanted
    } else if (g->type == GUARD_INTERRUPT) {
        InterruptChannel *chan = g->interrupt.channel;
        DISABLE;         
        if (g->interrupt.dest != NULL) {
            *(int *)g->interrupt.dest = chan->count;
        }
        chan->count = 0;  
        ENABLE;
    }

    // Return partner if any.
    // (At most one partner was returned by called routines above.)
    return partner;
}

/**
 *  RTC function of the idle process.
 */
static void idle(void *local)
{
Printf("In idle process\n");
    while (true);
}

/**
 *  Scheduler.
 *  input: base_pri      priority at which next lower-level
 *                       scheduler is running
 */
void schedule(int base_pri)
{
    // INTERRUPTS DISABLED
    Process *proc = NULL;     // the process running in this scheduler   //X
    while (true) {                                                       //X
                                                                         //X
        if (proc == NULL) {                                              //X
                                                                         //X
            // get highest priority for which there is a                 //X
            // ready process enqueued                                    //X
            int highest = highest_ready();                               //X
                                                                         //X
            // return if it's no higher than that of previously          //X
            // running scheduler                                         //X
            if (highest <= base_pri) {                                   //X
                return;                                                  //X
            }                                                            //X
                                                                         //X
            // take highest-priority ready process current               //X
            proc = current = take(highest);                              //X
        }                                                                //X
                                                                         //X
        // get scheduling state of current process                       //X
        int state = proc->state;                                         //X
                                                                         //X
        // enable interrupts                                             //X
        ENABLE;                                                          
                                                                         
        _Bool execute = false;    // true if proc should run its RTC code
        Process *partner = NULL; // process's communication partner    
                                                                      
        // if this is a new process..                                
        if (state == PROC_INITIAL) {                                
                                                                        
            // execute process's RTC code and advance state to Quiescent
            proc->rtc(LOCAL(proc));                                    
            proc->state = PROC_QUIESCENT;                                   
                                                                     
        // if process is not involved in an ALT..                   
        } else if (state == PROC_QUIESCENT) {                      
                                                                  
            // advance state to Enabling and enable proc's ALT   
            proc->state = PROC_ENABLING;                        
            _Bool ready = enable_alt(proc, &partner);                    //.
            // INTERRUPTS DISABLED                                       //X
                                                                         //X
            // advance state to Ready if enable_alt found a ready        //X
            // branch otherwise to Waiting                               //X
            if (proc->state == PROC_ENABLING) {                          //X
                proc->state = (ready ? PROC_READY : PROC_WAITING);       //X
            }                                                            //X
                                                                         //X
            // allow interrupts                                          //X
            state = proc->state;                                         //X
            ENABLE;                                                    
                                                                      
            // if process is Ready, either because it found a ready  
            // branch or because some partner readied a branch..    
            if (state == PROC_READY) {                             
                                                                  
                // disable the ALT, performing i/o if appropriate 
                // and discovering the i/o partner if any        
                partner = disable_alt(proc);                             //.
                                                                        
                // prepare to execute process's RTC code and           
                // set state to show process not involved in ALT      
                execute = true;                                      
                proc->state = PROC_QUIESCENT;                       
            }                                                      
                                                                  
        // if process if ready (because some partner readied a   
        // branch or a timeout expired..)                       
        } else if (state == PROC_READY) {                      
                                                              
            // disable the ALT, performing i/o if appropriate          
            // and discovering the i/o partner if any                 
            partner = disable_alt(proc);                                 //.
                                                                         
            // prepare to execute process's RTC code and                
            // set state to show process not involved in ALT           
            execute = true;                                           
            proc->state = PROC_QUIESCENT;                            
                                                                    
        } else {                                                   
            //ASSERT(false);                                      
        }//if                                                        
          
        // if there is an i/o partner..            
        if (partner != NULL) {                    
                                                 
            // disallow interrupts                               
            DISABLE;                                                     //X
                                                                         //X
            // ready the partner if it isn't already ready               //X
            readyProcessIfNecessary(partner);                            //X
                                                                         //X
            // allow interrupts                                          //X
            ENABLE;                                                   
        }                                                          
                                                                       
        // run process's RTC code 
        if (execute) {                                                
            proc->rtc(LOCAL(proc));                                  
        }                                                           
                                                                   
        // disallow interrupts                                    
        DISABLE;                                                         //X
                                                                         //X
        // if process has terminated, release its process record         //X
        if (proc->state == PROC_DONE) {                                  //X
            release_mem(proc->index, (char *)proc);                      //X
            proc = NULL;                                                 //X
                                                                         //X
        // if process is waiting (for i/o, timeout or interrupt),        //X
        // prepare to select new process                                 //X
        } else if (proc->state == PROC_WAITING) {                        //X
            proc = NULL;                                                 //X
        }                                                                //X
                                                                         //X
    }//while                                                             //X
} 

/** 
 *  Makes given process ready if it isn't already. 
 *  Called by e.g. the timeout interrupt handler.
 */
void readyProcessIfNecessary(Process *partner)
{
    // INTERRUPTS MUST BE DISABLED                                       //X
                                                                         //X
    // if process is still enabling, set its state to Ready              //X
    // for process to find when it disables                              //X
    if (partner->state == PROC_ENABLING) {                               //X
        partner->state = PROC_READY;                                     //X
                                                                         //X
    // if process is waiting on its ALT..                                //X
    } else if (partner->state = PROC_WAITING) {                          //X
                                                                         //X
        // advance process to Ready and put it on its ready queue        //X
        partner->state = PROC_READY;                                     //X
        append(partner);                                                 //X
                                                                         //X
        // if partner's priority is higher than current process's..      //X
        if (partner->pri > current->pri) {                               //X
                                                                         //X
            // save the current process                                  //X
            Process *prev = current;                                     //X
                                                                         //X
            // schedule the partner immediately (preemptively)           //X
            schedule(prev->pri);                                         //.
                                                                         //X
            // restore the current process                               //X
            current = prev;                                              //X
        }                                                                //X
                                                                         //X
    } else {                                                             //X
        //ASSERT(false);                                                 //X
    }                                                                    //X 
}

/** Handles user interrupts */
void user_interrupt_handler(int intrsrc)
{
    // INTERRUPTS MUST BE DISABLED
    InterruptChannel *chan = channel_for_interrupt[intrsrc-INTR_USER0];
    if (chan != NULL) {
        chan->count += 1;                         // incr interrupt count
        if (chan->waiting != NULL) {
            Process *waiting = chan->waiting;     // ready waiting process
            readyProcessIfNecessary(waiting);
        }
    }
}

/** Sends a user interrupt via software */
void send_software_interrupt(int intrno)
{
    // developer interrupt source number from interrupt number
    int intrsrc = intrno + INTR_USER0;

    // make sure it's valid interrupt soruce
    if (!(INTR_USER0 <= intrsrc < NINTR_SOURCES)) error("Invalid user interrupt number");

    // send user interrupt
    send_user_interrupt(intrsrc);
}

static void sched_init()
{
    // Calculate offset of process's local variables in 
    // combined process record/local vars allocation
    struct X { Process proc; void *start; };
    proc_offset = offsetof(struct X, start);

/***
#include <stdio.h>
Printf("proc_offset = %d\n", proc_offset);
Guard g[4];
Printf("sizeof(RealGuard[4]) = %d\n", sizeof(g));
Printf("sizeof(Alternation) = %d\n", sizeof(Alternation));
Channel c;
Printf("sizeof(Channel) = %d\n", sizeof(c));
***/

    // initialize the ready queues
    int i;
    for (i = PRI_MIN; i <= PRI_MAX; i++) {
        ReadyQ *q = &readyQ[i];
        q->head = q->tail = NULL;
    } 

    // define user interrupt handlers
    for (i = INTR_USER0; i < NINTR_SOURCES; i++) {
        define_interrupt_handler(i, user_interrupt_handler);
    }

    // start the idle process
    start_idle(idle);
}

/** Returns true if it is process's initial execution */
_Bool initial()
{
    return (current->state == PROC_INITIAL);
}

/**
 *  Initializes the system.
 *  memlen:  bytes of memory for dynamic allocation (process records)
 */
void initialize(unsigned int memlen)
{
    // initialize hardware module
    hardware_init();         // returns with interrupts disabled

    // and memory module
    memory_init(memlen);

    // and this module
    sched_init();

    // and timer module
    timer_init();
}

/** Called by main function to allow microcsp to run */
void run()
{
    // INTERRUPTS DISABLED

    // engage the scheduler
    schedule(PRI_MIN-1);
    //ASSERT(false);
}
