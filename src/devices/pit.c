#include "pit.h"
#include "../misc/util.h"

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
}

uint32_t pit_ticks() {
    return tick_count;
}