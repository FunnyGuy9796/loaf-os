#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/path.h>
#include <stdlib.h>

#define MAX_BUF 128

int main(int argc, char *argv[]) {
    (void)argc;

    if (!argv[1]) {
        printf("Usage: 'cat <filename>'");

        return 1;
    }

    char resolved[MAX_BUF];

    path_resolve_cwd(argv[1], resolved, sizeof(resolved));

    struct stat st;
    int status = stat(resolved, &st);

    if (status < 0) {
        printf("cat: '%s' not found...", resolved);

        return 1;
    }

    void *buf = malloc(st.size);
    int fd = open(resolved, O_RONLY);

    if (fd < 0) {
        printf("cat: unable to open '%s'...", resolved);

        return 1;
    }

    status = read(fd, buf, st.size);

    if (status <= 0) {
        printf("cat: unable to read '%s'", resolved);

        return 1;
    }

    printf("%s\n", (const char *)buf);

    return 0;
}