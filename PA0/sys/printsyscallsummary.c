#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <stdio.h>
#include <lab0.h>

static const char *sysname[NSYSCALLS] = {
    "sys_freemem","sys_chprio","sys_getpid","sys_getprio","sys_gettime",
    "sys_kill","sys_receive","sys_recvclr","sys_recvtim","sys_resume",
    "sys_scount","sys_sdelete","sys_send","sys_setdev","sys_setnok",
    "sys_screate","sys_signal","sys_signaln","sys_sleep","sys_sleep10",
    "sys_sleep100","sys_sleep1000","sys_sreset","sys_stacktrace",
    "sys_suspend","sys_unsleep","sys_wait"
};

int trace = 0;
struct sc_stat scstats[NPROC][NSYSCALLS];

void syscallsummary_start(void) {
    trace = 0;
    for (int p = 0; p < NPROC; p++) {
        for (int s = 0; s < NSYSCALLS; s++) {
            scstats[p][s].ct = 0;
            scstats[p][s].tt = 0;
        }
    }
    trace = 1;
}

void syscallsummary_stop(void) {
    trace = 0;
}

void printsyscallsummary(void) {
    int prev = trace;
    trace = 0;

    for (int p = 0; p < NPROC; p++) {
        int flag = 0;
        for (int s = 0; s < NSYSCALLS; s++) {
            if (scstats[p][s].ct) {
                flag = 1; 
                break; 
            }
        }
        if (flag) {
            kprintf("Process [pid:%d]\n", p);
            for (int s=0; s < NSYSCALLS; s++) {
                int c = scstats[p][s].ct;
                if (c) {
                    unsigned long avg = scstats[p][s].tt / c;
                    kprintf("\tSyscall: %s, count: %u, average execution time: %lu (ms)\n", sysname[s], c, avg);
                }
            }
        }
    }

    trace = prev;
}