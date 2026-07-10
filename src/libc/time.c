#include "include/time.h"
#include "include/syscall.h"

int gettime(struct tm *out) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_DATETIME), "b"(out)
        : "memory"
    );

    return ret;
}

uint32_t getuptime() {
    uint32_t ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETUPTIME)
        : "memory"
    );

    return ret;
}