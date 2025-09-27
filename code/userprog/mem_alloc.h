#ifndef MEM_ALLOC_H

#define MEM_ALLOC_H
#define PageSize 128 
#include "libgcc.h"
#include "syscall.h"

#define NULL ((void *)0)
#define divRoundDown(n, s) ((n) / (s))
#define divRoundUp(n, s) (((n) / (s)) + ((((n) % (s)) > 0) ? 1 : 0))


typedef struct
{
    void *start_addr; /* smallest address in the heap */
    void *end_addr;   /* highest address in the heap */
    void *first_free;
    sem_t sem_malloc;
} mem_pool;

typedef struct
{
    uint32_t flag_and_size;
} mem_std_block_header_footer_t;

typedef struct mem_std_free_block
{
    mem_std_block_header_footer_t header;
    struct mem_std_free_block *prev;
    struct mem_std_free_block *next;
} mem_std_free_block_t;

typedef struct
{
    mem_std_block_header_footer_t header;
} mem_std_allocated_block_t;

void mem_init(size_t size);

void *mem_alloc(size_t size);

void mem_free(void *ptr);

#endif // !MEM_ALLOC_H
