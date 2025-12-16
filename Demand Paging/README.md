# Demand Paging

## 1. Introduction

Demand paging is a method of mapping a large address space into a relatively
small amount of physical memory. It allows a program to use an address space
that is larger than the physical memory, and access non-contiguous sections of
the physical memory in a contiguous way.
Demand paging is accomplished by using a "backing store" (usually disk) to hold
pages of memory that are not currently in use.


## 2. Goal

The goal of this project is to implement the following system calls and their
supporting infrastructure (See [paging/xm.c](paging/xm.c),
[paging/vcreate.c](paging/vcreate.c), [paging/vgetmem.c](paging/vgetmem.c),
[paging/pr_debug.c](paging/pr_debug.c) and [paging/vfreemem.c](paging/vfreemem.c)).

1. **`SYSCALL xmmap(int virtpage, bsd_t source, int npages)`**
   Much like its Unix counterpart (see `man mmap`), it maps a source file
   ("backing store" here) of size `npages` pages to the virtual page `virtpage`.
   A process may call this multiple times to map data structures, code, etc.
1. **`SYSCALL xmunmap(int virtpage)`**
   This call, like `munmap`, should remove a virtual memory mapping.
   See `man munmap` for the details of the Unix call.
1. **`SYSCALL vcreate(int *procaddr, int ssize, int hsize, int priority, char *name, int nargs, long args)`**
   This call will create a new Xinu process.
   The difference from `create()` (in [create.c](/sys/create.c)) is that the
   process' heap will be private and exist in its virtual memory.
   The size of the heap (in number of pages) is specified by the user through
   `hsize`.
   `create()` should be left (mostly) unmodified.
   Processes created with `create()` should not have a private heap, but should
   still be able to use `xmmap()`.
1. **`WORD *vgetmem(unsigned int nbytes)`**
   Much like `getmem()` (in [getmem.c](/sys/getmem.c)), `vgetmem()` will
   allocate the desired amount of memory if possible.
   The difference is that `vgetmem()` will get the memory from a process'
   private heap located in virtual memory.
   `getmem()` still allocates memory from the regular Xinu kernel heap.
1. **`SYSCALL enable_pr_debug()`**
   This function will be used to turn on debug logging for page replacement
   (discussed later).
1. **`SYSCALL disable_pr_debug()`**
   Equivalent function to `enable_pr_debug` to turn off debug logging for
   page replacement.
1. **`SYSCALL vfreemem(struct mblock *block, unsigned int size_in_bytes)`**
   You will implement a corresponding `vfreemem()` for `vgetmem()` call.
   `vfreemem()` takes two parameters and returns `OK` or `SYSERR`
    The two parameters are similar to those of the original `freemem()` in Xinu
    (see [freemem.c](/sys/freemem.c)).
    The type of the first parameter `block` depends on your own implementation.

## 3. Overall Organization

The following sections discuss at a high level the organization of the system,
the various pieces that need to be implemented in Xinu and how they relate to
each other.

### 3.1 Memory and Backing Store

#### 3.1.1 Backing Stores

Virtual memory commonly uses disk spaces to extend the physical memory.
However, our version of Xinu has no file system support.
Instead, we will emulate disk space by using a section of physical memory
we refer to as the backing store (how it is emulated will be detailed
in [3.1.3](#312-memory-layout-and-backing-store-emulation)).
To access the backing store, you need to implement the following functions in
the directory [paging](/paging):

1. **`bsd_t`**
   is the type of backing store descriptors.
   Each descriptor is used to reference a backing store.
   Its type declaration is in [paging.h](/h/paging.h).
   This type is merely `unsigned int`.
   There are 8 backing stores.
   You will use the IDs 0 through 7 to identify them.
1. **`SYSCALL read_bs(char *dst, bsd_t bs_id, int page)`**
   copies the `page`-th page from the backing store referenced by `bs_id` to
   `dst`.
   It returns `OK` on success, `SYSERR` otherwise.
   The first page of a backing store is page zero.
1. **`SYSCALL write_bs(char *src, bsd_t bs_id, int page)`**
   copies a page referenced by `src` to the `page`-th page of the backing store
   referenced by `store`.
   It returns `OK` on success, `SYSERR` otherwise.

#### 3.1.2 Memory Layout and Backing Store Emulation

Since our version of Xinu does not have file system support, we need to emulate
the backing store with physical memory. In particular, consider the following 
Xinu memory layout:

```
--------------------------------------------
Virtual Memory       (pages 4096 and beyond)
--------------------------------------------
8 Backing Stores     (pages 2048 - 4095)
--------------------------------------------
1024 Frames          (pages 1024 - 2047)
--------------------------------------------
Physical Heap        (pages 406 - 1023)
--------------------------------------------
Hole                 (pages 160 - 405)
--------------------------------------------
Kernel Memory        (pages 25 - 159)
--------------------------------------------
Xinu text, data, bss (pages 0 - 24)
--------------------------------------------
```

A Xinu instance has 16 MB (4096 pages) of real memory in total. Divided as above.  
The first 25 pages (0-24) are the Xinu executable itself. The next is unspecified kernel memory
that you can leave as is. Pages 160-405 are the hole (see [initialize.c](sys/initialize.c)) and
you can leave these as is as well. After this is the sections you may need to update for this
assignment.

Pages 406-1023 are to be used as virtual heap space, the memory Xinu allocates when `getmem` is
called that can be used by processes created with `create`.

Pages 1024-2047 are referred to as frames. This is the physical memory that will contain page
directories, page tables, and back virtual memory.

Pages 2048-4095 are reserved for the backing stores.  We divide this into 8 distinct backing 
stores of 256 pages (each page is 4K size). This gives the backing stores the following 
physical memory ranges:

```
backing store 0: 0x00800000 - 0x008fffff
backing store 1: 0x00900000 - 0x009fffff
backing store 2: 0x00a00000 - 0x00afffff
backing store 3: 0x00b00000 - 0x00bfffff
backing store 4: 0x00c00000 - 0x00cfffff
backing store 5: 0x00d00000 - 0x00dfffff
backing store 6: 0x00e00000 - 0x00efffff
backing store 7: 0x00f00000 - 0x00ffffff
```

These 8 backing stores will serve a purpose similar to 
[swap space on disk](https://en.wikipedia.org/wiki/Memory_paging#Unix_and_Unix-like_systems) 
in a traditional operating system. While they are accessible with their memory addresses as any 
other physical memory in Xinu should be, we ask that you not interact with
them directly. Instead you will access them when through the functions discussed in
[3.1.1](#311-backing-stores) for the purposes of paging eviction and `xmmap` persistence.

Because this area is reserved for the backing stores, which emulate disk space, you need to
ensure that other processes do not use this memory (take a close look at 
[sys/i386.c](sys/i386.c), and pay attention to the variables `npages` and `maxaddr`).

#### 3.1.3 Page Tables and Page Directories

This project involves constructing a multi-level page table, consisting of two levels.
The first level is page directories which point to the corresponding page table. As 
discussed in class, page tables contain the final mapping to the frame (physical memory) 
that backs the virtual memory as well as metadata about the status of the virtual page 
and associated frame.

Both page tables and page directories can be placed in any free frame. Since each
process needs to have a starting point, you should preallocate page directories as soon
as a virtual process is created using `vcreate`.

However, there are a larger number of potential page tables than Xinu's physical memory 
can hold, so you will need to dynamically allocate and deallocate page tables as necessary. 
That is, the first time a page is legally touched (i.e. it has been mapped by the process) 
for which no page table is present, a page table should be allocated. Conversely, when a 
page table is no longer needed it should be removed to conserve space.

### 3.2 Supporting Data Structures

#### 3.2.1 Finding the backing store for a virtual address

To support page replacement, you will need to reserve a backing store for each
process that has a virtual heap. This means that you can only have a maximum of
8 virtual processes active at one time, and you can safely assume that grading
tests will not violate this.

In addition to reserving a backing store, you will need to track how virtual pages
and the reserved backing store correspond to each other to ensure the correct page
is swapped in and out as necessary. This is simply a direct mapping from the 0th
virtual page to the 0th page of the backing store. For example, page 4096 is the 0th
page of virtual memory and the process has reserved backing store 1, starting at 
page 2304. Should page 4096 need to be swapped out, the correesponding frame's data
would be written to paggeg 2304. Similarly, should you need to load page 4100, you
can determine that it is the 4th virtual page and would subsequently correspond to
backing store page 2308 as that is the 4th page of backing store 1.

To track these mappings, you may need to declare a new kernel data structure which 
maps virtual address spaces to backing store descriptors. We will call this 
*the backing store map*. It should be a tuple like:

```
{ pid, vpage, npages, store }
```

You should write a function that performs the lookup:

```
f(pid, vaddr) -> {store, pageoffset within store}
```

The function `xmmap()` will add a mapping to this table.
`xmunmap()` will remove a mapping from this table.

#### 3.2.2 Inverted Page Table

When writing out a dirty page you may notice the only way to figure out which
virtual page and process (and thus which backing store) a dirty frame belongs to
would be to traverse the page tables of every process looking for a frame
location that corresponds to the frame we wish to write out.
This is highly inefficient.
To prevent this, we use another kernel data structure, an *inverted page table* also
referred to in code and discussion as the *frame table*.
This table contains tuples like:

```
{ frame number, pid, virtual page number }
```

This structure is already defined as the frm_tab (see [paging.h](h/paging.h)) and should be
of size `NFRAMES` so that the the frame number is implicitly stored as the array index for 
that entry.
With this structure, we can easily find the pid and virtual page number of the
page held within any frame `i`.
From that we can easily find the backing store (using the backing store map) and
compute which page within the backing store corresponds with the page in frame
`i`.

You may also want to use this table to hold other information for page
replacement (i.e., any data needed to estimate page replacement policy).

### 3.3 Process Considerations

With each process having its own page directory and page tables, there are some
new considerations in dealing with processes.

#### 3.3.1 Process Creation

When a process is created we must also create a page directory and record its
address. This page directory will be resident in memory for the entire lifetime
of the process and can be cleaned up once the process terminates. The first 16 megabytes 
of each process's memory will be mapped to the 16 megabytes of Xinu's physical memory, 
so we must initialize the process' page directory accordingly.
This is important as our backing stores also depend on this correct mapping.

A mapping must be created for the new process's private heap (and optionally stack), 
if created with `vcreate()`. As discussed, the virtual heap will map to the backing store.
Should you choose to implement the optional private statck, you may want to use the same
backing store for both the heap and the stack (as with the kernel heap), `vgetmem()` taking from
one end and the stack growing from the other.

Implementing the virtual stack is entirely optional, but you **must** implement the virtual
heap.

#### 3.3.2 Process Destruction

When a process dies, the following should happen:

* All frames which currently hold any of its pages should be written to the
  backing store and be freed.
* All of its mappings should be removed from the backing store map.
* The backing stores for its heap (and stack if have chosen to implement a
  private stack) should be released (remember backing stores allocated to a
  process should persist unless the process explicitly releases them).
* The frame used for the page directory should be released.

#### 3.3.3 Context Switching

It should also be clear that as we switch between processes we must also switch
between memory spaces.
This is accomplished by adjusting the PDBR register with every context switch.
We must be careful, however, as this register must always point to a valid page
directory which is in RAM at a page boundary.

Think carefully about where you place this switch if you put it in `resched()` -
before or after the actual context switch.

#### 3.3.4 System Initialization

The NULL process is somewhat of a special case, as it builds itself in the
function `sysinit()`.
The NULL process should not have a private heap (like any processes created with
`create()`).

The following should occur at system initialization:

* Set the DS and SS segments' limits to their highest values.
  This will allow processes to use memory up to the 4 GB limit without
  generating general protection faults. You should verify these are correct,
  but will not necessarily have to make changes to the code.
* Make sure the initial stack pointer (`initsp`) is set to a real physical
  address (the highest physical address) as it is in normal Xinu.
  See [i386.c](sys/i386.c). And don't forget to reserve physical memory 
  frames 2048 - 4096 for backing store purposes.
* Initialize all necessary data structures.
* Create the page tables which will map pages 0 through 4095 to the physical
  16 MB. These will be called the global page tables.
* Allocate and initialize a page directory for the NULL process.
* Set the PDBR register to the page directory for the NULL process.
* Install the page fault interrupt service routine.
* Enable paging.

### 3.4 The Interrupt Service Routine (ISR)

As you know, a page fault triggers an interrupt 14. This is handled by [pfint.S](paging/pfintr.S).
When an interrupt occurs the machine pushes `CS:IP` and then an error code (see Intel Volume III chapter 5):

```
----------
error code
----------
    IP
----------
    CS
----------
    ...
    ...
```

It then jumps to a predetermined point, the ISR.
To specify the ISR we use the following routine (see [evec.c](/sys/evec.c)):

```c
set_evec(int interrupt, (void (*isr)(void))) 
```

While the interrupt is handled directly by assembly code, it would be very difficult
to implement page replacement entirely in assembly, so the preexisting code calls out
to a C function defined in [pfint.c](paging/pfint.c). You may write your page fault
handling code here.

### 3.5 Faults and Replacement Policy

#### 3.5.1 Page Faults

A page fault indicates one of two things: the virtual page on which the faulted
address exists is not present or the page table which contains the entry for the
page on which the faulted address exists is not present.
To deal with a page fault you must do the following:

* Get the faulted address `a`.
* Let `vp` be the virtual page number of the page containing the faulted
  address.
* Let `pd` point to the current page directory.
* Check that `a` is a legal address (i.e. that it has been mapped in `pd`).
  If it is not, print an error message and kill the process.
* Let `p` be the upper ten bits of `a`. [What does `p` represent? Refer to the class notes on multi-level page tables.]
* Let `q` be the bits [21:12] of `a`. [What does `q` represent? Refer to the class notes on multi-level page tables.]
* Let `pt` point to the `p`-th page table.
  If the `p`-th page table does not exist, obtain a frame for it and initialize
  it.
* To page in the faulted page do the following:
  * Using the backing store map, find the store `s` and page offset `o` which
    correspond to `vp`.
  * In the inverted page table, increase the reference count of the frame that
    holds `pt`.
    This indicates that one more of `pt`'s entries is marked as "present."
  * Obtain a free frame, `f`.
  * Copy the page `o` of store `s` to `f`.
  * Update `pt` to mark the appropriate entry as present and set any other
    fields.
    Also set the address portion within the entry to point to frame `f`.

#### 3.5.2 Obtaining a Free Frame

When a free frame is needed, it may be necessary to remove a resident page from
frame. You will use the second chance page replacement policy to make this
determination, see [3.5.3](#353-page-replacement-policy---second-chance-sc).

Your function to find a free page should do the following:

* Search inverted page table (frm_tab) for an empty frame. If one exists, stop.
* Else, pick a page to replace.
* Using the inverted page table, get `vp`, the virtual page number of the page
  to be replaced.
* Let `a` be `vp*4096` (the first virtual address on page `vp`).
* Let `p` be the high 10 bits of `a`.
  Let `q` be bits [21:12] of `a`.
* Let `pid` be the pid of the process owning `vp`.
* Let `pd` point to the page directory of process `pid`.
* Let `pt` point to the `pid`'s `p`-th page table.
* Mark the appropriate entry of `pt` as not present.
* If the page being removed belongs to the current process, invalidate the TLB
  entry for the page `vp` using the `invlpg` instruction (see Intel Volume
  III/II).
* In the inverted page table, decrement the reference count of the frame
  occupied by `pt`.
  If the reference count has reached zero, you should mark the appropriate entry
  in `pd` as being not present.
  This conserves frames by keeping only page tables which are necessary.
* If the dirty bit for page `vp` was set in its page table you must do the
  following:
  * Use the backing store map to find the store and page offset within store
    given `pid` and `a`.
    If the lookup fails, something is wrong.
    Print an error message and kill the process pid.
  * Write the page back to the backing store.

#### 3.5.3 Page Replacement Policy - Second-Chance (SC)

You must implement the following replacement algorithm called Second-Chance
(`SC`).
When a frame is allocated for a page, you insert the frame into a circular
queue.
When a page replacement occurs, `SC` first looks at the current position in the
queue (current position starts from the head of the queue) and checks to see
whether its reference bit is set (i.e., `pt_acc = 1`).
If it is not set, the page is swapped out.
Otherwise, the reference bit is cleared, the current position moves to the next
page and this process is repeated.
If all the pages have their reference bits set, on the second encounter, the
page will be swapped out, as it now has its reference bit cleared.

When `enable_pr_debug()` is called in `main()`, your program should turn on a
debugging option, so that when replacements occur, it will print ONLY the
replaced frame numbers (not addresses) for grading. Preferably print the frame
number, i.e., page 1024 = frame 0 -> prints frame 0, but either is acceptable
as long as they accurately represent the physical page being removed.

Note that you are free to add whatever structures you'd like in your inverted
page table.

## 4. Required API Calls

You must implement the system calls listed at the beginning of this handout
exactly as specified.
Be sure to check the parameters for validity.
For example, no process should be allowed to remap the lowest 16 MB of the
system (global memory).

Also, none of Xinu's other system call interfaces should be modified.

## 5. Details on the Intel Architecture and Xinu

The following might be useful for you to know:

* We are using the Intel Pentium chip, not the Pentium Pro or Pentium II.
  Some details of those chips do not apply.
* Xinu uses the flat memory model, i.e. physical address = linear addresses.
* The segments are set in [i386.c](/sys/i386.c) in the function `setsegs()`.
* Pages are 4k (4096 bytes) in size.
  Do not use 2M or 4M page size.
* The backend machines have 16 MB (4096 pages) of real memory.
* Some example code is given in the project directory for getting and setting
  the control registers.
  A useful function, `dump32(unsigned long)`, for dumping a binary number with
  labeled bits is also given (in [paging/dump32.c](/paging/dump32.c)).

## 6. Test File and Sample Output

### 6.1 `testmain.c`

```c
#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <stdio.h>
#include <paging.h>

#define PROC1_VADDR 0x40000000
#define PROC1_VPNO  0x40000
#define PROC2_VADDR 0x80000000
#define PROC2_VPNO  0x80000
#define TEST1_BS    1

void proc1_test1(char *msg, int lck) {
  char *addr;
  int i;

  if (xmmap(PROC1_VPNO, TEST1_BS, 100) == SYSERR) {
    kprintf("xmmap call failed\n");
    sleep(3);
    return;
  }

  addr = (char *) PROC1_VADDR;
  for (i = 0; i < 26; ++i) {
    *(addr + (i * NBPG)) = 'A' + i;
  }

  sleep(6);

  for (i = 0; i < 26; ++i) {
    kprintf("0x%08x: %c\n", addr + (i * NBPG), *(addr + (i * NBPG)));
  }

  xmunmap(PROC1_VPNO);
  return;
}

void proc1_test2(char *msg, int lck) {
  int *x;

  kprintf("ready to allocate heap space\n");
  x = vgetmem(1024);
  kprintf("heap allocated at %x\n", x);
  *x = 100;
  *(x + 1) = 200;

  kprintf("heap variable: %d %d\n", *x, *(x + 1));
  vfreemem(x, 1024);
}

int main() {
  int pid1;
  int pid2;

  kprintf("\n1: shared memory\n");
  pid1 = create(proc1_test1, 2000, 20, "proc1_test1", 0, NULL);
  resume(pid1);
  sleep(10);

  kprintf("\n2: vgetmem/vfreemem\n");
  pid1 = vcreate(proc1_test2, 2000, 100, 20, "proc1_test2", 0, NULL);
  kprintf("pid %d has private heap\n", pid1);
  resume(pid1);
  sleep(3);

  kprintf("\n3: Frame test\n");
  pid1 = create(proc1_test3, 2000, 20, "proc1_test3", 0, NULL);
  resume(pid1);
  sleep(3);
}
```

### 6.2 Sample output

```
testmain.c sample output:
1: shared memory
0x40000000: A
0x40001000: B
0x40002000: C
0x40003000: D
0x40004000: E
0x40005000: F
0x40006000: G
0x40007000: H
0x40008000: I
0x40009000: J
0x4000a000: K
0x4000b000: L
0x4000c000: M
0x4000d000: N
0x4000e000: O
0x4000f000: P
0x40010000: Q
0x40011000: R
0x40012000: S
0x40013000: T
0x40014000: U
0x40015000: V
0x40016000: W
0x40017000: X
0x40018000: Y
0x40019000: Z

2: vgetmem/vfreemem
pid 47 has private heap
ready to allocate heap space
heap allocated at 1000000
heap variable: 100 200
```