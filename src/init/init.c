#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

int main() {
    struct tm now;

    if (gettime(&now) != 0) {
        printf("failed to get time\n");

        return 1;
    }

    printf("\nloaf-os\n\n%02d:%02d:%02d %02d-%02d-%04d\n\n",
           now.tm_hour, now.tm_min, now.tm_sec,
           now.tm_mon, now.tm_mday, now.tm_year);
    
    printf("   /\\_/\\\n");
    printf("  ( o.o )\n");
    printf("   > ^ <\n");
    printf("   )   (\n\n");

    struct stat st;

    if (stat("hello.txt", &st) != 0) {
        printf("stat failed on 'hello.txt'\n");

        return 1;
    }

    char *bytes = (char *)malloc(st.size + 1);

    if (!bytes) {
        printf("out of memory\n");

        return 1;
    }

    int fd = open("hello.txt", 0);
    int n = read(fd, (void *)bytes, st.size);

    close(fd);

    if (n <= 0) {
        printf("read failed on 'hello.txt'\n");

        return 1;
    }

    bytes[n] = '\0';

    printf("%s\n\n", bytes);
    free(bytes);

    int cpid = spawn("test.bin");

    if (cpid < 0) {
        printf("failed to spawn test.bin\n");

        return -1;
    }

    if (wait(cpid) < 0) {
        printf("failed to wait on child process\n");

        return -1;
    }

    if (stat("test.txt", &st) != 0) {
        printf("stat failed on 'test.txt'\n");

        return 1;
    }

    bytes = (char *)malloc(st.size + 1);

    if (!bytes) {
        printf("out of memory\n");

        return 1;
    }

    fd = open("test.txt", 0);
    n = read(fd, (void *)bytes, st.size);

    close(fd);

    if (n <= 0) {
        printf("read failed on 'test.txt'\n");

        return 1;
    }

    bytes[n] = '\0';

    printf("%s\n\n", bytes);
    free(bytes);

    unlink("test.txt");

    if (stat("test.txt", &st) == 0) {
        printf("'test.txt' not deleted\n");

        return 1;
    }

    printf("'test.txt' deleted successfully\n");

    return 0;
}