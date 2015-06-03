
/**
 *  Two-process synchronization performance test.
 */

#include "microcsp.h"
#include <stdio.h>

Channel chan;

PROCESS(Sender)
    Guard guards[1];
    int x;
ENDPROC
void Sender_rtc(void *local)
{ 
    Sender *sender = (Sender *)local;
    if (initial()) {
        init_alt(&sender->guards[0], 1);
        activate(&sender->guards[0]);
        sender->x = 0;
        init_chanout_guard(&sender->guards[0], out(&chan), NULL);
    } else {
    }
}

PROCESS(Receiver)
    Guard guards[1];
    Time t0;
    int val;
ENDPROC
void Receiver_rtc(void *local)
{ 
    Receiver *receiver = (Receiver *)local;
    if (initial()) {
        receiver->t0 = Now();    
        printf("Receiver initial\n");
        init_alt(&receiver->guards[0], 1);
        activate(&receiver->guards[0]);
        init_chanin_guard(&receiver->guards[0], in(&chan), NULL, 0);
        receiver->val = 0;
    } else {
        if ((++receiver->val % 1000000) == 0) {
            Time t = Now();
            double rate = (double)receiver->val / (t - receiver->t0);
            double per = 1 / rate;
            printf("val = %d, %g nsec per communication\n", receiver->val, per);
        }
    }
}

int main(int argc, char **argv)
{
    initialize(32768);

    init_channel(&chan);
    Sender sender;
    START(Sender, &sender, 1);
    Receiver receiver;
    START(Receiver, &receiver, 1);

    run();
}
