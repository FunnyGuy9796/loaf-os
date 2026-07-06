#include <stdint.h>
#include "boot.h"
#include "linker_syms.h"
#include "vga.h"
#include "misc/printf.h"
#include "mm/pmm.h"
#include "mm/paging.h"
#include "mm/heap.h"
#include "idt/idt.h"
#include "devices/pit.h"
#include "devices/kbd.h"
#include "devices/rtc.h"
#include "devices/ata.h"

uint32_t kstart = (uint32_t)&_kernel_start;
uint32_t kend = (uint32_t)&_kernel_end;

void kmain(raw_boot_info_t *raw) {
    term_init();

    uint32_t highest_addr = 0x100000 + ((uint32_t)raw->ext_kb * 1024);
    uint32_t total_frames = highest_addr / PAGE_SIZE;

    pmm_init(raw, kend, highest_addr);
    paging_init(highest_addr);
    heap_init(256);    
    idt_init();
    idt_pic_remap();

    uint8_t mask = inb(PIC2_DATA);

    outb(PIC2_DATA, mask | (1 << 6));

    pit_init(100);
    rtc_init();

    __asm__ volatile ("sti");

    printf("\nloaf-os\n");
    
    rtc_time_t t = rtc_now();

    printf("\n%02d:%02d:%02d %02d-%02d-%04d\n\n", t.hours, t.minutes, t.seconds, t.month, t.day, t.year);

    printf("   /\\_/\\\n");
    printf("  ( o.o )\n");
    printf("   > ^ <\n");
    printf("   )   (\n");
    printf("\nThis is a hobby OS project that aims to revive the original 32-bit CPU, the 80386. This is inteded to just be a fun learning experience and something I can enjoy working on.\n");

    for (;;) {
        char c = kbd_getchar();

        if (c >= 0)
            printf("%c", c);
        
        __asm__ volatile ("hlt");
    }
}