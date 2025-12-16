
#include <conf.h>
#include <kernel.h>
#include <paging.h>

int pr_debug_flag = 0;

/*-------------------------------------------------------------------------
 * enable_pr_debug - turn on page replacement debug logging.
 *-------------------------------------------------------------------------
 */
SYSCALL enable_pr_debug()
{
  pr_debug_flag = 1;
  return OK;
}

/*-------------------------------------------------------------------------
 * disable_pr_debugging - turn off page replacement debug logging.
 *-------------------------------------------------------------------------
 */
SYSCALL disable_pr_debug()
{
  pr_debug_flag = 0;
  return OK;
}