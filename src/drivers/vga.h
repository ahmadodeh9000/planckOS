/*
 * vga.h - 80x25 VGA text-mode console.
 *
 * The text screen is a console_t sink (lib/console.h): print to it with
 * console_print / console_printf on vga_console().
 */
#ifndef vga_h__
#define vga_h__

#include "lib/console.h"

#define VGA_WIDTH   80
#define VGA_HEIGHT  25

console_t* vga_console(void);

#endif
