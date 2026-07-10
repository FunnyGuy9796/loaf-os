#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/path.h>

#define MAX_BUF 128

int main(int argc, char *argv[]) {
    (void)argc;

    char resolved[MAX_BUF];
    const char *input = argv[1] ? argv[1] : ".";

    path_resolve_cwd(input, resolved, sizeof(resolved));

    int fd = opendir(resolved);

    if (fd < 0) {
        printf("ls: cannot open %s\n", resolved);
        return 1;
    }

    fat_dirent_info_t entry;

    while (readdir(fd, &entry) == 1)
        printf("%s\n", entry.name);

    closedir(fd);

    return 0;
}