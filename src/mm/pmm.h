#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include "../boot.h"

#define PAGE_SHIFT 12
#define BITS_PER_WORD 32
#define BITS_SHIFT 5
#define BITS_MASK 31

extern boot_info_t boot_info;
extern uint32_t *phys_bitmap;
extern uint32_t bitmap_size;

void pmm_init(raw_boot_info_t *raw, uint32_t kend, uint32_t highest_addr);
uint32_t pmm_alloc_frame();
void pmm_free_frame(uint32_t addr);
uint32_t pmm_alloc_frames(uint32_t count);
void pmm_free_frames(uint32_t addr, uint32_t count);

#endif