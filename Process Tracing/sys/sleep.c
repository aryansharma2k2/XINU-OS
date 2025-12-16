/* sleep.c - sleep */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sleep.h>
#include <stdio.h>
#include <lab0.h>

/*------------------------------------------------------------------------
 * sleep  --  delay the calling process n seconds
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000;
extern int currpid;

SYSCALL	sleep(int n)
{
	long ts;
    if (trace) ts = ctr1000;

	STATWORD ps;    
	if (n<0 || clkruns==0) {
		if (trace) {
			scstats[currpid][IDX_sleep].ct++;
			scstats[currpid][IDX_sleep].tt += (ctr1000 - ts);
		}
		return(SYSERR);
	}
	if (n == 0) {
	        disable(ps);
		resched();
		restore(ps);
		if (trace) {
			scstats[currpid][IDX_sleep].ct++;
			scstats[currpid][IDX_sleep].tt += (ctr1000 - ts);
		}
		return(OK);
	}
	while (n >= 1000) {
		sleep10(10000);
		n -= 1000;
	}
	if (n > 0)
		sleep10(10*n);
	
	if (trace) {
		scstats[currpid][IDX_sleep].ct++;
		scstats[currpid][IDX_sleep].tt += (ctr1000 - ts);
	}
	return(OK);
}
