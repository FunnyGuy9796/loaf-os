#include "include/syscall.h"
#include "include/syscall_api.h"

void sys_exit(int code) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a"(SYS_EXIT), "b"(code)
        : "memory"
    );

    for (;;) {}
}

int sys_yield_hlt() {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_YIELD_HLT)
        : "memory"
    );

    return ret;
}

int sys_print(const char *msg) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_PRINT), "b"(msg)
        : "memory"
    );

    return ret;
}

void *sys_sbrk(int32_t delta) {
    uint32_t ret;
    
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SBRK), "b"(delta)
        : "memory"
    );

    return (ret == (uint32_t)-1) ? (void *)0 : (void *)ret;
}