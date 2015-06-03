

#ifndef  INTERNALS_HARDWARE_H
#define  INTERNALS_HARDWARE_H

#include "../timer.h"
#include <stddef.h>

/** timer ids */
#define TIMER_0 0
#define TIMER_1 1
#define NTIMERS 2

/** Interrupt sources */
#define INTR_ELAPSED   0
#define INTR_TIMEOUT   1
#define INTR_INTERPROC 2    // for future use
#define INTR_USER0     3
#define INTR_USER1     4
#define INTR_USER2     5
#define NINTR_SOURCES  6

/** Macros to enable and disable interrupts */
#define ENABLE  enable_interrupts()
#define DISABLE disable_interrupts()

/** Interrupt handler and function to define interrupt handler */
typedef void (*INTERRUPT_HANDLER)(int);
void define_interrupt_handler(int intrsrc, INTERRUPT_HANDLER handler);

/** Enable interrupts */
void enable_interrupts();

/** Disable interrupts */
void disable_interrupts();

/** Set interval timer for a single interval. */
void set_timer_single(int timerId, Time interval);

/** Set interval timer for a repeating interval. */
void set_timer_repeating(int timerId, Time interval);

/** Read designated timer */
Time read_timer(int timerId);

/** Initialize timer */
void init_timer(int timerId, int intrsrc);

/** Send user interrupt via software. */
void send_user_interrupt(int intrsrc);

/** Quit with error, reporting why */
void error(char *why);

/** Display formatted */
inline int Printf(char *fmt, ...);

/** Copy bytes */
inline void *Memcpy (void *dest, const void *src, size_t n);

/** Allocate memory */
inline void *Malloc(size_t size);

/** Initialize this module. */
void hardware_init();

#endif
