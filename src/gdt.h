#ifndef gdt_h__
#define gdt_h__

#include "common.h"

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

void gdt_init();



#endif