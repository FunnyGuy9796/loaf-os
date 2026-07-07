#include "paging.h"
#include "pmm.h"
#include "../misc/mem.h"

static uint32_t page_directory[1024] __attribute__((aligned(PAGE_SIZE)));
static uint32_t page_tables[MAX_TABLES][1024] __attribute__((aligned(PAGE_SIZE)));
static uint32_t kernel_directory_phys;

static inline uint32_t *pd_virt(uint32_t pd_phys) {
    return (uint32_t *)phys_to_virt(pd_phys);
}

static inline uint32_t *pt_virt(uint32_t pt_phys) {
    return (uint32_t *)phys_to_virt(pt_phys);
}

uint32_t paging_get_current_directory(void) {
    uint32_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

void paging_switch_directory(uint32_t pd_phys) {
    __asm__ volatile ("mov %0, %%cr3" :: "r"(pd_phys) : "memory");
}

static inline void invlpg(uint32_t vaddr) {
    __asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
}

uint32_t create_address_space() {
    uint32_t pd_phys = pmm_alloc_frame();
    uint32_t *pd = (uint32_t *)phys_to_virt(pd_phys);

    memset(pd, 0, PAGE_SIZE);
    
    uint32_t *kpd = (uint32_t *)phys_to_virt(paging_kernel_directory());

    for (int i = KERNEL_PDE_START; i < 1024; i++)
        pd[i] = kpd[i];

    return pd_phys;
}

int paging_map_page(uint32_t pd_phys, uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    uint32_t pd_index = vaddr >> 22;
    uint32_t pt_index = (vaddr >> 12) & 0x3ff;
    uint32_t *pd = pd_virt(pd_phys);

    if (!(pd[pd_index] & PAGE_PRESENT)) {
        uint32_t pt_phys = pmm_alloc_frame();

        if (!pt_phys)
            return 1;

        memset(pt_virt(pt_phys), 0, PAGE_SIZE);

        pd[pd_index] = pt_phys | PAGE_PRESENT | PAGE_RW | (flags & PAGE_USER);
    } else if (flags & PAGE_USER)
        pd[pd_index] |= PAGE_USER;

    uint32_t *pt = pt_virt(pd[pd_index] & ~0xfff);

    pt[pt_index] = (paddr & ~0xfff) | (flags & 0xfff) | PAGE_PRESENT;

    if (pd_phys == paging_get_current_directory())
        invlpg(vaddr);

    return 0;
}

void paging_unmap_page(uint32_t pd_phys, uint32_t vaddr) {
    uint32_t pd_index = vaddr >> 22;
    uint32_t pt_index = (vaddr >> 12) & 0x3ff;
    uint32_t *pd = pd_virt(pd_phys);

    if (!(pd[pd_index] & PAGE_PRESENT))
        return;

    uint32_t *pt = pt_virt(pd[pd_index] & ~0xfff);

    pt[pt_index] = 0;

    if (pd_phys == paging_get_current_directory())
        invlpg(vaddr);
}

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

    if (num_tables > MAX_TABLES)
        num_tables = MAX_TABLES;

    for (int i = 0; i < 1024; i++)
        page_directory[i] = 0;
    
    for (uint32_t t = 0; t < num_tables; t++) {
        for (int i = 0; i < 1024; i++) {
            uint32_t phys_addr = (t * 0x400000) + (i * 0x1000);

            page_tables[t][i] = phys_addr | PAGE_PRESENT | PAGE_RW;
        }

        uint32_t pt_phys = (uint32_t)page_tables[t] - KERNEL_VIRT_BASE;

        page_directory[KERNEL_PDE_START + t] = pt_phys | PAGE_PRESENT | PAGE_RW;
    }

    uint32_t pd_phys = (uint32_t)page_directory - KERNEL_VIRT_BASE;

    load_page_directory(pd_phys);
    enable_paging();

    kernel_directory_phys = pd_phys;
}

uint32_t paging_kernel_directory() {
    return kernel_directory_phys;
}

int paging_walk(uint32_t pd_phys, uint32_t vaddr, uint32_t *paddr_out) {
    uint32_t pd_index = vaddr >> 22;
    uint32_t pt_index = (vaddr >> 12) & 0x3ff;
    uint32_t page_offset = vaddr & 0xfff;
    uint32_t *pd = (uint32_t *)phys_to_virt(pd_phys);

    if (!(pd[pd_index] & PAGE_PRESENT))
        return 1;

    uint32_t *pt = (uint32_t *)phys_to_virt(pd[pd_index] & ~0xfff);

    if (!(pt[pt_index] & PAGE_PRESENT))
        return 1;

    if (paddr_out)
        *paddr_out = (pt[pt_index] & ~0xfff) | page_offset;

    return 0;
}