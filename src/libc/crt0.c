#include "include/syscall_api.h"

extern int main();

void _start() {
    int ret = main();

    sys_exit(ret);

    for (;;) {}
}