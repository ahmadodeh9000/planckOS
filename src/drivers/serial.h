/*
 * serial.h - COM1 serial port for debug logging.
 *
 * serial_printf() writes to the first serial port. QEMU forwards it to the
 * terminal (make run / make run-headless), so it keeps working when the
 * screen is in graphics mode or the kernel is in a bad state.
 */
#ifndef serial_h__
#define serial_h__

#include "lib/common.h"

void serial_init(void);
void serial_putc(char c);
void serial_printf(const char* fmt, ...);

#endif
