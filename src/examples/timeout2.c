
/**
 *  Two processes doing timeouts.
 */

#include "microcsp.h"
#include <stdio.h>

#define ONE_SEC 1000000000ULL

PROCESS(Proc1)
    Guard guards[1];
    Timeout timeout;
ENDPROC
void Proc1_rtc(void *local)
{ 
    Proc1 *proc1 = (Proc1 *)local;
    if (initial()) {
        init_alt(&proc1->guards[0], 1);
        activate(&proc1->guards[0]);
        init_timeout_guard(&proc1->guards[0], &proc1->timeout, Now()+ONE_SEC);
    }  else {
        init_timeout_guard(&proc1->guards[0], &proc1->timeout, Now()+2*ONE_SEC);
    }
    printf("Proc1: time now %llu\n", (unsigned long long)Now());
}

PROCESS(Proc2)
    Guard guards[1];
    Timeout timeout;
ENDPROC
void Proc2_rtc(void *local)
{ 
    Proc2 *proc2 = (Proc2 *)local;
    if (initial()) {
        init_alt(&proc2->guards[0], 1);
        activate(&proc2->guards[0]);
        init_timeout_guard(&proc2->guards[0], &proc2->timeout, Now()+2*ONE_SEC);
    }  else {
        init_timeout_guard(&proc2->guards[0], &proc2->timeout, Now()+2*ONE_SEC);
    }
    printf("Proc2: time now %llu\n", (unsigned long long)Now());
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
