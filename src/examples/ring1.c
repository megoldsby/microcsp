
/**
 *  Sends single token around a ring.
 *  Like ring test on github go site.
 */

#include "microcsp.h"
#include <stdbool.h>
#include <stdio.h>

#define RING_SIZE 100   // # of processes in ring
#define NS_PER_SEC  1000000000ULL

Channel channel[RING_SIZE];

static Time t0;    //starting time

PROCESS(Element)
    Guard guards[2];
    ChanIn *input; 
    ChanOut *output;
    int token;        // received/sent token
    _Bool start;      // if true, start with output
ENDPROC
void Element_rtc(void *local)
{ 
    // branch 0 for input, branch 1 for output 
    enum { IN=0, OUT };

    Element *element = (Element *)local;
    if (initial()) {
        // exactly one guard will be active at any one time
        init_alt(element->guards, 2);
        init_chanin_guard(&element->guards[IN], 
            element->input, &element->token, sizeof(element->token));
        init_chanout_guard(&element->guards[OUT],
            element->output, &element->token);
        
        // if this element is the starter, start with output active,
        // otherwise start with input active
        element->token = 0;
        set_active(&element->guards[IN], !element->start);
        set_active(&element->guards[OUT], element->start);

    } else {
        switch(selected()) {
        case IN:                
            // just input token, maybe report rate
            if (element->token > 0 && (element->token % 1000000 == 0)) {
                printf("token=%d, Now()=%llu, t0=%llu\n", 
                    element->token,
                    (unsigned long long)Now(),
                    (unsigned long long)t0);
                double sec = (double)(Now() - t0) / NS_PER_SEC;
                printf("Rate = %g\n", sec / (double)element->token);
            }
            // incr token and prepare to output it
            element->token++;
            deactivate(&element->guards[IN]);
            activate(&element->guards[OUT]);
            break;

        case OUT:           
            // just output token, prepare to input it
            activate(&element->guards[IN]);
            deactivate(&element->guards[OUT]);
            break;
        }
    }
}

int main(int argc, char **argv)
{
    initialize(70*RING_SIZE+24);

    // get the starting time
    t0 = Now();

    //  initialize the channels
    int i;
    for (i = 0; i < RING_SIZE; i++) {
        init_channel(&channel[i]);
    }

    // connect the elements of the ring
    Element element[RING_SIZE];  
    Channel *left, *right;
    for (i = 0, left = &channel[0]; i < RING_SIZE-1; i++) {
        right = &channel[(i + 1) % RING_SIZE];
        element[i].input = in(left);
        element[i].output = out(right);
        element[i].start = false;
        left = right;
    }
    element[i].input = in(left);    
    element[i].output = out(&channel[0]);    
    element[i].start = true;

    // start the elements of the ring
    for (i = 0; i < RING_SIZE; i++) {
        START(Element, &element[i], 1);
    }

    run();
}
