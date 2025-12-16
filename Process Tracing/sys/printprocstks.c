#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <stdio.h>
#include <lab0.h>

static inline unsigned long read_esp(void) {
    unsigned long v;
    __asm__ __volatile__("movl %%esp, %0" : "=r"(v));
    return v;
}

void printprocstks(int priority) {
    for(int pid=0; pid<NPROC; pid++) {
        struct pentry *proc = &proctab[pid];
        if (proc->pprio > priority) {
            unsigned long sp;
            if (pid == currpid) {
                sp = read_esp();
            } 
            else {
                sp = (unsigned long)proc->pesp;
            }
            kprintf("Process [%s]\n", proc->pname);
            kprintf("\tpid: %d\n", pid);
            kprintf("\tpriority: %d\n", proc->pprio);
            kprintf("\tbase: 0x%08x\n", (unsigned)proc->pbase);
            kprintf("\tlimit: 0x%08x\n", (unsigned)proc->plimit);
            kprintf("\tlen: %u\n", (unsigned)proc->pstklen);
            kprintf("\tpointer: 0x%08x\n", (unsigned)sp);
        }
    }

}