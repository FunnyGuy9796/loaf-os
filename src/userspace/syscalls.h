#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "../idt/isr.h"

void syscall_dispatch(registers_t *regs);

#endif