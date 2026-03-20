#ifndef FS_TYPES_H
#define FS_TYPES_H

#include <stdint.h>
#include "fs_config.h"

typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t block_size;
    uint32_t num_data_blocks;
    uint32_t free_block_count;
    uint32_t bitmap_block;
    uint32_t root_dir_block;
    uint32_t first_data_block;
    uint64_t timestamp;
} Superblock;

typedef struct {
    char     name[MAX_FILENAME_LEN];
    uint32_t file_size;
    uint32_t first_data_block;
    uint8_t  is_used;
} DirEntry;

typedef struct {
    DirEntry entries[MAX_DIR_ENTRIES];
} RootDir;

#endif