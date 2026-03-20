#ifndef FS_CONFIG_H
#define FS_CONFIG_H

#define BLOCK_SIZE          4096
#define TOTAL_BLOCKS        1024
#define MAX_FILENAME_LEN    16
#define MAX_DIR_ENTRIES     64
#define NUM_DATA_BLOCKS     (TOTAL_BLOCKS - 3)  /* superblock + bitmap + root dir */

#define SUPERBLOCK_MAGIC    0xDEADBEEF

/* block layout */
#define SUPERBLOCK_BLOCK    0
#define BITMAP_BLOCK        1
#define ROOT_DIR_BLOCK      2
#define FIRST_DATA_BLOCK    3

#define BITMAP_SIZE_BYTES   ((NUM_DATA_BLOCKS + 7) / 8)

#endif