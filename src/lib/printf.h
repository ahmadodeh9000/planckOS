/*
 * printf.h - formatted text output to the VGA text console.
 *
 * Supports %s %c %d %x. printf_rainbow is the same but cycles colors,
 * used for the shell banner.
 */
#ifndef printf_h__
#define printf_h__

#include "lib/common.h"

void printf(const char* fmt, ...);
void printf_rainbow(const char* fmt, ...);

#endif
