/*
 * math.c - integer helpers and single-precision math on the x87 FPU.
 *
 * There is no C library, so sqrtf, expf and friends are written here with
 * x87 instructions. The x87 has a stack of eight 80-bit registers,
 * st(0) on top. In the inline assembly below, the "t" constraint means
 * "in st(0)" and "u" means "in st(1)"; GCC loads the inputs there and
 * takes the result from st(0).
 *
 * The x87 only knows a few transcendental building blocks:
 *   fsqrt   st0 = sqrt(st0)
 *   fyl2x   st1 * log2(st0), popping one register
 *   f2xm1   st0 = 2^st0 - 1, only for -1 <= st0 <= 1
 *   fscale  st0 = st0 * 2^(integer part of st1)
 *   fsin, fcos
 * and the constants fldl2e (log2 e) and fldln2 (ln 2). Everything else is
 * rewritten in terms of log2 and 2^x, as explained at each function.
 */
#include "lib/common.h"
#include "lib/math.h"
#include "drivers/serial.h"

#define SELF_TEST_TOLERANCE  1e-5f  /* relative error allowed by the self-test */
#define MICRO                1000000.0f

int int_abs(int v) {
    return (v < 0) ? -v : v;
}

float sqrtf(float x) {
    float result;
    asm("fsqrt" : "=t"(result) : "0"(x));
    return result;
}

/* 2^x. f2xm1 only accepts -1..1, so split x into an integer n (frndint
   rounds to nearest) and a fraction f = x - n in -0.5..0.5:
   2^x = 2^f * 2^n, where 2^f = f2xm1(f) + 1 and "* 2^n" is fscale. */
static float exp2_x87(float x) {
    float n;
    asm("frndint" : "=t"(n) : "0"(x));
    float f = x - n;

    float result;
    asm("f2xm1\n\t"
        "fld1\n\t"
        "faddp\n\t"         /* st0 = 2^f */
        "fscale"            /* st0 = 2^f * 2^n (n is st1) */
        : "=t"(result) : "0"(f), "u"(n));
    return result;
}

/* e^x = 2^(x * log2(e)). */
float expf(float x) {
    float log2e;
    asm("fldl2e" : "=t"(log2e));
    return exp2_x87(x * log2e);
}

/* ln(x) = ln(2) * log2(x), which is exactly one fyl2x with st1 = ln(2). */
float logf(float x) {
    float result;
    asm("fldln2\n\t"        /* st0 = ln 2, st1 = x */
        "fxch\n\t"          /* st0 = x, st1 = ln 2 */
        "fyl2x"             /* st0 = ln 2 * log2(x) */
        : "=t"(result) : "0"(x));
    return result;
}

/* log2(x) = 1 * log2(x): fyl2x with st1 = 1. */
static float log2_x87(float x) {
    float result;
    asm("fld1\n\t"
        "fxch\n\t"
        "fyl2x"
        : "=t"(result) : "0"(x));
    return result;
}

/* x^y = 2^(y * log2(x)). log2 only exists for x > 0, so 0 and negative
   bases are handled first: a negative base only works with a whole-number
   exponent, and its sign comes from whether that exponent is odd. */
float powf(float x, float y) {
    if (x == 0.0f) {
        return (y == 0.0f) ? 1.0f : 0.0f;
    }
    if (x > 0.0f) {
        return exp2_x87(y * log2_x87(x));
    }
    int whole = (int)y;
    if ((float)whole != y) {
        return 0.0f / 0.0f;     /* NaN, like the C library */
    }
    float magnitude = exp2_x87(y * log2_x87(-x));
    return (whole & 1) ? -magnitude : magnitude;
}

float sinf(float x) {
    float result;
    asm("fsin" : "=t"(result) : "0"(x));
    return result;
}

float cosf(float x) {
    float result;
    asm("fcos" : "=t"(result) : "0"(x));
    return result;
}

/* ---- self-test ---------------------------------------------------- */

typedef struct {
    const char* name;
    float got;
    float expected;
} math_check_t;

static bool math_close(float got, float expected) {
    float error = got - expected;
    float scale = expected < 0 ? -expected : expected;
    if (error < 0) {
        error = -error;
    }
    return error <= SELF_TEST_TOLERANCE * (scale > 1.0f ? scale : 1.0f);
}

bool math_self_test(void) {
    const float pi = 3.14159265f;
    math_check_t checks[] = {
        { "sqrtf(2)",          sqrtf(2.0f),            1.41421356f },
        { "expf(1)",           expf(1.0f),             2.71828183f },
        { "expf(-10)",         expf(-10.0f),           4.5399930e-5f },
        { "logf(10)",          logf(10.0f),            2.30258509f },
        { "powf(10000, 0.5)",  powf(10000.0f, 0.5f),   100.0f },
        { "powf(-2, 3)",       powf(-2.0f, 3.0f),      -8.0f },
        { "sinf(pi/6)",        sinf(pi / 6.0f),        0.5f },
        { "cosf(pi/3)",        cosf(pi / 3.0f),        0.5f },
    };
    int count = sizeof(checks) / sizeof(checks[0]);
    int failed = 0;

    for (int i = 0; i < count; i++) {
        if (!math_close(checks[i].got, checks[i].expected)) {
            /* serial_printf has no %f, so print millionths. */
            serial_printf("math: %s = %d/1000000, expected %d/1000000\n", checks[i].name,
                          (int)(checks[i].got * MICRO), (int)(checks[i].expected * MICRO));
            failed++;
        }
    }
    if (failed) {
        serial_printf("math: self-test FAILED (%d of %d)\n", failed, count);
    } else {
        serial_printf("math: self-test passed (%d checks)\n", count);
    }
    return failed == 0;
}
