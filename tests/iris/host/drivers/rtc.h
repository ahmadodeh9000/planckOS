/* rtc.h (host shim) - always midnight, so the random seed is the same. */
#ifndef rtc_h__
#define rtc_h__
#include <stdint.h>
typedef struct { uint8_t second, minute, hour, day, month; uint16_t year; } rtc_time_t;
static inline void rtc_read(rtc_time_t* t) { t->second = t->minute = t->hour = 0; }
#endif
