#include "include/sys/stat.h"
#include "include/syscall.h"

int stat(const char *path, struct stat *out) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_STAT), "b"(path), "c"(out)
        : "memory"
    );

    return ret;
}