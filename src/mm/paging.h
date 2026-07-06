#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2
#define PAGE_USER 0x4
#define PAGE_SIZE 4096
#define MAX_TABLES 17

void paging_init(uint32_t highest_addr);

#endif