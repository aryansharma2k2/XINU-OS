/* vgetmem.c - vgetmem */

#include <conf.h>
#include <kernel.h>
#include <mem.h>
#include <proc.h>
#include <paging.h>

extern struct pentry proctab[];
/*------------------------------------------------------------------------
 * vgetmem - allocate virtual heap storage, returning lowest WORD address
 *------------------------------------------------------------------------
 */
WORD	*vgetmem(nbytes)
	unsigned nbytes;
{
	STATWORD ps;    
	struct	mblock	*p, *q, *leftover;
	struct	pentry	*pptr;

	disable(ps);
	
	// Get current process entry
	pptr = &proctab[currpid];
	
	// Check if process has a virtual heap (created with vcreate)
	if (pptr->vmemlist == NULL) {
		restore(ps);
		return( (WORD *)SYSERR );
	}
	
	if (nbytes == 0) {
		restore(ps);
		return( (WORD *)SYSERR );
	}
	
	nbytes = (unsigned int) roundmb(nbytes);
	
	// Search for a free block in the virtual heap
	for (q = pptr->vmemlist, p = pptr->vmemlist->mnext;
	     p != (struct mblock *) NULL;
	     q = p, p = p->mnext) {
		if (p->mlen == nbytes) {
			// Exact match - remove the block
			q->mnext = p->mnext;
			restore(ps);
			return( (WORD *)p );
		} else if (p->mlen > nbytes) {
			// Split the block only if leftover is large enough for an mblock header
			unsigned leftover_size = p->mlen - nbytes;
			if (leftover_size < sizeof(struct mblock)) {
				// Leftover too small, give whole block
				q->mnext = p->mnext;
				restore(ps);
				return( (WORD *)p );
			}
			// Split the block
			leftover = (struct mblock *)( (unsigned)p + nbytes );
			q->mnext = leftover;
			leftover->mnext = p->mnext;
			leftover->mlen = leftover_size;
			restore(ps);
			return( (WORD *)p );
		}
	}
	
	restore(ps);
	return( (WORD *)SYSERR );
}


