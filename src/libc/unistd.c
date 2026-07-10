#include "include/unistd.h"
#include "include/syscall.h"

int open(const char *path, int writable) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_OPEN), "b"(path), "c"(writable)
        : "memory"
    );

    return ret;
}

int close(int fd) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CLOSE), "b"(fd)
        : "memory"
    );

    return ret;
}

int read(int fd, void *buf, uint32_t len) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(len)
        : "memory"
    );

    return ret;
}

int write(int fd, const void *buf, uint32_t len) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(fd), "c"(buf), "d"(len)
        : "memory"
    );

    return ret;
}

int unlink(const char *path) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_UNLINK), "b"(path)
        : "memory"
    );

    return ret;
}

int mkdir(const char *path) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_MKDIR), "b"(path)
        : "memory"
    );

    return ret;
}

int rmdir(const char *path) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_RMDIR), "b"(path)
        : "memory"
    );

    return ret;
}

int opendir(const char *path) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_OPENDIR), "b"(path)
        : "memory"
    );

    return ret;
}

int readdir(int fd, fat_dirent_info_t *out) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READDIR), "b"(fd), "c"(out)
        : "memory"
    );

    return ret;
}

int closedir(int fd) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CLOSEDIR), "b"(fd)
        : "memory"
    );

    return ret;
}

int chdir(const char *path) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CHDIR), "b"(path)
        : "memory"
    );

    return ret;
}

int getcwd(char *buf, uint32_t max) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETCWD), "b"(buf), "c"(max)
        : "memory"
    );

    return ret;
}

int spawn(const char *path, int argc, char *const argv[]) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SPAWN), "b"(path), "c"(argc), "d"(argv)
        : "memory"
    );

    return ret;
}

int kill(uint32_t pid) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_KILL), "b"(pid)
        : "memory"
    );

    return ret;
}

int wait(uint32_t pid) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WAIT), "b"(pid)
        : "memory"
    );

    return ret;
}

uint32_t usleep(uint32_t ms) {
    uint32_t ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SLEEP), "b"(ms)
        : "memory"
    );

    return ret;
}

int ps(proc_info_t *buf, int max) {
    int ret;

    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_PS), "b"(buf), "c"(max)
        : "memory"
    );

    return ret;
}

int getchar(int blocking) {
    int ret;

    if (blocking) {
        __asm__ volatile (
            "int $0x80"
            : "=a"(ret)
            : "a"(SYS_GETCHAR)
            : "memory"
        );
    } else {
        __asm__ volatile (
            "int $0x80"
            : "=a"(ret)
            : "a"(SYS_GETCHAR_NB)
            : "memory"
        );
    }

    return ret;
}