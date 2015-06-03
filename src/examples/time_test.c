
/**
 *  Minimal test: should print 'In Proc1_rtc' exactly once.
 */

#include "microcsp.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

#define NS_PER_SEC 1000000000
#define NS_PER_US 1000

static void sleeper(int sec)
{
    struct timespec req;    
    struct timespec rem;    
    req.tv_sec = sec;
    req.tv_nsec = 0;
    int r = -1;
    while (r) {
        r = nanosleep(&req, &rem);
        req = rem;
    } 
}

Time get_system_time()
{
    struct timeval time;
    int r = gettimeofday(&time, NULL);
    if (r) error("get_system_time gettimeofday");
    Time t = (uint64_t)time.tv_sec * NS_PER_SEC + (uint64_t)time.tv_usec * NS_PER_US;
    return t;
}

PROCESS(Proc1)
// no local variables
ENDPROC;
void Proc1_rtc(Proc1 *local)
{ 
    printf("In Proc1_rtc\n");
    Time st0 = get_system_time();
    Time t0 = Now();
    while (true) {
        Time st = get_system_time();
        Time t = Now();
        printf("sys %llu, now %llu\n", 
            (unsigned long long)(st-st0), (unsigned long long)(t-t0));
        sleeper(1);
    }    
    terminate();
}

int main(int argc, char **argv)
{
    initialize(32768);

    Proc1 local;
    START(Proc1, &local, 1);

    run();
}
