#ifndef PRINTF_H
#define PRINTF_H

#include <stdarg.h>
#include "util.h"
#include "../vga.h"

int snprintf(char *buf, size_t size, const char *fmt, ...);
int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list args);
void panic(const char *fmt, ...);

#endif