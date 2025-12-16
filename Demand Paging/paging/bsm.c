/* bsm.c - manage the backing store mapping*/

#include <conf.h>
#include <kernel.h>
#include <paging.h>
#include <proc.h>

bs_map_t bsm_tab[MAX_BS];
xmmap_entry_t xmmap_tab[MAX_XMMAP_ENTRIES];
int xmmap_count = 0;

/*-------------------------------------------------------------------------
 * init_bsm - initialize bsm_tab
 *-------------------------------------------------------------------------
 */
SYSCALL init_bsm()
{
    int i;
    for (i = 0; i < MAX_BS; i++) {
        bsm_tab[i].bs_status = BSM_UNMAPPED;
        bsm_tab[i].bs_pid = -1;
        bsm_tab[i].bs_vpno = 0;
        bsm_tab[i].bs_npages = 0;
        bsm_tab[i].bs_sem = 0;
        bsm_tab[i].bs_type = BS_TYPE_VHEAP;
    }
    for (i = 0; i < MAX_XMMAP_ENTRIES; i++) {
        xmmap_tab[i].xm_pid = -1;
        xmmap_tab[i].xm_vpno = 0;
        xmmap_tab[i].xm_npages = 0;
        xmmap_tab[i].xm_bs_id = -1;
    }
    xmmap_count = 0;
    return OK;
}

/*-------------------------------------------------------------------------
 * get_bsm - get a free entry from bsm_tab
 * 
 * out variables: avail
 *
 * If a backing store is available, the call returns OK and sets avail
 * to the index of the free backing store.
 * 
 * If no backing stores are available, return SYSERR. You may set avail
 * to any value or not set it at all, since callers should always check
 * the return code prior to making use of out variables.
 *-------------------------------------------------------------------------
 */
SYSCALL get_bsm(int* avail)
{
    int i;
    // Only return backing stores that are completely free (unmapped)
    // This is for VHEAP which requires exclusive ownership
    for (i = 0; i < MAX_BS; i++) {
        if (bsm_tab[i].bs_status == BSM_UNMAPPED) {
            if (avail) *avail = i;
            return OK;
        }
    }
    return SYSERR;
}


/*-------------------------------------------------------------------------
 * free_bsm - free backing store i in the bsm_tab
 *
 * Returns OK if the backing store was successfully freed or if the backing
 * store was already free.
 * 
 * Returns SYSERR if the backing store could not be successfully freed.
 *-------------------------------------------------------------------------
 */
SYSCALL free_bsm(int i)
{
    if (i < 0 || i >= MAX_BS) {
        return SYSERR;
    }
    /* Free if mapped or already free */
    bsm_tab[i].bs_status = BSM_UNMAPPED;
    bsm_tab[i].bs_pid = -1;
    bsm_tab[i].bs_vpno = 0;
    bsm_tab[i].bs_npages = 0;
    bsm_tab[i].bs_sem = 0;
    bsm_tab[i].bs_type = BS_TYPE_VHEAP;
    return OK;
}

/*-------------------------------------------------------------------------
 * bsm_lookup - find the backing store and page corresponding to the given
 * pid and vaddr.
 * 
 * out variables: store, pageth
 * 
 * If there is a corresponding backing store and the vaddr is valid,
 * return OK, set store to the backing store index, and pageth to the 
 * relative page number within the backing store.
 * 
 * If no mapping exists or the vaddr is invalid, return SYSERR. You
 * may set store and pageth to any value or not set them at all, since 
 * callers should always check the return code prior to making use of 
 * out variables.
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_lookup(int pid, long vaddr, int* store, int* pageth)
{
    int i;
    unsigned long vpno;
    if (isbadpid(pid)) {
        return SYSERR;
    }
    vpno = ((unsigned long)vaddr) >> 12;
    
    // Check VHEAP mappings (exclusive ownership)
    for (i = 0; i < MAX_BS; i++) {
        if (bsm_tab[i].bs_status == BSM_MAPPED && 
            bsm_tab[i].bs_type == BS_TYPE_VHEAP && 
            bsm_tab[i].bs_pid == pid) {
            int start = bsm_tab[i].bs_vpno;
            int count = bsm_tab[i].bs_npages;
            if ((int)vpno >= start && (int)vpno < (start + count)) {
                if (store) *store = i;
                if (pageth) *pageth = (int)vpno - start;
                return OK;
            }
        }
    }
    
    return SYSERR;
}


/*-------------------------------------------------------------------------
 * bsm_map - add a mapping into bsm_tab for the given pid starting at the
 * vpno.
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_map(int pid, int vpno, int source, int npages)
{
    if (isbadpid(pid) || source < 0 || source >= MAX_BS || npages <= 0 || npages > 256) {
        return SYSERR;
    }

    // This function is for VHEAP mappings (exclusive ownership)
    // Check if backing store is already used for xmmap (shared)
    if (bsm_tab[source].bs_status == BSM_MAPPED && 
        bsm_tab[source].bs_type == BS_TYPE_XMMAP) {
        // Backing store is used for xmmap - cannot use for VHEAP
        return SYSERR;
    }

    // Check if backing store is already owned by another process for VHEAP
    if (bsm_tab[source].bs_status == BSM_MAPPED && 
        bsm_tab[source].bs_type == BS_TYPE_VHEAP && 
        bsm_tab[source].bs_pid != pid) {
        // Another process owns this backing store for VHEAP
        return SYSERR;
    }

     // Map backing store for this process's VHEAP (exclusive ownership)
    bsm_tab[source].bs_status = BSM_MAPPED;
    bsm_tab[source].bs_type = BS_TYPE_VHEAP;
    bsm_tab[source].bs_pid = pid;
    bsm_tab[source].bs_vpno = vpno;
    bsm_tab[source].bs_npages = npages;
    return OK;
}



/*-------------------------------------------------------------------------
 * bsm_unmap - delete the mapping from bsm_tab that corresponds to the 
 * given pid, vpno pair.
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_unmap(int pid, int vpno)
{
    int i;
    if (isbadpid(pid)) {
        return SYSERR;
    }
    for (i = 0; i < MAX_BS; i++) {
        if (bsm_tab[i].bs_status == BSM_MAPPED && 
            bsm_tab[i].bs_type == BS_TYPE_VHEAP &&
            bsm_tab[i].bs_pid == pid && 
            bsm_tab[i].bs_vpno == vpno) {
            bsm_tab[i].bs_status = BSM_UNMAPPED;
            bsm_tab[i].bs_pid = -1;
            bsm_tab[i].bs_vpno = 0;
            bsm_tab[i].bs_npages = 0;
            bsm_tab[i].bs_sem = 0;
            bsm_tab[i].bs_type = BS_TYPE_VHEAP;
            return OK;
        }
    }
    return SYSERR;
}

/*-------------------------------------------------------------------------
 * xmmap_lookup - find the xmmap mapping for the given pid and vaddr.
 * 
 * out variables: store, pageth
 * 
 * If there is a corresponding xmmap mapping, return OK, set store to the 
 * backing store index, and pageth to the relative page number within the 
 * backing store.
 * 
 * If no mapping exists or the vaddr is invalid, return SYSERR.
 *-------------------------------------------------------------------------
 */
SYSCALL xmmap_lookup(int pid, long vaddr, int* store, int* pageth)
{
    int i;
    unsigned long vpno;
    if (isbadpid(pid)) {
        return SYSERR;
    }
    vpno = ((unsigned long)vaddr) >> 12;
    
    // Check xmmap mappings (shared backing stores)
    for (i = 0; i < xmmap_count; i++) {
        if (xmmap_tab[i].xm_pid == pid &&
            (int)vpno >= xmmap_tab[i].xm_vpno &&
            (int)vpno < xmmap_tab[i].xm_vpno + xmmap_tab[i].xm_npages) {
            if (store) *store = xmmap_tab[i].xm_bs_id;
            if (pageth) *pageth = (int)vpno - xmmap_tab[i].xm_vpno;
            return OK;
        }
    }
    
    return SYSERR;
}


