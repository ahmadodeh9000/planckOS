/*
 * logo.h - the planckOS logo shown when a shell starts.
 */
#ifndef logo_h__
#define logo_h__

#include "lib/console.h"

/* Print the logo, centered on an 80-column console. */
void logo_print(console_t* con);

#endif
