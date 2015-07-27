
/*
 * The commstime demo.
 */
#include "microcsp.h"
#include <stdio.h>
#define REPORT_INTERVAL 1000000

/** Prefix process */
PROCESS(Prefix)
    int x;
    ChanIn *in;
    ChanOut *out;
    Guard guards[2];
ENDPROC
void Prefix_rtc(void *local)
{
    enum {IN=0, OUT };
    Prefix *prefix = (Prefix *)local;
    if (initial()) {
        init_alt(prefix->guards, 2);
        init_chanin_guard(&prefix->guards[IN], prefix->in, &prefix->x, sizeof(int));
        init_chanout_guard(&prefix->guards[OUT], prefix->out, &prefix->x);
        activate(&prefix->guards[IN]);     // will initially output
        deactivate(&prefix->guards[OUT]);
    } 
    // reverse guards each time around
    set_active(&prefix->guards[IN], !is_active(&prefix->guards[IN]));
    set_active(&prefix->guards[OUT], !is_active(&prefix->guards[OUT]));
}

/** Delta process */
PROCESS(Delta)
    int x;
    ChanIn *in;
    ChanOut *out1;
    ChanOut *out2;
    Guard guards[3];
ENDPROC
void Delta_rtc(void *local)
{
    enum {IN=0, OUT1, OUT2 };
    Delta *delta = (Delta *)local;
    Guard *inGuard = &delta->guards[IN];
    Guard *out1Guard = &delta->guards[OUT1];
    Guard *out2Guard = &delta->guards[OUT2];
    if (initial()) {
        init_alt(delta->guards, 3);
        init_chanin_guard(inGuard, delta->in, &delta->x, sizeof(int));
        init_chanout_guard(out1Guard, delta->out1, &delta->x);
        init_chanout_guard(out2Guard, delta->out2, &delta->x);
        activate(inGuard);      // will initially input
        deactivate(out1Guard);
        deactivate(out2Guard);
    } else {
        switch(selected()) {
        case IN:                    // just read value
            deactivate(inGuard);    // will write it on out1
            activate(out1Guard);
            break;

        case OUT1:                  // just wrote value on out1
            deactivate(out1Guard);  // will write it on out2
            activate(out2Guard);
            break;
            
        case OUT2:                  // just wrote value on out2
            deactivate(out2Guard);  // will input value
            activate(inGuard);
            break;
        } 
    }
}

/** Succ process */
PROCESS(Succ)
    int x;
    ChanIn *in;
    ChanOut *out;
    Guard guards[2];
ENDPROC
void Succ_rtc(void *local)
{
    enum {IN=0, OUT };
    Succ *succ = (Succ *)local;
    Guard *inGuard = &succ->guards[IN];
    Guard *outGuard = &succ->guards[OUT];
    if (initial()) {
        init_alt(succ->guards, 2);
        init_chanin_guard(inGuard, succ->in, &succ->x, sizeof(int));
        init_chanout_guard(outGuard, succ->out, &succ->x);
        deactivate(inGuard);     // will initially input
        activate(outGuard);
    }
    succ->x += 1;      // input overwrites, incr before output
    set_active(inGuard, !is_active(inGuard));
    set_active(outGuard, !is_active(outGuard));
}

/** Consume process */
PROCESS(Consume)
    Time t0;
    int x;
    ChanIn *in;
    Guard guards[1];
ENDPROC
void Consume_rtc(void *local)
{
    Consume *consume = (Consume *)local;
    if (initial()) {
        init_alt(consume->guards, 1);
        init_chanin_guard(consume->guards, consume->in, &consume->x, sizeof(int));
        activate(consume->guards);
        consume->t0 = Now();       // starting time
    } else {
        if (consume->x % REPORT_INTERVAL == 0 && consume->x > 0) {
            Time t = Now();     
            printf("Time = %llu nsec\n", (unsigned long long)t);
            double rate = (double)(t - consume->t0) / REPORT_INTERVAL;
            printf("Time per loop = %g nsec\n", rate);
            printf("Context switch = %g nsec\n", (rate/4));
            consume->t0 = Now();   // starting time
        }
    }
}

// channels
Channel a, b, c, d;

int main(int argc, char ** argv)
{
    // initialize system
    initialize(1024);

    // initialize the channels
    init_channel(&a);
    init_channel(&b);
    init_channel(&c);
    init_channel(&d);

    // instantiate the processes
    Prefix prefix;    
    Delta delta;
    Succ succ;
    Consume consume;

    // connect them
    prefix.in = in(&d);
    prefix.out = out(&a);
    delta.in = in(&a);
    delta.out1 = out(&b);
    delta.out2 = out(&c);
    succ.in = in(&b);
    succ.out = out(&d);
    consume.in = in(&c);

    // and start them
    START(Prefix, &prefix, 1);
    START(Delta, &delta, 1);
    START(Succ, &succ, 1);
    START(Consume, &consume, 1);
     
    // let them run
    run();
}
