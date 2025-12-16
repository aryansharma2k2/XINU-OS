/* chprio.c - chprio */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <stdio.h>
#include <lab0.h>

/*------------------------------------------------------------------------
 * chprio  --  change the scheduling priority of a process
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000;
extern int currpid;

SYSCALL chprio(int pid, int newprio)
{
	long ts;
    if (trace) ts = ctr1000;

	STATWORD ps;    
	struct	pentry	*pptr;

	disable(ps);
	if (isbadpid(pid) || newprio<=0 ||
	    (pptr = &proctab[pid])->pstate == PRFREE) {
		restore(ps);
		if (trace) {
			scstats[currpid][IDX_chprio].ct++;
			scstats[currpid][IDX_chprio].tt += (ctr1000 - ts);
		}
		return(SYSERR);
	}
	pptr->pprio = newprio;
	restore(ps);
	if (trace) {
		scstats[currpid][IDX_chprio].ct++;
		scstats[currpid][IDX_chprio].tt += (ctr1000 - ts);
	}
	return(newprio);
}
