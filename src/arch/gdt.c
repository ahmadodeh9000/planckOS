/*
 * gdt.c - builds the GDT and the (not yet loaded) TSS descriptor.
 *
 * Segmentation is effectively switched off: every code and data segment
 * has base 0 and a 4 GB limit, and paging does the real memory protection.
 * The segments still have to exist because the CPU checks privilege
 * levels through them.
 */
#include "arch/gdt.h"
#include "lib/string.h"

#define GDT_ENTRIES         6
#define GDT_TSS_INDEX       5
#define SEGMENT_LIMIT_4GB   0xFFFFF     /* in 4 KB units when granularity is set */
#define RPL_USER            0x3

/* Access bytes: present, privilege level, type */
#define ACCESS_KERNEL_CODE  0x9A
#define ACCESS_KERNEL_DATA  0x92
#define ACCESS_USER_CODE    0xFA
#define ACCESS_USER_DATA    0xF2
#define ACCESS_TSS          0xE9        /* present, ring 3, 32-bit available TSS */

/* Flags nibble: 4 KB granularity, 32-bit segment */
#define FLAGS_4K_32BIT      0xCF
#define FLAGS_BYTE          0x00

typedef struct {
    uint16_t limit;             /* limit bits 0-15                   */
    uint16_t low_base;          /* base bits 0-15                    */
    uint8_t  mid_base;          /* base bits 16-23                   */
    uint8_t  access;
    uint8_t  granularity;       /* limit bits 16-19 + flags          */
    uint8_t  high_base;         /* base bits 24-31                   */
} __attribute__((packed)) gdt_entry;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_pointer;

typedef struct {
    uint32_t prev_tss;
    uint32_t esp0, ss0;         /* stack loaded on a switch to ring 0 */
    uint32_t esp1, ss1;
    uint32_t esp2, ss2;
    uint32_t cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint32_t trap;
    uint32_t iomap_base;
} __attribute__((packed)) tss_entry;

extern void gdt_flush(gdt_pointer* ptr);

static gdt_entry gdt[GDT_ENTRIES];
static gdt_pointer gdt_p;
static tss_entry tss;

static void gdt_set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt[i].low_base    = base & 0xFFFF;
    gdt[i].mid_base    = (base >> 16) & 0xFF;
    gdt[i].high_base   = (base >> 24) & 0xFF;
    gdt[i].limit       = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    gdt[i].access      = access;
}

static void gdt_write_tss(int index, uint16_t ss0, uint32_t esp0) {
    uint32_t base  = (uint32_t)&tss;
    uint32_t limit = sizeof(tss) - 1;    /* a limit is the last valid offset */

    gdt_set_entry(index, base, limit, ACCESS_TSS, FLAGS_BYTE);
    memset(&tss, 0, sizeof(tss));

    tss.ss0  = ss0;
    tss.esp0 = esp0;
    tss.cs   = GDT_KERNEL_CODE_SELECTOR | RPL_USER;
    tss.ss = tss.ds = tss.es = tss.fs = tss.gs = GDT_KERNEL_DATA_SELECTOR | RPL_USER;
}

void gdt_init(void) {
    gdt_p.limit = sizeof(gdt) - 1;
    gdt_p.base  = (uint32_t)&gdt;

    gdt_set_entry(0, 0, 0, 0, 0);                                           /* null */
    gdt_set_entry(1, 0, SEGMENT_LIMIT_4GB, ACCESS_KERNEL_CODE, FLAGS_4K_32BIT);
    gdt_set_entry(2, 0, SEGMENT_LIMIT_4GB, ACCESS_KERNEL_DATA, FLAGS_4K_32BIT);
    gdt_set_entry(3, 0, SEGMENT_LIMIT_4GB, ACCESS_USER_CODE, FLAGS_4K_32BIT);
    gdt_set_entry(4, 0, SEGMENT_LIMIT_4GB, ACCESS_USER_DATA, FLAGS_4K_32BIT);

    /* TODO: esp0 must point at a kernel stack before user mode exists. The
       CPU loads it from the TSS on every ring 3 -> ring 0 switch. */
    gdt_write_tss(GDT_TSS_INDEX, GDT_KERNEL_DATA_SELECTOR, 0);

    gdt_flush(&gdt_p);
}
