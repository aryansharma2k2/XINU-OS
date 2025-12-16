/* recvclr.c - recvclr */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <stdio.h>
#include <lab0.h>

/*------------------------------------------------------------------------
 *  recvclr  --  clear messages, returning waiting message (if any)
 *------------------------------------------------------------------------
 */

extern unsigned long ctr1000;
extern int currpid;

SYSCALL	recvclr()
{
	long ts;
    if (trace) ts = ctr1000;

	STATWORD ps;    
	WORD	msg;

	disable(ps);
	if (proctab[currpid].phasmsg) {
		proctab[currpid].phasmsg = 0;
		msg = proctab[currpid].pmsg;
	} else
		msg = OK;
	restore(ps);
	if (trace) {
		scstats[currpid][IDX_recvclr].ct++;
		scstats[currpid][IDX_recvclr].tt += (ctr1000 - ts);
	}
	return(msg);
}
