/*
 * event.c - a fixed-size ring buffer of input events.
 *
 * Interrupt handlers are the only writers (they move 'head') and the main
 * loop is the only reader (it moves 'tail'). One CPU, and IRQ handlers run
 * with interrupts off, so a push can never be interrupted by another push.
 * That means no lock is needed. Both indexes are volatile so the compiler
 * re-reads them every time, and a compiler barrier makes sure an event is
 * fully copied before the index that publishes (or frees) it changes.
 */
#include "gui/event.h"

#define EVENT_QUEUE_SIZE 256    /* a power of two, so '%' is cheap */

#define compiler_barrier() asm volatile("" ::: "memory")

static event_t queue[EVENT_QUEUE_SIZE];
static volatile uint32_t head = 0;  /* next slot to write */
static volatile uint32_t tail = 0;  /* next slot to read  */

void event_push(const event_t* event) {
    uint32_t next = (head + 1) % EVENT_QUEUE_SIZE;

    if (next == tail) {
        return;     /* full: drop the event rather than overwrite */
    }
    queue[head] = *event;
    compiler_barrier();
    head = next;
}

bool event_poll(event_t* event) {
    if (tail == head) {
        return false;
    }
    *event = queue[tail];
    compiler_barrier();
    tail = (tail + 1) % EVENT_QUEUE_SIZE;
    return true;
}

void event_wait(event_t* event) {
    for (;;) {
        /*
         * Check and sleep with interrupts off. Otherwise an event could
         * arrive between the check and 'hlt', and we would sleep until the
         * next unrelated interrupt. 'sti' only takes effect after the next
         * instruction, so "sti; hlt" can't miss the wake-up.
         */
        asm volatile("cli");
        if (event_poll(event)) {
            asm volatile("sti");
            return;
        }
        asm volatile("sti; hlt");
    }
}
