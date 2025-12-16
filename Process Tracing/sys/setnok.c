/* setnok.c - setnok */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <stdio.h>
#include <lab0.h>

/*------------------------------------------------------------------------
 *  setnok  -  set next-of-kin (notified at death) for a given process
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000;
extern int currpid;

SYSCALL	setnok(int nok, int pid)
{
	long ts;
    if (trace) ts = ctr1000;

	STATWORD ps;    
	struct	pentry	*pptr;

	disable(ps);
	if (isbadpid(pid)) {
		restore(ps);
		if (trace) {
			scstats[currpid][IDX_setnok].ct++;
			scstats[currpid][IDX_setnok].tt += (ctr1000 - ts);
		}
		return(SYSERR);
	}
	pptr = &proctab[pid];
	pptr->pnxtkin = nok;
	restore(ps);
	if (trace) {
		scstats[currpid][IDX_setnok].ct++;
		scstats[currpid][IDX_setnok].tt += (ctr1000 - ts);
	}
	return(OK);
}
