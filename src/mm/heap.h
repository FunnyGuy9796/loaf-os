#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

typedef struct block_header {
    size_t size;
    struct block_header *next;
    uint8_t free;
} block_header_t;

#define BLOCK_HEADER_SIZE sizeof(block_header_t)

void heap_init(uint32_t num_frames);
uint32_t kmalloc(uint32_t bytes);
void kfree(uint32_t ptr);
void heap_dump();

#endif