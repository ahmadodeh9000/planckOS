/*
 * math.h - integer helpers and single-precision math on the x87 FPU.
 *
 * The float functions behave like the C library ones the language model
 * needs (src/llm). Call fpu_init() before using them, and never use them
 * in an IRQ handler (see arch/fpu.h).
 */
#ifndef MATH_H__
#define MATH_H__

#include "lib/common.h"

int int_abs(int v);

float sqrtf(float x);
float expf(float x);
float logf(float x);
float powf(float x, float y);
float sinf(float x);
float cosf(float x);

/* Check every function above against known values and log the result to
   serial. Returns true if all of them were close enough. */
bool math_self_test(void);

#endif
