/* gettime.c - gettime */

#include <conf.h>
#include <kernel.h>
#include <date.h>
#include <lab0.h>

extern int getutim(unsigned long *);
extern unsigned long ctr1000;
extern int currpid;

/*------------------------------------------------------------------------
 *  gettime  -  get local time in seconds past Jan 1, 1970
 *------------------------------------------------------------------------
 */
SYSCALL	gettime(long *timvar)
{
    long ts;
    if (trace) ts = ctr1000;
    /* long	now; */

	/* FIXME -- no getutim */
    if (trace) {
		scstats[currpid][IDX_gettime].ct++;
		scstats[currpid][IDX_gettime].tt += (ctr1000 - ts);
	}
    return OK;
}
