#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <mark.h>
#include <bufpool.h>
#include <paging.h>

/*-------------------------------------------------------------------------
 * write_bs - write one page of data from src to the backing store bs_id, 
 * page page.
 *-------------------------------------------------------------------------
 */
SYSCALL write_bs(char *src, bsd_t bs_id, int page) {

  // Check illegal bs_id
  if (bs_id < 0 || bs_id > 7) {
    return SYSERR;
  }

  // Check illegal page
  if (page < 0 || page > 255) {
    return SYSERR;
  }

  // Check illegal src pointer
  if (src == NULL) {
    return SYSERR;
  }

  char *phy_addr = (char *)(BACKING_STORE_BASE + bs_id * BACKING_STORE_UNIT_SIZE + page * NBPG);

  bcopy((void *)src, (void *)phy_addr, NBPG);

  return OK;
}

