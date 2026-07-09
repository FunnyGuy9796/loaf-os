#include "pit.h"
#include "../misc/util.h"
#include "../multi/process.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND 0x43

static volatile uint32_t tick_count = 0;

void pit_init(uint32_t frequency) {
    uint32_t divisor = 1193182 / frequency;

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xff);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xff);
}

void pit_handler() {
    tick_count++;

    if (scheduler_ready) {
        process_t *proc = proc_list;

        do {
            if (proc->state == PROC_BLOCKED && tick_count >= proc->resume_tick) {
                proc->state = PROC_READY;
                proc->resume_tick = -1;
            }

            proc = proc->next;
        } while (proc != NULL);
    }
}

uint32_t pit_ticks() {
    return tick_count;
}