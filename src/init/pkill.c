#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    (void)argc;

    if (!argv[1]) {
        printf("Usage: 'pkill <pid>'\n");

        return 1;
    }

    uint32_t pid = (uint32_t)atoi(argv[1]);

    if (kill(pid) < 0) {
        printf("failed to kill PID %d\n", pid);

        return 1;
    }

    return 0;
}