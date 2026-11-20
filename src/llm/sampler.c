/*
 * sampler.c - picks the next token from the model's scores (logits).
 *
 * Ported from run.c by Andrej Karpathy (https://github.com/karpathy/llama2.c,
 * MIT license, see src/llm/LICENSE.llama2c), without top-p sampling (it
 * needs a sort).
 *
 * Temperature 0 always takes the highest score (argmax), so the output is
 * the same every time: that's what llm-test relies on. Otherwise the
 * scores are divided by the temperature and turned into probabilities, and
 * a random number picks one. Below 1 the likely tokens get even likelier
 * (safer, more repetitive text); above 1 the choice gets wilder.
 */
#include "llm/sampler.h"
#include "llm/model.h"

#define XORSHIFT_MULTIPLIER 0x2545F4914F6CDD1DULL
#define FLOAT_MANTISSA      16777216.0f     /* 2^24 */

void sampler_init(sampler_t* s, int vocab_size, float temperature, uint64_t seed) {
    s->vocab_size = vocab_size;
    s->temperature = temperature;
    s->rng_state = seed ? seed : 1;     /* xorshift gets stuck at 0 */
}

/* xorshift* (https://en.wikipedia.org/wiki/Xorshift#xorshift*): a fast,
   decent random number generator in three shifts and a multiply. */
static uint32_t random_u32(uint64_t* state) {
    *state ^= *state >> 12;
    *state ^= *state << 25;
    *state ^= *state >> 27;
    return (*state * XORSHIFT_MULTIPLIER) >> 32;
}

/* A random float in [0, 1): 24 random bits, the precision of a float. */
static float random_f32(uint64_t* state) {
    return (random_u32(state) >> 8) / FLOAT_MANTISSA;
}

static int sample_argmax(const float* probabilities, int n) {
    int max_i = 0;
    float max_p = probabilities[0];
    for (int i = 1; i < n; i++) {
        if (probabilities[i] > max_p) {
            max_i = i;
            max_p = probabilities[i];
        }
    }
    return max_i;
}

/* Walk the running total of the probabilities until it passes 'coin'. */
static int sample_mult(const float* probabilities, int n, float coin) {
    float cdf = 0.0f;
    for (int i = 0; i < n; i++) {
        cdf += probabilities[i];
        if (coin < cdf) {
            return i;
        }
    }
    return n - 1;       /* in case rounding kept the total below 1 */
}

int sample(sampler_t* s, float* logits) {
    if (s->temperature == 0.0f) {
        return sample_argmax(logits, s->vocab_size);
    }
    for (int i = 0; i < s->vocab_size; i++) {
        logits[i] /= s->temperature;
    }
    softmax(logits, s->vocab_size);
    return sample_mult(logits, s->vocab_size, random_f32(&s->rng_state));
}
