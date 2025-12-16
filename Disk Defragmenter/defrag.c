#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N_DBLOCKS 10
#define N_IBLOCKS 4

typedef struct superblock {
    int blocksize;
    int inode_offset;
    int data_offset;
    int swap_offset;
    int free_inode;
    int free_block;
} superblock;

typedef struct inode {
    int next_inode;
    int protect;
    int nlink;
    int size;
    int uid;
    int gid;
    int ctime;
    int mtime;
    int atime;
    int dblocks[N_DBLOCKS];
    int iblocks[N_IBLOCKS];
    int i2block;
    int i3block;
} inode;

static void die(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
}

// read 4-byte little-endian int from bytes
static int readIntLE(const unsigned char *p) {
    unsigned int b0 = p[0];
    unsigned int b1 = p[1];
    unsigned int b2 = p[2];
    unsigned int b3 = p[3];
    unsigned int x = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    return (int)x;
}

// write 4-byte little-endian int to bytes
static void writeIntLE(unsigned char *p, int v) {
    unsigned int x = (unsigned int)v;
    p[0] = (unsigned char)(x & 0xFF);
    p[1] = (unsigned char)((x >> 8) & 0xFF);
    p[2] = (unsigned char)((x >> 16) & 0xFF);
    p[3] = (unsigned char)((x >> 24) & 0xFF);
}

// copy one data block from old index to new index
static void copy_data_block(
    const unsigned char *inbuf,
    unsigned char *outbuf,
    size_t data_region_start,
    size_t blocksize,
    int old_idx,
    int new_idx
) {
    unsigned char *dst = outbuf + data_region_start + (size_t)new_idx * blocksize;
    const unsigned char *src = inbuf + data_region_start + (size_t)old_idx * blocksize;
    memcpy(dst, src, blocksize);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <disk_image>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *input_name = argv[1];

    FILE *in = fopen(input_name, "rb");
    if (!in) {
        perror("fopen input");
        return EXIT_FAILURE;
    }

    // get file size using fseek/ftell
    if (fseek(in, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(in);
        return EXIT_FAILURE;
    }
    long fsize = ftell(in);
    if (fsize < 0) {
        perror("ftell");
        fclose(in);
        return EXIT_FAILURE;
    }
    rewind(in);

    size_t disk_size = (size_t)fsize;

    unsigned char *inbuf = (unsigned char *)malloc(disk_size);
    if (!inbuf) {
        fclose(in);
        die("malloc inbuf failed");
    }

    size_t bytes_read = fread(inbuf, 1, disk_size, in);
    fclose(in);
    if (bytes_read != disk_size) {
        free(inbuf);
        die("fread did not read full file");
    }

    // assignment says these should be true, so we keep these checks
    if (sizeof(superblock) != 24) {
        fprintf(stderr, "superblock size is %zu, expected 24\n", sizeof(superblock));
        free(inbuf);
        return EXIT_FAILURE;
    }
    if (sizeof(inode) != 100) {
        fprintf(stderr, "inode size is %zu, expected 100\n", sizeof(inode));
        free(inbuf);
        return EXIT_FAILURE;
    }

    unsigned char *outbuf = (unsigned char *)malloc(disk_size);
    if (!outbuf) {
        free(inbuf);
        die("malloc outbuf failed");
    }
    memcpy(outbuf, inbuf, disk_size);

    // superblock is at offset 512
    unsigned char *sb_bytes = inbuf + 512;
    superblock sb;
    sb.blocksize = readIntLE(sb_bytes + 0);
    sb.inode_offset = readIntLE(sb_bytes + 4);
    sb.data_offset = readIntLE(sb_bytes + 8);
    sb.swap_offset = readIntLE(sb_bytes + 12);
    sb.free_inode = readIntLE(sb_bytes + 16);
    sb.free_block = readIntLE(sb_bytes + 20);

    size_t blocksize = (size_t)sb.blocksize;
    if (blocksize == 0) {
        free(inbuf);
        free(outbuf);
        die("blocksize is 0 in superblock");
    }

    const size_t BOOT_SIZE = 512;
    const size_t SUPER_SIZE = 512;
    const size_t SUPER_END = BOOT_SIZE + SUPER_SIZE;

    size_t inode_region_start = SUPER_END + (size_t)sb.inode_offset * blocksize;
    size_t data_region_start = SUPER_END + (size_t)sb.data_offset * blocksize;
    size_t swap_region_start = SUPER_END + (size_t)sb.swap_offset * blocksize;

    size_t inode_region_size = data_region_start - inode_region_start;
    size_t data_region_size = swap_region_start - data_region_start;
    size_t swap_region_size = disk_size - swap_region_start;

    size_t n_inodes = inode_region_size / sizeof(inode);
    size_t inode_rem = inode_region_size % sizeof(inode);
    size_t n_data_blocks = data_region_size / blocksize;

    fprintf(stderr, "Disk size: %zu bytes\n", disk_size);
    fprintf(stderr, "Block size: %zu bytes\n", blocksize);
    fprintf(stderr, "Inodes: %zu (remainder %zu bytes)\n", n_inodes, inode_rem);
    fprintf(stderr, "Data blocks: %zu\n", n_data_blocks);
    fprintf(stderr, "Swap size: %zu bytes\n", swap_region_size);

    int *file_blocks = (int *)malloc(n_data_blocks * sizeof(int));
    if (!file_blocks) {
        free(inbuf);
        free(outbuf);
        die("malloc file_blocks failed");
    }

    int next_new_block = 0;

    for (size_t i = 0; i < n_inodes; i++) {
        size_t inode_offset = inode_region_start + i * sizeof(inode);
        inode old_ino;
        memcpy(&old_ino, inbuf + inode_offset, sizeof(inode));

        if (old_ino.nlink == 0) {
            // free inode: copy as is
            memcpy(outbuf + inode_offset, &old_ino, sizeof(inode));
            continue;
        }

        inode new_ino = old_ino;

        if (old_ino.size <= 0) {
            // empty file: clear pointers
            for (int j = 0; j < N_DBLOCKS; j++) new_ino.dblocks[j] = -1;
            for (int j = 0; j < N_IBLOCKS; j++) new_ino.iblocks[j] = -1;
            new_ino.i2block = -1;
            new_ino.i3block = -1;
            memcpy(outbuf + inode_offset, &new_ino, sizeof(inode));
            continue;
        }

        int blocks_needed = (int)((old_ino.size + (int)blocksize - 1) / (int)blocksize);
        if (blocks_needed <= 0) {
            for (int j = 0; j < N_DBLOCKS; j++) new_ino.dblocks[j] = -1;
            for (int j = 0; j < N_IBLOCKS; j++) new_ino.iblocks[j] = -1;
            new_ino.i2block = -1;
            new_ino.i3block = -1;
            memcpy(outbuf + inode_offset, &new_ino, sizeof(inode));
            continue;
        }

        int ptrs_per_block = (int)(blocksize / 4);
        int collected = 0;

        // gather direct blocks
        for (int d = 0; d < N_DBLOCKS && collected < blocks_needed; d++) {
            int idx = old_ino.dblocks[d];
            if (idx < 0) break;
            file_blocks[collected++] = idx;
        }

        // gather from single indirect blocks
        for (int ib = 0; ib < N_IBLOCKS && collected < blocks_needed; ib++) {
            int ib_idx = old_ino.iblocks[ib];
            if (ib_idx < 0) continue;
            const unsigned char *ib_blk = inbuf + data_region_start + (size_t)ib_idx * blocksize;
            for (int j = 0; j < ptrs_per_block && collected < blocks_needed; j++) {
                int dblk = readIntLE(ib_blk + 4 * j);
                if (dblk < 0) break;
                file_blocks[collected++] = dblk;
            }
        }

        // gather from double indirect
        if (old_ino.i2block >= 0 && collected < blocks_needed) {
            int i2_idx = old_ino.i2block;
            const unsigned char *i2_blk = inbuf + data_region_start + (size_t)i2_idx * blocksize;
            for (int d = 0; d < ptrs_per_block && collected < blocks_needed; d++) {
                int ind_idx = readIntLE(i2_blk + 4 * d);
                if (ind_idx < 0) break;
                const unsigned char *ind_blk = inbuf + data_region_start + (size_t)ind_idx * blocksize;
                for (int j = 0; j < ptrs_per_block && collected < blocks_needed; j++) {
                    int dblk = readIntLE(ind_blk + 4 * j);
                    if (dblk < 0) break;
                    file_blocks[collected++] = dblk;
                }
            }
        }

        // gather from triple indirect
        if (old_ino.i3block >= 0 && collected < blocks_needed) {
            int i3_idx = old_ino.i3block;
            const unsigned char *i3_blk = inbuf + data_region_start + (size_t)i3_idx * blocksize;
            for (int t = 0; t < ptrs_per_block && collected < blocks_needed; t++) {
                int i2_idx = readIntLE(i3_blk + 4 * t);
                if (i2_idx < 0) break;
                const unsigned char *i2_blk = inbuf + data_region_start + (size_t)i2_idx * blocksize;
                for (int d = 0; d < ptrs_per_block && collected < blocks_needed; d++) {
                    int ind_idx = readIntLE(i2_blk + 4 * d);
                    if (ind_idx < 0) break;
                    const unsigned char *ind_blk = inbuf + data_region_start + (size_t)ind_idx * blocksize;
                    for (int j = 0; j < ptrs_per_block && collected < blocks_needed; j++) {
                        int dblk = readIntLE(ind_blk + 4 * j);
                        if (dblk < 0) break;
                        file_blocks[collected++] = dblk;
                    }
                }
            }
        }

        if (collected < blocks_needed) {
            blocks_needed = collected;
        }

        int logical_idx = 0;
        int remaining = blocks_needed;

        for (int j = 0; j < N_DBLOCKS; j++) new_ino.dblocks[j] = -1;
        for (int j = 0; j < N_IBLOCKS; j++) new_ino.iblocks[j] = -1;
        new_ino.i2block = -1;
        new_ino.i3block = -1;

        // direct blocks
        for (int j = 0; j < N_DBLOCKS && remaining > 0; j++) {
            if (next_new_block >= (int)n_data_blocks) {
                free(file_blocks);
                free(inbuf);
                free(outbuf);
                die("out of data blocks (direct)");
            }
            int new_idx = next_new_block++;
            new_ino.dblocks[j] = new_idx;
            int old_idx = file_blocks[logical_idx++];
            copy_data_block(inbuf, outbuf, data_region_start, blocksize, old_idx, new_idx);
            remaining--;
        }

        // single indirect blocks
        for (int ib = 0; ib < N_IBLOCKS && remaining > 0; ib++) {
            if (next_new_block >= (int)n_data_blocks) {
                free(file_blocks);
                free(inbuf);
                free(outbuf);
                die("out of data blocks (single indirect)");
            }
            int ind_idx = next_new_block++;
            new_ino.iblocks[ib] = ind_idx;

            unsigned char *ind_blk = outbuf + data_region_start + (size_t)ind_idx * blocksize;

            for (int j = 0; j < ptrs_per_block; j++) {
                if (remaining <= 0) {
                    writeIntLE(ind_blk + 4 * j, -1);
                } else {
                    if (next_new_block >= (int)n_data_blocks) {
                        free(file_blocks);
                        free(inbuf);
                        free(outbuf);
                        die("out of data blocks (data via single indirect)");
                    }
                    int data_idx = next_new_block++;
                    writeIntLE(ind_blk + 4 * j, data_idx);
                    int old_idx = file_blocks[logical_idx++];
                    copy_data_block(inbuf, outbuf, data_region_start, blocksize, old_idx, data_idx);
                    remaining--;
                }
            }
        }

        // double indirect blocks
        if (remaining > 0) {
            if (next_new_block >= (int)n_data_blocks) {
                free(file_blocks);
                free(inbuf);
                free(outbuf);
                die("out of data blocks (double indirect root)");
            }
            int i2_idx = next_new_block++;
            new_ino.i2block = i2_idx;

            unsigned char *i2_blk = outbuf + data_region_start + (size_t)i2_idx * blocksize;

            for (int d = 0; d < ptrs_per_block; d++) {
                if (remaining <= 0) {
                    writeIntLE(i2_blk + 4 * d, -1);
                    continue;
                }

                if (next_new_block >= (int)n_data_blocks) {
                    free(file_blocks);
                    free(inbuf);
                    free(outbuf);
                    die("out of data blocks (double indirect child)");
                }
                int ind_idx = next_new_block++;
                writeIntLE(i2_blk + 4 * d, ind_idx);

                unsigned char *ind_blk = outbuf + data_region_start + (size_t)ind_idx * blocksize;

                for (int j = 0; j < ptrs_per_block; j++) {
                    if (remaining <= 0) {
                        writeIntLE(ind_blk + 4 * j, -1);
                    } else {
                        if (next_new_block >= (int)n_data_blocks) {
                            free(file_blocks);
                            free(inbuf);
                            free(outbuf);
                            die("out of data blocks (data via double indirect)");
                        }
                        int data_idx = next_new_block++;
                        writeIntLE(ind_blk + 4 * j, data_idx);
                        int old_idx = file_blocks[logical_idx++];
                        copy_data_block(inbuf, outbuf, data_region_start, blocksize, old_idx, data_idx);
                        remaining--;
                    }
                }
            }
        }

        // triple indirect blocks
        if (remaining > 0) {
            if (next_new_block >= (int)n_data_blocks) {
                free(file_blocks);
                free(inbuf);
                free(outbuf);
                die("out of data blocks (triple indirect root)");
            }
            int i3_idx = next_new_block++;
            new_ino.i3block = i3_idx;

            unsigned char *i3_blk = outbuf + data_region_start + (size_t)i3_idx * blocksize;

            for (int t = 0; t < ptrs_per_block; t++) {
                if (remaining <= 0) {
                    writeIntLE(i3_blk + 4 * t, -1);
                    continue;
                }

                if (next_new_block >= (int)n_data_blocks) {
                    free(file_blocks);
                    free(inbuf);
                    free(outbuf);
                    die("out of data blocks (triple->double)");
                }
                int dbl_idx = next_new_block++;
                writeIntLE(i3_blk + 4 * t, dbl_idx);

                unsigned char *dbl_blk = outbuf + data_region_start + (size_t)dbl_idx * blocksize;

                for (int d = 0; d < ptrs_per_block; d++) {
                    if (remaining <= 0) {
                        writeIntLE(dbl_blk + 4 * d, -1);
                        continue;
                    }

                    if (next_new_block >= (int)n_data_blocks) {
                        free(file_blocks);
                        free(inbuf);
                        free(outbuf);
                        die("out of data blocks (triple->double->ind)");
                    }
                    int ind_idx = next_new_block++;
                    writeIntLE(dbl_blk + 4 * d, ind_idx);

                    unsigned char *ind_blk = outbuf + data_region_start + (size_t)ind_idx * blocksize;

                    for (int j = 0; j < ptrs_per_block; j++) {
                        if (remaining <= 0) {
                            writeIntLE(ind_blk + 4 * j, -1);
                        } else {
                            if (next_new_block >= (int)n_data_blocks) {
                                free(file_blocks);
                                free(inbuf);
                                free(outbuf);
                                die("out of data blocks (data via triple indirect)");
                            }
                            int data_idx = next_new_block++;
                            writeIntLE(ind_blk + 4 * j, data_idx);
                            int old_idx = file_blocks[logical_idx++];
                            copy_data_block(inbuf, outbuf, data_region_start, blocksize, old_idx, data_idx);
                            remaining--;
                        }
                    }
                }
            }
        }

        memcpy(outbuf + inode_offset, &new_ino, sizeof(inode));
    }

    int used_blocks = next_new_block;
    fprintf(stderr, "Used data blocks after defrag: %d of %zu\n", used_blocks, n_data_blocks);

    int first_free_block;
    if (used_blocks >= (int)n_data_blocks) {
        first_free_block = -1;
    } else {
        first_free_block = used_blocks;
        for (int b = first_free_block; b < (int)n_data_blocks; b++) {
            unsigned char *blk = outbuf + data_region_start + (size_t)b * blocksize;
            int next = (b == (int)n_data_blocks - 1) ? -1 : (b + 1);
            writeIntLE(blk, next);
            if (blocksize > 4) {
                memset(blk + 4, 0, blocksize - 4);
            }
        }
    }

    unsigned char *out_sb_bytes = outbuf + 512;
    writeIntLE(out_sb_bytes + 20, first_free_block);

    FILE *out = fopen("disk_defrag", "wb");
    if (!out) {
        perror("fopen output");
        free(file_blocks);
        free(inbuf);
        free(outbuf);
        return EXIT_FAILURE;
    }

    size_t bytes_written = fwrite(outbuf, 1, disk_size, out);
    fclose(out);
    if (bytes_written != disk_size) {
        free(file_blocks);
        free(inbuf);
        free(outbuf);
        die("fwrite did not write full file");
    }

    free(file_blocks);
    free(inbuf);
    free(outbuf);

    fprintf(stderr, "Defragmentation complete. Output written to 'disk_defrag'.\n");
    return EXIT_SUCCESS;
}