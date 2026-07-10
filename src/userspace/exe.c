#include "exe.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../mm/paging.h"
#include "../fs/fat.h"
#include "../misc/mem.h"
#include "../multi/process.h"

#define USER_STACK_TOP 0xbffff000
#define USER_STACK_SIZE 0x4000
#define USER_HEAP_MAX (USER_STACK_TOP - USER_STACK_SIZE - 0x1000)

static int map_and_copy(uint32_t pd_phys, uint32_t vaddr, const void *src, uint32_t size, uint32_t flags) {
    uint32_t start = vaddr & ~0xfff;
    uint32_t end = (vaddr + size + 0xfff) & ~0xfff;
    const uint8_t *s = (const uint8_t *)src;

    for (uint32_t va = start; va < end; va += PAGE_SIZE) {
        uint32_t frame;
        uint32_t existing_phys;

        if (paging_walk(pd_phys, va, &existing_phys) == 0) {
            frame = existing_phys & ~0xfff;

            if (flags & PAGE_RW)
                paging_map_page(pd_phys, va, frame, flags);
        } else {
            frame = pmm_alloc_frame();

            if (!frame)
                return 1;

            if (paging_map_page(pd_phys, va, frame, flags))
                return 1;

            memset(phys_to_virt(frame), 0, PAGE_SIZE);
        }

        uint32_t page_off = (va == start) ? (vaddr - start) : 0;
        uint32_t copy_len = PAGE_SIZE - page_off;
        uint32_t remaining = size - (uint32_t)(s - (const uint8_t *)src);

        if (remaining < copy_len)
            copy_len = remaining;

        if (src && copy_len > 0)
            memcpy((uint8_t *)phys_to_virt(frame) + page_off, s, copy_len);

        s += copy_len;
    }

    return 0;
}

int exec_load(const char *path, process_t *proc) {
    fat_stat_t st;

    if (fat_stat(path, &st))
        return 1;

    uint32_t file_buf = kmalloc(fat_read_alloc_size(st.size));

    if (!file_buf)
        return 2;

    uint32_t size;

    if (fat_read_file(path, (void *)file_buf, &size))
        return 3;

    exe_header_t *hdr = (exe_header_t *)file_buf;

    if (hdr->magic != EXE_MAGIC)
        return 4;

    uint32_t pd_phys = paging_create_address_space();

    if (hdr->code_size && map_and_copy(pd_phys, hdr->code_vaddr, (uint8_t *)file_buf + sizeof(exe_header_t), hdr->code_size, PAGE_PRESENT | PAGE_USER))
        return 5;

    if (hdr->data_size && map_and_copy(pd_phys, hdr->data_vaddr, (uint8_t *)file_buf + sizeof(exe_header_t) + hdr->code_size, hdr->data_size, PAGE_PRESENT | PAGE_RW | PAGE_USER))
        return 6;

    if (hdr->bss_size && map_and_copy(pd_phys, hdr->bss_vaddr, NULL, hdr->bss_size, PAGE_PRESENT | PAGE_RW | PAGE_USER))
        return 7;

    if (map_and_copy(pd_phys, USER_STACK_TOP - USER_STACK_SIZE, NULL, USER_STACK_SIZE, PAGE_PRESENT | PAGE_RW | PAGE_USER))
        return 8;

    kfree(file_buf);

    proc->pd_phys = pd_phys;
    proc->entry_point = hdr->entry_point;
    proc->user_stack_top = USER_STACK_TOP;

    uint32_t heap_start = hdr->bss_vaddr + hdr->bss_size;

    heap_start = (heap_start + 0xfff) & ~0xfff;

    proc->heap_start = heap_start;
    proc->heap_break = heap_start;
    proc->heap_max = USER_HEAP_MAX;

    return 0;
}