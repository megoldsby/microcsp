
#include "microcsp.h"           // SENDS SINGLE TOKEN AROUND A RING
#include <stdbool.h>
#include <stdio.h>              // underlying system is Linux
#define RING_SIZE 100           // # of processes in ring
#define NS_PER_SEC  1000000000ULL
Channel channel[RING_SIZE];     // channels connecting the ring
static Time t0;                 //starting time
PROCESS(Element)                // THE RING ELEMENT'S LOCAL VARIABLES
    Guard guards[2];            //...................................     
    ChanIn *input;              //...................................
    ChanOut *output;            //...................................
    int token;                  //...................................
    _Bool start;                //...................................
ENDPROC                         //...................................
void Element_rtc(void *local)   // THE RING ELEMENT'S LOGIC
{ 
    enum { IN=0, OUT };                 // branch 0 for input, 1 for output
    Element *element = (Element *)local;
    if (initial()) {
        init_alt(element->guards, 2);   // exactly one guard active at any one time
        init_chanin_guard(&element->guards[IN], 
            element->input, &element->token, sizeof(element->token));
        init_chanout_guard(&element->guards[OUT],
            element->output, &element->token);
        element->token = 0;            // if starter, start with output else input
        set_active(&element->guards[IN], !element->start);
        set_active(&element->guards[OUT], element->start);
    } else {
        switch(selected()) {
        case IN:                // just read token, maybe report rate
            if (element->token > 0 && (element->token % 1000000 == 0)) {
                printf("token=%d, Now()=%llu, t0=%llu\n", 
                    element->token,
                    (unsigned long long)Now(),
                    (unsigned long long)t0);
                double sec = (double)(Now() - t0) / NS_PER_SEC;
                printf("Rate = %g\n", sec / (double)element->token);
            }
            element->token++;  // incr token and prepare to write it
            deactivate(&element->guards[IN]);
            activate(&element->guards[OUT]);
            break;
        case OUT:             // just wrote token, prepare to read it
            activate(&element->guards[IN]);
            deactivate(&element->guards[OUT]);
            break;
        }
    }
}
int main(int argc, char **argv)
{
    initialize(70*RING_SIZE+24);          // initialize the system
    t0 = Now();                           // get the starting time
    int i;                                // initialize the channels
    for (i = 0; i < RING_SIZE; i++) {
        init_channel(&channel[i]);
    }
    Element element[RING_SIZE];           // instantiate the elements of the ring
    Channel *left, *right;                // connect the elements of the ring
    for (i = 0, left = &channel[0]; i < RING_SIZE-1; i++) {
        right = &channel[(i + 1) % RING_SIZE];
        element[i].input = in(left);
        element[i].output = out(right);
        element[i].start = false;
        left = right;
    }
    element[i].input = in(left);    
    element[i].output = out(&channel[0]);    
    element[i].start = true;             // make last element the starter
    for (i = 0; i < RING_SIZE; i++) {    // start the elements of the ring
        START(Element, &element[i], 1);
    }
    run();                               // let them run
}
