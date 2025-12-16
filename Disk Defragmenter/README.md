# A Defragmenter

## 1. Introduction

The old Unix file system can get decreasing performance because of fragmentation. File system defragmenters improve the performance by laying out all blocks of a file in a sequential order on disk, and we are going to implement one in this.

### Disk structure ###

You will be given a disk image containing a file system, and it will be correct, i.e., no corruption. Disk images are divided into several sections as below. We will discuss how to differentiate the sections shortly.

```
--------------------------------------------
Boot Block                    512 bytes
--------------------------------------------
Super Block                   512 bytes
--------------------------------------------
Inode Region                  (variable)
--------------------------------------------
Data Region                   (variable)
--------------------------------------------
Swap Space                    (variable)
--------------------------------------------
```

#### Boot Block ####

The contents of the boot block aren't relevant to the assignment. Just remember that it occupies the first 512 bytes.

#### Super Block ####

In your code, the super block will be represented by this struct:
```c
  struct superblock {
    int blocksize; /* size of blocks in bytes */
    int inode_offset; /* offset of inode region in blocks */
    int data_offset; /* data region offset in blocks */
    int swap_offset; /* swap region offset in blocks */
    int free_inode; /* head of free inode list */
    int free_block; /* head of free block list */
  }
```

While the super block region itself is 512 bytes, the struct only occupies a small portion of it—this should be 24 bytes total, 6 `int` fields at 4 bytes each. The rest is unused. The `superblock` struct itself defines most of the relevant information for the disk image that you will need when defragmenting. Each of the offset fields gives the starting point of that region in terms of blocks from the end of the super block region. Each region ends immediately before the start of the next region, except the swap region which runs to the end of the disk. For example, give the following values

- blocksize: 512
- inode_offset: 0 
- data_offset: 10
- swap_offset: 30
- disk size: 17920 bytes

The inode region would cover bytes 1024-6143 in the disk image, the data region bytes 6144-16383, and the swap region bytes 16384-17920.

The `free_inode` field is the head of a linked list of currently unused inodes. These are in terms of the `inode` struct (see next section) and relative to the start of the inode region. Thus, a value of 0 would mean that the first inode in the inode region is currently free. See the next section for additional details on how this list is structured.

The `free_block` field is the head of a linked list of currently unused data blocks. This is in terms of `block_size`, but unlike the offset fields, it is relative to the start of the data region. For example, were it to have a value of 0, it would mean bytes 6144-6655. The first four bytes of a free block contain an integer index to the next free block; the last free block contains -1 for the index. The head of the free block list is also a index into the data block region.

#### Inode Region ####

The inode region is effectively a large array of inodes, and the inode structure is given as follows:
```c
  #define N_DBLOCKS 10 
  #define N_IBLOCKS 4  
  
  struct inode {  
      int next_inode; /* list for free inodes */  
      int protect;        /*  protection field */ 
      int nlink;  /* Number of links to this file */ 
      int size;  /* Number of bytes in file */   
      int uid;   /* Owner's user ID */  
      int gid;   /* Owner's group ID */  
      int ctime;  /* Time field */  
      int mtime;  /* Time field */  
      int atime;  /* Time field */  
      int dblocks[N_DBLOCKS];   /* Pointers to data blocks */  
      int iblocks[N_IBLOCKS];   /* Pointers to indirect blocks */  
      int i2block;     /* Pointer to doubly indirect block */  
      int i3block;     /* Pointer to triply indirect block */  
   };
```
An unused inode has zero in the `nlink` field and the `next_inode` field contains the index of the next free inode, and the last free inode contains -1 for the index. For inodes in use, the `next_inode` field is not used. The inodes in the inode region are all contiguous. Independent of block boundaries, the whole region is viewed as one big array. So it is possible for there to be an inode that overlaps two blocks. It is also possible for the inode region to not be an even multiple of the struct size. In this case, there may be some unused space where an inode cannot fit before the start of the data region.

The size field of the inode is used to determine which data block pointers are valid. If the file is small enough to be fit in `N_DBLOCKS` blocks, the indirect blocks are not used. Note that there may be more than one indirect block. However, there is only one pointer to a double indirect block and one pointer to a triple indirect block. All block pointers are indexes relative to the start of the data block region.

#### Data Region ####

The data region is where file data is stored. This should be treated simply as an array of blocks. As mentioned, free blocks will contain the index of the next free block in the linked list. In-use blocks will contain whatever relevant file data.

#### Swap Space ####

This region is also largely unused for the assignment and its specific contents aren't relevant.

## 2. Goal

You should read in the disk image, find inodes containing valid files, and write out a new image containing the same set of files, with the same inode numbers, but **with all the blocks in a file laid out contiguously**. Thus, if a file originally contains blocks `{6,2,15,22,84,7}` and it is relocated to the beginning of the data section, then the new blocks would be `{0,1,2,3,4,5}`. If there are indirect pointer blocks, the block containing the pointers need to be before the blocks being pointed to, and this applies to all level of indirect pointers. For example, a doubly indirect block should come before the first indirect block it points to which would be immediately before the first data block it points to.

After defragmenting, your new disk image should contain the same boot block and swap space (just copy both), a new superblock with the same list of free inodes but a new list of free blocks sorted from lowest to highest (to prevent future fragmentation), new inodes for valid files, and data blocks at their new locations.

You do not need to copy/save the contents of free blocks when you create the defragmented disk. Instead, set the first four bytes to point to the next free block and all subsequent bytes to 0s.

Your program should take one parameter, i.e., the name of the disk image (e.g., `images_frag/disk_frag_1`). The output disk image should be named as "`disk_defrag`". During grading, we will be testing your compiled executable in an isolated environment so ensure that your code does not depend on anything other than the input argument.

You will need to do binary file I/O to read in the data files. You can do this with the `fread()` C library function. Here is some sample code:
```c
   FILE * f;
   unsigned char * buffer;
   size_t bytes;
   buffer = malloc(10*1024*1024);
   f = fopen("disk_frag","r");
   bytes =  fread(buffer,10*1024*1024,1,f);
```
Please do not use this directly, as it does no error checking. If you want to find out how big a file is from within your program, you can use the `fstat()` function. You can change the type of the buffer if you see a fit.

To help you understand how to do cast in C and read the disk image, we provide the following code to show how to read the superblock:
```c
   superblock *pSB = (superblock*) &(buffer[512]);
   ... pSB->blocksize ...;
   ... pSB->inode_offset ...;
```
You can also use the following code:
```c
   superblock sb;
   sb.blocksize = *((int *) &(buffer[512]));
   sb.inode_offset = *((int *) &(buffer[512 + 4]));
```
Try to make sense of the code above, from which you should get an idea on how to read the disk image. Note the code above assuming the buffer is of the `char *` or `unsigned char *` type. If you choose to declare your buffer to something else, e.g., `int *`, you need to adjust the code above.

To avoid potential compiler-induced problems in the code above, please add "`-O0`" into your compiler flags. Also please check (1) the address buffer returned by `malloc` can be divided by four and (2) `sizeof(struct superblock)` is 24 and `sizeof(struct inode)` is 100. If a violation is found, please let the instructor/TA know.

A different and safer approach of reading a word from an arbitrary address is as following:
```c
   superblock sb;
   int readIntAt(unsigned char *p)
   {
      return *(p+3) * 256 * 256 * 256 + *(p+2) * 256 * 256 + *(p+1) * 256 + *p;
   }
   sb.blocksize = readIntAt(buffer + 512);
   sb.inode_offset = readIntAt(buffer + 512 + 4);
```

## 3. Disk images 
You can find two directories: `images_frag` and `images_defrag`. The `images_frag` directory contains test input images: `disk_frag_1`, `disk_frag_2`, and `disk_frag_3`. The expected outputs are in the `images_defrag` directory. That is, for an input image `disk_frag_x`, your output should be identical to `disk_defrag_x`. You can use `diff` command. You can also use the scripts in the `diff_scripts` directory to check your match percentages. It contains two versions, one for Python which should work with either Python 2 or 3, and one for the Deno JavaScript runtime. These scripts parallel the comparison we'll be using during grading and should match each other exactly (modulo formatting differences); let the instructors know if they don't.

The input images follow the following rules:
  * All unused inodes have -1 for the last four index-related pointer fields and 0 for other fields after next_inode. For inodes in use, index-related pointer fields that are not used should have values -1.
  * A data block used as an indirect pointer block will have -1 for all the unused pointers beyond file size.
  * A free data block will contain only 0 after the first four bytes for pointing to the next free data block. A data block in use will contain only 0 beyond the file size.
  * Unused bytes in the super block and the inode region are 0. For the inode region, if the total size is 1024 bytes, then the last 24 bytes are unused bytes, as each inode is 100 bytes and the inode region can hold 10 inodes

## 4. Sample `defrag.c`
The code below is just for an example purpose. This code assumes that a separate set of files, including `disk_image.h` and `.c`, implements the actual functionalities and defines the data structures. You do not have to follow this structure. In fact, you can implement everything in a single c file. 
```c
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "disk_image.h"

#define OUTPUT_FILE_NAME "disk_defrag"

/* Disk image created from input file */
diskImage di;
/* Pointer to defragmented disk image */
diskImage *defragged = NULL;

int main( int argc, char *argv[ ] )
{
    //Open the input file
    FILE *in;
    char *fileName = argv[ 1 ];
    in = fopen( fileName, "r" );

    //Check for errors opening the input file
    if ( !in )
    {
        //handle error: could not open file        
    }

    //Load a disk image from the input file
    readDiskImage( in, &di );
    fclose( in );

    //Defrag the disk image
    defragged = defragDiskImage( &di );

    //Create an output file
    FILE *out = fopen( OUTPUT_FILE_NAME, "w+" );

    //Check for errors creating the output file
    if ( !out )
    {
        //handle error: could not create file        
    }

    //Write defragged to an output file
    writeDiskImage( out, defragged );
    fclose( out );

    //Cleanup
    freeDiskImage( &di );
    freeDiskImage( defragged );
    free( defragged );
}

```