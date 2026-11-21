/*
 * jump.h - setjmp and longjmp: jump back out of deep function calls; and
 * call_on_stack: run a function on a different stack.
 *
 * setjmp(env) saves where it was called from and returns 0. A later
 * longjmp(env, value), from any function called since, throws away all
 * the frames in between and makes that setjmp return again, this time
 * with 'value'. The Higgson compiler uses it to stop at its first error,
 * however deep in the parser it is (docs/HIGGSON.md, "How the port works").
 *
 * Only jump to a setjmp whose function is still running, and note that it
 * skips any clean-up the jumped-over functions would have done.
 */
#ifndef jump_h__
#define jump_h__

#include "lib/common.h"

/* The registers a C function must keep (ebx esi edi ebp), plus esp and
   the return address: everything needed to resume after the setjmp. */
#define JUMP_REGISTERS  6

typedef uint32_t jmp_buf[JUMP_REGISTERS];

/* returns_twice tells GCC that setjmp can return a second time, so it
   doesn't keep values in registers across it that longjmp would undo. */
int setjmp(jmp_buf env) __attribute__((returns_twice));

/* Never returns; setjmp returns 'value' instead (1 if value is 0, so it
   can always be told apart from the first return). */
void longjmp(jmp_buf env, int value) __attribute__((noreturn));

/* Call fn(arg) on another stack, whose top (highest address, 16-byte
   aligned) is stack_top, and return when it does. For deep recursion that
   would overflow the 8 KB kernel stack. A longjmp from inside fn to a
   setjmp made before the call is fine: it restores the old esp too. */
void call_on_stack(void (*fn)(void*), void* arg, void* stack_top);

#endif
