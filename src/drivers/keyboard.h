/*
 * keyboard.h - PS/2 keyboard (US layout) with a line-input buffer.
 */
#ifndef keyboard_h__
#define keyboard_h__

#include "lib/common.h"

void keyboard_init(void);

/* Block until Enter is pressed, copy the line (without '\n') into 's'
   and return its length. 's' must hold KEYBOARD_LINE_MAX bytes. */
#define KEYBOARD_LINE_MAX   2048
uint32_t readline(char* s);

#endif
