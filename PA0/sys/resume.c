/* resume.c - resume */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <stdio.h>
#include <lab0.h>

/*------------------------------------------------------------------------
 * resume  --  unsuspend a process, making it ready; return the priority
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000;
extern int currpid;

SYSCALL resume(int pid)
{
	long ts;
    if (trace) ts = ctr1000;

	STATWORD ps;    
	struct	pentry	*pptr;		/* pointer to proc. tab. entry	*/
	int	prio;			/* priority to return		*/

	disable(ps);
	if (isbadpid(pid) || (pptr= &proctab[pid])->pstate!=PRSUSP) {
		restore(ps);
		if (trace) {
			scstats[currpid][IDX_resume].ct++;
			scstats[currpid][IDX_resume].tt += (ctr1000 - ts);
		}
		return(SYSERR);
	}
	prio = pptr->pprio;
	ready(pid, RESCHYES);
	restore(ps);
	if (trace) {
		scstats[currpid][IDX_resume].ct++;
		scstats[currpid][IDX_resume].tt += (ctr1000 - ts);
	}
	return(prio);
}
