#ifndef SYS_STAT_H
#define SYS_STAT_H

#include <stdint.h>

struct stat {
    char name[13];
    uint32_t size;
    uint16_t first_cluster;
    uint8_t attr;
};

int stat(const char *path, struct stat *out);

#endif