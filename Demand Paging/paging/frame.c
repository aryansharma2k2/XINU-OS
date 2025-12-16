/* frame.c - manage physical frames */
#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

/* Inverted page table (frame table) */
fr_map_t frm_tab[NFRAMES];

/* Second-Chance page replacement queue */
static int sc_next[NFRAMES];  /* next frame index in circular queue */
static int sc_head = -1;      /* head of circular queue (-1 if empty) */
static int sc_count = 0;      /* number of frames in queue */

/* Debug flag for page replacement */
extern int pr_debug_flag;

/*-------------------------------------------------------------------------
 * init_frm - initialize frm_tab (the inverted page table)
 *-------------------------------------------------------------------------
 */
SYSCALL init_frm()
{
  int i;
  for (i = 0; i < NFRAMES; i++) {
    frm_tab[i].fr_status = FRM_UNMAPPED;
    frm_tab[i].fr_pid = -1;
    frm_tab[i].fr_vpno = 0;
    frm_tab[i].fr_refcnt = 0;
    frm_tab[i].fr_type = FR_PAGE;
    frm_tab[i].fr_dirty = 0;
    sc_next[i] = -1;  /* Initialize queue pointers */
  }
  sc_head = -1;
  sc_count = 0;
  return OK;
}

static SYSCALL write_dirty_page(int pid, int vpno, int frm_idx)
{
  unsigned long vaddr;
  int store, pageth;
  unsigned long page_phys_addr;
  pd_t *pd;
  pt_t *pt;
  unsigned int pd_idx, pt_idx;
  int i;
  int xmmap_found = 0;
  int heap_found = 0;
  
  // Calculate virtual address
  vaddr = (unsigned long)vpno << 12;
  
  // Get page directory and page table
  pd = (pd_t *) proctab[pid].pdbr;
  pd_idx = (vaddr >> 22) & 0x3FF;
  pt_idx = (vaddr >> 12) & 0x3FF;
  
  if (!pd[pd_idx].pd_pres) {
    kprintf("write_dirty_page: Page directory entry not present for pid %d\n", pid);
    return SYSERR;
  }
  
  pt = (pt_t *)(pd[pd_idx].pd_base << 12);
  if (!pt[pt_idx].pt_pres) {
    kprintf("write_dirty_page: Page table entry not present for pid %d\n", pid);
    return SYSERR;
  }
  
  // Get physical address of the frame
  page_phys_addr = (FRAME0 + frm_idx) * NBPG;
  
  // Check xmmap mappings (shared backing stores)
  for (i = 0; i < xmmap_count; i++) {
    if (xmmap_tab[i].xm_pid == pid &&
        (int)vpno >= xmmap_tab[i].xm_vpno &&
        (int)vpno < xmmap_tab[i].xm_vpno + xmmap_tab[i].xm_npages) {
      store = xmmap_tab[i].xm_bs_id;
      pageth = (int)vpno - xmmap_tab[i].xm_vpno;
      
      // Write to xmmap backing store
      if (write_bs((char *)page_phys_addr, (bsd_t)store, pageth) == SYSERR) {
        kprintf("write_dirty_page: write_bs failed for xmmap pid %d store %d page %d\n", pid, store, pageth);
        return SYSERR;
      }
      xmmap_found = 1;
    }
  }
  
  // Check VHEAP mappings
  for (i = 0; i < MAX_BS; i++) {
    if (bsm_tab[i].bs_status == BSM_MAPPED && 
        bsm_tab[i].bs_type == BS_TYPE_VHEAP && 
        bsm_tab[i].bs_pid == pid) {
      int start = bsm_tab[i].bs_vpno;
      int count = bsm_tab[i].bs_npages;
      if ((int)vpno >= start && (int)vpno < (start + count)) {
        store = i;
        pageth = (int)vpno - start;
        
        // Write to VHEAP backing store
        if (write_bs((char *)page_phys_addr, (bsd_t)store, pageth) == SYSERR) {
          kprintf("write_dirty_page: write_bs failed for VHEAP pid %d store %d page %d\n", pid, store, pageth);
          return SYSERR;
        }
        heap_found = 1;
      }
    }
  }
  
  if (!xmmap_found && !heap_found) {
    kprintf("write_dirty_page: No backing store mapping for pid %d vpno %d\n", pid, vpno);
    return SYSERR;
  }
  
  return OK;
}

/*-------------------------------------------------------------------------
 * sync_bs_page - write back any dirty pages that map to the given backing
 * store page. This is needed for shared xmmap pages to ensure consistency.
 * Also invalidates the current process's page if present (unless it's dirty),
 * so it will be reloaded with the latest data from backing store.
 * 
 * Returns OK if successful, SYSERR otherwise.
 *-------------------------------------------------------------------------
 */
SYSCALL sync_bs_page(int store, int pageth, int curr_vpno)
{
  int i;
  int pid, vpno;
  pd_t *pd;
  pt_t *pt;
  unsigned int pd_idx, pt_idx;
  unsigned long vaddr;
  int frm_idx;
  int wrote_back = 0;
  
  // Check all xmmap mappings to find processes that might have this BS page dirty
  for (i = 0; i < xmmap_count; i++) {
    if (xmmap_tab[i].xm_bs_id == store && xmmap_tab[i].xm_pid != -1) {
      pid = xmmap_tab[i].xm_pid;
      
      // Calculate which virtual page in this mapping corresponds to pageth
      vpno = xmmap_tab[i].xm_vpno + pageth;
      
      // Check if this virtual page is within the mapping range
      if (pageth >= 0 && pageth < xmmap_tab[i].xm_npages) {
        vaddr = (unsigned long)vpno << 12;
        pd_idx = (vaddr >> 22) & 0x3FF;
        pt_idx = (vaddr >> 12) & 0x3FF;
        
        // Get page directory
        pd = (pd_t *) proctab[pid].pdbr;
        
        if (pd[pd_idx].pd_pres) {
          pt = (pt_t *)(pd[pd_idx].pd_base << 12);
          
          // Check if page is present and dirty
          if (pt[pt_idx].pt_pres && pt[pt_idx].pt_dirty) {
            // Find the frame index
            frm_idx = (int)pt[pt_idx].pt_base - FRAME0;
            
            if (frm_idx >= 0 && frm_idx < NFRAMES) {
              // Write dirty page back to backing store
              if (write_dirty_page(pid, vpno, frm_idx) == SYSERR) {
                kprintf("sync_bs_page: Failed to write dirty page for pid %d vpno %d\n", pid, vpno);
                return SYSERR;
              }
              
              // Clear dirty bit after successful write
              pt[pt_idx].pt_dirty = 0;
              if (frm_tab[frm_idx].fr_status == FRM_MAPPED) {
                frm_tab[frm_idx].fr_dirty = 0;
              }
              wrote_back = 1;
            }
          }
        }
      }
    }
  }
  
  // If we wrote back pages, invalidate ALL processes' pages for this backing store page
  // (except those that are dirty, meaning they have the latest version)
  // This ensures that when any process accesses this page, it will be reloaded with latest data
  if (wrote_back) {
    // Go through all xmmap mappings for this backing store page and invalidate
    // non-dirty pages so they'll be reloaded
    for (i = 0; i < xmmap_count; i++) {
      if (xmmap_tab[i].xm_bs_id == store && xmmap_tab[i].xm_pid != -1) {
        pid = xmmap_tab[i].xm_pid;
        
        // Calculate which virtual page in this mapping corresponds to pageth
        vpno = xmmap_tab[i].xm_vpno + pageth;
        
        // Check if this virtual page is within the mapping range
        if (pageth >= 0 && pageth < xmmap_tab[i].xm_npages) {
          vaddr = (unsigned long)vpno << 12;
          pd_idx = (vaddr >> 22) & 0x3FF;
          pt_idx = (vaddr >> 12) & 0x3FF;
          
          // Get page directory
          pd = (pd_t *) proctab[pid].pdbr;
          
          if (pd[pd_idx].pd_pres) {
            pt = (pt_t *)(pd[pd_idx].pd_base << 12);
            
            // If page is present and not dirty, invalidate it to force reload
            if (pt[pt_idx].pt_pres && !pt[pt_idx].pt_dirty) {
              int old_frm_idx = (int)pt[pt_idx].pt_base - FRAME0;
              pt[pt_idx].pt_pres = 0;
              
              // Decrement reference count of old frame
              if (old_frm_idx >= 0 && old_frm_idx < NFRAMES) {
                frm_tab[old_frm_idx].fr_refcnt--;
                if (frm_tab[old_frm_idx].fr_refcnt == 0) {
                  frm_tab[old_frm_idx].fr_status = FRM_UNMAPPED;
                  frm_tab[old_frm_idx].fr_pid = -1;
                  frm_tab[old_frm_idx].fr_vpno = 0;
                  frm_tab[old_frm_idx].fr_type = FR_PAGE;
                  frm_tab[old_frm_idx].fr_dirty = 0;
                }
              }
              
              // Decrement page table reference count
              int pt_frm_idx = (int)pd[pd_idx].pd_base - FRAME0;
              if (pt_frm_idx >= 0 && pt_frm_idx < NFRAMES) {
                frm_tab[pt_frm_idx].fr_refcnt--;
                if (frm_tab[pt_frm_idx].fr_refcnt == 0) {
                  pd[pd_idx].pd_pres = 0;
                }
              }
              
              // Invalidate TLB for this process
              // Note: We need to switch to this process's context to invalidate its TLB
              // But that's complex, so we'll invalidate all TLBs
              invltlb(0);
            }
          }
        }
      }
    }
  }
  
  return OK;
}

/*-------------------------------------------------------------------------
 * add_to_sc_queue - add a frame to the Second-Chance circular queue
 *-------------------------------------------------------------------------
 */
void add_to_sc_queue(int frm_idx)
{
  /* Check if frame is already in queue */
  if (sc_next[frm_idx] != -1) {
    return;  /* Already in queue */
  }
  
  if (sc_head == -1) {
    /* Queue is empty, initialize */
    sc_head = frm_idx;
    sc_next[frm_idx] = frm_idx;  /* Point to itself (circular) */
    sc_count = 1;
  } else {
    /* Add to end of queue (before head) */
    int current = sc_head;
    /* Find the last element (points to head) */
    while (sc_next[current] != sc_head) {
      current = sc_next[current];
    }
    /* Insert new frame after last element, before head */
    sc_next[frm_idx] = sc_head;
    sc_next[current] = frm_idx;
    sc_count++;
  }
}

/*-------------------------------------------------------------------------
 * remove_from_sc_queue - remove a frame from the Second-Chance queue
 *-------------------------------------------------------------------------
 */
static void remove_from_sc_queue(int frm_idx)
{
  if (sc_head == -1) {
    return;  /* Queue is empty */
  }
  
  if (sc_next[frm_idx] == -1) {
    return;  /* Frame not in queue */
  }
  
  if (sc_count == 1) {
    /* Only one element */
    sc_head = -1;
    sc_next[frm_idx] = -1;
    sc_count = 0;
    return;
  }
  
  /* Find previous element */
  int current = sc_head;
  while (sc_next[current] != frm_idx) {
    current = sc_next[current];
    if (current == sc_head) {
      /* Frame not found in queue */
      return;
    }
  }
  
  /* Remove from queue */
  sc_next[current] = sc_next[frm_idx];
  if (sc_head == frm_idx) {
    sc_head = sc_next[frm_idx];
  }
  sc_next[frm_idx] = -1;
  sc_count--;
}

/*-------------------------------------------------------------------------
 * evict_frame - Second-Chance page replacement algorithm
 * 
 * Returns the frame index to evict, or SYSERR if no frame can be evicted.
 *-------------------------------------------------------------------------
 */
static int evict_frame(void)
{
  int candidate;
  int start;
  int pid, vpno;
  unsigned long vaddr;
  unsigned int pd_idx, pt_idx;
  pd_t *pd;
  pt_t *pt;
  
  if (sc_head == -1) {
    int i;
    for (i = 5; i < NFRAMES; i++) {
      if (frm_tab[i].fr_status == FRM_MAPPED && frm_tab[i].fr_type == FR_PAGE) {
        return i;
      }
    }
    return SYSERR;
  }
  
  start = sc_head;
  candidate = sc_head;
  
  int first_pass = 1;
  int iterations = 0;
  int max_iterations = sc_count * 2 + 10;  
  
  do {
    iterations++;
    if (iterations > max_iterations) {
      break;
    }
    
    pid = frm_tab[candidate].fr_pid;
    vpno = frm_tab[candidate].fr_vpno;
    
    if (pid == -1 || frm_tab[candidate].fr_status != FRM_MAPPED || 
        frm_tab[candidate].fr_type != FR_PAGE) {
      int next = sc_next[candidate];
      if (next == -1 || next == candidate) {
        break;
      }
      candidate = next;
      if (candidate == start) {
        if (!first_pass) {
          break;
        }
        first_pass = 0;
      }
      continue;
    }
    
    vaddr = (unsigned long)vpno << 12;
    pd_idx = (vaddr >> 22) & 0x3FF;
    pt_idx = (vaddr >> 12) & 0x3FF;
    
    pd = (pd_t *) proctab[pid].pdbr;
    if (!pd || !pd[pd_idx].pd_pres) {
      int next = sc_next[candidate];
      if (next == -1 || next == candidate) {
        break;
      }
      candidate = next;
      if (candidate == start) {
        if (!first_pass) {
          break;
        }
        first_pass = 0;
      }
      continue;
    }
    
    pt = (pt_t *)(pd[pd_idx].pd_base << 12);
    if (!pt || !pt[pt_idx].pt_pres) {
      /* Skip if page table entry not present */
      int next = sc_next[candidate];
      if (next == -1 || next == candidate) {
        break;
      }
      candidate = next;
      if (candidate == start) {
        if (!first_pass) {
          break;
        }
        first_pass = 0;
      }
      continue;
    }
    
    if (!pt[pt_idx].pt_acc) {
      int next = sc_next[candidate];
      if (next != -1 && next != candidate) {
        sc_head = next;
      } else {
        sc_head = -1;
      }
      return candidate;
    }
    
    /* Clear the reference bit */
    pt[pt_idx].pt_acc = 0;
    
    int next = sc_next[candidate];
    if (next == -1 || next == candidate) {
      break;
    }
    candidate = next;
    
    sc_head = candidate;
    
    if (candidate == start) {
      if (!first_pass) {
        break;
      }
      first_pass = 0;  
    }
    
  } while (1);

  if (candidate >= 0 && candidate < NFRAMES &&
      frm_tab[candidate].fr_status == FRM_MAPPED &&
      frm_tab[candidate].fr_type == FR_PAGE) {
    return candidate;
  }
  
  int i;
  for (i = 5; i < NFRAMES; i++) {
    if (frm_tab[i].fr_status == FRM_MAPPED && frm_tab[i].fr_type == FR_PAGE) {
      return i;
    }
  }
  
  return SYSERR;
}

/*-------------------------------------------------------------------------
 * get_frm - get a free frame according page replacement policy
 * 
 * out variables: avail
 * 
 * If a frame is available, return OK and set avail to the available frame.
 * 
 * If no frames are available, return SYSERR and set avail to any value or 
 * not set it at all, since callers should always check the return code prior 
 * to making use of out variables.
 *-------------------------------------------------------------------------
 */
SYSCALL get_frm(int* avail)
{
  int i;
  int evict_idx;
  int evict_pid, evict_vpno;
  unsigned long evict_vaddr;
  unsigned int pd_idx, pt_idx;
  pd_t *pd;
  pt_t *pt;
  int pt_frm_idx;
  
  // Skip frames 0-4 which are used for global PTs and NULL PD 
  for (i = 5; i < NFRAMES; i++) {
    if (frm_tab[i].fr_status == FRM_UNMAPPED) {
      if (avail) *avail = i;
      return OK;
    }
  }
  
  evict_idx = evict_frame();
  if (evict_idx == SYSERR) {
    return SYSERR;
  }
  
  if (pr_debug_flag) {
    kprintf("%d\n", evict_idx);
  }
  
  // Get information about the frame to evict
  evict_pid = frm_tab[evict_idx].fr_pid;
  evict_vpno = frm_tab[evict_idx].fr_vpno;
  evict_vaddr = (unsigned long)evict_vpno << 12;
  
  // Get page directory and page table
  pd = (pd_t *) proctab[evict_pid].pdbr;
  pd_idx = (evict_vaddr >> 22) & 0x3FF;
  pt_idx = (evict_vaddr >> 12) & 0x3FF;
  
  if (!pd[pd_idx].pd_pres) {
    kprintf("get_frm: Page directory not present for evicted page pid %d\n", evict_pid);
    return SYSERR;
  }
  
  pt = (pt_t *)(pd[pd_idx].pd_base << 12);
  
  // Check if page is dirty and write it back
  if (pt[pt_idx].pt_dirty) {
    // Also update frame table dirty bit
    frm_tab[evict_idx].fr_dirty = 1;
    
    // Write dirty page back to backing store
    if (write_dirty_page(evict_pid, evict_vpno, evict_idx) == SYSERR) {
      kprintf("get_frm: Failed to write dirty page for pid %d vpno %d\n", evict_pid, evict_vpno);
      kill(evict_pid);
      return SYSERR;
    }
    
    // Clear dirty bit in page table
    pt[pt_idx].pt_dirty = 0;
    frm_tab[evict_idx].fr_dirty = 0;
  }
  
  // Mark page table entry as not present
  pt[pt_idx].pt_pres = 0;
  
  // Invalidate TLB if this is the current process
  if (evict_pid == currpid) {
    invltlb(evict_vaddr);
  }
  
  // Decrement reference count of page table frame
  pt_frm_idx = (int)pd[pd_idx].pd_base - FRAME0;
  if (pt_frm_idx >= 0 && pt_frm_idx < NFRAMES) {
    frm_tab[pt_frm_idx].fr_refcnt--;
    // If reference count reaches zero, mark page table as not present
    if (frm_tab[pt_frm_idx].fr_refcnt == 0) {
      pd[pd_idx].pd_pres = 0;
    }
  }
  
  remove_from_sc_queue(evict_idx);
  
  // Clear the frame table entry
  frm_tab[evict_idx].fr_status = FRM_UNMAPPED;
  frm_tab[evict_idx].fr_pid = -1;
  frm_tab[evict_idx].fr_vpno = 0;
  frm_tab[evict_idx].fr_refcnt = 0;
  frm_tab[evict_idx].fr_type = FR_PAGE;
  frm_tab[evict_idx].fr_dirty = 0;
  
  if (avail) *avail = evict_idx;
  return OK;
}

/*-------------------------------------------------------------------------
 * free_frm - free frame i
 *-------------------------------------------------------------------------
 */
SYSCALL free_frm(int i)
{

  if (i < 0 || i >= NFRAMES) {
    return SYSERR;
  }
  
  remove_from_sc_queue(i);
  
  frm_tab[i].fr_status = FRM_UNMAPPED;
  frm_tab[i].fr_pid = -1;
  frm_tab[i].fr_vpno = 0;
  frm_tab[i].fr_refcnt = 0;
  frm_tab[i].fr_type = FR_PAGE;
  frm_tab[i].fr_dirty = 0;
  return OK;
}



