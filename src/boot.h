#ifndef BOOT_H
#define BOOT_H

#include <stdint.h>

#define MEM_USABLE 1
#define MEM_RESERVED 2

typedef struct {
    uint32_t base;
    uint32_t length;
    uint8_t type;
} mem_region_t;

typedef struct __attribute__((packed)) {
    uint16_t conv_kb;
    uint16_t ext_kb;
    uint16_t ebda_segment;
} raw_boot_info_t;

typedef struct {
    uint16_t conv_mem;
    uint16_t ext_mem;
    uint32_t ebda_start;
    mem_region_t regions[8];
    uint8_t region_count;
} boot_info_t;

#endif