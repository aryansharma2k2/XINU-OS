/* kill.c - kill */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <mem.h>
#include <io.h>
#include <q.h>
#include <stdio.h>
#include <paging.h>

/*------------------------------------------------------------------------
 * kill  --  kill a process and remove it from the system
 *------------------------------------------------------------------------
 */
SYSCALL kill(int pid)
{
	STATWORD ps;    
	struct	pentry	*pptr;		/* points to proc. table for pid*/
	int	dev;
	int	i;
	int	vpno;
	unsigned long	vaddr;
	unsigned long	page_phys_addr;
	unsigned long	pt_phys_addr;
	pd_t		*pd;
	pt_t		*pt;
	unsigned int	pd_idx, pt_idx;
	int		store, pageth;
	int		page_frm_idx;

	disable(ps);
	if (isbadpid(pid) || (pptr= &proctab[pid])->pstate==PRFREE) {
		restore(ps);
		return(SYSERR);
	}
	if (--numproc == 0)
		xdone();

	dev = pptr->pdevs[0];
	if (! isbaddev(dev) )
		close(dev);
	dev = pptr->pdevs[1];
	if (! isbaddev(dev) )
		close(dev);
	dev = pptr->ppagedev;
	if (! isbaddev(dev) )
		close(dev);
	
	// Clean up virtual memory if this is a virtual process
	if (pptr->is_virtual) {
		pd = (pd_t *)pptr->pdbr;
		
		// Step 1: Iterate through page tables to find and write dirty pages
		// This is faster than iterating through all frames since we only check mapped pages
		for (pd_idx = 4; pd_idx < 1024; pd_idx++) {
			if (!pd[pd_idx].pd_pres) {
				continue; // Skip if page table doesn't exist
			}
			
			pt_phys_addr = pd[pd_idx].pd_base << 12;
			pt = (pt_t *)pt_phys_addr;
			
			// Iterate through all entries in this page table
			for (pt_idx = 0; pt_idx < 1024; pt_idx++) {
				if (!pt[pt_idx].pt_pres) {
					continue; // Skip if page is not present
				}
				
				// Calculate virtual page number and address
				vpno = (pd_idx << 10) | pt_idx;
				vaddr = (unsigned long)vpno << 12;
				
				// Get frame index from page table entry
				page_frm_idx = (int)pt[pt_idx].pt_base - FRAME0;
				
				// Verify this frame actually belongs to this process
				if (page_frm_idx < 0 || page_frm_idx >= NFRAMES ||
				    frm_tab[page_frm_idx].fr_pid != pid ||
				    frm_tab[page_frm_idx].fr_type != FR_PAGE) {
					continue; // Skip if frame doesn't belong to this process
				}
				
				// Write dirty page to backing store
				if (pt[pt_idx].pt_dirty) {
					// Find backing store for this page
					if (bsm_lookup(pid, vaddr, &store, &pageth) == OK) {
						page_phys_addr = (FRAME0 + page_frm_idx) * NBPG;
						write_bs((char *)page_phys_addr, (bsd_t)store, pageth);
					}
					if (xmmap_lookup(pid, vaddr, &store, &pageth) == OK) {
						page_phys_addr = (FRAME0 + page_frm_idx) * NBPG;
						write_bs((char *)page_phys_addr, (bsd_t)store, pageth);
					}
				}
				
				// Mark page table entry as not present
				pt[pt_idx].pt_pres = 0;
				pt[pt_idx].pt_dirty = 0;
				
				// Free the page frame
				free_frm(page_frm_idx);
				
				// Decrement reference count of page table frame
				{
					int pt_frm_idx = (int)pd[pd_idx].pd_base - FRAME0;
					if (pt_frm_idx >= 0 && pt_frm_idx < NFRAMES) {
						frm_tab[pt_frm_idx].fr_refcnt--;
					}
				}
			}
		}
		
		
		// Remove bsm_tab entries for virtual heap
		if (pptr->vhpno > 0) {
			bsm_unmap(pid, pptr->vhpno);
		}
		
		// Clean up xmmap entries for this process
		{
			int i;
			for (i = 0; i < xmmap_count; i++) {
				if (xmmap_tab[i].xm_pid == pid) {
					// Clear the xmmap entry
					xmmap_tab[i].xm_pid = -1;
					xmmap_tab[i].xm_vpno = 0;
					xmmap_tab[i].xm_npages = 0;
					xmmap_tab[i].xm_bs_id = -1;
				}
			}
		}
		
		// Free backing store reservation
		if (pptr->store >= 0 && pptr->store < MAX_BS) {
			free_bsm(pptr->store);
		}
		
		// Free page directory frame
		for (i = 0; i < NFRAMES; i++) {
			if (frm_tab[i].fr_status == FRM_MAPPED && 
			    frm_tab[i].fr_pid == pid &&
			    frm_tab[i].fr_type == FR_DIR) {
				free_frm(i);
				break;
			}
		}
		
		// Clear process memory management fields
		pptr->pdbr = 0;
		pptr->store = -1;
		pptr->vhpno = 0;
		pptr->vhpnpages = 0;
	}

	// Step 2: Free all page table frames owned by this process
	for (i = 0; i < NFRAMES; i++) {
		if (frm_tab[i].fr_status == FRM_MAPPED && 
			frm_tab[i].fr_pid == pid &&
			frm_tab[i].fr_type == FR_TBL) {
			
			// Find which page directory entry points to this table
			for (pd_idx = 0; pd_idx < 1024; pd_idx++) {
				if (pd[pd_idx].pd_pres && 
					(int)pd[pd_idx].pd_base == (FRAME0 + i)) {
					pd[pd_idx].pd_pres = 0;
					pd[pd_idx].pd_base = 0;
					break;
				}
			}
			
			// Free the page table frame
			free_frm(i);
		}
	}
	
	send(pptr->pnxtkin, pid);

	freestk(pptr->pbase, pptr->pstklen);
	switch (pptr->pstate) {

	case PRCURR:	pptr->pstate = PRFREE;	/* suicide */
			resched();

	case PRWAIT:	semaph[pptr->psem].semcnt++;

	case PRREADY:	dequeue(pid);
			pptr->pstate = PRFREE;
			break;

	case PRSLEEP:
	case PRTRECV:	unsleep(pid);
						/* fall through	*/
	default:	pptr->pstate = PRFREE;
	}
	restore(ps);
	return(OK);
}
