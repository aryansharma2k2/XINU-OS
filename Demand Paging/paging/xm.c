/* xm.c = xmmap xmunmap */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>


/*-------------------------------------------------------------------------
 * xmmap - map the virtual page to the backing store source with a 
 * permitted access range of npages (<= 256) for the calling process.
 * 
 * Return OK if the call succeeded and SYSERR if it failed for any reason.
 *-------------------------------------------------------------------------
 */
SYSCALL xmmap(int virtpage, bsd_t source, int npages)
{
  int i;
  int bs_id = (int)source;
  
  /* Validate params */
  if (virtpage < 4096) { /* Do not map first 16MB */
    return SYSERR;
  }
  if (bs_id < 0 || bs_id >= MAX_BS) {
    return SYSERR;
  }
  if (npages <= 0 || npages > 256) {
    return SYSERR;
  }

  // Check if backing store is reserved for virtual heap (exclusive ownership)
  if (bsm_tab[bs_id].bs_status == BSM_MAPPED && 
      bsm_tab[bs_id].bs_type == BS_TYPE_VHEAP) {
    return SYSERR;
  }

  // Check if this process already has an xmmap mapping for this virtpage
  for (i = 0; i < xmmap_count; i++) {
    if (xmmap_tab[i].xm_pid == currpid && xmmap_tab[i].xm_vpno == virtpage) {
      return SYSERR;
    }
  }

  // Find free slot in xmmap_tab
  for (i = 0; i < MAX_XMMAP_ENTRIES; i++) {
    if (xmmap_tab[i].xm_pid == -1) {
      // Add xmmap entry
      xmmap_tab[i].xm_pid = currpid;
      xmmap_tab[i].xm_vpno = virtpage;
      xmmap_tab[i].xm_npages = npages;
      xmmap_tab[i].xm_bs_id = bs_id;
      
      if (i >= xmmap_count) {
        xmmap_count = i + 1;
      }
      
      // Mark backing store as used for xmmap (shared)
      if (bsm_tab[bs_id].bs_status == BSM_UNMAPPED) {
        bsm_tab[bs_id].bs_status = BSM_MAPPED;
        bsm_tab[bs_id].bs_type = BS_TYPE_XMMAP;
        bsm_tab[bs_id].bs_pid = BS_XMMAP_PID;  
      }
      return OK;
    }
  }
  
  // No free slot in xmmap_tab
  return SYSERR;
}



/*-------------------------------------------------------------------------
 * xmunmap - free the xmmapping corresponding to virtpage for the calling 
 * process.
 * 
 * This call should not affect any other processes' xmmappings.
 * 
 * Return OK if the unmapping succeeded. Return SYSERR if the unmapping
 * failed or virtpage did not correspond to an existing xmmapping for
 * the calling process.
 *-------------------------------------------------------------------------
 */
SYSCALL xmunmap(int virtpage)
{
  unsigned long vaddr;
  int store, pageth;
  int i;
  unsigned long vpno;
  unsigned long start_vpno, end_vpno;
  pd_t *pd;
  pt_t *pt;
  unsigned int pd_idx, pt_idx;
  unsigned long page_vaddr;
  int frm_idx;
  unsigned long page_phys_addr;
  
  if (virtpage < 4096) {
    return SYSERR;
  }
  
  // Find the full mapping info (start vpno and npages) from xmmap_tab
  // IMPORTANT: We check xmmap_tab directly instead of using bsm_lookup
  // because bsm_lookup prioritizes VHEAP mappings, which could return
  // the wrong mapping if a process has both VHEAP and xmmap that overlap
  start_vpno = 0;
  int npages = 0;
  int xmmap_idx = -1;
  int xmmap_bs_id = -1;
  
  // Search in xmmap_tab for this process's mapping
  for (i = 0; i < xmmap_count; i++) {
    if (xmmap_tab[i].xm_pid == currpid && 
        (int)virtpage >= xmmap_tab[i].xm_vpno && 
        (int)virtpage < xmmap_tab[i].xm_vpno + xmmap_tab[i].xm_npages) {
      start_vpno = xmmap_tab[i].xm_vpno;
      npages = xmmap_tab[i].xm_npages;
      xmmap_bs_id = xmmap_tab[i].xm_bs_id;
      xmmap_idx = i;
      break;
    }
  }
  
  if (npages == 0 || xmmap_idx == -1 || xmmap_bs_id < 0) {
    return SYSERR;  // Mapping not found
  }
  
  end_vpno = start_vpno + npages;
  
  // Get page directory
  pd = (pd_t *) proctab[currpid].pdbr;
  
  // Iterate through all pages in the mapping range
  // For shared backing stores (xmmap), each process maps to the same backing store pages
  // Example: Process A maps vpage 4100->BS page 0, Process B maps vpage 4200->BS page 0
  // When Process A writes back, we calculate pageth = vpno - start_vpno directly
  // from the xmmap_tab entry (not using bsm_lookup which might return VHEAP mapping)
  for (vpno = start_vpno; vpno < end_vpno; vpno++) {
    page_vaddr = vpno << 12;
    pd_idx = (page_vaddr >> 22) & 0x3FF;
    pt_idx = (page_vaddr >> 12) & 0x3FF;
    
    // Check if page table exists
    if (pd[pd_idx].pd_pres) {
      pt = (pt_t *)(pd[pd_idx].pd_base << 12);
      
      // Check if page is present and dirty
      if (pt[pt_idx].pt_pres && pt[pt_idx].pt_dirty) {
        // Find the frame index
        frm_idx = (int)pt[pt_idx].pt_base - FRAME0;
        
        if (frm_idx >= 0 && frm_idx < NFRAMES) {
          // Get physical address of the frame
          page_phys_addr = (FRAME0 + frm_idx) * NBPG;
          
          // Calculate backing store page number directly from xmmap mapping
          store = xmmap_bs_id;
          pageth = (int)vpno - start_vpno;
          
          // Write dirty page back to backing store
          // For shared backing stores, this write updates the shared BS page
          // that other processes mapping the same BS will read from
          if (write_bs((char *)page_phys_addr, (bsd_t)store, pageth) == SYSERR) {
            kprintf("xmunmap: Failed to write dirty page vpno %d to BS %d page %d\n", 
                    (int)vpno, store, pageth);
            // Continue with other pages even if one fails
          } else {
            // Clear dirty bit after successful write
            pt[pt_idx].pt_dirty = 0;
            if (frm_tab[frm_idx].fr_status == FRM_MAPPED) {
              frm_tab[frm_idx].fr_dirty = 0;
            }
          }
        }
      }
    }
  }
  
  // Now unmap the mapping - remove from xmmap_tab
  if (xmmap_idx >= 0) {
    // Clear the xmmap entry
    xmmap_tab[xmmap_idx].xm_pid = -1;
    xmmap_tab[xmmap_idx].xm_vpno = 0;
    xmmap_tab[xmmap_idx].xm_npages = 0;
    xmmap_tab[xmmap_idx].xm_bs_id = -1;
    
    // Check if this was the last xmmap for this backing store
    int has_other_xmmaps = 0;
    for (i = 0; i < xmmap_count; i++) {
      if (xmmap_tab[i].xm_bs_id == xmmap_bs_id && xmmap_tab[i].xm_pid != -1) {
        has_other_xmmaps = 1;
        break;
      }
    }
    
    // If no other xmmaps use this backing store, mark it as unmapped
    if (!has_other_xmmaps) {
      bsm_tab[xmmap_bs_id].bs_status = BSM_UNMAPPED;
      bsm_tab[xmmap_bs_id].bs_type = BS_TYPE_VHEAP;
      bsm_tab[xmmap_bs_id].bs_pid = -1;
    }
    
    return OK;
  }
  
  return SYSERR;
}
