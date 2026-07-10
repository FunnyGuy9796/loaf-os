#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

int main() {
    struct stat st;

    if (stat("/bin/shell", &st) < 0) {
        printf("'/bin/shell' not found\n");

        return -1;
    }

    for (;;) {
        printf("%u: /bin/shell\n", getuptime());

        int shell_pid = spawn("/bin/shell", 0, NULL);

        if (shell_pid < 0) {
            printf("failed to spawn shell\n");

            return -1;
        }

        int exit_code = wait(shell_pid);

        printf("shell exited (code %d), restarting...\n", exit_code);
    }

    return 0;
}