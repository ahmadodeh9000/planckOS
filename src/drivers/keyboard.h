/*
 * keyboard.h - PS/2 keyboard (US layout).
 *
 * Every key press that produces a character becomes an EVENT_KEY in the
 * event queue (gui/event.h).
 */
#ifndef keyboard_h__
#define keyboard_h__

void keyboard_init(void);

#endif
