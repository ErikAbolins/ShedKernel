#include "easyfs.h"
#include "mm.h"
#include "kprintf.h"
#include "string.h"

uint8_t fs_disk[TOTAL_BLOCKS * BLOCK_SIZE];

Superblock *fs_sb      = NULL;
uint8_t    *fs_bitmap  = NULL;
RootDir    *fs_rootdir = NULL;


void fs_init(void) {
    /* zero the whole disk */
    for (int i = 0; i < TOTAL_BLOCKS * BLOCK_SIZE; i++)
        fs_disk[i] = 0;

    fs_sb      = (Superblock *)(fs_disk + SUPERBLOCK_BLOCK * BLOCK_SIZE);
    fs_bitmap  = (uint8_t    *)(fs_disk + BITMAP_BLOCK     * BLOCK_SIZE);
    fs_rootdir = (RootDir    *)(fs_disk + ROOT_DIR_BLOCK   * BLOCK_SIZE);

    /* fill in superblock */
    fs_sb->magic           = SUPERBLOCK_MAGIC;
    fs_sb->total_blocks    = TOTAL_BLOCKS;
    fs_sb->block_size      = BLOCK_SIZE;
    fs_sb->num_data_blocks = NUM_DATA_BLOCKS;
    fs_sb->free_block_count= NUM_DATA_BLOCKS;
    fs_sb->bitmap_block    = BITMAP_BLOCK;
    fs_sb->root_dir_block  = ROOT_DIR_BLOCK;
    fs_sb->first_data_block= FIRST_DATA_BLOCK;
    fs_sb->timestamp       = 0;

    kprintf("easyfs: initialised. %d blocks, %d data blocks\n",
            TOTAL_BLOCKS, NUM_DATA_BLOCKS);
}



void fs_write_block(uint32_t block, const void *buf) {
    uint8_t *dst = fs_disk + block * BLOCK_SIZE;
    const uint8_t *src = buf;
    for (int i = 0; i < BLOCK_SIZE; i++)
        dst[i] = src[i];
}

void fs_read_block(uint32_t block, void *buf) {
    uint8_t *src = fs_disk + block * BLOCK_SIZE;
    uint8_t *dst = buf;
    for (int i = 0; i < BLOCK_SIZE; i++)
        dst[i] = src[i];
}



bool fs_block_is_free(uint32_t block) {
    if (block >= fs_sb->num_data_blocks) return false;
    return (fs_bitmap[block / 8] & (1 << (block % 8))) == 0;
}

bool fs_mark_block_used(uint32_t block) {
    if (block >= fs_sb->num_data_blocks) return false;
    fs_bitmap[block / 8] |= (1 << (block % 8));
    fs_sb->free_block_count--;
    return true;
}

bool fs_mark_block_free(uint32_t block) {
    if (block >= fs_sb->num_data_blocks) return false;
    fs_bitmap[block / 8] &= ~(1 << (block % 8));
    fs_sb->free_block_count++;
    return true;
}

int fs_alloc_block(void) {
    for (uint32_t i = 0; i < fs_sb->num_data_blocks; i++) {
        if (fs_block_is_free(i)) {
            fs_mark_block_used(i);
            return (int)(fs_sb->first_data_block + i);
        }
    }
    return -1;  /* no free blocks */
}



DirEntry *fs_find_file(const char *name) {
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (fs_rootdir->entries[i].is_used &&
            strcmp(fs_rootdir->entries[i].name, name) == 0)
            return &fs_rootdir->entries[i];
    }
    return NULL;
}

int fs_create_file(const char *name) {
    if (fs_find_file(name)) {
        kprintf("easyfs: file '%s' already exists\n", name);
        return -1;
    }

    /* find a free directory slot */
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (!fs_rootdir->entries[i].is_used) {
            int block = fs_alloc_block();
            if (block < 0) {
                kprintf("easyfs: no free blocks\n");
                return -1;
            }
            /* copy name safely */
            int j;
            for (j = 0; j < MAX_FILENAME_LEN - 1 && name[j]; j++)
                fs_rootdir->entries[i].name[j] = name[j];
            fs_rootdir->entries[i].name[j] = '\0';

            fs_rootdir->entries[i].file_size       = 0;
            fs_rootdir->entries[i].first_data_block = (uint32_t)block;
            fs_rootdir->entries[i].is_used         = 1;
            return 0;
        }
    }

    kprintf("easyfs: root directory full\n");
    return -1;
}

int fs_delete_file(const char *name) {
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (fs_rootdir->entries[i].is_used &&
            strcmp(fs_rootdir->entries[i].name, name) == 0) {
            fs_mark_block_free(fs_rootdir->entries[i].first_data_block - fs_sb->first_data_block);
            fs_rootdir->entries[i].is_used = 0;
            return 0;
        }
    }
    kprintf("easyfs: file '%s' not found\n", name);
    return -1;
}

void fs_list_files(void) {
    int found = 0;
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (fs_rootdir->entries[i].is_used) {
            kprintf("  %s (%d bytes)\n",
                    fs_rootdir->entries[i].name,
                    fs_rootdir->entries[i].file_size);
            found++;
        }
    }
    if (!found) kprintf("  (empty)\n");
}