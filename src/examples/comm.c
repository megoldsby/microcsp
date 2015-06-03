
/**
 *  One process sends a message to another.
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
        sender->x = 123;
        init_chanout_guard(&sender->guards[0], out(&chan), &sender->x);
    } else {
        printf("Sender: message sent\n");
        terminate();
    }
}

PROCESS(Receiver)
    Guard guards[1];
    int y;
ENDPROC
void Receiver_rtc(void *local)
{ 
    Receiver *receiver = (Receiver *)local;
    if (initial()) {
        init_alt(&receiver->guards[0], 1);
        activate(&receiver->guards[0]);
        init_chanin_guard(
            &receiver->guards[0], in(&chan), &receiver->y, sizeof(int));
    } else {
        printf("Receiver: received message %d\n", receiver->y);
        terminate();
    }
}

int main(int argc, char **argv)
{
    initialize(32768);

    init_channel(&chan);
    Sender slocal;
    START(Sender, &slocal, 1);
    Receiver rlocal;
    START(Receiver, &rlocal, 2);

    run();
}
