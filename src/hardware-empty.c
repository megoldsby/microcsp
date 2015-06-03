
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


/**
 *  Defines an interrupt handler for the given interrupt source.
 */
void define_interrupt_handler(int intrsrc, INTERRUPT_HANDLER handler)
{

}

/** Enable interrupts */
void enable_interrupts()
{
}

/** Disable interrupts */
void disable_interrupts()
{
}

/** Set interval timer for a single interval. */
void set_timer_single(int timerId, Time interval)
{
}

/** Set interval timer for a repeating interval. */
void set_timer_repeating(int timerId, Time interval)
{
}

/** Reads designated timer and returns time */
Time read_timer(int timerId)
{  
}

/** Creates a timer */
void init_timer(int timerId, int intrsrc)
{
}

/** Sends user interrupt via software. */
void send_user_interrupt(int intrsrc)
{
}

/** Should not happen */
void error(char *why)
{
}

/** Copies byte from src to dest */
inline void *Memcpy(void *dest, const void *src, size_t n)
{
}

/** Allocates memory */
inline void *Malloc(size_t size)
{
}

inline int Printf(char *fmt, ...)
{
}

/** Initializes this module. */
void hardware_init()
{
}

