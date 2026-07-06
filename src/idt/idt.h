#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xa0
#define PIC2_DATA 0xa1

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} idt_descriptor_t;

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t type_attr);
void idt_pic_remap();
void idt_init();

#endif