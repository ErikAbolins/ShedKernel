/*
 * mm.c - Memory management
 *
 * Covers: physical page frame allocator, dynamic heap (best-fit
 *         linked-list allocator), malloc / mem_free / realloc.
 */

#include "mm.h"
#include "kprintf.h"
#include <stdint.h>

#define NULL_POINTER        ((void*)0)
#define DYNAMIC_MEM_NODE_SIZE   sizeof(dynamic_mem_node_t)

#define PAGE_SIZE           4096
#define PHYS_MEM_END        0x8000000   /* 128 MB */

#define PHYS_TO_VIRT(p) ((p) + 0xC0000000)

typedef unsigned int u32;

extern u32 kernel_end;

static uint8_t           dynamic_mem_area[DYNAMIC_MEM_TOTAL_SIZE];
static dynamic_mem_node_t *dynamic_mem_start;
static u32               next_free_page = 0;


/* =========================================================
 * Physical page allocator
 * ========================================================= */

void paging_alloc_init(void)
{
    next_free_page = ((u32)&kernel_end + 0xFFF) & ~0xFFF;
}

u32 alloc_page_frame(void)
{
    if (next_free_page >= PHYS_MEM_END)
        return 0;
    u32 addr = next_free_page;
    next_free_page += PAGE_SIZE;
    return addr;
}


void map_kernel_page(u32 virt, u32 phys) {
    extern u32 *page_directory;

    u32 dir_idx = virt >> 22;
    u32 table_idx = (virt >> 12) & 0x3FF;

    if(!(page_directory[dir_idx] & 1)) {
        u32 new_table = alloc_page_frame();

        u32 *tbl = (u32*)PHYS_TO_VIRT(new_table);

        for (int i = 0; i < 1024; i++) tbl[i] = 0;

        page_directory[dir_idx] = new_table | 3;
    }
    u32 *table = (u32*)PHYS_TO_VIRT(page_directory[dir_idx] & ~0xFFF);

    table[table_idx] = (phys & ~0xFFF) | 3;

    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}


/* =========================================================
 * Dynamic heap
 * ========================================================= */

void init_dynamic_mem(void)
{
    dynamic_mem_start       = (dynamic_mem_node_t *)dynamic_mem_area;
    dynamic_mem_start->size = DYNAMIC_MEM_TOTAL_SIZE - DYNAMIC_MEM_NODE_SIZE;
    dynamic_mem_start->next = NULL_POINTER;
    dynamic_mem_start->prev = NULL_POINTER;
    dynamic_mem_start->used = false;
}

static void *find_best_mem_block(dynamic_mem_node_t *heap, size_t size)
{
    dynamic_mem_node_t *best  = NULL_POINTER;
    uint32_t            best_size = DYNAMIC_MEM_TOTAL_SIZE + 1;
    dynamic_mem_node_t *cur   = heap;

    while (cur) {
        if (!cur->used && cur->size >= size && cur->size <= best_size) {
            best      = cur;
            best_size = cur->size;
        }
        cur = cur->next;
    }
    return best;
}

void *malloc(size_t size)
{
    dynamic_mem_node_t *block = find_best_mem_block(dynamic_mem_start, size);

    if (block == NULL_POINTER)
        return NULL_POINTER;

    if (block->size >= size + DYNAMIC_MEM_NODE_SIZE) {
        /* Carve a new node out of the tail of this block */
        dynamic_mem_node_t *new_node =
            (dynamic_mem_node_t *)((uint8_t *)block + DYNAMIC_MEM_NODE_SIZE +
                                   block->size - size - DYNAMIC_MEM_NODE_SIZE);

        new_node->size = size;
        new_node->used = true;
        new_node->next = block->next;
        new_node->prev = block;

        if (block->next != NULL_POINTER)
            block->next->prev = new_node;

        block->next  = new_node;
        block->size -= size + DYNAMIC_MEM_NODE_SIZE;

        return (void *)((uint8_t *)new_node + DYNAMIC_MEM_NODE_SIZE);
    }

    block->used = true;
    return (void *)((uint8_t *)block + DYNAMIC_MEM_NODE_SIZE);
}

void mem_free(void *p)
{
    if (p == NULL_POINTER)
        return;

    dynamic_mem_node_t *node =
        (dynamic_mem_node_t *)((uint8_t *)p - DYNAMIC_MEM_NODE_SIZE);

    if (node == NULL_POINTER)
        return;

    node->used = false;

    node = merge_next_node_into_current(node);
    merge_current_node_into_previous(node);
}

void *realloc(void *p, size_t size)
{
    if (p == NULL_POINTER)
        return malloc(size);

    if (size == 0) {
        mem_free(p);
        return NULL_POINTER;
    }

    if ((void *)p < (void *)dynamic_mem_area ||
        (void *)p >= (void *)dynamic_mem_area + DYNAMIC_MEM_TOTAL_SIZE)
        return NULL_POINTER;

    dynamic_mem_node_t *node =
        (dynamic_mem_node_t *)((uint8_t *)p - DYNAMIC_MEM_NODE_SIZE);

    if (node->size >= size)
        return p;

    void *new_p = malloc(size);
    if (new_p == NULL_POINTER)
        return NULL_POINTER;

    uint8_t *src = (uint8_t *)p;
    uint8_t *dst = (uint8_t *)new_p;
    for (size_t i = 0; i < node->size; i++)
        dst[i] = src[i];

    mem_free(p);
    return new_p;
}


/* =========================================================
 * Block coalescing helpers
 * ========================================================= */

void *merge_next_node_into_current(dynamic_mem_node_t *node)
{
    dynamic_mem_node_t *next = node->next;
    if (next != NULL_POINTER && !next->used) {
        node->size += next->size + DYNAMIC_MEM_NODE_SIZE;
        node->next  = next->next;
        if (node->next != NULL_POINTER)
            node->next->prev = node;
    }
    return node;
}

void *merge_current_node_into_previous(dynamic_mem_node_t *node)
{
    dynamic_mem_node_t *prev = node->prev;
    if (prev != NULL_POINTER && !prev->used) {
        prev->size += node->size + DYNAMIC_MEM_NODE_SIZE;
        prev->next  = node->next;
        if (node->next != NULL_POINTER)
            node->next->prev = prev;
    }
    return prev;
}
