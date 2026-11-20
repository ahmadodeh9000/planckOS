/*
 * fpu.c - turns on the x87 floating-point unit.
 *
 * Three bits in CR0 decide how the CPU treats FPU instructions:
 *
 *   EM (bit 2)  "emulate": when set, every FPU instruction traps (#NM) so
 *               an OS without an FPU could emulate it. We have one: clear.
 *   MP (bit 1)  "monitor coprocessor": makes WAIT/FWAIT honor the TS bit.
 *               Set, as Intel recommends for every CPU since the 486.
 *   NE (bit 5)  "numeric error": report FPU errors as exception 16 (#MF)
 *               instead of through the old external IRQ 13 wiring.
 *
 * fninit then resets the FPU: all exceptions masked (so 1/0 gives inf
 * instead of trapping), round to nearest, 64-bit internal precision.
 */
#include "arch/fpu.h"
#include "lib/common.h"

#define CR0_MP  (1 << 1)
#define CR0_EM  (1 << 2)
#define CR0_NE  (1 << 5)

void fpu_init(void) {
    uint32_t cr0;

    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~CR0_EM;
    cr0 |= CR0_MP | CR0_NE;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
    asm volatile("fninit");
}
