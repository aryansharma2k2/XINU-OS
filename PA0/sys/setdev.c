/* setdev.c - setdev */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <lab0.h>

/*------------------------------------------------------------------------
 *  setdev  -  set the two device entries in the process table entry
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000;
extern int currpid;

SYSCALL	setdev(int pid, int dev1, int dev2)
{
	long ts;
    if (trace) ts = ctr1000;

	short	*nxtdev;

	if (isbadpid(pid)) {
		if (trace) {
			scstats[currpid][IDX_setdev].ct++;
			scstats[currpid][IDX_setdev].tt += (ctr1000 - ts);
		}
		return(SYSERR);
	}
	nxtdev = (short *) proctab[pid].pdevs;
	*nxtdev++ = dev1;
	*nxtdev = dev2;
	if (trace) {
		scstats[currpid][IDX_setdev].ct++;
		scstats[currpid][IDX_setdev].tt += (ctr1000 - ts);
	}
	return(OK);
}
