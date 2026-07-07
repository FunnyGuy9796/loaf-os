#ifndef GDT_H
#define GDT_H

#include <stdint.h>

struct __attribute__((packed)) gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
};

struct __attribute__((packed)) gdt_descriptor {
    uint16_t limit;
    uint32_t base;
};

typedef struct __attribute__((packed)) {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1, ss1, esp2, ss2;
    uint32_t cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} tss_entry_t;

void gdt_init();
void tss_set_kernel_stack(uint32_t esp0);

#endif