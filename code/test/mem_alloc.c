#include "mem_alloc.h"
#include "syscall.h"
#define HEADER_FOOTER_SIZE sizeof(mem_std_block_header_footer_t)
mem_pool mem;
int is_block_used(mem_std_block_header_footer_t *m) { return (((m->flag_and_size) >> 31) & 1UL); }

/* Returns 1 the if block is free, or 1 if the block is free */
int is_block_free(mem_std_block_header_footer_t *m) { return (is_block_used(m) == 0); }

/* Modifies a block header (or footer) to mark it as used */
void set_block_used(mem_std_block_header_footer_t *m)
{
    m->flag_and_size = ((m->flag_and_size) | (1UL << 31));
}

/* Modifies a block header (or footer) to mark it as free */
void set_block_free(mem_std_block_header_footer_t *m)
{
    m->flag_and_size = ((m->flag_and_size) & ~(1UL << 31));
}

/* Returns the size of a block (as stored in the header/footer) */
size_t get_block_size(mem_std_block_header_footer_t *m)
{
    uint32_t res = ((m->flag_and_size) & ~(1UL << 31));
    return (size_t)res;
}

/* Modifies a block header (or footer) to update the size of the block */
void set_block_size(mem_std_block_header_footer_t *m, size_t size)
{
    uint32_t s = (uint32_t)size;
    uint32_t flag = (m->flag_and_size) & (1UL << 31);
    m->flag_and_size = flag | s;
}
size_t get_full_block_size(mem_std_block_header_footer_t *block_header)
{
    return get_block_size(block_header) + 2 * HEADER_FOOTER_SIZE;
}

mem_std_block_header_footer_t *get_footer(mem_std_free_block_t *block)
{
    char *block_end_addr = (char *)block + get_full_block_size(&block->header);
    return (mem_std_block_header_footer_t *)((char *)block_end_addr - HEADER_FOOTER_SIZE);
}

void mem_init(size_t size)
{
    SemInit(&mem.sem_malloc, 1);
    int n_pages = divRoundUp(size + 2 * HEADER_FOOTER_SIZE, PageSize);
    void *start_addr = Sbrk(n_pages);

    if(start_addr == NULL || n_pages == 0)
    {
        mem.start_addr = NULL;
        mem.end_addr = NULL;
        mem.first_free = 0;
        return;
    }
    size = PageSize * n_pages;
    mem_std_free_block_t *block = (mem_std_free_block_t *)start_addr;
    set_block_free(&block->header);
    set_block_size(&block->header, size - 2 * HEADER_FOOTER_SIZE);
    block->prev = NULL;
    block->next = NULL;
    mem.start_addr = start_addr;
    mem.end_addr = start_addr + size;
    mem.first_free = block;

    mem_std_block_header_footer_t *footer = get_footer(block);
    set_block_free(footer);
    set_block_size(footer, size - 2 * HEADER_FOOTER_SIZE);
}

void *mem_alloc(size_t size)
{
    SemWait(&mem.sem_malloc);
    mem_std_free_block_t *cur = mem.first_free;
    size_t cur_block_size = 0;
    while(cur != NULL && (cur_block_size = get_block_size(&cur->header)) < size)
    {
        cur = cur->next;
    }
    if(cur == NULL)
    {
        SemPost(&mem.sem_malloc);
        return NULL;
    }
    cur_block_size = get_full_block_size(&cur->header);
    size_t splitted_size = cur_block_size - size - 2 * HEADER_FOOTER_SIZE;
    mem_std_free_block_t *prev = cur->prev;
    mem_std_free_block_t *next = cur->next;
    if(splitted_size <= 2 * HEADER_FOOTER_SIZE)
    {
        // Under this size we don't need to split the block
        size += splitted_size;
        splitted_size = -1;
    }
    set_block_used(&cur->header);
    set_block_size(&cur->header, size);

    // Footers
    mem_std_block_header_footer_t *footer_cur = get_footer(cur);
    set_block_used(footer_cur);
    set_block_size(footer_cur, size);

    if(splitted_size != -1)
    {
        mem_std_free_block_t *splitted_block =
            (mem_std_free_block_t *)((char *)cur + size + 2 * HEADER_FOOTER_SIZE);
        set_block_free(&splitted_block->header);
        set_block_size(&splitted_block->header, splitted_size - 2 * HEADER_FOOTER_SIZE);
        mem_std_block_header_footer_t *footer_splitted = get_footer(splitted_block);
        set_block_free(footer_splitted);
        set_block_size(footer_splitted, splitted_size - 2 * HEADER_FOOTER_SIZE);
        // Update the prev and the next
        splitted_block->prev = cur->prev;
        splitted_block->next = cur->next;
        if(prev != NULL)
        {
            prev->next = splitted_block;
        }
        else
        {
            // If the current block wasn't having a predecessor,
            // then it was the head of the free list
            mem.first_free = splitted_block;
        }
        if(next != NULL)
        {
            next->prev = splitted_block;
        }
    }
    else
    {
        if(prev != NULL)
        {
            prev->next = next;
        }
        else
        {
            // If the current block wasn't having a predecessor, then it was the head
            // of the free list
            mem.first_free = next;
        }
        if(next != NULL)
        {
            next->prev = prev;
        }
        // else : last_addr = first_free, handled by the find_valid_block function
    }

    SemPost(&mem.sem_malloc);
    return ((char *)cur) + HEADER_FOOTER_SIZE; // Skip the header
}

char *find_previous_free_block_footer(mem_std_free_block_t *block)
{
    char *prev_footer_addr = (char *)block - HEADER_FOOTER_SIZE;
    while(prev_footer_addr >= (char *)mem.start_addr &&
          !is_block_free((mem_std_block_header_footer_t *)prev_footer_addr))
    {
        prev_footer_addr = prev_footer_addr -
                           get_full_block_size((mem_std_block_header_footer_t *)prev_footer_addr);
    }
    return prev_footer_addr;
}

void merge_block(mem_std_free_block_t *block, mem_std_free_block_t *next_block)
{
    set_block_size(&block->header, get_block_size(&block->header) +
                                       get_block_size(&next_block->header) +
                                       2 * HEADER_FOOTER_SIZE);
    set_block_size(get_footer(block), get_block_size(&block->header));
}

void mem_free(void *ptr)
{
    SemWait(&mem.sem_malloc);
    mem_std_free_block_t *prev_block = NULL, *next_block = NULL;
    mem_std_free_block_t *free_block = (mem_std_free_block_t *)(ptr - HEADER_FOOTER_SIZE);
    // Update header and footer
    mem_std_block_header_footer_t *free_block_footer = get_footer(free_block);
    set_block_free(&free_block->header);
    set_block_free(free_block_footer);

    // Find the prev block footer
    char *prev_footer_addr = find_previous_free_block_footer(free_block);
    // Update the prev block if it exist
    if(prev_footer_addr < (char *)mem.start_addr)
    {
        // No free prev, start of the list, and the next is the first free
        next_block = mem.first_free;
        mem.first_free = free_block;
        free_block->prev = NULL;
    }
    else
    {
        // Get the prev block from his footer
        prev_block =
            (mem_std_free_block_t *)(prev_footer_addr -
                                     get_full_block_size(
                                         (mem_std_block_header_footer_t *)prev_footer_addr) +
                                     HEADER_FOOTER_SIZE);
        // The next of the free block is the next of the prev block
        next_block = prev_block->next;
        if(prev_footer_addr + HEADER_FOOTER_SIZE == (char *)free_block)
        {
            // Merge the block, they are contiguous
            merge_block(prev_block, free_block);
            free_block = prev_block;
        }
        else
        {
            // Update the free list
            prev_block->next = free_block;
            free_block->prev = prev_block;
        }
    }

    // We have already deduce the next block from the previous
    if(next_block != NULL)
    {
        if((char *)next_block - HEADER_FOOTER_SIZE == (char *)free_block_footer)
        {
            // Merge the block, they are contiguous
            merge_block(free_block, next_block);
            free_block->next = next_block->next;
        }
        else
        {
            next_block->prev = free_block;
            free_block->next = next_block;
        }
    }
  SemPost(&mem.sem_malloc);
}
