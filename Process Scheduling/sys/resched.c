/* resched.c  -  resched */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sched.h>

unsigned long currSP;	/* REAL sp of current process */
extern int ctxsw(int, int, int, int);
double expdev(double);
/*-----------------------------------------------------------------------
 * resched  --  reschedule processor to highest priority ready process
 *
 * Notes:	Upon entry, currpid gives current process id.
 *		Proctab[currpid].pstate gives correct NEXT state for
 *			current process if other than PRREADY.
 *------------------------------------------------------------------------
 */

static void linux_start_epoch(void)
{
    for (int i = 0; i < NPROC; i++) {
        struct pentry *p = &proctab[i];

        if (i == 0) {
            p->linux_quantum  = 0;
            p->linux_remain   = 0;
            p->linux_goodness = 0;
            p->seen_epoch     = 1;
            continue;
        }

        p->pprio = p->baseprio_linux; //mid epoch chprio stuff

        if ((p->seen_epoch == 0) || (p->seen_epoch && p->linux_remain == 0)) { //check if we've seen it b4
            p->linux_quantum = p->pprio;
        } 
		else {
            int unused = p->linux_remain;
            p->linux_quantum = p->pprio + (unused / 2);
        }

        p->linux_remain = p->linux_quantum;
		if (p->linux_remain > 0) {
			p->linux_goodness = p->pprio + p->linux_remain;
		}
		else {
			p->linux_goodness = 0;
		}
        p->seen_epoch = 1;
    }
}

static int linux_best_runnable_pid(void)
{
    int bestpid = -1;
    int bestgood = -1;

    int x = q[rdyhead].qnext;
    while (x != rdytail) {
        int pid = x;
        struct pentry *p = &proctab[pid];
        if (p->linux_goodness > 0) {
            if (p->linux_goodness > bestgood) {
                bestgood = p->linux_goodness;
                bestpid = pid;
            }
        }
        x = q[x].qnext;
    }

    if (proctab[currpid].pstate == PRCURR) { //consider current also
        struct pentry *cp = &proctab[currpid];
        if (cp->linux_goodness > 0) {
            if (cp->linux_goodness > bestgood) {
                bestgood = cp->linux_goodness;
                bestpid = currpid;
            }
        }
    }

    return bestpid;
}

static void linux_charge_old_running_time(struct pentry *optr)
{
    if (!optr || optr->pstate != PRCURR) return; //got to be a running process

    int used = QUANTUM - preempt;	//how much og slice it used
    //if (used < 0) used = 0;
    if (used > QUANTUM) {
		used = QUANTUM;
	}

    optr->linux_remain -= used;
    if (optr->linux_remain < 0) {
		optr->linux_remain = 0;
	}

    if (optr->linux_remain > 0) {
        optr->linux_goodness = optr->pprio + optr->linux_remain;
	} 
	else {
        optr->linux_goodness = 0;
	}
}

int resched()
{
	register struct	pentry	*optr;	/* pointer to old process entry */
	register struct	pentry	*nptr;	/* pointer to new process entry */

	switch (getschedclass()) 
	{
	case EXPDISTSCHED:
	{
		int oldpid = currpid;
    	optr = &proctab[oldpid];
		if (optr->pstate == PRCURR) {
			optr->pstate = PRREADY;
			insert(oldpid,rdyhead,optr->pprio);
		}
		double r = expdev(0.1);
		int first = q[rdyhead].qnext;
		int last  = q[rdytail].qprev;
		int nextpid;
		if (first == rdytail) {
        	nextpid = 0;
		} 
		else {
			int minprio = q[first].qkey;
			int maxprio = q[last].qkey;

			if (r < (double)minprio) {
				nextpid = first;
			} 
			else if (r >= (double)maxprio) {
				nextpid = last;
			} 
			else {
				int x = first;
				int found = -1;
				while (x != rdytail) {
					if ((double)q[x].qkey > r) { 
						found = x; 
						break; 
					}
					x = q[x].qnext;
				}
				if (found < 0) found = last;
				nextpid = found;
			}

			nextpid = dequeue(nextpid);
		}
		currpid = nextpid;
		nptr = &proctab[currpid];
		nptr->pstate = PRCURR;

#ifdef RTCLOCK
    	preempt = QUANTUM;
#endif

		ctxsw((int)&optr->pesp, (int)optr->pirmask,(int)&nptr->pesp, (int)nptr->pirmask);
		return OK;
	}

	case LINUXSCHED:
	{
		int oldpid = currpid;
		optr = &proctab[oldpid];

		if (optr->pstate == PRCURR) {
			linux_charge_old_running_time(optr);
			optr->pstate = PRREADY;
			insert(oldpid, rdyhead, optr->pprio);
		}

		int pick = linux_best_runnable_pid();
		if (pick < 0) {
			linux_start_epoch();
			pick = linux_best_runnable_pid();
			if (pick < 0) {
				pick = 0; //run null if nothing again? unsure
			}
		}

		if (pick != 0) {
			int inrq = 0; //checking if its in ready queue.
			int y = q[rdyhead].qnext;
			while (y != rdytail) { 
				if (y == pick) { 
					inrq = 1; 
					break; 
				} 
					y = q[y].qnext; 
			}
			if (inrq) {
				dequeue(pick);
			}
		}
		currpid = pick;
		nptr = &proctab[currpid];
		nptr->pstate = PRCURR;

#ifdef RTCLOCK
		if (currpid == 0) {
			preempt = QUANTUM; //for null
		} else {
			int slice = nptr->linux_remain;
			if (slice <= 0) slice = 1;
			if (slice > QUANTUM) slice = QUANTUM;
			preempt = slice;
		}
#endif

		ctxsw((int)&optr->pesp, (int)optr->pirmask,(int)&nptr->pesp, (int)nptr->pirmask);
		return OK;
	}

	default:
		/* no switch needed if current process priority higher than next*/
		if ( ( (optr= &proctab[currpid])->pstate == PRCURR) &&
		(lastkey(rdytail)<optr->pprio)) {
			return(OK);
		}
		
		/* force context switch */

		if (optr->pstate == PRCURR) {
			optr->pstate = PRREADY;
			insert(currpid,rdyhead,optr->pprio);
		}

		/* remove highest priority process at end of ready list */

		nptr = &proctab[ (currpid = getlast(rdytail)) ];
		nptr->pstate = PRCURR;		/* mark it currently running	*/
#ifdef	RTCLOCK
		preempt = QUANTUM;		/* reset preemption counter	*/
#endif
		
		ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
		
		/* The OLD process returns here when resumed. */
		return OK;
	}
}
