#include <stdint.h>
#include "include/stdlib.h"

void itoa(int value, char *buf, int base) {
    char tmp[32];
    char digits[] = "0123456789abcdef";
    int i = 0;
    int neg = 0;

    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        
        return;
    }

    if (value < 0 && base == 10) {
        neg = 1;
        value = -value;
    }

    while (value > 0) {
        tmp[i++] = digits[value % base];
        value /= base;
    }

    if (neg)
        tmp[i++] = '-';
    
    int j = 0;

    while (i > 0)
        buf[j++] = tmp[--i];
    
    buf[j] = '\0';
}

void utoa(uint32_t value, char *buf, int base) {
    char tmp[64];
    char digits[] = "0123456789abcdef";
    int i = 0;

    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';

        return;
    }

    while (value > 0) {
        tmp[i++] = digits[value % base];
        value /= base;
    }

    int j = 0;

    while (i > 0)
        buf[j++] = tmp[--i];
    
    buf[j] = '\0';
}