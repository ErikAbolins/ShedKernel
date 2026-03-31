#ifndef MM_H
#define MM_H


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define NULL_POINTER            ((void*)0)
#define DYNAMIC_MEM_TOTAL_SIZE  (1024U * 1024U)
typedef unsigned int u32;


typedef struct dynamic_mem_node {
    uint32_t size;
    uint32_t used;
    struct dynamic_mem_node *next;
    struct dynamic_mem_node *prev;
} dynamic_mem_node_t;


void init_dynamic_mem(void);


void *malloc(size_t size);

void mem_free(void *p);

void *realloc(void *p, size_t size);

void *merge_next_node_into_current(dynamic_mem_node_t *node);
void *merge_current_node_into_previous(dynamic_mem_node_t *node);

void paging_alloc_init();
u32 alloc_page_frame();
void map_kernel_page(u32 virt, u32 phys);


#endif /* MM_H */