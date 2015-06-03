

/**
 *  Two processes, one sending interrupts, one receiving them.
 */

#include "microcsp.h"
#include <stdio.h>

#define ONE_SEC 1000000000ULL

Channel chan[2];

PROCESS(Proc1)
    Guard guards[2];
    int intr0;
    int intr1;
ENDPROC
void Proc1_rtc(void *local)
{ 
    enum { INTR0=0, INTR1 };
    Proc1 *proc1 = (Proc1 *)local;
    if (initial()) {
        init_alt(&proc1->guards[0], 2);
        activate(&proc1->guards[0]);
        activate(&proc1->guards[1]);
        connect_interrupt_to_channel(in(&chan[0]), 0);
        init_chanin_guard_for_interrupt(
            &proc1->guards[0], in(&chan[0]), &proc1->intr0);
        connect_interrupt_to_channel(in(&chan[1]), 1);
        init_chanin_guard_for_interrupt(
            &proc1->guards[1], in(&chan[1]), &proc1->intr1);
    }  else {
        printf("boing!\n");
        switch(selected()) {
            case INTR0:
                printf("INTR0 received by Proc1, intr0=%d\n", proc1->intr0);
                break; 

            case INTR1:
                printf("INTR1 received by Proc1, intr1=%d\n", proc1->intr1);
                break; 
        } 
    }
}

PROCESS(Proc2)
    Guard guards[1];
    Timeout timeout;
    uint8_t which;    
ENDPROC
void Proc2_rtc(void *local)
{ 
    Proc2 *proc2 = (Proc2 *)local;
    if (initial()) {
        init_alt(&proc2->guards[0], 2);
        activate(&proc2->guards[0]);
        init_timeout_guard(&proc2->guards[0], &proc2->timeout, Now()+ONE_SEC);
        proc2->which = 0;
    }  else {
        send_software_interrupt(proc2->which);
        proc2->which = (proc2->which + 1) % 2;
        init_timeout_guard(&proc2->guards[0], &proc2->timeout, Now()+ONE_SEC);
    }
}

int main(int argc, char **argv)
{
    initialize(32768);

    Proc1 local1;
    START(Proc1, &local1, 1);
    Proc2 local2;
    START(Proc2, &local2, 2);

    run();
}
