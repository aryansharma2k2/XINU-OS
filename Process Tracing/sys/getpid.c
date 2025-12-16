/* getpid.c - getpid */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <lab0.h>
/*------------------------------------------------------------------------
 * getpid  --  get the process id of currently executing process
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000; /* already provided by XINU */
extern int currpid;

SYSCALL getpid()
{
	long ts;
    if (trace) ts = ctr1000;
	
	if (trace) {
		scstats[currpid][IDX_getpid].ct++;
        scstats[currpid][IDX_getpid].tt += (ctr1000 - ts);
	}
	return(currpid);
}
