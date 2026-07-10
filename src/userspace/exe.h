#ifndef EXE_H
#define EXE_H

#include <stdint.h>

#define EXE_MAGIC 0x46414f4c

typedef struct process process_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t entry_point;
    uint32_t code_vaddr;
    uint32_t code_size;
    uint32_t data_vaddr;
    uint32_t data_size;
    uint32_t bss_vaddr;
    uint32_t bss_size;
} exe_header_t;

int exec_load(const char *path, process_t *proc);

#endif