/* invltlb.c - invalidate a TLB entry */

#include <conf.h>
#include <kernel.h>
#include <paging.h>

/*------------------------------------------------------------------------
 * invltlb - invalidate TLB entry for a given virtual address
 *------------------------------------------------------------------------
 */
SYSCALL invltlb(unsigned long vaddr)
{
	/* Invalidate TLB entry using invlpg instruction */
	asm volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
	return OK;
}

