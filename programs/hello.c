/*
 * hello.c - the smallest planckOS program: print a few lines and return.
 *
 * Build it and put it on the disk with 'make programs', then type
 * 'exec HELLO.ELF' in a Terminal.
 */
#include "planck.h"

int _start(void) {
    sys_print("Hello from HELLO.ELF!\n");
    sys_print("I was loaded at ");
    sys_print_hex((uint32_t)&_start);
    sys_print("and I talk to the kernel through int 0x80.\n");
    return 0;
}
