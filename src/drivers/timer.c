/*
 * timer.c - PIT channel 0 on IRQ0, the kernel's clock.
 *
 * The PIT counts down from a divisor at 1.193182 MHz and raises IRQ0 each
 * time it reaches zero. With divisor = PIT_BASE_HZ / TIMER_HZ we get
 * TIMER_HZ interrupts per second. The IRQ handler only increments a
 * counter; uptime and sleeps are computed from that counter.
 */
#include "drivers/timer.h"
#include "arch/idt.h"

#define PIT_BASE_HZ         1193182
#define PIT_CHANNEL0_PORT   0x40
#define PIT_COMMAND_PORT    0x43
#define PIT_CMD_CH0_SQUARE  0x36    /* channel 0, lo/hi byte, mode 3 (square wave) */
#define TIMER_IRQ           0
#define MS_PER_SECOND       1000

static volatile uint32_t ticks = 0;

static void timer_handler(registers_t* regs) {
    (void)regs;
    ticks++;
}

void timer_install(void) {
    uint32_t divisor = PIT_BASE_HZ / TIMER_HZ;

    outportb(PIT_COMMAND_PORT, PIT_CMD_CH0_SQUARE);
    outportb(PIT_CHANNEL0_PORT, divisor & 0xFF);
    outportb(PIT_CHANNEL0_PORT, (divisor >> 8) & 0xFF);

    irq_install_handler(TIMER_IRQ, timer_handler);
}

uint32_t timer_ticks(void) {
    return ticks;
}

/* Sleeps at least 'ms' milliseconds, rounded up to whole ticks.
   Interrupts must be enabled or this never returns. */
void sleep_ms(uint32_t ms) {
    uint32_t wait = (ms * TIMER_HZ + MS_PER_SECOND - 1) / MS_PER_SECOND;
    uint32_t start = ticks;

    while (ticks - start < wait) {
        asm volatile("hlt");
    }
}
