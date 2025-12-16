#include <conf.h>
#include <kernel.h>
#include <mark.h>
#include <bufpool.h>
#include <proc.h>
#include <paging.h>

/*-------------------------------------------------------------------------
 * read_bs - read the specified page from the backing store corresponding 
 * to bs_id into dst.
 * 
 * out variables: dst
 *-------------------------------------------------------------------------
 */
SYSCALL read_bs(char *dst, bsd_t bs_id, int page) {

  // Check illegal bs_id
  if (bs_id < 0 || bs_id > 7) {
    return SYSERR;
  }

  // Check illegal page
  if (page < 0 || page > 255) {
    return SYSERR;
  }

  // Check illegal dst pointer
  if (dst == NULL) {
    return SYSERR;
  }

  char *phy_addr = (char *)(BACKING_STORE_BASE + bs_id * BACKING_STORE_UNIT_SIZE + page * NBPG);

  bcopy((void *)phy_addr, (void *)dst, NBPG);

  return OK;
}


