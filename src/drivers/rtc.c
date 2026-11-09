/*
 * rtc.c - CMOS real-time clock.
 *
 * The RTC is a battery-backed chip that keeps the date and time while the
 * machine is off. We select a register by writing its number to port 0x70
 * and read its value from port 0x71.
 *
 * Two quirks:
 *   - Values are usually BCD (0x45 means 45). Status register B says which.
 *   - The chip updates once a second; reading during an update can give a
 *     torn value (e.g. 12:59 -> 12:00). So we wait for "update in progress"
 *     to clear, read twice, and repeat until both reads match.
 *
 * Reading is slow port I/O, so never call this from an IRQ handler. The
 * GUI calls it once a second when it receives EVENT_TIMER.
 */
#include "drivers/rtc.h"
#include "arch/io.h"

#define CMOS_ADDRESS_PORT   0x70
#define CMOS_DATA_PORT      0x71

#define RTC_REG_SECOND      0x00
#define RTC_REG_MINUTE      0x02
#define RTC_REG_HOUR        0x04
#define RTC_REG_DAY         0x07
#define RTC_REG_MONTH       0x08
#define RTC_REG_YEAR        0x09
#define RTC_REG_STATUS_A    0x0A
#define RTC_REG_STATUS_B    0x0B

#define RTC_A_UPDATING      0x80    /* update in progress */
#define RTC_B_24_HOUR       0x02    /* hours are 0-23, not 12-hour */
#define RTC_B_BINARY        0x04    /* values are binary, not BCD */
#define RTC_HOUR_PM         0x80    /* PM flag in 12-hour mode */

#define RTC_CENTURY         2000    /* the chip only stores two year digits */
#define BCD_LOW_DIGIT       0x0F    /* BCD keeps one decimal digit per nibble */
#define BCD_HIGH_SHIFT      4
#define MONTHS_PER_YEAR     12
#define MONTH_NAME_LEN      3

static const char* const month_names[MONTHS_PER_YEAR] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static uint8_t cmos_read(uint8_t reg) {
    outportb(CMOS_ADDRESS_PORT, reg);
    return inportb(CMOS_DATA_PORT);
}

static uint8_t bcd_to_binary(uint8_t value) {
    return (value & BCD_LOW_DIGIT) + (value >> BCD_HIGH_SHIFT) * 10;
}

/* Reads the raw registers once, after any update has finished. */
static void rtc_read_raw(rtc_time_t* t) {
    while (cmos_read(RTC_REG_STATUS_A) & RTC_A_UPDATING) {
    }

    t->second = cmos_read(RTC_REG_SECOND);
    t->minute = cmos_read(RTC_REG_MINUTE);
    t->hour   = cmos_read(RTC_REG_HOUR);
    t->day    = cmos_read(RTC_REG_DAY);
    t->month  = cmos_read(RTC_REG_MONTH);
    t->year   = cmos_read(RTC_REG_YEAR);
}

static bool rtc_same(const rtc_time_t* a, const rtc_time_t* b) {
    return a->second == b->second && a->minute == b->minute &&
           a->hour == b->hour && a->day == b->day &&
           a->month == b->month && a->year == b->year;
}

/* Converts BCD / 12-hour values into plain binary 24-hour values. */
static void rtc_normalize(rtc_time_t* t, uint8_t status_b) {
    uint8_t is_pm = t->hour & RTC_HOUR_PM;
    t->hour &= ~RTC_HOUR_PM;

    if (!(status_b & RTC_B_BINARY)) {
        t->second = bcd_to_binary(t->second);
        t->minute = bcd_to_binary(t->minute);
        t->hour   = bcd_to_binary(t->hour);
        t->day    = bcd_to_binary(t->day);
        t->month  = bcd_to_binary(t->month);
        t->year   = bcd_to_binary((uint8_t)t->year);
    }

    if (!(status_b & RTC_B_24_HOUR)) {
        t->hour %= 12;          /* 12 AM -> 0 */
        if (is_pm) {
            t->hour += 12;
        }
    }

    t->year += RTC_CENTURY;
}

void rtc_read(rtc_time_t* t) {
    rtc_time_t check;

    rtc_read_raw(t);
    do {
        check = *t;
        rtc_read_raw(t);
    } while (!rtc_same(t, &check));

    rtc_normalize(t, cmos_read(RTC_REG_STATUS_B));
}

/* Writes 'value' as exactly two digits. */
static void put_two_digits(char* out, uint32_t value) {
    out[0] = '0' + value / 10 % 10;
    out[1] = '0' + value % 10;
}

void rtc_format_clock(const rtc_time_t* t, char* out) {
    put_two_digits(out, t->hour);
    out[2] = ':';
    put_two_digits(out + 3, t->minute);
    out[RTC_CLOCK_LEN] = '\0';
}

void rtc_format_day(const rtc_time_t* t, char* out) {
    /* Months are 1-12; anything else means the chip gave us garbage. */
    const char* month = (t->month >= 1 && t->month <= MONTHS_PER_YEAR)
                        ? month_names[t->month - 1] : "???";

    put_two_digits(out, t->day);
    out[2] = ' ';
    for (int i = 0; i < MONTH_NAME_LEN; i++) {
        out[3 + i] = month[i];
    }
    out[6] = ' ';
    put_two_digits(out + 7, t->year / 100);
    put_two_digits(out + 9, t->year % 100);
    out[RTC_DAY_LEN] = '\0';
}

void rtc_format_date(const rtc_time_t* t, char* out) {
    put_two_digits(out, t->year / 100);
    put_two_digits(out + 2, t->year % 100);
    out[4] = '-';
    put_two_digits(out + 5, t->month);
    out[7] = '-';
    put_two_digits(out + 8, t->day);
    out[10] = ' ';
    put_two_digits(out + 11, t->hour);
    out[13] = ':';
    put_two_digits(out + 14, t->minute);
    out[16] = ':';
    put_two_digits(out + 17, t->second);
    out[RTC_DATE_LEN] = '\0';
}
