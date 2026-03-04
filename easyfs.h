#ifndef EASYFS_H
#define EASYFS_H

#include <stdint.h>
#include <stdbool.h>
#include "fs_config.h"
#include "fs_types.h"

/* the entire filesystem lives in this array */
extern uint8_t fs_disk[TOTAL_BLOCKS * BLOCK_SIZE];

/* pointers into fs_disk */
extern Superblock *fs_sb;
extern uint8_t    *fs_bitmap;
extern RootDir    *fs_rootdir;

/* init */
void fs_init(void);

/* bitmap ops */
bool fs_block_is_free(uint32_t block);
bool fs_mark_block_used(uint32_t block);
bool fs_mark_block_free(uint32_t block);
int  fs_alloc_block(void);   /* returns block number or -1 */

/* directory ops */
int  fs_create_file(const char *name);
int  fs_delete_file(const char *name);
DirEntry *fs_find_file(const char *name);
void fs_list_files(void);

/* block read/write */
void fs_write_block(uint32_t block, const void *buf);
void fs_read_block(uint32_t block, void *buf);

#endif