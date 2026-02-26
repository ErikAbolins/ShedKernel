#ifndef MM_H
#define MM_H


#define NULL_POINTER            ((void*)0)
#define DYNAMIC_MEM_TOTAL_SIZE  (1024U * 1024U)
typedef unsigned int size_t;
typedef unsigned char  uint8_t;
typedef unsigned int   uint32_t;
typedef unsigned char  bool;
#define true  1
#define false 0


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

#endif /* MM_H */