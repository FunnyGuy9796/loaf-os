#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

size_t strlen(const char *str);
size_t strnlen(const char *str, size_t maxlen);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
void itoa(int value, char *buf, int base);
void utoa(uint32_t value, char *buf, int base);

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;

    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));

    return value;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;

    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));

    return value;
}

#endif