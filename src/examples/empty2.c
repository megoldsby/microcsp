

/**
 */

#include "microcsp.h"
#include <stdio.h>

PROCESS(Proc1)
// no local variables
ENDPROC;
void Proc1_rtc(Proc1 *local)
{ 
    printf("In Proc1_rtc\n");
    //terminate();
}

PROCESS(Proc2)
// no local variables
ENDPROC;
void Proc2_rtc(Proc2 *local)
{ 
    printf("In Proc2_rtc\n");
    //terminate();
}

int main(int argc, char **argv)
{
    initialize(32768);

    Proc1 local1;
    START(Proc1, &local1, 1);
    Proc2 local2;
    //START(Proc2, &local2, 1);
    START(Proc2, &local2, 2);

    run();
}
