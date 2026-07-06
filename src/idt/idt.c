#include "idt.h"
#include "../misc/util.h"

static idt_entry_t idt[256];
static idt_descriptor_t idt_desc;

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t type_attr) {
    idt[num].offset_low = handler & 0xffff;
    idt[num].offset_high = (handler >> 16) & 0xffff;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].type_attr = type_attr;
}

void idt_pic_remap() {
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, 0x00);
    outb(PIC2_DATA, 0x00);
}

extern void isr32();
extern void isr33();

void idt_init() {
    idt_set_gate(32, (uint32_t)isr32, 0x08, 0x8e);
    idt_set_gate(33, (uint32_t)isr33, 0x08, 0x8e);

    idt_desc.limit = sizeof(idt) - 1;
    idt_desc.base = (uint32_t)&idt;

    __asm__ volatile ("lidt %0" :: "m"(idt_desc));
}