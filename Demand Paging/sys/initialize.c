/* initialize.c - nulluser, sizmem, sysinit */

#include <conf.h>
#include <i386.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <sleep.h>
#include <mem.h>
#include <tty.h>
#include <q.h>
#include <io.h>
#include <paging.h>

/*#define DETAIL */
#define HOLESIZE	(600)	
#define	HOLESTART	(640 * 1024)
#define	HOLEEND		((1024 + HOLESIZE) * 1024)  
/* Extra 600 for bootp loading, and monitor */

extern	int	main();	/* address of user's main prog	*/

extern	int	start();

LOCAL		sysinit();

// External functions for paging
extern	void	write_cr3(unsigned long);
extern	void	enable_paging(void);
extern	int	set_evec(unsigned int, u_long);  // Install interrupt handler 
extern	void	pfintr(void);                     // Page fault ISR 

/* Declarations of major kernel variables */
struct	pentry	proctab[NPROC]; /* process table			*/
int	nextproc;		/* next process slot to use in create	*/
struct	sentry	semaph[NSEM];	/* semaphore table			*/
int	nextsem;		/* next sempahore slot to use in screate*/
struct	qent	q[NQENT];	/* q table (see queue.c)		*/
int	nextqueue;		/* next slot in q structure to use	*/
char	*maxaddr;		/* max memory address (set by sizmem)	*/
struct	mblock	memlist;	/* list of free memory blocks		*/
#ifdef	Ntty
struct  tty     tty[Ntty];	/* SLU buffers and mode control		*/
#endif

/* active system status */
int	numproc;		/* number of live user processes	*/
int	currpid;		/* id of currently running process	*/
int	reboot = 0;		/* non-zero after first boot		*/

int	rdyhead,rdytail;	/* head/tail of ready list (q indicies)	*/
char 	vers[100];
int	console_dev;		/* the console device			*/

/*  added for the demand paging */
int page_replace_policy = SC;

// Global page tables - store physical addresses of the 4 page tables 
unsigned long global_pt_addrs[4];  // Physical addresses of 4 global page tables 

/************************************************************************/
/***				NOTE:				      ***/
/***								      ***/
/***   This is where the system begins after the C environment has    ***/
/***   been established.  Interrupts are initially DISABLED, and      ***/
/***   must eventually be enabled explicitly.  This routine turns     ***/
/***   itself into the null process after initialization.  Because    ***/
/***   the null process must always remain ready to run, it cannot    ***/
/***   execute code that might cause it to be suspended, wait for a   ***/
/***   semaphore, or put to sleep, or exit.  In particular, it must   ***/
/***   not do I/O unless it uses kprintf for polled output.           ***/
/***								      ***/
/************************************************************************/

/*------------------------------------------------------------------------
 *  nulluser  -- initialize system and become the null process (id==0)
 *------------------------------------------------------------------------
 */
nulluser()				/* babysit CPU when no one is home */
{
        int userpid;

	console_dev = SERIAL0;		/* set console to COM0 */

	initevec();

	kprintf("system running up!\n");
	sysinit();

	enable();		/* enable interrupts */

	sprintf(vers, "PC Xinu %s", VERSION);
	kprintf("\n\n%s\n", vers);
	if (reboot++ < 1)
		kprintf("\n");
	else
		kprintf("   (reboot %d)\n", reboot);


	kprintf("%d bytes real mem\n",
		(unsigned long) maxaddr+1);
#ifdef DETAIL	
	kprintf("    %d", (unsigned long) 0);
	kprintf(" to %d\n", (unsigned long) (maxaddr) );
#endif	

	kprintf("%d bytes Xinu code\n",
		(unsigned long) ((unsigned long) &end - (unsigned long) start));
#ifdef DETAIL	
	kprintf("    %d", (unsigned long) start);
	kprintf(" to %d\n", (unsigned long) &end );
#endif

#ifdef DETAIL	
	kprintf("%d bytes user stack/heap space\n",
		(unsigned long) ((unsigned long) maxaddr - (unsigned long) &end));
	kprintf("    %d", (unsigned long) &end);
	kprintf(" to %d\n", (unsigned long) maxaddr);
#endif	
	
	kprintf("clock %sabled\n", clkruns == 1?"en":"dis");


	/* create a process to execute the user's main program */
	userpid = create(main,INITSTK,INITPRIO,INITNAME,INITARGS);
	resume(userpid);

	while (TRUE)
		/* empty */;
}

// Identity mapping for physical pages 
int init_global_page_tables()
{
	int i, j;
	pt_t *pt;          
	unsigned long phys_addr;
	unsigned long frame_num;
	int frame_idx;  /* Frame index in frm_tab */
	
	
	for (i = 0; i < 4; i++) {
		frame_idx = i;  // Global page tables will be in frames 0-3 
		phys_addr = (FRAME0 + frame_idx) * NBPG;
		
		global_pt_addrs[i] = phys_addr;
		pt = (pt_t *)phys_addr;
		// Zero out the entire page table and set values
		for (j = 0; j < 1024; j++) {
			frame_num = (i * 1024) + j;

			pt[j].pt_pres = 1;   
			pt[j].pt_write = 1;  // Two bits set to 1, given in readme.  
			pt[j].pt_user = 0;
			pt[j].pt_pwt = 0;
			pt[j].pt_pcd = 0;
			pt[j].pt_acc = 0;
			pt[j].pt_dirty = 0;
			pt[j].pt_mbz = 0;
			pt[j].pt_global = 0;
			pt[j].pt_avail = 0;
			pt[j].pt_base = (unsigned int)(frame_num);  // Set physical frame number
		}

		frm_tab[frame_idx].fr_status = FRM_MAPPED;
		frm_tab[frame_idx].fr_pid = NULLPROC;
		frm_tab[frame_idx].fr_vpno = 0;
		frm_tab[frame_idx].fr_refcnt = 1024;
		frm_tab[frame_idx].fr_type = FR_TBL;
		frm_tab[frame_idx].fr_dirty = 0;

		/* Debug print */
		kprintf("Global page table %d initialized at phys addr 0x%08x (frame %d)\n",
		        i, global_pt_addrs[i], frame_idx);
	}
	// TODO: Remove after debugging
	kprintf("Global page tables initialized successfully\n");
	return OK;
}

// Allocate and initialize page directory for NULL process
int init_null_pd()
{
	int i;
	pd_t *pd;                    
	unsigned long phys_addr;
	unsigned long pt_phys_addr;
	unsigned long pt_frame_num;
	int frame_idx = 4; // reserve frame 4 for NULL process page directory
	
	phys_addr = (FRAME0 + frame_idx) * NBPG;
	pd = (pd_t *)phys_addr;
	
	// Zero out and set up all 1024 entries in the page directory
	for (i = 0; i < 1024; i++) {
		if (i < 4) {
			// First 4 entries point to global page tables (identity mapping for pages 0-4095)
			pt_phys_addr = global_pt_addrs[i];
			pt_frame_num = pt_phys_addr >> 12;
			pd[i].pd_pres = 1;              // Page table is present 
			pd[i].pd_write = 1;
			pd[i].pd_user = 0;
			pd[i].pd_pwt = 0;
			pd[i].pd_pcd = 0;
			pd[i].pd_acc = 0;
			pd[i].pd_mbz = 0;
			pd[i].pd_fmb = 0;
			pd[i].pd_global = 0;
			pd[i].pd_avail = 0;
			pd[i].pd_base = (unsigned int)(pt_frame_num);
		} else {
			// All other entries are not present
			pd[i].pd_pres = 0;
			pd[i].pd_write = 0;
			pd[i].pd_user = 0;
			pd[i].pd_pwt = 0;
			pd[i].pd_pcd = 0;
			pd[i].pd_acc = 0;
			pd[i].pd_mbz = 0;
			pd[i].pd_fmb = 0;
			pd[i].pd_global = 0;
			pd[i].pd_avail = 0;
			pd[i].pd_base = 0;
		}
	}
	
	/* Store the page directory address in NULL process entry */
	proctab[NULLPROC].pdbr = phys_addr;

	frm_tab[frame_idx].fr_status = FRM_MAPPED;
	frm_tab[frame_idx].fr_pid = NULLPROC;
	frm_tab[frame_idx].fr_vpno = 0;
	frm_tab[frame_idx].fr_refcnt = 0;
	frm_tab[frame_idx].fr_type = FR_DIR;
	frm_tab[frame_idx].fr_dirty = 0;

	kprintf("NULL process page directory initialized at phys addr 0x%08x (frame %d)\n", 
	        phys_addr, frame_idx);
	return OK;
}

/*------------------------------------------------------------------------
 *  sysinit  --  initialize all Xinu data structeres and devices
 *------------------------------------------------------------------------
 */
LOCAL
sysinit()
{
	static	long	currsp;
	int	i,j;
	struct	pentry	*pptr;
	struct	sentry	*sptr;
	struct	mblock	*mptr;
	

	numproc = 0;			/* initialize system variables */
	nextproc = NPROC-1;
	nextsem = NSEM-1;
	nextqueue = NPROC;		/* q[0..NPROC-1] are processes */

	/* initialize free memory list */
	/* PC version has to pre-allocate 640K-1024K "hole" */
	if (maxaddr+1 > HOLESTART) {
		memlist.mnext = mptr = (struct mblock *) roundmb(&end);
		mptr->mnext = (struct mblock *)HOLEEND;
		mptr->mlen = (int) truncew(((unsigned) HOLESTART -
	     		 (unsigned)&end));
        mptr->mlen -= 4;

		mptr = (struct mblock *) HOLEEND;
		mptr->mnext = 0;
		mptr->mlen = (int) truncew((unsigned)maxaddr - HOLEEND -
	      		NULLSTK);
/*
		mptr->mlen = (int) truncew((unsigned)maxaddr - (4096 - 1024 ) *  4096 - HOLEEND - NULLSTK);
*/
	} else {
		/* initialize free memory list */
		memlist.mnext = mptr = (struct mblock *) roundmb(&end);
		mptr->mnext = 0;
		mptr->mlen = (int) truncew((unsigned)maxaddr - (int)&end -
			NULLSTK);
	}
	

	for (i=0 ; i<NPROC ; i++)	/* initialize process table */
		proctab[i].pstate = PRFREE;


#ifdef	MEMMARK
	_mkinit();			/* initialize memory marking */
#endif

#ifdef	RTCLOCK
	clkinit();			/* initialize r.t.clock	*/
#endif

	mon_init();     /* init monitor */

	/* Initialize inverted page table */
	init_frm();

	// Initialize global page tables (identity mapping for pages 0-4095)
	init_global_page_tables();

	// Allocate and initialize page directory for NULL process */
	init_null_pd();

	// Set CR3 (PDBR) to point to NULL process page directory */
	write_cr3(proctab[NULLPROC].pdbr);

	// Install page fault interrupt service routine (interrupt 14) */
	set_evec(14, (u_long)pfintr);

	// Enable paging - this must be done AFTER setting up page tables and CR3 */
	enable_paging();

	/* Initialize backing store mapping table */
	init_bsm();

#ifdef NDEVS
	for (i=0 ; i<NDEVS ; i++ ) {	    
	    init_dev(i);
	}
#endif

	pptr = &proctab[NULLPROC];	/* initialize null process entry */
	pptr->pstate = PRCURR;
	for (j=0; j<7; j++)
		pptr->pname[j] = "prnull"[j];
	pptr->plimit = (WORD)(maxaddr + 1) - NULLSTK;
	pptr->pbase = (WORD) maxaddr - 3;
/*
	pptr->plimit = (WORD)(maxaddr + 1) - NULLSTK - (4096 - 1024 )*4096;
	pptr->pbase = (WORD) maxaddr - 3 - (4096-1024)*4096;
*/
	pptr->pesp = pptr->pbase-4;	/* for stkchk; rewritten before used */
	*( (int *)pptr->pbase ) = MAGIC;
	pptr->paddr = (WORD) nulluser;
	pptr->pargs = 0;
	pptr->pprio = 0;
	currpid = NULLPROC;

	for (i=0 ; i<NSEM ; i++) {	/* initialize semaphores */
		(sptr = &semaph[i])->sstate = SFREE;
		sptr->sqtail = 1 + (sptr->sqhead = newqueue());
	}

	rdytail = 1 + (rdyhead=newqueue());/* initialize ready list */


	return(OK);
}

stop(s)
char	*s;
{
	kprintf("%s\n", s);
	kprintf("looping... press reset\n");
	while(1)
		/* empty */;
}

delay(n)
int	n;
{
	DELAY(n);
}


#define	NBPG	4096

/*------------------------------------------------------------------------
 * sizmem - return memory size (in pages)
 *------------------------------------------------------------------------
 */
long sizmem()
{
	unsigned char	*ptr, *start, stmp, tmp;
	int		npages;

	/* at least now its hacked to return
	   the right value for the Xinu lab backends (16 MB) */

	return 4096; 

	start = ptr = 0;
	npages = 0;
	stmp = *start;
	while (1) {
		tmp = *ptr;
		*ptr = 0xA5;
		if (*ptr != 0xA5)
			break;
		*ptr = tmp;
		++npages;
		ptr += NBPG;
		if ((int)ptr == HOLESTART) {	/* skip I/O pages */
			npages += (1024-640)/4;
			ptr = (unsigned char *)HOLEEND;
		}
	}
	return npages;
}
