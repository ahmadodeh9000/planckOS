/*
 * fpu.h - turns on the x87 floating-point unit.
 *
 * Floating point is only for normal kernel code (the language model in
 * src/llm). IRQ handlers must never use float or double: the interrupt
 * stubs save the general registers but not the FPU's state, so an IRQ
 * that touched the FPU would corrupt the calculation it interrupted.
 */
#ifndef fpu_h__
#define fpu_h__

void fpu_init(void);

#endif
