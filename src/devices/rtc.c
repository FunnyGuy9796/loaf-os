#include "rtc.h"
#include "pit.h"
#include "../misc/util.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x02
#define RTC_HOURS 0x04
#define RTC_DAY 0x07
#define RTC_MONTH 0x08
#define RTC_YEAR 0x09
#define RTC_STATUS_A 0x0a
#define RTC_STATUS_B 0x0b
#define RTC_RESYNC_INTERVAL_SEC 60

rtc_time_t boot_time = {0};
uint32_t boot_epoch = 0;
uint32_t boot_ticks = 0;

static const uint16_t days_in_month[2][12] = {
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);

    return inb(CMOS_DATA);
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0f) + ((bcd >> 4) * 10);
}

static int is_leap(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static uint32_t days_since_epoch_to_year(uint16_t year) {
    uint32_t days = 0;

    for (uint16_t y = 1970; y < year; y++)
        days += is_leap(y) ? 366 : 365;

    return days;
}

rtc_time_t rtc_read() {
    while (cmos_read(RTC_STATUS_A) & 0x80);

    rtc_time_t t;

    t.seconds = cmos_read(RTC_SECONDS);
    t.minutes = cmos_read(RTC_MINUTES);
    t.hours = cmos_read(RTC_HOURS);
    t.day = cmos_read(RTC_DAY);
    t.month = cmos_read(RTC_MONTH);
    t.year = cmos_read(RTC_YEAR);

    uint8_t status_b = cmos_read(RTC_STATUS_B);

    if (!(status_b & 0x04)) {
        t.seconds = bcd_to_bin(t.seconds);
        t.minutes = bcd_to_bin(t.minutes);
        t.hours = bcd_to_bin(t.hours);
        t.day = bcd_to_bin(t.day);
        t.month = bcd_to_bin(t.month);
        t.year = bcd_to_bin(t.year);
    }

    t.year += 2000;

    return t;
}

uint32_t rtc_to_epoch(rtc_time_t *t) {
    uint32_t days = days_since_epoch_to_year(t->year);
    int leap = is_leap(t->year);

    for (uint8_t m = 1; m < t->month; m++)
        days += days_in_month[leap][m - 1];

    days += t->day - 1;

    return days * 86400 + t->hours   * 3600 + t->minutes * 60 + t->seconds;
}

rtc_time_t epoch_to_rtc(uint32_t epoch) {
    rtc_time_t t;

    t.seconds = epoch % 60; epoch /= 60;
    t.minutes = epoch % 60; epoch /= 60;
    t.hours = epoch % 24; epoch /= 24;

    uint32_t days = epoch;
    uint16_t year = 1970;

    while (1) {
        uint16_t days_in_year = is_leap(year) ? 366 : 365;

        if (days < days_in_year)
            break;

        days -= days_in_year;
        year++;
    }

    t.year = year;

    int leap = is_leap(year);
    uint8_t month = 1;

    while (days >= days_in_month[leap][month - 1]) {
        days -= days_in_month[leap][month - 1];
        month++;
    }

    t.month = month;
    t.day = days + 1;

    return t;
}

uint32_t rtc_now_epoch() {
    uint32_t elapsed_ticks = pit_ticks() - boot_ticks;
    uint32_t elapsed_sec = elapsed_ticks / 100;

    if (elapsed_sec >= RTC_RESYNC_INTERVAL_SEC) {
        boot_time = rtc_read();
        boot_epoch = rtc_to_epoch(&boot_time);
        boot_ticks = pit_ticks();

        return boot_epoch;
    }

    return boot_epoch + elapsed_sec;
}

void rtc_init() {
    boot_time = rtc_read();
    boot_epoch = rtc_to_epoch(&boot_time);
    boot_ticks = pit_ticks();
}

rtc_time_t rtc_now() {
    uint32_t epoch = rtc_now_epoch();

    return epoch_to_rtc(epoch);
}