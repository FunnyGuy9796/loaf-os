#include <stdio.h>
#include <string.h>
#include <time.h>

int main(int argc, char *argv[]) {
    (void)argc;
    struct tm now;

    if (gettime(&now) != 0) {
        printf("datetime: failed to get time...\n");

        return 1;
    }

    if (argv[1] && strcmp(argv[1], "-u") == 0)
        printf("%u ticks\n", getuptime());
    else
        printf("%02d:%02d:%02d %02d-%02d-%04d\n",
            now.tm_hour, now.tm_min, now.tm_sec,
            now.tm_mon, now.tm_mday, now.tm_year);

    return 0;
}