#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include "../arch.h"

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2
#define PAGE_USER 0x4
#define PAGE_SIZE 4096
#define MAX_TABLES 17

#define KERNEL_PDE_START (KERNEL_VIRT_BASE >> 22)

static inline void *phys_to_virt(uint32_t phys) {
    return (void *)(phys + KERNEL_VIRT_BASE);
}

static inline uint32_t virt_to_phys(const void *virt) {
    return (uint32_t)virt - KERNEL_VIRT_BASE;
}

uint32_t create_address_space();
int paging_map_page(uint32_t pd_phys, uint32_t vaddr, uint32_t paddr, uint32_t flags);
void paging_unmap_page(uint32_t pd_phys, uint32_t vaddr);
uint32_t paging_get_current_directory(void);
void paging_switch_directory(uint32_t pd_phys);
void paging_init(uint32_t highest_addr);
uint32_t paging_kernel_directory();
int paging_walk(uint32_t pd_phys, uint32_t vaddr, uint32_t *paddr_out);

#endif