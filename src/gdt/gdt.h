#ifndef gdt_h__
#define gdt_h__

#include "../common.h"

typedef struct {
    uint16_t limit;         // lower 16 bit limit 
    uint16_t low_base;          // lower 16 bit base
    uint8_t mid_base;           // next 8 bits of base
    uint8_t access;             // access byte
    uint8_t granularity;        // limit + flags        
    uint8_t high_base;         // higher 16 bits of base

} __attribute__((packed)) gdt_entry;

typedef struct {
    uint16_t limit;
    uint32_t base;

} __attribute__((packed)) gdt_pointer;

typedef struct {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint32_t trap;
    uint32_t iomap_base;
} __attribute__((packed)) tss_entry;

void gdt_init();
void write_tss(uint32_t num, uint16_t ss0, uint32_t esp0);


#endif