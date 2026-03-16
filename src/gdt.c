#include "gdt.h"

#define GDT_ENTRIES 5

extern void gdt_flush(gdt_pointer*);

static gdt_entry gdt[GDT_ENTRIES];
static gdt_pointer gdt_p;

static void set_gdt_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].low_base     = base & 0xFFFF;
    gdt[i].mid_base     = (base >> 16) & 0xFF;
    gdt[i].high_base    = (base >> 24) & 0xFF;

    gdt[i].limit = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F);
    gdt[i].granularity |= (gran & 0xF0);
    gdt[i].access = access;
}


void gdt_init() {

    gdt_p.limit = (sizeof(gdt_entry) * 5)  - 1;
    gdt_p.base  = (uint32_t) &gdt;

    set_gdt_entry(0,0,0,0,0);               /* Null Descriptor          */
    set_gdt_entry(1,0,0xFFFFF,0x9A,0xCF);   /* Kernel Mode Code Segment */
    set_gdt_entry(2,0,0xFFFFF,0x92,0xCF);   /* Kernel Mode Data Segment */
    set_gdt_entry(3,0,0xFFFFF,0xFA,0xCF);   /* User Mode Code Segment   */
    set_gdt_entry(4,0,0xFFFFF,0xF2,0xCF);   /* User Mode Data Segment   */

    gdt_flush(&gdt_p);
}