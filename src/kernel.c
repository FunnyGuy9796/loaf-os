#include <stdint.h>
#include "boot.h"
#include "arch.h"
#include "linker_syms.h"
#include "vga.h"
#include "misc/printf.h"
#include "misc/mem.h"
#include "mm/pmm.h"
#include "mm/paging.h"
#include "mm/heap.h"
#include "gdt/gdt.h"
#include "idt/idt.h"
#include "devices/pit.h"
#include "devices/kbd.h"
#include "devices/rtc.h"
#include "devices/ata.h"
#include "fat.h"
#include "multi/process.h"

extern uint32_t stack_top;
extern void enter_usermode(uint32_t entry, uint32_t user_stack, uint32_t pd_phys);

void kmain(raw_boot_info_t *raw) {
    gdt_init();
    tss_set_kernel_stack((uint32_t)&stack_top);

    uint16_t ext_kb = raw->ext_kb;
    uint32_t highest_addr = 0x100000 + ((uint32_t)ext_kb * 1024);
    uint32_t total_frames = highest_addr / PAGE_SIZE;
    uint32_t kend_phys = (uint32_t)&_kernel_end - KERNEL_VIRT_BASE;

    pmm_init(raw, kend_phys, highest_addr);
    paging_init(highest_addr);
    term_init();
    heap_init(256);
    idt_init();
    idt_pic_remap();

    uint8_t mask = inb(PIC2_DATA);

    outb(PIC2_DATA, mask | (1 << 6));

    pit_init(100);
    rtc_init();
    
    if (fat_init())
        panic("kernel.c: kmain() -> fat_init() failed\n");

    __asm__ volatile ("sti");

    printf("\nloaf-os\n");
    
    rtc_time_t t = rtc_now();

    printf("\n%02d:%02d:%02d %02d-%02d-%04d\n\n", t.hours, t.minutes, t.seconds, t.month, t.day, t.year);

    printf("   /\\_/\\\n");
    printf("  ( o.o )\n");
    printf("   > ^ <\n");
    printf("   )   (\n\n");

    fat_stat_t st;
    uint32_t size;
    uint32_t buf;

    if (fat_stat("hello.txt", &st) == 0)
        buf = kmalloc(fat_read_alloc_size(st.size));
    else
        panic("kernel.c: kmain() -> hello.txt not found\n");

    if (fat_read_file("hello.txt", (void *)buf, &size))
        panic("kernel.c: kmain() -> read failed\n");
    else
        printf("%s\n\n", (char *)buf);
    
    int err = 0;
    process_t *init = process_create("init.bin", &err);

    if (!init) {
        __asm__ volatile ("cli");

        for (;;)
            __asm__ volatile ("hlt");
    }
    
    printf("loaded init.bin, pid=%d, entry=0x%x\n", init->pid, init->entry_point);

    process_run(init);
}