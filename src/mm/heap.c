#include "heap.h"
#include "pmm.h"
#include "paging.h"
#include "../misc/printf.h"

static block_header_t *free_list = NULL;
static uint32_t heap_start = 0;
static uint32_t heap_end = 0;

void heap_init(uint32_t num_frames) {
    heap_start = pmm_alloc_frames(num_frames);

    free_list = (block_header_t *)heap_start;
    free_list->size = (num_frames * PAGE_SIZE) - BLOCK_HEADER_SIZE;
    free_list->next = NULL;
    free_list->free = 1;

    heap_end = heap_start + free_list->size;
}

uint32_t kmalloc(uint32_t bytes) {
    bytes = (bytes + 3) & ~3;

    block_header_t *curr = free_list;

    while (curr) {
        if (curr->free && curr->size >= bytes) {
            if (curr->size >= bytes + BLOCK_HEADER_SIZE + 4) {
                block_header_t *old_next = curr->next;
                block_header_t *new_block = (block_header_t *)((uint8_t *)curr + BLOCK_HEADER_SIZE + bytes);

                new_block->size = curr->size - bytes - BLOCK_HEADER_SIZE;
                new_block->next = old_next;
                new_block->free = 1;

                curr->size = bytes;
                curr->next = new_block;
            }

            curr->free = 0;

            return (uint32_t)((uint8_t *)curr + BLOCK_HEADER_SIZE);
        }

        curr = curr->next;
    }

    return 0;
}

void kfree(uint32_t ptr) {
    if (ptr == 0)
        return;
    
    if (ptr < heap_start || ptr >= heap_end) {
        printf("[W] heap.c: kfree() -> pointer 0x%x outside heap bounds\n", ptr);

        return;
    }
    
    block_header_t *block = (block_header_t *)(ptr - BLOCK_HEADER_SIZE);

    if (block->free) {
        printf("[W] heap.c: kfree() -> double-free at 0x%x\n", ptr);

        return;
    }

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