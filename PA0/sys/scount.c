/* scount.c - scount */

#include <conf.h>
#include <kernel.h>
#include <sem.h>
#include <lab0.h>

/*------------------------------------------------------------------------
 *  scount  --  return a semaphore count
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000;
extern int currpid;

SYSCALL scount(int sem)
{
	long ts;
    if (trace) ts = ctr1000;
extern	struct	sentry	semaph[];

	if (isbadsem(sem) || semaph[sem].sstate==SFREE) {
		if (trace) {
			scstats[currpid][IDX_scount].ct++;
			scstats[currpid][IDX_scount].tt += (ctr1000 - ts);
		}
		return(SYSERR);
	}
	if (trace) {
		scstats[currpid][IDX_scount].ct++;
		scstats[currpid][IDX_scount].tt += (ctr1000 - ts);
	}
	return(semaph[sem].semcnt);
}
