
/**
 *  Sends tokens around a ring.
 *  Replicates kroc mtring
 */

#include "microcsp.h"
#include <math.h>
#include <stdio.h>
#include <time.h>

#define RING_SIZE 256   // # of processes in ring
#define NTOKENS 64      // # of processes in ring
#define CYCLES  10000   // # of times around the ring 
Time t0;                // starting time

Channel channel[RING_SIZE];

PROCESS(Element)
    Guard guards[2];
    ChanIn *input;
    ChanOut *output;
    int token;     
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

        // process will start by inputting a token
        activate(&element->guards[IN]);
        deactivate(&element->guards[OUT]);
        
    } else {

        switch (selected()) {
        case IN:
            // incr token if nonzero, in any case prepare to output it
            if (element->token > 0) {
                element->token++;
            }
            deactivate(&element->guards[IN]);
            activate(&element->guards[OUT]); 
            break;
      
        case OUT:
            // if zero token, end process, else prepare to input next token
            if (element->token == 0) {
                terminate();
            }
            activate(&element->guards[IN]);
            deactivate(&element->guards[OUT]);
            break;
        }
    }
}

PROCESS(Root)
    Guard guards[2];
    int cycles;         // # of times around ring
    short nrTokens;     // # of tokens in flight
    ChanIn *input;     
    ChanOut *output;
    int token;         
    int count;   
    int sum;            // sum of token values in last cycle
    uint8_t state;      // state of process
ENDPROC
void Root_rtc(void *local)
{
    // branch 0 for input, branch 1 for output 
    enum { IN=0, OUT };

    // state values
    enum {
        INIT=0,     
        INJECTING,
        CYCLING,
        COLLECTING,
        FINAL
    };

    Root *root = (Root *)local;
    if (initial()) {

        // exactly one guard will be active at any one time
        init_alt(root->guards, 2);
        init_chanin_guard(&root->guards[IN], 
            root->input, &root->token, sizeof(root->token));
        init_chanout_guard(&root->guards[OUT], root->output, &root->token);

        // process will start by outputting a token '1'
        root->state = INIT;
        root->token = 1;
        deactivate(&root->guards[IN]);
        activate(&root->guards[OUT]);

    } else {        

        switch(root->state) {
        case INIT:
            switch(selected()) {
            case OUT:
                // just output initial token, prepare to input it
                activate(&root->guards[IN]);
                deactivate(&root->guards[OUT]);
                break;
            case IN:
                // just input the initial token, prepare to inject
                deactivate(&root->guards[IN]);
                activate(&root->guards[OUT]);
                root->token = 1;
                root->state = INJECTING;
                root->count = 0;
                printf("start\n");
                break;
            }
            break;
                 
        case INJECTING:
            switch(selected()) {
            case OUT:
                // keep injecting until reach nrTokens, then start cycling
                if (++root->count < root->nrTokens) {
                    root->token++;
                } else {
                    root->state = CYCLING;
                    root->count = 0;
                    activate(&root->guards[IN]);
                    deactivate(&root->guards[OUT]);
                }
                break;
            case IN:
                error("No inputs in INJECTING state");
                break;
            }
            break;

        case CYCLING:
            switch(selected()) {
            case IN:
                // just input token, prepare to output it
                root->token++;
                deactivate(&root->guards[IN]);
                activate(&root->guards[OUT]);
                break;
            case OUT:
                // if cycles done, prepare to read and sum tokens
                if (++root->count == root->cycles * NTOKENS) {
                    root->state = COLLECTING;
                    root->count = root->sum = 0;
                }
                // in any case, prepare to read token 
                activate(&root->guards[IN]);
                deactivate(&root->guards[OUT]);
                break;
            }
            break;

        case COLLECTING:
            switch(selected()) {
            case IN:
                // add received token to sum,
                // if have read all tokens, display sum and prepare to
                // output zero token
                root->sum += root->token;     
                if (++root->count == root->nrTokens) {
                    printf("Sum of tokens = %d\n", root->sum);
                    root->token = 0;
                    root->state = FINAL;
                    deactivate(&root->guards[IN]);
                    activate(&root->guards[OUT]);
                }
                break;
            case OUT:
                error("No input in COLLECTING state");
                break;
            }
            break;

        case FINAL:
            switch(selected()) {
            case OUT:
                // just sent zero token, prepare to read it
                activate(&root->guards[IN]);
                deactivate(&root->guards[OUT]);
                break;
            case IN:
                // just read zero token, end process
                if (root->token != 0) error("Not zero token!");
                Time t1 = Now();
                printf("Elapsed time = %llu\n", (unsigned long long)(t1-t0));
                terminate();
                break;
            }
            break;
        }//switch(root->state)
    }
}

int main(int argc, char **argv)
{
    // read command-line arguments 
    int cycles = CYCLES;
    int nrTokens = NTOKENS;

    initialize(20520);

    //  initialize the channels
    int i;
    for (i = 0; i < RING_SIZE; i++) {
        init_channel(&channel[i]);
    }

    // connect the members of the ring
    Element element[RING_SIZE-1];  
    for (i = 0; i < RING_SIZE-1; i++) {
        element[i].input = in(&channel[i]);
        element[i].output = out(&channel[i+1]);
    }
    Root root;
    root.input = in(&channel[i]);
    root.output = out(&channel[0]);
    root.cycles = cycles;
    root.nrTokens = nrTokens;

    // get starting time
    t0 = Now();

    // start the ring
    START(Root, &root, 1);
    for (i = 0; i < RING_SIZE-1; i++) {
        START(Element, &element[i], 1);
    }

    run();
}
