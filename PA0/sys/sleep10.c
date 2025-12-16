/* sleep10.c - sleep10 */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sleep.h>
#include <stdio.h>
#include <lab0.h>

/*------------------------------------------------------------------------
 * sleep10  --  delay the caller for a time specified in tenths of seconds
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000;
extern int currpid;

SYSCALL	sleep10(int n)
{
	long ts;
    if (trace) ts = ctr1000;

	STATWORD ps;    
	if (n < 0  || clkruns==0) {
		if (trace) {
			scstats[currpid][IDX_sleep10].ct++;
			scstats[currpid][IDX_sleep10].tt += (ctr1000 - ts);
		}
	    return(SYSERR);
	}
	disable(ps);
	if (n == 0) {		/* sleep10(0) -> end time slice */
	        ;
	} else {
		insertd(currpid,clockq,n*100);
		slnempty = TRUE;
		sltop = &q[q[clockq].qnext].qkey;
		proctab[currpid].pstate = PRSLEEP;
	}
	resched();
        restore(ps);
	if (trace) {
		scstats[currpid][IDX_sleep10].ct++;
		scstats[currpid][IDX_sleep10].tt += (ctr1000 - ts);
	}
	return(OK);
}
