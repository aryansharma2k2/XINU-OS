XINU is a small Unix-like operating system originally developed by Douglas Comer for instructional purposes at Purdue University. As part of lab assignments, we had to re-implement or improve some aspects of XINU. It runs on a hardware emulator like QEMU.

PA1 - Basic kernel programming practice on XINU OS. Wrote functions to trace all different system calls and log results.

PA2 - Implemented two new scheduling policies, viz., Exponential Distribution Scheduler and Linux-like Scheduler in XINU OS to tackle starvation problem which is prevalent in traditional priority based scheduler policy in XINU.

PA3 - Implemented a memory mapping scheme in XINU with support for demand paging, backing store management and implemented page replacement policies like Second Chance & Aging.

PA4 - Building a disk defragmenter given certain disk images.

Building XINU

To compile the XINU kernel, run `make` in the `compile` directory as follows:

```shell
cd compile
make depend
make
```
This creates an OS image called `xinu.elf`.

The `make depend` directive configures some necessary information for compiling the project with the `make` command. Typically you will only need to run `make depend` the first time you build the project or when modifying the Makefile, such as adding new files.

Running and debugging XINU

    The XINU image runs on the QEMU emulator machines. To boot up the image, type:
    ```shell
    make run
    ```
    XINU should start running and print messages.

    Typing `Ctrl-a` then `c` (not `Ctrl-c`, make sure you release the `Ctrl` key) will always bring you to "(qemu)" prompt. From there, you can quit by typing `q`.

    To debug XINU, run the image in the **debug mode** by:
    ```shell
    make debug
    ```
    Then execute GDB in **another ssh session**:
    ```shell
    gdb xinu.elf
    ```
    In the (gdb) console, connect to the remote server by:
    ```shell
    target remote localhost:1234
    ```
    You can use many debugging features provided by GDB, e.g., adding a break point at the main function:
    ```shell
    b main
    ```
    To run to the next breakpoint, type:
    ```shell
    c
    ```
    The detailed document for GDB can be found [here](https://www.sourceware.org/gdb).
