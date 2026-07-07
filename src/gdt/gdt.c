#include "gdt.h"
#include "../misc/mem.h"

#define GDT_ENTRIES 6

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_descriptor gdt_desc;
static tss_entry_t tss __attribute__((aligned(16)));

extern void gdt_flush(uint32_t);
extern void tss_flush();

static void gdt_set(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_low = base & 0xffff;
    gdt[i].base_mid = (base >> 16) & 0xff;
    gdt[i].base_high = (base >> 24) & 0xff;
    gdt[i].limit_low = limit & 0xffff;
    gdt[i].granularity = ((limit >> 16) & 0x0f) | (gran & 0xf0);
    gdt[i].access = access;
}

void gdt_init() {
    gdt_set(0, 0, 0, 0, 0);
    gdt_set(1, 0, 0xfffff, 0x9a, 0xcf);
    gdt_set(2, 0, 0xfffff, 0x92, 0xcf);
    gdt_set(3, 0, 0xfffff, 0xfa, 0xcf);
    gdt_set(4, 0, 0xfffff, 0xf2, 0xcf);

    memset(&tss, 0, sizeof(tss));

    uint32_t tss_base = (uint32_t)&tss;
    uint32_t tss_limit = tss_base + sizeof(tss) - 1;

    gdt_set(5, tss_base, tss_limit, 0x89, 0x00);

    gdt_desc.limit = sizeof(gdt) - 1;
    gdt_desc.base  = (uint32_t)&gdt;

    gdt_flush((uint32_t)&gdt_desc);
    tss_flush();
}

void tss_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
    tss.ss0 = 0x10;
}