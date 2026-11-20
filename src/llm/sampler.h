/*
 * sampler.h - picks the next token from the model's scores (logits).
 */
#ifndef sampler_h__
#define sampler_h__

#include "lib/common.h"

typedef struct {
    int vocab_size;
    float temperature;      /* 0 = always the most likely token */
    uint64_t rng_state;     /* xorshift state, must not be 0 */
} sampler_t;

void sampler_init(sampler_t* s, int vocab_size, float temperature, uint64_t seed);

/* Pick a token. Changes 'logits' when the temperature isn't 0. */
int sample(sampler_t* s, float* logits);

#endif
