/* user.c - main */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <stdio.h>
#include <paging.h>

/*------------------------------------------------------------------------
 *  main_test6  --  user main program for test 6
 *------------------------------------------------------------------------
 */
#define TPASSED 1
#define TFAILED 0

void proc1_test6() {
		int PAGE0 = 0x40000;
		int i,j,temp;
		int addrs[1200];
		int cnt = 0;
		//can go up to  (NFRAMES - 5 frames for null prc - 1pd for main - 1pd + 1pt frames for this proc)
		//frame for pages will be from 1032-2047
		int maxpage = (NFRAMES - (5 + 1 + 1 + 1));

		for (i=0;i<=maxpage/150;i++){
            if (xmmap(PAGE0+i*150, i, 150) == SYSERR) {
                kprintf("xmmap call failed\n");
                return;
            }
            for(j=0;j < 150;j++)
            {
                //store the virtual addresses
                addrs[cnt++] = (PAGE0+(i*150) + j) << 12;
            }
		}

		/* all of these should generate page fault, no page replacement yet
		   acquire all free frames, starting from 1032 to 2047, lower frames are acquired first
		   */
		for(i=0; i < maxpage; i++)
		{
            *((int *)addrs[i]) = i + 1;
		}

		//trigger page replacement, this should clear all access bits of all pages
		//expected output: frame 1032 will be swapped out
		kprintf("\n\t 6.1 Expected replaced frame: 8, page: 1032\n\t");
		*((int *)addrs[maxpage]) = maxpage + 1;

		for(i=1; i <= maxpage; i++)
		{
            if ((i != 600) && (i != 800)) { 
                //reset access bits of all pages except these
                *((int *)addrs[i])= i+1;
            }
		}
		//Expected page to be swapped: 1032+600 = 1632
		kprintf("\n\t 6.2 Expected replaced frame: 608, page: 1632\n\t");
		*((int *)addrs[maxpage+1]) = maxpage + 2;
		temp = *((int *)addrs[maxpage+1]);
		if (temp != maxpage +2)
			kprintf("\tFAILED!\n");

		kprintf("\n\t 6.3 Expected replaced frame: 808, page: 1832\n\t");
		*((int *)addrs[maxpage+2]) = maxpage + 3;
		temp = *((int *)addrs[maxpage+2]);
		if (temp != maxpage +3)
			kprintf("\tFAILED!\n");


		for (i=0;i<=maxpage/150;i++){
            xmunmap(PAGE0+(i*150));
		}

}
void test6(){
	int pid1;

	kprintf("\nTest 6: Test SC page replacement policy\n");
	enable_pr_debug();
	pid1 = create(proc1_test6, 2000, 20, "proc1_test6", 0, NULL);

	resume(pid1);
	sleep(10);
	kill(pid1);

	kprintf("\n\t Finished! Check error and replaced frames\n");
}

int main() {
    test6();
    shutdown();
}