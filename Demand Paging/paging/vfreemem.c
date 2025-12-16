/* vfreemem.c - vfreemem */

#include <conf.h>
#include <kernel.h>
#include <mem.h>
#include <proc.h>
#include <paging.h>

extern struct pentry proctab[];
/*------------------------------------------------------------------------
 * vfreemem - free a virtual memory block, returning it to vmemlist
 *------------------------------------------------------------------------
 */
SYSCALL	vfreemem(block, size)
	struct	mblock	*block;
	unsigned size;
{
	STATWORD ps;    
	struct	mblock	*p, *q;
	unsigned top;
	struct	pentry	*pptr;
	unsigned long	vheap_start;
	unsigned long	vheap_end;

	// Get current process entry
	pptr = &proctab[currpid];
	
	// Check if process has a virtual heap
	if (pptr->vmemlist == NULL) {
		return(SYSERR);
	}
	
	// Calculate virtual heap bounds
	vheap_start = pptr->vhpno * NBPG;
	vheap_end = vheap_start + (pptr->vhpnpages * NBPG);
	
	if (size == 0 || (unsigned)block < vheap_start || (unsigned)block >= vheap_end) {
		return(SYSERR);
	}
	
	size = (unsigned)roundmb(size);
	disable(ps);
	
	// Find the correct position in the free list
	for (p = pptr->vmemlist->mnext, q = pptr->vmemlist;
	     p != (struct mblock *) NULL && p < block;
	     q = p, p = p->mnext) {
	}
	
	// Check for overlapping blocks
	top = q->mlen + (unsigned)q;
	if ((top > (unsigned)block && q != pptr->vmemlist) ||
	    (p != NULL && (size + (unsigned)block) > (unsigned)p)) {
		restore(ps);
		return(SYSERR);
	}
	
	// Merge with previous block if adjacent
	if (q != pptr->vmemlist && top == (unsigned)block) {
		q->mlen += size;
	} else {
		block->mlen = size;
		block->mnext = p;
		q->mnext = block;
		q = block;
	}
	
	// Merge with next block if adjacent
	if (p != NULL && (unsigned)(q->mlen + (unsigned)q) == (unsigned)p) {
		q->mlen += p->mlen;
		q->mnext = p->mnext;
	}
	
	restore(ps);
	return(OK);
}
