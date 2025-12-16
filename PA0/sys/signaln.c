/* signaln.c - signaln */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sem.h>
#include <stdio.h>
#include <lab0.h>

/*------------------------------------------------------------------------
 *  signaln -- signal a semaphore n times
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000;
extern int currpid;

SYSCALL signaln(int sem, int count)
{
	long ts;
    if (trace) ts = ctr1000;

	STATWORD ps;    
	struct	sentry	*sptr;

	disable(ps);
	if (isbadsem(sem) || semaph[sem].sstate==SFREE || count<=0) {
		restore(ps);
		if (trace) {
			scstats[currpid][IDX_signaln].ct++;
			scstats[currpid][IDX_signaln].tt += (ctr1000 - ts);
		}
		return(SYSERR);
	}
	sptr = &semaph[sem];
	for (; count > 0  ; count--)
		if ((sptr->semcnt++) < 0)
			ready(getfirst(sptr->sqhead), RESCHNO);
	resched();
	restore(ps);
	if (trace) {
		scstats[currpid][IDX_signaln].ct++;
		scstats[currpid][IDX_signaln].tt += (ctr1000 - ts);
	}
	return(OK);
}
