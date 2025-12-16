/* wait.c - wait */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sem.h>
#include <stdio.h>
#include <lab0.h>

/*------------------------------------------------------------------------
 * wait  --  make current process wait on a semaphore
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000;
extern int currpid;

SYSCALL	wait(int sem)
{
	long ts;
    if (trace) ts = ctr1000;

	STATWORD ps;    
	struct	sentry	*sptr;
	struct	pentry	*pptr;

	disable(ps);
	if (isbadsem(sem) || (sptr= &semaph[sem])->sstate==SFREE) {
		restore(ps);
		if (trace) {
			scstats[currpid][IDX_wait].ct++;
			scstats[currpid][IDX_wait].tt += (ctr1000 - ts);
		}
		return(SYSERR);
	}
	
	if (--(sptr->semcnt) < 0) {
		(pptr = &proctab[currpid])->pstate = PRWAIT;
		pptr->psem = sem;
		enqueue(currpid,sptr->sqtail);
		pptr->pwaitret = OK;
		resched();
		restore(ps);
		if (trace) {
			scstats[currpid][IDX_wait].ct++;
			scstats[currpid][IDX_wait].tt += (ctr1000 - ts);
		}
		return pptr->pwaitret;
	}
	restore(ps);
	if (trace) {
		scstats[currpid][IDX_wait].ct++;
		scstats[currpid][IDX_wait].tt += (ctr1000 - ts);
	}
	return(OK);
}
