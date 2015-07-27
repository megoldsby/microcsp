
#include "microcsp.h"           // SENDS SINGLE TOKEN AROUND A RING
#include <stdbool.h>
#include <stdio.h>              // underlying system is Linux
#define RING_SIZE 256           // # of processes in ring
#define REPORT_INTERVAL 1000000  
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
    enum { IN=0, OUT };         // branch 0 for input, 1 for output
    Element *element = (Element *)local;
    if (initial()) {                   // exactly one guard active 
        init_alt(element->guards, 2);  // at any one time 
        init_chanin_guard(&element->guards[IN], 
            element->input, &element->token, sizeof(element->token));
        init_chanout_guard(&element->guards[OUT],
            element->output, &element->token);
        element->token = 0;     // if starter start with o/p else i/p
        set_active(&element->guards[IN], !element->start);
        set_active(&element->guards[OUT], element->start);
    } else {
        switch(selected()) {
        case IN:                // just read token, maybe report rate
            if (element->token > 0 && 
                   (element->token % REPORT_INTERVAL == 0)) {
                double sec = (double)(Now() - t0) / NS_PER_SEC;
                printf("Rate = %g\n", sec / (double)element->token);
            }
            element->token++;   // incr token, prepare to write it
            deactivate(&element->guards[IN]);
            activate(&element->guards[OUT]);
            break;
        case OUT:               // just wrote, prepare to read 
            activate(&element->guards[IN]);
            deactivate(&element->guards[OUT]);
            break;
        }
    }
}
int main(int argc, char **argv)
{
    initialize(70*RING_SIZE+24);  // initialize the system
    int i;                        // initialize the channels
    for (i = 0; i < RING_SIZE; i++) {
        init_channel(&channel[i]);
    }
    Element element[RING_SIZE];   // instantiate the ring elements 
    Channel *left, *right;        // connect the ring elements
    for (i = 0, left = &channel[0]; i < RING_SIZE; i++) {
        right = &channel[(i + 1) % RING_SIZE];
        element[i].input = in(left);
        element[i].output = out(right);
        element[i].start = false;
        left = right;
    }
    element[0].start = true;            // make first element starter
    t0 = Now();                         // get the starting time
    for (i = 0; i < RING_SIZE; i++) {   // start ring elements
        START(Element, &element[i], 1);
    }
    run();                              // let them run
}
