#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
    int count = ps(NULL, 0);
    proc_info_t *procs = (proc_info_t *)malloc(count * sizeof(proc_info_t));
    int written = ps(procs, count);

    printf("PID  PPID STATE    NAME\n");
    printf("---- ---- -------- --------\n");

    for (int i = 0; i < written; i++) {
        char *state;

        switch (procs[i].state) {
            case 0: {
                state = "ready";

                break;
            }

            case 1: {
                state = "running";

                break;
            }

            case 2: {
                state = "blocked";

                break;
            }

            case 3: {
                state = "dead";

                break;
            }
        }

        printf("%-4d %-4d %-8s %s\n", procs[i].pid, procs[i].ppid, state, procs[i].name);
    }

    return 0;
}