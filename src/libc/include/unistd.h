#ifndef UNISTD_H
#define UNISTD_H

#include <stdint.h>
#include <stddef.h>

#define O_RONLY 0
#define O_RW 1
#define NONBLOCK 0
#define BLOCK 1

#define MAX_PROC_NAME 32

typedef enum {
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_DEAD
} proc_state_t;

typedef struct {
    uint32_t pid;
    uint32_t ppid;
    proc_state_t state;
    char name[MAX_PROC_NAME];
} proc_info_t;

typedef struct {
    char name[13];
    uint32_t size;
    uint16_t first_cluster;
    uint8_t attr;
} fat_dirent_info_t;

int open(const char *path, int writable);
int close(int fd);
int read(int fd, void *buf, uint32_t len);
int write(int fd, const void *buf, uint32_t len);
int unlink(const char *path);
int mkdir(const char *path);
int rmdir(const char *path);
int opendir(const char *path);
int readdir(int fd, fat_dirent_info_t *out);
int closedir(int fd);
int chdir(const char *path);
int getcwd(char *buf, uint32_t max);
int spawn(const char *path, int argc, char *const argv[]);
int kill(uint32_t pid);
int wait(uint32_t pid);
uint32_t usleep(uint32_t ms);
int ps(proc_info_t *buf, int max);
int getchar(int blocking);

#endif