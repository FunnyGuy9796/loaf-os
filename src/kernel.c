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
#include "fs/fat.h"
#include "multi/process.h"

extern uint32_t stack_top;
extern void enter_usermode(uint32_t entry, uint32_t user_stack, uint32_t pd_phys);

void kmain(raw_boot_info_t *raw) {
    gdt_init();
    tss_set_kernel_stack((uint32_t)&stack_top);

    uint16_t ext_kb = raw->ext_kb;
    uint32_t highest_addr = 0x100000 + ((uint32_t)ext_kb * 1024);
    uint32_t kend_phys = (uint32_t)&_kernel_end - KERNEL_VIRT_BASE;

    pmm_init(raw, kend_phys, highest_addr);
    paging_init(highest_addr);
    term_init();
    heap_init(256);
    idt_init();
    idt_pic_remap();

    uint8_t mask = inb(PIC2_DATA);

    outb(PIC2_DATA, mask | (1 << 6));

    pit_init(1000);
    rtc_init();
    
    if (fat_init())
        panic("kernel.c: kmain() -> fat_init() failed\n");

    __asm__ volatile ("sti");
    
    int err = 0;

    char *idle_argv[] = { "idle.bin" };
    int idle_argc = 1;

    process_t *idle = process_create("/bin/idle", idle_argc, idle_argv, &err);

    if (!idle)
        panic("kernel.c: kmain() -> idle process not found\n");

    char *init_argv[] = { "init.bin" };
    int init_argc = 1;

    process_t *init = process_create("/bin/init", init_argc, init_argv, &err);

    if (!init)
        panic("kernel.c: kmain() -> init process not found\n");
    
    scheduler_add(init);

    idle_proc = idle;
    scheduler_ready = 1;

    process_run(init);
}