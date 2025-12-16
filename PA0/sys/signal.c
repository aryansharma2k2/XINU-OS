/* signal.c - signal */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sem.h>
#include <stdio.h>
#include <lab0.h>

/*------------------------------------------------------------------------
 * signal  --  signal a semaphore, releasing one waiting process
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000;
extern int currpid;

SYSCALL signal(int sem)
{
	long ts;
    if (trace) ts = ctr1000;

	STATWORD ps;    
	register struct	sentry	*sptr;

	disable(ps);
	if (isbadsem(sem) || (sptr= &semaph[sem])->sstate==SFREE) {
		restore(ps);
		if (trace) {
			scstats[currpid][IDX_signal].ct++;
			scstats[currpid][IDX_signal].tt += (ctr1000 - ts);
		}
		return(SYSERR);
	}
	if ((sptr->semcnt++) < 0)
		ready(getfirst(sptr->sqhead), RESCHYES);
	restore(ps);
	if (trace) {
		scstats[currpid][IDX_signal].ct++;
		scstats[currpid][IDX_signal].tt += (ctr1000 - ts);
	}
	return(OK);
}
