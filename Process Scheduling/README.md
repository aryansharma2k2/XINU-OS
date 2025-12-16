# Process Scheduling

## Tasks

This repository implements two alternative CPU scheduling policies for Xinu designed to prevent process starvation, a limitation of the default priority-based scheduler where lower-priority ready processes may never receive CPU time. Both implemented schedulers ensure fair execution among eligible processes while correctly handling the NULL process, which is scheduled only when no other processes are ready. In Xinu, priorities are represented as integers with the NULL process fixed at priority 0 and all other processes having priorities of at least 1; these values are interpreted differently depending on the scheduling policy. The new schedulers adapt priority usage to guarantee progress for all ready processes, demonstrating practical trade-offs between fairness and responsiveness compared to the default highest-priority-first approach.


1. **Scheduler 1: Exponential Distribution Scheduler (`EXPDISTSCHED`)**

    The first scheduling policy is the exponential distribution scheduler. This scheduler chooses the next process based on a random value that follows the [*exponential distribution*](https://en.wikipedia.org/wiki/Exponential_distribution). When a rescheduling occurs, the scheduler generates a random number with the exponential distribution, and chooses a process with the lowest priority that is greater than the random number. 
    - If the random value is less than the lowest priority in the ready queue, the process with the lowest priority is chosen. 
    - If the random value is greater than or equal to the highest priority in the ready queue, the process with the highest priority is chosen. 
    - When there are processes having the same priority, the scheduler should select only one of these processes to run in a scheduling window. Once this process is run, it should not be selected again until all the other processes at that same priority level have had a chance to run. For example if you have processes A and B with priority 20 and C with priority 10, a valid order might be `A, C, C, B, C, A`, but it should not be `A, C, C, A, C, B` because A runs twice before B has gotten a chance to run.
    
    (**hint**: The moment you insert a process into the ready queue, the queue is ordered and try observing the order in which priorities of the same value are inserted. It may help to experiment with different insertion/removal behaviors with a pencil and paper to observe how interactions will occur.)

    For example, let us assume that the scheduler uses a random value with the exponential distribution of $λ = 0.1$, and there are three processes A, B, and C, whose priorities are 10, 20, and 30, respectively. When rescheduling happens, if a random value is less than 10, process A will be chosen by the scheduler. If the random value is greater than or equal to 10 and less than 20, process B will be chosen. If the random value is greater than or equal to 20, process C will be chosen. The probability that a process is chosen by the scheduler follows the exponential distribution. As shown in the figure below, the ratio of processes A (priority 10), B (priority 20), and C (priority 30) is 0.63 : 0.23 : 0.14.

    ![graph](expdist.jpg)
    
    In order to implement this scheduler, use the `expdev(double lambda)` function in [sys/math.c](sys/math.c), which generates random numbers that follow the exponential distribution. As the value of `lambda`, use 0.1 so it follows the same distribution as the above graph.

1. **Scheduler 2: Linux-like Scheduler (`LINUXSCHED`)**

    This scheduling algorithm loosely emulates the Linux scheduler in the 2.2 kernel. We consider all the processes "conventional processes" and use the policies of the `SCHED_OTHER` scheduling class within the 2.2 kernel. 
    
    To implement this scheduler, we must introduce three new terms in addition to the existing process priority:

    - Epoch: This is a period of time in which all runnable processes are permitted to execute up to their allotted time. Once all runnable processes have finished, Xinu should start a new epoch, thus allowing the processes to run again. A process ceases being runnable when it is no longer on the ready queue, for example if it terminates, waits on a lock, or goes to sleep.
    - Quantum: This is the amount of time a process is permitted to run within an epoch. Typically it will be used all at once but may be broken up, such as if the process sleeps. The quantum maps to clock ticks (timmer interrupts), so a quantum of 10 would correspond to 10 clock ticks.
    - Goodness: This serves as a dynamic priority for a given process within an epoch. It is calculated by `goodness = priority + remainingQuantum` unless the quantum has been completely used for the current epoch. In this case, `goodness = 0` and the process is no longer eligible to run for the current epoch.

    At the beginning of each epoch, the scheduler calculates the quantum for **all** existing processes, including ones that aren't currently runnable. 
    
    - If a process is new or used its entire quantum in the previous epoch, `quantum = priority`. 
    - If a process did not use its entire quantum in the previous epoch, such as a process that went to sleep and later awoke, `quantum = priority + floor(unused/2)`. For example, a counter of 5 and a priority of 10 produce a new quantum value of 12. (**note**: This carryover is approximtely equivalent to the geometric seris 1/2 + 1/4 + 1/8... which approaches 1, so there is no need to worry about unbounded quantum growth.)

    Any processes created in the middle of an epoch have to wait until the following epoch to be scheduled. Any priority changes in the middle of an epoch, e.g., through `chprio()`, only take effect in the next epoch.

    The scheduling process always chooses the eligible process with the highest goodness value and allows it to run for the duration of its quantum unless it yields (**hint**: look into `clkint.S`, also consider how to handle the `NULL` process). Goodness should be updated whenever the scheduler runs so that process that yield have a goodness value reflecting their remaining quantum at the time they yielded. Unlike the expdistched, processes with a higher priority value are considered higher priority. This is the desired behavior.

    Examples of how processes should be scheduled under this scheduler are as follows:
    - If there are processes P1, P2, P3 with priority 10, 20, 15, the initial time quantum for each process would be P1 = 10, P2 = 20, and P3 = 15, so the maximum CPU time for an epoch would be 10 + 20 + 15 = 45. 
    - The possible schedule for two epochs, assuming no processes yield, would be P2, P3, P1 | P2, P3, P1, but not P2, P3, P2, P1 | P3, P1 where | signifies the end of a quantum.
    - If P1 yields with remaining quantum = 5 (e.g., by invoking sleep) in the first epoch, the time quantum for each process in the second epoch could be P1 = 10 + floor(5/2) = 12, P2 = 20, and P3 = 15, thereby the maximum CPU time would be 12 + 20 + 15 = 47.

### Other Implementation Details

1. `void setschedclass(int sched_class)`

    This function should change the scheduling type to either `EXPDISTSCHED` or `LINUXSCHED`. Scheduling policies will only be set prior to running tests, so there is no need to handle policy switching.

2. `int getschedclass()`
    
    This function should return the scheduling class, which should be either `EXPDISTSCHED` or `LINUXSCHED`.

3. Each of the scheduling class should be defined as a constant:

    ```c
    #define EXPDISTSCHED 1
    #define LINUXSCHED 2
    ```

4. Some of the source files of interest are: `create.c`, `resched.c`, `resume.c`, `suspend.c`, `ready.c`, `proc.h`, `kernel.h`, etc.

## Test Cases

The following `testmain.c` can be used to test it.

```c
/* testmain.c */

#include <conf.h>
#include <kernel.h>
#include <q.h>
#include <sched.h>
#include <stdio.h>
#include <math.h>

#define LOOP	50

int prA, prB, prC;
int proc_a(char), proc_b(char), proc_c(char);
int proc(char);
volatile int a_cnt = 0;
volatile int b_cnt = 0;
volatile int c_cnt = 0;
volatile int s = 0;

int main() {
    int i;
    int count = 0;
    char buf[8];

    srand(1234);

    kprintf("Please Input:\n");
    while ((i = read(CONSOLE, buf, sizeof(buf))) < 1)
        ;
    buf[i] = 0;
    s = atoi(buf);
    kprintf("Get %d\n", s);

    // EXPDISTSCHED
    if (s < 2) {
        setschedclass(EXPDISTSCHED);
        prA = create(proc_a, 2000, 10, "proc A", 1, 'A');
        prB = create(proc_b, 2000, 20, "proc B", 1, 'B');
        prC = create(proc_c, 2000, 30, "proc C", 1, 'C');
        resume(prA);
        resume(prB);
        resume(prC);
        sleep(10);
        kill(prA);
        kill(prB);
        kill(prC);

        kprintf("\nTest Result: A = %d, B = %d, C = %d\n", a_cnt, b_cnt, c_cnt);
    }
    // LINUXSCHED
    else {
        setschedclass(LINUXSCHED);
        resume(prA = create(proc, 2000, 5, "proc A", 1, 'A'));
        resume(prB = create(proc, 2000, 50, "proc B", 1, 'B'));
        resume(prC = create(proc, 2000, 90, "proc C", 1, 'C'));

        while (count++ < LOOP) {
            kprintf("M");
            for (i = 0; i < 1000000; i++)
                ;
        }
        kprintf("\n");
    }
    return 0;
}

int proc_a(char c) {
    int i;
    kprintf("Start... %c\n", c);
    b_cnt = 0;
    c_cnt = 0;

    while (1) {
        for (i = 0; i < 10000; i++) {
            // busy wait so process is still active but incrementing doesn't grow excessively
        }
        a_cnt++;
    }
    return 0;
}

int proc_b(char c) {
    int i;
    kprintf("Start... %c\n", c);
    a_cnt = 0;
    c_cnt = 0;

    while (1) {
        for (i = 0; i < 10000; i++) {
            // busy wait so process is still active but incrementing doesn't grow excessively
        }
        b_cnt++;
    }
    return 0;
}

int proc_c(char c) {
    int i;
    kprintf("Start... %c\n", c);
    a_cnt = 0;
    b_cnt = 0;

    while (1) {
        for (i = 0; i < 10000; i++) {
            // busy wait so process is still active but incrementing doesn't grow excessively
        }
        c_cnt++;
    }
    return 0;
}

int proc(char c) {
    int i;
    int count = 0;

    while (count++ < LOOP) {
        kprintf("%c", c);
        for (i = 0; i < 1000000; i++) {
            // busy wait so process is still active but doesn't print excessively
        }
    }
    return 0;
}
```    

#### For Exponential Distribution Scheduler:

Three processes A, B, and C are created with priorities 10, 20, 30. In each process, we keep increasing a variable, so the variable value is proportional to CPU time. Note that the ratio should be close to 0.63 : 0.23 : 0.14, as the three processes are scheduled based on the exponential distribution. You are expected to get similar results as follows:

```
Start... A
Start... B
Start... C

Test Result: A = 982590, B = 370179, C = 206867
```

The counts do not need to match these and it is likely you will see variation based on a given run and your implemenation details. The important part is the ratios of the counts are close to the specified 0.63 : 0.23 : 0.14. Some variation is permissible due to the randomized nature of the algorithm, but this variation will almost certainly be less than +/-0.05. Variation greater than this is likely an implementation issue.

#### For Linux-like Scheduler:

Three processes A, B, and C are created with priorities 5, 50, and 90. In each process, we print a character ('A', 'B', or 'C') at a certain time interval for LOOP times (LOOP = 50). The main process also prints a character ('M') at the same time interval. You are expected to get similar results as follows:

```
MCCCCCCCCCCCCCBBBBBBBMMMACCCCCCCCCCCCCBB
BBBBBMMMACCCCCCCCCCCCBBBBBBBMMMACCCCCCCC
CCCCBBBBBBBMMBBBBBBBMMMABBBBBBBMMMABBBBB
BBMMMBMMAMMMAMMMMMAMMMAMMMAMMMMMMAMMAMMM
MMAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
```

There are two primary things to keep in mind when reviewing the output: 

1. The order of the letters.
2. The approximate ratio of their counts within an epoch. 

In the case above, M prints first because it is the only process active for the first epoch. Once the second epoch starts, C is the highest priority so it runs until its quantum has been used up before any of the other tasks get a chance to run and then does not run again until all other processes have had a chance to run. Within each quantum, the count of letters is approximately 12 (C) : 7 (B) : 3 (M) : 1 (A). This approximates the ratios of each priority where B ~ 0.5C, M ~ 0.5B, A = 0.25M. Toward the end, higher priority processes finish so epochs consist of only the reamining lower priority processes.

Because rescheduling does not account for progress within a process, these counts may vary slightly between epochs—for example, in the above C prints 13 times in the first epoch but only 12 in the second. This variation should be relatively small, typically +/-1 print for a given process. Because A typically only prints once for the given ratios, it is possible you will see epochs where A does not print at all. This should be relativelt rare though; if you are seeing many epochs where A (or any other process) is not printing, it may be a sign of implementation issues.