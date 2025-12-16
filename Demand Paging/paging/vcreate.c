/* vcreate.c - vcreate */
    
#include <conf.h>
#include <i386.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <mem.h>
#include <io.h>
#include <paging.h>

extern unsigned long global_pt_addrs[4];

/*
static unsigned long esp;
*/

LOCAL	newpid();
/*------------------------------------------------------------------------
 * vcreate - create a process similar to the create syscall, but with a 
 * virtual heap rather than using the shared physical heap.
 *------------------------------------------------------------------------
 */
SYSCALL vcreate(procaddr,ssize,hsize,priority,name,nargs,args)
	int	*procaddr;		/* procedure address		*/
	int	ssize;			/* stack size in words		*/
	int	hsize;			/* virtual heap size in pages	*/
	int	priority;		/* process priority > 0		*/
	char	*name;			/* name (for debugging)		*/
	int	nargs;			/* number of args that follow	*/
	long	args;			/* arguments (treated like an	*/
					/* array in the code)		*/
{
	STATWORD 	ps;    
	int		pid;		/* stores new process id	*/
	struct	pentry	*pptr;		/* pointer to proc. table entry */
	int		i;
	int		bs_id;		/* backing store ID */
	int		pd_frame_idx;	/* page directory frame index */
	int		pt_frame_idx;	/* page table frame index */
	unsigned long	pt_phys_addr;
	pd_t		*pd;
	pt_t		*pt;
	int		vhpno = 4096;	/* virtual heap starts at page 4096 */
	int		vhpnpages = 256; 
	
	disable(ps);
	
	// Reserve backing store for virtual heap first
	if (get_bsm(&bs_id) == SYSERR) {
		restore(ps);
		return(SYSERR);
	}

	// Validate heap size (should be 256 pages for pages 4096-4351)
	if (hsize > 0 && hsize <= 256) {
		vhpnpages = hsize;
	}
	
	// Temporarily enable interrupts before calling create() since it manages its own interrupt state
	restore(ps);
	
	// Use create() to handle stack creation and basic process setup
	pid = create(procaddr, ssize, priority, name, nargs, args);
	
	// Re-disable interrupts for remaining work
	disable(ps);
	
	if (pid == SYSERR) {
		free_bsm(bs_id);
		restore(ps);
		return(SYSERR);
	}
	
	pptr = &proctab[pid];
	pd = (pd_t *)pptr->pdbr;  // Get the page directory that create() allocated
	
	// Extract frame index from pdbr
	pd_frame_idx = (pptr->pdbr / NBPG) - FRAME0;

	// Map virtual heap (pages 4096-4351) to reserved backing store
	// Page 4096 is in page directory index 4 (4096 / (1024*4096) = 4)
	// Create a page table for this region
	if (get_frm(&pt_frame_idx) == SYSERR) {
		// Clean up on failure
		free_bsm(bs_id);
		restore(ps);
		return(SYSERR);
	}
	pptr->is_virtual = 1;
	pt_phys_addr = (FRAME0 + pt_frame_idx) * NBPG;
	pt = (pt_t *)pt_phys_addr;
	
	// Initialize all page table entries as not present (will cause page faults)
	for (i = 0; i < 1024; i++) {
		pt[i].pt_pres = 0;
		pt[i].pt_write = 1;
		pt[i].pt_user = 0;
		pt[i].pt_pwt = 0;
		pt[i].pt_pcd = 0;
		pt[i].pt_acc = 0;
		pt[i].pt_dirty = 0;
		pt[i].pt_mbz = 0;
		pt[i].pt_global = 0;
		pt[i].pt_avail = 0;
		pt[i].pt_base = 0;
	}

	// Update page directory entry 4 to point to this page table
	pd[4].pd_pres = 1;
	pd[4].pd_write = 1;
	pd[4].pd_user = 0;
	pd[4].pd_pwt = 0;
	pd[4].pd_pcd = 0;
	pd[4].pd_acc = 0;
	pd[4].pd_mbz = 0;
	pd[4].pd_fmb = 0;
	pd[4].pd_global = 0;
	pd[4].pd_avail = 0;
	pd[4].pd_base = (unsigned int)(FRAME0 + pt_frame_idx);

	// Update frame table for page directory
	frm_tab[pd_frame_idx].fr_status = FRM_MAPPED;
	frm_tab[pd_frame_idx].fr_pid = pid;
	frm_tab[pd_frame_idx].fr_vpno = 0;
	frm_tab[pd_frame_idx].fr_refcnt = 0;
	frm_tab[pd_frame_idx].fr_type = FR_DIR;
	frm_tab[pd_frame_idx].fr_dirty = 0;

	// Update frame table for page table
	frm_tab[pt_frame_idx].fr_status = FRM_MAPPED;
	frm_tab[pt_frame_idx].fr_pid = pid;
	frm_tab[pt_frame_idx].fr_vpno = 0;
	frm_tab[pt_frame_idx].fr_refcnt = 0;
	frm_tab[pt_frame_idx].fr_type = FR_TBL;
	frm_tab[pt_frame_idx].fr_dirty = 0;

	// Add entry to bsm_tab for virtual heap
	if (bsm_map(pid, vhpno, bs_id, vhpnpages) == SYSERR) {
		// Clean up on failure
		free_frm(pt_frame_idx);
		free_bsm(bs_id);
		restore(ps);
		return(SYSERR);
	}

	// Store backing store info in process structure
	pptr->store = bs_id;
	pptr->vhpno = vhpno;
	pptr->vhpnpages = vhpnpages;

	// Initialize virtual heap free list
	// vmemlist points to a header structure at the beginning of virtual heap
	pptr->vmemlist = (struct mblock *)(vhpno * NBPG);
	
	// Map the first page of the virtual heap to initialize the free list header
	// This is needed so vgetmem can access the header without causing a fault
	int first_page_frm_idx;
	if (get_frm(&first_page_frm_idx) == OK) {
		unsigned long first_page_phys = (FRAME0 + first_page_frm_idx) * NBPG;
		
		// Read from backing store to initialize the page
		if (read_bs((char *)first_page_phys, (bsd_t)bs_id, 0) == OK) {
			// Now initialize the page table entry for the first virtual heap page
			pt[0].pt_pres = 1;
			pt[0].pt_write = 1;
			pt[0].pt_user = 0;
			pt[0].pt_base = (unsigned int)(FRAME0 + first_page_frm_idx);
			
			// Initialize free list header and first block using physical address
			// (we can access it directly since we're mapping kernel space)
			struct mblock *vheader = (struct mblock *)first_page_phys;
			// mnext should point to the next block in virtual address space
			vheader->mnext = (struct mblock *)((vhpno * NBPG) + sizeof(struct mblock));
			vheader->mlen = 0;  // Dummy header node
			
			// Initialize first free block (at offset sizeof(struct mblock))
			struct mblock *first_block_phys = (struct mblock *)((unsigned)vheader + sizeof(struct mblock));
			first_block_phys->mnext = (struct mblock *)NULL;
			first_block_phys->mlen = vhpnpages * NBPG;
			
			// Update frame table
			frm_tab[first_page_frm_idx].fr_status = FRM_MAPPED;
			frm_tab[first_page_frm_idx].fr_pid = pid;
			frm_tab[first_page_frm_idx].fr_vpno = vhpno;
			frm_tab[first_page_frm_idx].fr_refcnt = 1;
			frm_tab[first_page_frm_idx].fr_type = FR_PAGE;
			frm_tab[first_page_frm_idx].fr_dirty = 0;
			
			// Add frame to Second-Chance queue
			{
				extern void add_to_sc_queue(int frm_idx);
				add_to_sc_queue(first_page_frm_idx);
			}
			
			// Increment ref count of page table
			frm_tab[pt_frame_idx].fr_refcnt++;
		} else {
			// If read_bs fails, free the frame
			free_frm(first_page_frm_idx);
		}
	}

	restore(ps);

	return(pid);
}

/*------------------------------------------------------------------------
 * newpid  --  obtain a new (free) process id
 *------------------------------------------------------------------------
 */
LOCAL	newpid()
{
	int	pid;			/* process id to return		*/
	int	i;

	for (i=0 ; i<NPROC ; i++) {	/* check all NPROC slots	*/
		if ( (pid=nextproc--) <= 0)
			nextproc = NPROC-1;
		if (proctab[pid].pstate == PRFREE)
			return(pid);
	}
	return(SYSERR);
}
