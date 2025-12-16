#ifndef _LAB0_H_
#define _LAB0_H_
#include <proc.h>
long zfunction(long param);
void printprocstks(int priority);

#define NSYSCALLS 27
struct sc_stat {
    unsigned ct;
    unsigned long tt;
};
extern int trace;
extern struct sc_stat scstats[NPROC][NSYSCALLS];
void syscallsummary_start(void);
void syscallsummary_stop(void);
void printsyscallsummary(void);

#define IDX_freemem     0
#define IDX_chprio      1
#define IDX_getpid      2
#define IDX_getprio     3
#define IDX_gettime     4
#define IDX_kill        5
#define IDX_receive     6
#define IDX_recvclr     7
#define IDX_recvtim     8
#define IDX_resume      9
#define IDX_scount      10
#define IDX_sdelete     11
#define IDX_send        12
#define IDX_setdev      13
#define IDX_setnok      14
#define IDX_screate     15
#define IDX_signal      16
#define IDX_signaln     17
#define IDX_sleep       18
#define IDX_sleep10     19
#define IDX_sleep100    20
#define IDX_sleep1000   21
#define IDX_sreset      22
#define IDX_stacktrace  23
#define IDX_suspend     24
#define IDX_unsleep     25
#define IDX_wait        26
#endif