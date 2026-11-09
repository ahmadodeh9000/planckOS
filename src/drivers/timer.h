/*
 * timer.h - PIT (8253/8254) system tick.
 */
#ifndef timer_h__
#define timer_h__

#include "lib/common.h"

#define TIMER_HZ    100     /* ticks per second */

void timer_install(void);
uint32_t timer_ticks(void);
void sleep_ms(uint32_t ms);

/* Uptime as "HH:MM:SS" ('out' needs TIMER_UPTIME_LEN + 1 bytes). */
#define TIMER_UPTIME_LEN 8
void timer_format_uptime(char* out);

#endif
