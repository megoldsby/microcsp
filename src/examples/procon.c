
/**
 *  Producer/consumer example with two producers.
 *  Can also demonstrate difference between fair alt and alt pri.
 */

#include "microcsp.h"
#include <stdio.h>

Channel chan1, chan2;

PROCESS(Producer)
    Guard guards[1];
    int start;
    ChanOut *chanout;
    int x;
ENDPROC
void Producer_rtc(void *local)
{ 
    Producer *producer = (Producer *)local;
    if (initial()) {
        init_alt(&producer->guards[0], 1);
        activate(&producer->guards[0]);
        producer->x = producer->start;
        init_chanout_guard(&producer->guards[0], producer->chanout, &producer->x);
    } else {
        producer->x += 2;
    }
}

PROCESS(Consumer)
    Guard guards[2];
    int val;
ENDPROC
void Consumer_rtc(void *local)
{ 
    Consumer *consumer = (Consumer *)local;
    if (initial()) {
        printf("Consumer initial\n");
        init_alt(&consumer->guards[0], 2);
        //init_alt_pri(&consumer->guards[0], 2);
        activate(&consumer->guards[0]);
        init_chanin_guard(
            &consumer->guards[0], in(&chan1), &consumer->val, sizeof(int));
        activate(&consumer->guards[1]);
        init_chanin_guard(
            &consumer->guards[1], in(&chan2), &consumer->val, sizeof(int));
    } else {
        printf("Consumer: received message %d in branch %d\n", 
            consumer->val, selected());
    }
}

int main(int argc, char **argv)
{
    initialize(32768);

    init_channel(&chan1);
    init_channel(&chan2);
    Producer producer1;
    producer1.start = 1;
    producer1.chanout = out(&chan1);
    START(Producer, &producer1, 1);
    Producer producer2;
    producer2.start = 2;
    producer2.chanout = out(&chan2);
    START(Producer, &producer2, 1);
    Consumer consumer;
    START(Consumer, &consumer, 1);

    run();
}
