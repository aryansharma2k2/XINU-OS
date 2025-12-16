/* paging.h */

typedef unsigned int	 bsd_t;

/* Structure for a page directory entry */

typedef struct {

  unsigned int pd_pres	: 1;		/* page table present?		*/
  unsigned int pd_write : 1;		/* page is writable?		*/
  unsigned int pd_user	: 1;		/* is use level protection?	*/
  unsigned int pd_pwt	: 1;		/* write through cachine for pt?*/
  unsigned int pd_pcd	: 1;		/* cache disable for this pt?	*/
  unsigned int pd_acc	: 1;		/* page table was accessed?	*/
  unsigned int pd_mbz	: 1;		/* must be zero			*/
  unsigned int pd_fmb	: 1;		/* four MB pages?		*/
  unsigned int pd_global: 1;		/* global (ignored)		*/
  unsigned int pd_avail : 3;		/* for programmer's use		*/
  unsigned int pd_base	: 20;		/* location of page table?	*/
} pd_t;

/* Structure for a page table entry */

typedef struct {

  unsigned int pt_pres	: 1;		/* page is present?		*/
  unsigned int pt_write : 1;		/* page is writable?		*/
  unsigned int pt_user	: 1;		/* is use level protection?	*/
  unsigned int pt_pwt	: 1;		/* write through for this page? */
  unsigned int pt_pcd	: 1;		/* cache disable for this page? */
  unsigned int pt_acc	: 1;		/* page was accessed?		*/
  unsigned int pt_dirty : 1;		/* page was written?		*/
  unsigned int pt_mbz	: 1;		/* must be zero			*/
  unsigned int pt_global: 1;		/* should be zero in 586	*/
  unsigned int pt_avail : 3;		/* for programmer's use		*/
  unsigned int pt_base	: 20;		/* location of page?		*/
} pt_t;

typedef struct{
  unsigned int pg_offset : 12;		/* page offset			*/
  unsigned int pt_offset : 10;		/* page table offset		*/
  unsigned int pd_offset : 10;		/* page directory offset	*/
} virt_addr_t;

typedef struct{
  int bs_status;			/* MAPPED or UNMAPPED		*/
  int bs_pid;				/* process id (or BS_XMMAP_PID for shared xmmap) */
  int bs_vpno;				/* starting virtual page number (for VHEAP only) */
  int bs_npages;			/* number of pages in the store (for VHEAP only) */
  int bs_sem;				/* semaphore mechanism ?	*/
  int bs_type;				/* BS_TYPE_VHEAP or BS_TYPE_XMMAP */
} bs_map_t;

/* Structure to track individual xmmap mappings (for shared backing stores) */
typedef struct {
  int xm_pid;				/* process id */
  int xm_vpno;				/* starting virtual page number */
  int xm_npages;			/* number of pages */
  int xm_bs_id;				/* backing store id */
} xmmap_entry_t;

typedef struct{
  int fr_status;			/* MAPPED or UNMAPPED		*/
  int fr_pid;				/* process id using this frame  */
  int fr_vpno;				/* corresponding virtual page no*/
  int fr_refcnt;			/* reference count		*/
  int fr_type;				/* FR_DIR, FR_TBL, FR_PAGE	*/
  int fr_dirty;
}fr_map_t;

// the backing store mappings of the currently active process
extern bs_map_t bsm_tab[];
// the inverted page table used for page allocation and replacement
extern fr_map_t frm_tab[];
// xmmap mappings (for shared backing stores) 
#define MAX_XMMAP_ENTRIES (MAX_BS * NPROC)
extern xmmap_entry_t xmmap_tab[];
extern int xmmap_count;
/* Prototypes for required API calls */
SYSCALL xmmap(int, bsd_t, int);
SYSCALL xunmap(int);

/* Backing store management APIs */
SYSCALL init_bsm();
SYSCALL get_bsm(int* avail);
SYSCALL free_bsm(int i);
SYSCALL bsm_lookup(int pid, long vaddr, int* store, int* pageth);
SYSCALL xmmap_lookup(int pid, long vaddr, int* store, int* pageth);
SYSCALL bsm_map(int pid, int vpno, int source, int npages);
SYSCALL bsm_unmap(int pid, int vpno);

/* given calls for dealing with backing store */

SYSCALL read_bs(char *, bsd_t, int);
SYSCALL write_bs(char *, bsd_t, int);
SYSCALL invltlb(unsigned long);

#define NBPG		4096	/* number of bytes per page	*/
#define FRAME0		1024	/* zero-th frame		*/
#define NFRAMES 	1024	/* number of frames		*/

#define BSM_UNMAPPED	0
#define BSM_MAPPED	1

// Backing store types
#define BS_TYPE_VHEAP	1	// Exclusive: used for virtual heap 
#define BS_TYPE_XMMAP	2	// Shared: used for xmmap (multiple processes can share)

// Special pid value to indicate shared xmmap backing store 
#define BS_XMMAP_PID	-2

#define FRM_UNMAPPED	0
#define FRM_MAPPED	1

#define FR_PAGE		0
#define FR_TBL		1
#define FR_DIR		2

#define SC 3
#define AGING 4

#define BACKING_STORE_BASE	0x00800000
#define BACKING_STORE_UNIT_SIZE 0x00100000

/* Number of backing stores */
#define MAX_BS 8
