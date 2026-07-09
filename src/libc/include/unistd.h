#ifndef UNISTD_H
#define UNISTD_H

#include <stdint.h>
#include <stddef.h>

int open(const char *path, int writable);
int close(int fd);
int read(int fd, void *buf, uint32_t len);
int write(int fd, const void *buf, uint32_t len);
int unlink(const char *path);
int mkdir(const char *path);
int rmdir(const char *path);
int spawn(const char *path);
int kill(uint32_t pid);
int wait(uint32_t pid);
uint32_t usleep(uint32_t ms);

#endif