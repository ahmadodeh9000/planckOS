/* timer.h (host shim) - a clock that never moves, so runs repeat exactly. */
#ifndef timer_h__
#define timer_h__
#include <stdint.h>
#define TIMER_HZ 100
static inline uint32_t timer_ticks(void) { return 0; }
#endif
