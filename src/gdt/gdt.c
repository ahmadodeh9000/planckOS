#include "gdt.h"
#include "../util.h"

#define GDT_ENTRIES 6

extern void gdt_flush(gdt_pointer*);
extern void tss_flush();

static gdt_entry gdt[GDT_ENTRIES];
static gdt_pointer gdt_p;

static tss_entry tss;

static void set_gdt_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].low_base     = base & 0xFFFF;
    gdt[i].mid_base     = (base >> 16) & 0xFF;
    gdt[i].high_base    = (base >> 24) & 0xFF;

    gdt[i].limit = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F);
    gdt[i].granularity |= (gran & 0xF0);
    gdt[i].access = access;
}

void write_tss(uint32_t num, uint16_t ss0, uint32_t esp0) {
    uint32_t base   = (uint32_t) &tss;
    uint32_t limit  = base + sizeof(tss);

    set_gdt_entry(num,base,limit,0xE9,0x0);
    kmemset(&tss,0,sizeof(tss));
    
    tss.ss0     = ss0;
    tss.esp0    = esp0;

    tss.cs      = 0x08 | 0x3;
    tss.ss = tss.ds = tss.es = tss.fs = tss.gs = 0x10 | 0x3;

    
}

void gdt_init() {

    gdt_p.limit = (sizeof(gdt_entry) * GDT_ENTRIES)  - 1;
    gdt_p.base  = (uint32_t) &gdt;

    set_gdt_entry(0,0,0,0,0);               /* Null Descriptor          */
    set_gdt_entry(1,0,0xFFFFF,0x9A,0xCF);   /* Kernel Mode Code Segment */
    set_gdt_entry(2,0,0xFFFFF,0x92,0xCF);   /* Kernel Mode Data Segment */
    set_gdt_entry(3,0,0xFFFFF,0xFA,0xCF);   /* User Mode Code Segment   */
    set_gdt_entry(4,0,0xFFFFF,0xF2,0xCF);   /* User Mode Data Segment   */

    write_tss(5,0x10,0x0);                  /* TSS */

    gdt_flush(&gdt_p);
}