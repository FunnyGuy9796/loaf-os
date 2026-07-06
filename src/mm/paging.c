#include "paging.h"

static uint32_t page_directory[1024] __attribute__((aligned(PAGE_SIZE)));
static uint32_t page_tables[MAX_TABLES][1024] __attribute((aligned(PAGE_SIZE)));

static inline void load_page_directory(uint32_t pd_phys) {
    __asm__ volatile ("mov %0, %%cr3" :: "r"(pd_phys));
}

static inline void enable_paging() {
    uint32_t cr0;

    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));

    cr0 |= 0x80000000;

    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));
}

void paging_init(uint32_t highest_addr) {
    uint32_t num_tables = (highest_addr / 0x400000) + 1;

    for (int i = 0; i < 1024; i++)
        page_directory[i] = 0;
    
    for (uint32_t t = 0; t < num_tables; t++) {
        for (int i = 0; i < 1024; i++) {
            uint32_t phys_addr = (t * 0x400000) + (i * 0x1000);

            page_tables[t][i] = phys_addr | PAGE_PRESENT | PAGE_RW;
        }

        page_directory[t] = ((uint32_t)page_tables[t]) | PAGE_PRESENT | PAGE_RW;
    }

    load_page_directory((uint32_t)page_directory);
    enable_paging();
}