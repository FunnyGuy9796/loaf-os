#include <stddef.h>
#include "include/stdlib.h"
#include "include/syscall_api.h"

typedef struct block_header {
    uint32_t size;
    struct block_header *next;
    uint8_t free;
} block_header_t;

#define BLOCK_HEADER_SIZE sizeof(block_header_t)
#define MIN_GROWTH 4096

static block_header_t *free_list = NULL;
static uint32_t heap_end = 0;

static int grow_heap(uint32_t need) {
    uint32_t grow_amount = need > MIN_GROWTH ? need : MIN_GROWTH;
    void *base = sys_sbrk((int32_t)grow_amount);

    if (!base)
        return 0;

    block_header_t *new_block = (block_header_t *)base;

    new_block->size = grow_amount - BLOCK_HEADER_SIZE;
    new_block->free = 1;
    new_block->next = NULL;

    if (!free_list)
        free_list = new_block;
    else {
        block_header_t *curr = free_list;

        while (curr->next)
            curr = curr->next;

        curr->next = new_block;
    }

    heap_end = (uint32_t)base + grow_amount;

    return 1;
}

void *malloc(uint32_t size) {
    size = (size + 3) & ~3;

    block_header_t *curr = free_list;

    while (curr) {
        if (curr->free && curr->size >= size) {
            if (curr->size >= size + BLOCK_HEADER_SIZE + 4) {
                block_header_t *old_next = curr->next;
                block_header_t *new_block = (block_header_t *)((uint8_t *)curr + BLOCK_HEADER_SIZE + size);

                new_block->size = curr->size - size - BLOCK_HEADER_SIZE;
                new_block->next = old_next;
                new_block->free = 1;

                curr->size = size;
                curr->next = new_block;
            }

            curr->free = 0;

            return (uint8_t *)curr + BLOCK_HEADER_SIZE;
        }

        curr = curr->next;
    }

    if (!grow_heap(size + BLOCK_HEADER_SIZE))
        return NULL;

    return malloc(size);
}

void free(void *ptr) {
    if (!ptr)
        return;

    block_header_t *block = (block_header_t *)((uint8_t *)ptr - BLOCK_HEADER_SIZE);

    block->free = 1;

    block_header_t *curr = free_list;

    while (curr && curr->next) {
        if (curr->free && curr->next->free && (uint8_t *)curr + BLOCK_HEADER_SIZE + curr->size == (uint8_t *)curr->next) {
            curr->size += BLOCK_HEADER_SIZE + curr->next->size;
            curr->next = curr->next->next;
        } else
            curr = curr->next;
    }
}