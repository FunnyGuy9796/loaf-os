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
        for (process_t *proc = proc_list; proc != NULL; proc = proc->next) {
            if (proc->state == PROC_DEAD) {
                process_cleanup(proc);

                continue;
            }

            if (proc->state != PROC_BLOCKED || proc->block != PROC_SLEEP)
                continue;

            if (tick_count < proc->resume_tick)
                continue;

            proc->state = PROC_READY;
            proc->block = PROC_NONE;
            proc->resume_tick = (uint32_t)-1;
        }
    }
}

uint32_t pit_ticks() {
    return tick_count;
}