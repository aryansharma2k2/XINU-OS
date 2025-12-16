/* getprio.c - getprio */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <stdio.h>
#include <lab0.h>

/*------------------------------------------------------------------------
 * getprio -- return the scheduling priority of a given process
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000;
extern int currpid;

SYSCALL getprio(int pid)
{
	long ts;
    if (trace) ts = ctr1000;

	STATWORD ps;    
	struct	pentry	*pptr;

	disable(ps);
	if (isbadpid(pid) || (pptr = &proctab[pid])->pstate == PRFREE) {
		restore(ps);
		if (trace) {
			scstats[currpid][IDX_getprio].ct++;
			scstats[currpid][IDX_getprio].tt += (ctr1000 - ts);
		}
		return(SYSERR);
	}
	restore(ps);
	if (trace) {
		scstats[currpid][IDX_getprio].ct++;
		scstats[currpid][IDX_getprio].tt += (ctr1000 - ts);
	}
	return(pptr->pprio);
}
