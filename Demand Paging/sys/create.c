/* create.c - create, newpid */
    
#include <conf.h>
#include <i386.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <mem.h>
#include <io.h>
#include <paging.h>

extern unsigned long global_pt_addrs[4];

LOCAL int newpid();

/*------------------------------------------------------------------------
 *  create  -  create a process to start running a procedure
 *------------------------------------------------------------------------
 */
SYSCALL create(procaddr,ssize,priority,name,nargs,args)
	int	*procaddr;		/* procedure address		*/
	int	ssize;			/* stack size in words		*/
	int	priority;		/* process priority > 0		*/
	char	*name;			/* name (for debugging)		*/
	int	nargs;			/* number of args that follow	*/
	long	args;			/* arguments (treated like an	*/
					/* array in the code)		*/
{
	unsigned long	savsp, *pushsp;
	STATWORD 	ps;    
	int		pid;		/* stores new process id	*/
	struct	pentry	*pptr;		/* pointer to proc. table entry */
	int		i;
	unsigned long	*a;		/* points to list of args	*/
	unsigned long	*saddr;		/* stack address		*/
	int		INITRET();

	disable(ps);
	if (ssize < MINSTK)
		ssize = MINSTK;
	ssize = (int) roundew(ssize);
	if (((saddr = (unsigned long *)getstk(ssize)) ==
	    (unsigned long *)SYSERR ) ||
	    (pid=newpid()) == SYSERR || priority < 1 ) {
		restore(ps);
		return(SYSERR);
	}

	numproc++;
	pptr = &proctab[pid];

	pptr->fildes[0] = 0;	/* stdin set to console */
	pptr->fildes[1] = 0;	/* stdout set to console */
	pptr->fildes[2] = 0;	/* stderr set to console */

	for (i=3; i < _NFILE; i++)	/* others set to unused */
		pptr->fildes[i] = FDFREE;

	pptr->pstate = PRSUSP;
	for (i=0 ; i<PNMLEN && (int)(pptr->pname[i]=name[i])!=0 ; i++)
		;
	pptr->pprio = priority;
	pptr->pbase = (long) saddr;
	pptr->pstklen = ssize;
	pptr->psem = 0;
	pptr->phasmsg = FALSE;
	pptr->plimit = pptr->pbase - ssize + sizeof (long);	
	pptr->pirmask[0] = 0;
	pptr->pnxtkin = BADPID;
	pptr->pdevs[0] = pptr->pdevs[1] = pptr->ppagedev = BADDEV;
	
	// Allocate and initialize page directory for the process
	// Map first 16 MB (pages 0-4095) to physical memory using global page tables
	int pd_frame_idx;
	unsigned long phys_addr;
	unsigned long pt_phys_addr;
	unsigned long pt_frame_num;
	pd_t *pd;
	
	if (get_frm(&pd_frame_idx) == SYSERR) {
		restore(ps);
		return(SYSERR);
	}
	
	phys_addr = (FRAME0 + pd_frame_idx) * NBPG;
	pptr->pdbr = phys_addr;
	pd = (pd_t *)phys_addr;
	
	// Initialize all page directory entries to zero
	for (i = 0; i < 1024; i++) {
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
	
	// Map first 16 MB (pages 0-4095) to physical memory using global page tables
	// Pages 0-4095 correspond to page directory indices 0-3 (each covers 1024 pages)
	for (i = 0; i < 4; i++) {
		pt_phys_addr = global_pt_addrs[i];
		pt_frame_num = pt_phys_addr >> 12;
		pd[i].pd_base = (unsigned int)(pt_frame_num);
		pd[i].pd_pres = 1;
		pd[i].pd_write = 1;
		pd[i].pd_user = 0;
		pd[i].pd_pwt = 0;
		pd[i].pd_pcd = 0;
		pd[i].pd_acc = 0;
		pd[i].pd_mbz = 0;
		pd[i].pd_fmb = 0;
		pd[i].pd_global = 0;
		pd[i].pd_avail = 0;
	}
	
	// Update frame table for page directory
	frm_tab[pd_frame_idx].fr_status = FRM_MAPPED;
	frm_tab[pd_frame_idx].fr_pid = pid;
	frm_tab[pd_frame_idx].fr_vpno = 0;
	frm_tab[pd_frame_idx].fr_refcnt = 0;
	frm_tab[pd_frame_idx].fr_type = FR_DIR;
	frm_tab[pd_frame_idx].fr_dirty = 0;
	
	pptr->is_virtual = 0;

		/* Bottom of stack */
	*saddr = MAGIC;
	savsp = (unsigned long)saddr;

	/* push arguments */
	pptr->pargs = nargs;
	a = (unsigned long *)(&args) + (nargs-1); /* last argument	*/
	for ( ; nargs > 0 ; nargs--)	/* machine dependent; copy args	*/
		*--saddr = *a--;	/* onto created process' stack	*/
	*--saddr = (long)INITRET;	/* push on return address	*/

	*--saddr = pptr->paddr = (long)procaddr; /* where we "ret" to	*/
	*--saddr = savsp;		/* fake frame ptr for procaddr	*/
	savsp = (unsigned long) saddr;

/* this must match what ctxsw expects: flags, regs, old SP */
/* emulate 386 "pushal" instruction */
	*--saddr = 0;
	*--saddr = 0;	/* %eax */
	*--saddr = 0;	/* %ecx */
	*--saddr = 0;	/* %edx */
	*--saddr = 0;	/* %ebx */
	*--saddr = 0;	/* %esp; fill in below */
	pushsp = saddr;
	*--saddr = savsp;	/* %ebp */
	*--saddr = 0;		/* %esi */
	*--saddr = 0;		/* %edi */
	*pushsp = pptr->pesp = (unsigned long)saddr;

	restore(ps);

	return(pid);
}

/*------------------------------------------------------------------------
 * newpid  --  obtain a new (free) process id
 *------------------------------------------------------------------------
 */
LOCAL int newpid()
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
