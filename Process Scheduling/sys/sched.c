#include <sched.h>

static int _curr_sched_class = 0; //XINU sched is default

void setschedclass(int sched)
{
    _curr_sched_class = sched;
}

int getschedclass(void)
{
    return _curr_sched_class;
}