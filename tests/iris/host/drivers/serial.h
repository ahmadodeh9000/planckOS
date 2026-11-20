/* serial.h (host shim) - the serial log goes to stderr. */
#ifndef serial_h__
#define serial_h__
#include <stdio.h>
#define serial_printf(...) fprintf(stderr, __VA_ARGS__)
#endif
