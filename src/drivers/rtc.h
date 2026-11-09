/*
 * rtc.h - CMOS real-time clock (wall-clock date and time).
 */
#ifndef rtc_h__
#define rtc_h__

#include "lib/common.h"

typedef struct {
    uint8_t  second;
    uint8_t  minute;
    uint8_t  hour;      /* 0-23 */
    uint8_t  day;
    uint8_t  month;
    uint16_t year;      /* full year, e.g. 2026 */
} rtc_time_t;

/* Read the current date and time (slow port I/O: not from IRQ handlers). */
void rtc_read(rtc_time_t* t);

/* "HH:MM" ('out' needs RTC_CLOCK_LEN + 1 bytes). */
#define RTC_CLOCK_LEN 5
void rtc_format_clock(const rtc_time_t* t, char* out);

/* "YYYY-MM-DD HH:MM:SS" ('out' needs RTC_DATE_LEN + 1 bytes). */
#define RTC_DATE_LEN 19
void rtc_format_date(const rtc_time_t* t, char* out);

#endif
