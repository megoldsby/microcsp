

/**
 *  Two processes, one sending interrupts, one receiving them.
 */

#include "microcsp.h"
#include <stdio.h>

#define ONE_SEC 1000000000ULL

Channel chan[2];

PROCESS(Proc1)
    Guard guards[2];
ENDPROC
void Proc1_rtc(void *local)
{ 
    enum { CH0=0, CH1 };
    Proc1 *proc1 = (Proc1 *)local;
    if (initial()) {
        init_alt(&proc1->guards[0], 2);
        activate(&proc1->guards[0]);
        activate(&proc1->guards[1]);
        init_chanin_guard(&proc1->guards[0], in(&chan[0]), NULL, 0);
        init_chanin_guard(&proc1->guards[1], in(&chan[1]), NULL, 0);
    }  else {
        printf("boing!\n");
        switch(selected()) {
            case CH0:
                printf("Proc1 receives signal on CH0\n");
                break; 

            case CH1:
                printf("Proc1 receives signal on CH1\n");
                break; 
        } 
    }
}

PROCESS(Proc2)
    Guard guards[2];
    uint8_t which;    
ENDPROC
void Proc2_rtc(void *local)
{ 
    Proc2 *proc2 = (Proc2 *)local;
    if (initial()) {
        init_alt(&proc2->guards[0], 2);
        init_chanout_guard(&proc2->guards[0], out(&chan[0]), NULL);
        activate(&proc2->guards[0]);
        init_chanout_guard(&proc2->guards[1], out(&chan[1]), NULL);
        deactivate(&proc2->guards[1]);
        proc2->which = 0;
    }  else {
        deactivate(&proc2->guards[proc2->which]);
        proc2->which = (proc2->which + 1) % 2;
        activate(&proc2->guards[proc2->which]);
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
