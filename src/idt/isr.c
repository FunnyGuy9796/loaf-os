#include "isr.h"
#include "idt.h"
#include "../devices/pit.h"
#include "../devices/kbd.h"
#include "../misc/printf.h"

void isr_handler(registers_t *regs) {
    switch (regs->int_no) {
        case 32: {
            pit_handler();
            outb(PIC1_COMMAND, 0x20);

            break;
        }

        case 33: {
            kbd_handler();
            outb(PIC1_COMMAND, 0x20);

            break;
        }

        default:
            printf("Unhandled interrupt: %d\n", regs->int_no);
    }
}