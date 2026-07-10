#include "pmm.h"
#include "paging.h"
#include "../misc/printf.h"

boot_info_t boot_info;
uint32_t *phys_bitmap;
uint32_t bitmap_size = 0;

static inline uint32_t addr_to_frame(uint32_t addr) {
    return (uint32_t)(addr >> PAGE_SHIFT);
}

static inline uint32_t frame_to_addr(uint32_t frame) {
    return frame << PAGE_SHIFT;
}

static inline uint32_t bit_to_addr(uint32_t index, uint32_t bit) {
    uint32_t frame = (index << BITS_SHIFT) | bit;

    return frame_to_addr(frame);
}

static inline void set_frame(uint32_t addr) {
    uint32_t frame = addr_to_frame(addr);
    uint32_t index = frame >> BITS_SHIFT;
    uint32_t bit = frame & BITS_MASK;

    phys_bitmap[index] |= (1U << bit);
}

static inline void clear_frame(uint32_t addr) {
    uint32_t frame = addr_to_frame(addr);
    uint32_t index = frame >> BITS_SHIFT;
    uint32_t bit = frame & BITS_MASK;

    phys_bitmap[index] &= ~(1U << bit);
}

static inline int test_frame(uint32_t addr) {
    uint32_t frame = addr_to_frame(addr);
    uint32_t index = frame >> BITS_SHIFT;
    uint32_t bit = frame & BITS_MASK;

    return (phys_bitmap[index] & (1U << bit)) != 0;
}

void pmm_init(raw_boot_info_t *raw, uint32_t kend, uint32_t highest_addr) {
    boot_info.conv_mem = raw->conv_kb;
    boot_info.ext_mem = raw->ext_kb;
    boot_info.ebda_start = (uint32_t)raw->ebda_segment * 16;

    uint8_t i = 0;

    boot_info.regions[i++] = (mem_region_t){ 0x00000, 0x500, MEM_RESERVED };

    uint32_t conv_end = (uint32_t)boot_info.conv_mem * 1024;

    boot_info.regions[i++] = (mem_region_t){ 0x500, conv_end - 0x500, MEM_USABLE };

    if (conv_end < 0xa0000)
        boot_info.regions[i++] = (mem_region_t){ conv_end, 0xa0000 - conv_end, MEM_RESERVED };
    
    boot_info.regions[i++] = (mem_region_t){ 0xa0000, 0x20000, MEM_RESERVED };
    boot_info.regions[i++] = (mem_region_t){ 0xc0000, 0x8000, MEM_RESERVED };
    boot_info.regions[i++] = (mem_region_t){ 0xc8000, 0x28000, MEM_RESERVED };
    boot_info.regions[i++] = (mem_region_t){ 0xf0000, 0x10000, MEM_RESERVED };

    if (boot_info.ext_mem > 0)
        boot_info.regions[i++] = (mem_region_t){ 0x100000, (uint32_t)boot_info.ext_mem * 1024, MEM_USABLE };

    boot_info.region_count = i;

    uint32_t bitmap_phys = (kend + 0xfff) & ~0xfffU;

    phys_bitmap = (uint32_t *)phys_to_virt(bitmap_phys);

    uint32_t total_frames = highest_addr / PAGE_SIZE;

    bitmap_size = (total_frames + 31) / 32;
    
    for (uint8_t i = 0; i < boot_info.region_count; i++) {
        mem_region_t region = boot_info.regions[i];

        if (region.type == MEM_RESERVED) {
            for (uint32_t addr = region.base; addr < region.base + region.length; addr += PAGE_SIZE)
                set_frame(addr);
        } else if (region.type == MEM_USABLE) {
            uint32_t start = (region.base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            uint32_t end = region.base + region.length;

            for (uint32_t addr = start; addr < end; addr += PAGE_SIZE)
                clear_frame(addr);
        }
    }

    uint32_t bitmap_end_phys = bitmap_phys + (bitmap_size * sizeof(uint32_t));

    for (uint32_t addr = bitmap_phys; addr < bitmap_end_phys; addr += PAGE_SIZE)
        set_frame(addr);
}

uint32_t pmm_alloc_frame() {
    for (uint32_t frame = 0; frame < bitmap_size * 32; frame++) {
        uint32_t addr = frame_to_addr(frame);

        if (!test_frame(addr)) {
            set_frame(addr);

            return addr;
        }
    }

    panic("pmm.c: pmm_alloc_frame() -> out of physical memory\n");

    return (uint32_t)-1;
}

void pmm_free_frame(uint32_t addr) {
    clear_frame(addr);
}

uint32_t pmm_alloc_frames(uint32_t count) {
    uint32_t total_frames = bitmap_size * 32;
    uint32_t run_start = 0, run_len = 0;

    for (uint32_t frame = 0; frame < total_frames; frame++) {
        if (!test_frame(frame_to_addr(frame))) {
            if (run_len == 0)
                run_start = frame;

            run_len++;

            if (run_len == count) {
                for (uint32_t f = run_start; f < run_start + count; f++)
                    set_frame(frame_to_addr(f));

                return frame_to_addr(run_start);
            }
        } else
            run_len = 0;
    }

    panic("pmm.c: pmm_alloc_frames() -> not enough contiguous physical memory\n");

    return (uint32_t)-1;
}

void pmm_free_frames(uint32_t addr, uint32_t count) {
    for (uint32_t i = 0; i < count; i++)
        clear_frame(addr + i * PAGE_SIZE);
}