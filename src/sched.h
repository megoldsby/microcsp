
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

#ifndef  SCHED_H
#define  SCHED_H

#include "timer.h"

/** Forward declarations */
typedef struct Channel Channel;
typedef struct ChanIn ChanIn;
typedef struct ChanOut ChanOut;
typedef struct Guard Guard;

#include "internals/sched.h"

// Eight priority levels (0 = Idle process, 7 = most urgent))
#define PRI_MIN      0
#define PRI_MAX      7
#define PRI_DEFAULT  1
/** In effect, ISRs run at priority 8 */

/** Macros for defining a process's name and local variables */
#define               \
PROCESS(P)            \
typedef struct P P;   \
struct P {           
// User puts local variable definitions here
#define ENDPROC       \
};

/** Macro for starting a process */
#define START(P, parg, pri) start(P##_rtc, parg, sizeof(P), pri) 
/** The user must supply a function void (*P_rtc)(P *) */

/** Starts a process */
void start(void (*rtc)(), void *local_struct, unsigned int local_size, int pri);

/** Terminates a process (called by the terminating process) */
void terminate();

/** Initializes channel */
inline void init_channel(Channel *chan);

/** Returns the input end of a channel. */
inline ChanIn *in(Channel *chan);

/** Returns the outut end of a channel. */
inline ChanOut *out(Channel *chan);

/** Initializes alternation */
inline void init_alt(Guard *guards, int size);

/** Initializes channel input guard */
inline void init_chanin_guard(
    Guard *guard, ChanIn *chan, void *dest, unsigned int len);

/** Initizliaes channel input guard for interrupt */
inline void init_chanin_guard_for_interrupt(
                      Guard *guard, ChanIn *chan, void *dest); 

/** Initializes channel output guard */
inline void init_chanout_guard(Guard *guard, ChanOut *chan, void *src);

/** Initializes skip guard */
inline void init_skip_guard(Guard *guard);

/** Initializes timeout guard */
inline void init_timeout_guard(Guard *guard, Timeout *timeout, Time time);

/** Activates a guard */
inline void activate(Guard *guard);

/** Decctivates a guard */
inline void deactivate(Guard *guard);

/** Returns true if guard is active */
inline _Bool is_active(Guard *guard);

/** Makes a guard active or inactive */
inline void set_active(Guard *guard, _Bool active);

/** Returns selected branch of ALT */
int selected();

/** Connects user interrupt to channel */
void connect_interrupt_to_channel(ChanIn *chan, int intrno);

/** Disconnects user interrupt from channel */
void disconnect_interrupt_from_channel(int intrno);

/** Sends a user interrupt via software */
void send_software_interrupt(int intrno);

/** Initializes microcsp */
void initialize(unsigned int memlen);

/** Allows microcsp to run when called by application's main function. */
void run();

/** Returns true if it is process's initial execution */
_Bool initial();

/** Should not happen */
void error(char *why);
void error2(char *why, int errno);

#endif
