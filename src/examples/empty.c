
/**
 *  Minimal test: should print 'In Proc1_rtc' exactly once.
 */

#include "microcsp.h"
#include <stdio.h>

PROCESS(Proc1)
// no local variables
ENDPROC;
void Proc1_rtc(Proc1 *local)
{ 
    printf("In Proc1_rtc\n");
    terminate();
}

int main(int argc, char **argv)
{
    initialize(32768);

    Proc1 local;
    START(Proc1, &local, 1);

    run();
}
