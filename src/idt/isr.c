#include "isr.h"
#include "idt.h"
#include "../devices/pit.h"
#include "../devices/kbd.h"
#include "../userspace/syscalls.h"
#include "../multi/process.h"
#include "../misc/printf.h"

static inline uint32_t get_cr2() {
    uint32_t cr2;

    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    return cr2;
}

static void handle_fault(registers_t *regs) {
    if ((regs->cs & 0x3) == 3) {
        printf("[E] process pid=%d faulted: int=%d err=0x%x eip=0x%x cr2=0x%x\n",
            curr_proc ? (int32_t)curr_proc->pid : -1,
            regs->int_no, regs->err_code, regs->eip,
            regs->int_no == 14 ? get_cr2() : 0);
        
        curr_proc->state = PROC_DEAD;

        process_t *proc = proc_list;

        do {
            if (proc->target_pid == curr_proc->pid && proc->state == PROC_BLOCKED) {
                proc->state = PROC_READY;
                proc->target_pid = -1;
            }

            proc = proc->next;
        } while (proc != NULL);

        schedule(regs);
    } else
        panic("kernel fault: int=%d err=0x%x eip=0x%x\n", regs->int_no, regs->err_code, regs->eip);
}

void isr_handler(registers_t *regs) {
    switch (regs->int_no) {
        case 32: {
            pit_handler();
            outb(PIC1_COMMAND, 0x20);
            schedule(regs);

            break;
        }

        case 33: {
            kbd_handler();
            outb(PIC1_COMMAND, 0x20);

            break;
        }

        case 128: {
            syscall_dispatch(regs);

            break;
        }

        case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
        case 8: case 9: case 10: case 11: case 12: case 13: case 14:
        case 16: case 17: case 18: case 19: {
            handle_fault(regs);

            break;
        }

        default:
            printf("unhandled interrupt: %d\n", regs->int_no);
    }
}