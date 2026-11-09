/*
 * event.h - the input event queue.
 *
 * Interrupt handlers (keyboard, mouse) describe what happened as an event_t
 * and push it here. The main loop takes events out and acts on them. IRQ
 * handlers never draw or do slow work themselves.
 */
#ifndef event_h__
#define event_h__

#include "lib/common.h"

typedef enum {
    EVENT_KEY,          /* a key producing 'key' was pressed      */
    EVENT_MOUSE_MOVE,   /* the pointer moved to (x, y)            */
    EVENT_MOUSE_DOWN,   /* 'button' was pressed at (x, y)         */
    EVENT_MOUSE_UP,     /* 'button' was released at (x, y)        */
    EVENT_TIMER,        /* another second of uptime has passed    */
} event_type;

#define MOUSE_BUTTON_LEFT   (1 << 0)
#define MOUSE_BUTTON_RIGHT  (1 << 1)
#define MOUSE_BUTTON_MIDDLE (1 << 2)

typedef struct {
    event_type type;
    int x, y;           /* pointer position (mouse events)        */
    uint8_t button;     /* MOUSE_BUTTON_* (down/up events)        */
    char key;           /* ASCII character, '\b', '\n', '\t'      */
} event_t;

/* Add an event. Safe to call from an interrupt handler. If the queue is
   full the event is dropped. */
void event_push(const event_t* event);

/* Take the oldest event. Returns false if the queue is empty. */
bool event_poll(event_t* event);

/* Like event_poll, but sleeps until an event arrives. */
void event_wait(event_t* event);

#endif
