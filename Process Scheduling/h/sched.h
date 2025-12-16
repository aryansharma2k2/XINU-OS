#ifndef _SCHED_H_
#define _SCHED_H_

#define EXPDISTSCHED 1
#define LINUXSCHED 2

void setschedclass(int sched);
int  getschedclass(void);

#endif