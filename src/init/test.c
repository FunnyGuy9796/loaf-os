#include <stdio.h>
#include <unistd.h>
#include <string.h>

char *msg = "this is new text";

int main() {
    int fd = open("test.txt", 1);

    write(fd, (void *)msg, strlen(msg));
    close(fd);

    for (int i = 0; i < 15; i++) {
        printf("%d\n", i);
        usleep(1000);
    }

    printf("\n");

    return 0;
}