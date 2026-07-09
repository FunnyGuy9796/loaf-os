#ifndef SYSCALL_API_H
#define SYSCALL_API_H

#include <stdint.h>

void sys_exit(int code);
int sys_yield_hlt();
int sys_print(const char *msg);
void *sys_sbrk(int32_t delta);

#endif