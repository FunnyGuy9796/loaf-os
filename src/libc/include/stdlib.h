#ifndef STDLIB_USER_H
#define STDLIB_USER_H

#include <stdint.h>

void itoa(int value, char *buf, int base);
void utoa(uint32_t value, char *buf, int base);
int atoi(const char *str);
void *malloc(uint32_t size);
void free(void *ptr);

#endif