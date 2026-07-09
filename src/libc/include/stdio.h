#ifndef STDIO_USER_H
#define STDIO_USER_H

#include <stdarg.h>
#include <stddef.h>

int snprintf(char *buf, size_t size, const char *fmt, ...);
int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list args);

#endif