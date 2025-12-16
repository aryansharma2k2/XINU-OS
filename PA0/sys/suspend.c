/* suspend.c - suspend */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <stdio.h>
#include <lab0.h>

/*------------------------------------------------------------------------
 *  suspend  --  suspend a process, placing it in hibernation
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000;
extern int currpid;

SYSCALL	suspend(int pid)
{
	long ts;
    if (trace) ts = ctr1000;

	STATWORD ps;    
	struct	pentry	*pptr;		/* pointer to proc. tab. entry	*/
	int	prio;			/* priority returned		*/

	disable(ps);
	if (isbadpid(pid) || pid==NULLPROC ||
	 ((pptr= &proctab[pid])->pstate!=PRCURR && pptr->pstate!=PRREADY)) {
		restore(ps);
		if (trace) {
			scstats[currpid][IDX_suspend].ct++;
			scstats[currpid][IDX_suspend].tt += (ctr1000 - ts);
		}
		return(SYSERR);
	}
	if (pptr->pstate == PRREADY) {
		pptr->pstate = PRSUSP;
		dequeue(pid);
	}
	else {
		pptr->pstate = PRSUSP;
		resched();
	}
	prio = pptr->pprio;
	restore(ps);
	if (trace) {
		scstats[currpid][IDX_suspend].ct++;
		scstats[currpid][IDX_suspend].tt += (ctr1000 - ts);
	}
	return(prio);
}
