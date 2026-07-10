#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include "fat.h"

#define MAX_OPEN_FILES 128
#define MAX_FDS_PER_PROC 16
#define MAX_DIRS_PER_PROC 8

typedef struct {
    int in_use;
    char path[64];
    uint32_t offset;
    uint32_t size;
    int writable;
} open_file_t;

typedef struct {
    uint16_t cluster;
    uint32_t index;
    int in_use;
} dir_handle_t;

extern open_file_t open_files[MAX_OPEN_FILES];

int file_open(const char *path, int writable);
void file_close(int idx);
int file_read(int idx, void *buf, uint32_t len);
int file_write(int idx, const void *buf, uint32_t len);

#endif