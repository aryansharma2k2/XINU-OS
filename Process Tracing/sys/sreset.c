/* sreset.c - sreset */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sem.h>
#include <stdio.h>
#include <lab0.h>

/*------------------------------------------------------------------------
 *  sreset  --  reset the count and queue of a semaphore
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000;
extern int currpid;

SYSCALL sreset(int sem, int count)
{
	long ts;
    if (trace) ts = ctr1000;

	STATWORD ps;    
	struct	sentry	*sptr;
	int	pid;
	int	slist;

	disable(ps);
	if (isbadsem(sem) || count<0 || semaph[sem].sstate==SFREE) {
		restore(ps);
		if (trace) {
			scstats[currpid][IDX_sreset].ct++;
			scstats[currpid][IDX_sreset].tt += (ctr1000 - ts);
		}
		return(SYSERR);
	}
	sptr = &semaph[sem];
	slist = sptr->sqhead;
	while ((pid=getfirst(slist)) != EMPTY)
		ready(pid,RESCHNO);
	sptr->semcnt = count;
	resched();
	restore(ps);
	if (trace) {
		scstats[currpid][IDX_sreset].ct++;
		scstats[currpid][IDX_sreset].tt += (ctr1000 - ts);
	}
	return(OK);
}
