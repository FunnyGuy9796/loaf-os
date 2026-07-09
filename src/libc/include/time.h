#ifndef TIME_H
#define TIME_H

#include <stdint.h>

struct tm {
    uint8_t tm_sec;
    uint8_t tm_min;
    uint8_t tm_hour;
    uint8_t tm_mday;
    uint8_t tm_mon;
    uint16_t tm_year;
};

int gettime(struct tm *out);

#endif