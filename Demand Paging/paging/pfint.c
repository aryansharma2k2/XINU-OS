/* pfint.c - pfint */

#include <conf.h>
#include <kernel.h>
#include <paging.h>
#include <proc.h>

extern unsigned long read_cr2(void);  /* Get faulted virtual address from CR2 */

/*-------------------------------------------------------------------------
 * pfint - paging fault ISR
 *-------------------------------------------------------------------------
 */
SYSCALL pfint()
{
  unsigned long fault_addr;        // Virtual address that caused the fault 
  unsigned long vpno;              // Virtual page number 
  unsigned int pd_idx;             // Page directory index (bits [31:22]) 
  unsigned int pt_idx;             // Page table index (bits [21:12]) 
  unsigned int pg_offset;          // Page offset (bits [11:0]) 
  pd_t *pd;                        // Pointer to page directory 
  pt_t *pt;                        // Pointer to page table 
  int store, pageth;               // Backing store lookup results
  int frm_index;                   // Allocated frame index (frm_tab index)
  int page_frm_index;              // Frame index for the faulted page
  unsigned long pt_phys_addr;
  unsigned long page_phys_addr;
  int is_xmmap = 0;                // Whether this is an xmmap page
  
  // Get the faulted virtual address from CR2 register 
  fault_addr = read_cr2();
  
  // Print all virtual addresses issued by the CPU
  // kprintf("Page fault: pid=%d, VA=0x%08lx (VPNO=%ld, PD=%d, PT=%d, Offset=0x%03x)\n", 
  //         currpid, fault_addr, fault_addr >> 12, 
  //         (fault_addr >> 22) & 0x3FF, (fault_addr >> 12) & 0x3FF, fault_addr & 0xFFF);
  
  // Extract the components from the virtual address 
  pd_idx = (fault_addr >> 22) & 0x3FF;      // Upper 10 bits [31:22] 
  pt_idx = (fault_addr >> 12) & 0x3FF;      // Middle 10 bits [21:12] 
  pg_offset = fault_addr & 0xFFF;           // Lower 12 bits [11:0] */
  vpno = fault_addr >> 12;                  // Virtual page number (address / 4096) */
  
  // Get pointer to current process's page directory 
  pd = (pd_t *) proctab[currpid].pdbr;
  
  // Validate mapping exists in backing store
  // Try xmmap first, then heap
  if (xmmap_lookup(currpid, fault_addr, &store, &pageth) == OK) {
    is_xmmap = 1;
  } else if (bsm_lookup(currpid, fault_addr, &store, &pageth) == SYSERR) {
    int killed_pid = currpid;
    kprintf("Illegal access by pid %d at 0x%08x - killing process\n", killed_pid, fault_addr);
    kill(killed_pid);
    // kill() calls resched() if process was PRCURR, which switches to another process
    // After resched(), currpid is different, so we should never return to the killed process
    // Just return SYSERR - the killed process should never execute again
    return SYSERR;
  }
  

  // Ensure page table exists; if not, allocate and initialize
  if (!pd[pd_idx].pd_pres) {
    if (get_frm(&frm_index) == SYSERR) {
      kprintf("No free frame for page table; pid %d fault at 0x%08x\n", currpid, fault_addr);
      kill(currpid);
      return SYSERR;
    }

    pt_phys_addr = (FRAME0 + frm_index) * NBPG;
    pt = (pt_t *)pt_phys_addr;

    // Initialize the page table entries as not present, writable
    {
      for (int i = 0; i < 1024; i++) {
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
    }

    // Update page directory entry to point to this new page table
    pd[pd_idx].pd_pres = 1;
    pd[pd_idx].pd_write = 1;
    pd[pd_idx].pd_user = 0;
    pd[pd_idx].pd_pwt = 0;
    pd[pd_idx].pd_pcd = 0;
    pd[pd_idx].pd_acc = 0;
    pd[pd_idx].pd_mbz = 0;
    pd[pd_idx].pd_fmb = 0;
    pd[pd_idx].pd_global = 0;
    pd[pd_idx].pd_avail = 0;
    pd[pd_idx].pd_base = (unsigned int)(FRAME0 + frm_index);

    // Update inverted page table entry for the page table
    frm_tab[frm_index].fr_status = FRM_MAPPED;
    frm_tab[frm_index].fr_pid = currpid;
    frm_tab[frm_index].fr_vpno = 0;
    frm_tab[frm_index].fr_refcnt = 0; 
    frm_tab[frm_index].fr_type = FR_TBL;
    frm_tab[frm_index].fr_dirty = 0;
  } else {
    // Page table exists; get its address
    pt_phys_addr = (pd[pd_idx].pd_base << 12);
    pt = (pt_t *)pt_phys_addr;
  }
  
  // For shared xmmap pages, if page is present but not dirty, we need to check
  // if other processes have written to it and reload if necessary
  if (is_xmmap && pt[pt_idx].pt_pres && !pt[pt_idx].pt_dirty) {
    extern SYSCALL sync_bs_page(int, int, int);
    // This will write back other processes' dirty pages and invalidate our page
    // if other processes have written
    sync_bs_page(store, pageth, (int)vpno);
    
    // After sync_bs_page, check if our page was invalidated
    // If it was, we need to reload it below
  }
  
  // If page not present, bring it in from backing store
  if (!pt[pt_idx].pt_pres) {
    // For shared xmmap pages, write back any dirty pages from other processes
    // before reading, to ensure we get the latest data
    // (sync_bs_page may have already been called above if page was present)
    if (is_xmmap) {
      extern SYSCALL sync_bs_page(int, int, int);
      sync_bs_page(store, pageth, (int)vpno);
    }
    
    if (get_frm(&page_frm_index) == SYSERR) {
      kprintf("No free frame for page-in; pid %d fault at 0x%08x\n", currpid, fault_addr);
      kill(currpid);
      return SYSERR;
    }

    page_phys_addr = (FRAME0 + page_frm_index) * NBPG;
    if (read_bs((char *)page_phys_addr, (bsd_t)store, pageth) == SYSERR) {
      kprintf("read_bs failed: store %d page %d for pid %d\n", store, pageth, currpid);
      kill(currpid);
      return SYSERR;
    }

    // Update page table entry
    pt[pt_idx].pt_pres = 1;
    pt[pt_idx].pt_write = 1;
    pt[pt_idx].pt_user = 0;
    pt[pt_idx].pt_pwt = 0;
    pt[pt_idx].pt_pcd = 0;
    pt[pt_idx].pt_acc = 0;
    pt[pt_idx].pt_dirty = 0;
    pt[pt_idx].pt_mbz = 0;
    pt[pt_idx].pt_global = 0;
    pt[pt_idx].pt_avail = 0;
    pt[pt_idx].pt_base = (unsigned int)(FRAME0 + page_frm_index);

    // Update frame table for the loaded page
    frm_tab[page_frm_index].fr_status = FRM_MAPPED;
    frm_tab[page_frm_index].fr_pid = currpid;
    frm_tab[page_frm_index].fr_vpno = (int)vpno;
    frm_tab[page_frm_index].fr_refcnt = 1;
    frm_tab[page_frm_index].fr_type = FR_PAGE;
    frm_tab[page_frm_index].fr_dirty = 0;

    // Add frame to Second-Chance queue (only for page frames)
    {
      extern void add_to_sc_queue(int frm_idx);
      add_to_sc_queue(page_frm_index);
    }

    // Increment reference count of the page table frame
    {
      int pt_frm_index = (int)pd[pd_idx].pd_base - FRAME0;
      if (pt_frm_index >= 0 && pt_frm_index < NFRAMES) {
        frm_tab[pt_frm_index].fr_refcnt++;
      }
    }
  }
  return OK;
}


