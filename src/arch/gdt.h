/*
 * gdt.h - global descriptor table.
 *
 * gdt_init() installs a flat memory model (every segment spans the full
 * 4 GB) with kernel and user code/data segments plus a TSS descriptor.
 */
#ifndef gdt_h__
#define gdt_h__

#include "lib/common.h"

#define GDT_KERNEL_CODE_SELECTOR    0x08
#define GDT_KERNEL_DATA_SELECTOR    0x10

void gdt_init(void);

#endif
