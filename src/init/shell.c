#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/path.h>

#define MAX_BUF 128
#define MAX_ARGS 16

static char buffer[MAX_BUF];
static int buf_index = 0;
static char path_buf[MAX_BUF + 6];
static char cwd_display[MAX_BUF] = "/";

static int parse_args(char *line, char *argv[], int max_args) {
    int argc = 0;
    char *p = line;

    while (*p && argc < max_args) {
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '\0')
            break;

        argv[argc++] = p;

        while (*p && *p != ' ' && *p != '\t')
            p++;

        if (*p) {
            *p = '\0';
            p++;
        }
    }

    return argc;
}

static const char *resolve_bin_path(const char *name) {
    if (name[0] == '/')
        return name;

    size_t len = strlen(name);

    if (len > MAX_BUF)
        len = MAX_BUF;

    memcpy(path_buf, "/bin/", 5);
    memcpy(path_buf + 5, name, len);

    path_buf[5 + len] = '\0';

    return path_buf;
}

static const char *resolve_exec_path(const char *name) {
    if (strchr(name, '/') != NULL) {
        static char exec_resolved[MAX_BUF];

        path_resolve_cwd(name, exec_resolved, sizeof(exec_resolved));

        return exec_resolved;
    }

    return resolve_bin_path(name);
}

static void prompt() {
    buf_index = 0;

    getcwd(cwd_display, (int)MAX_BUF);
    memset(&buffer, 0, sizeof(buffer));
    printf("loaf-os:%s> ", cwd_display);
}

int main() {
    struct tm now;

    if (gettime(&now) != 0) {
        printf("failed to get time\n");

        return 1;
    }

    printf("\x1b[2J\x1b[Hloaf-os\n\n%02d:%02d:%02d %02d-%02d-%04d\n\n",
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

    prompt();

    for (;;) {
        int c = getchar(BLOCK);

        if (c > -1) {
            printf("%c", c);

            if (c == '\n') {
                char *argv[MAX_ARGS];
                int argc = parse_args(buffer, argv, MAX_ARGS);
                char *last_arg = argv[argc - 1];
                
                if (argc == 0)
                    prompt();
                else if (strcmp(argv[0], "cd") == 0) {
                    const char *target = argc >= 2 ? argv[1] : "/";
                    char resolved[MAX_BUF];

                    path_resolve_cwd(target, resolved, sizeof(resolved));

                    struct stat st;

                    if (stat(resolved, &st) != 0)
                        printf("cd: no such directory: %s\n", resolved);
                    else if (!(st.attr & 0x10))
                        printf("cd: not a directory: %s\n", resolved);
                    else
                        chdir(resolved);
                    
                    prompt();
                } else {
                    int pid = spawn(resolve_exec_path(argv[0]), argc, argv);

                    if (pid < 0)
                        printf("command not found...");
                    else if (strcmp(last_arg, "&") != 0)
                        wait(pid);
                    
                    prompt();
                }
            } else if (c == '\b') {
                if (buf_index > 0) {
                    buf_index--;
                    buffer[buf_index] = '\0';
                } else
                    printf(" ");
            } else if (buf_index < MAX_BUF - 1)
                buffer[buf_index++] = c;
        }
    }

    return 0;
}