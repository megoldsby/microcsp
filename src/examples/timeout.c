
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
    } 
    printf("Time now %llu\n", (unsigned long long)Now());
    init_timeout_guard(&proc1->guards[0], &proc1->timeout, Now()+ONE_SEC);
}

int main(int argc, char **argv)
{
    initialize(32768);

    Proc1 local;
    START(Proc1, &local, 1);

    run();
}
