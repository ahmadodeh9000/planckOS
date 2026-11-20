/*
 * model.c - a Llama 2 transformer, ported from llama2.c's run.c.
 *
 * Ported from run.c by Andrej Karpathy (https://github.com/karpathy/llama2.c,
 * MIT license, see src/llm/LICENSE.llama2c). The math in forward() is kept
 * close to the original so the two can be compared line by line; what
 * changed is the plumbing around it:
 *   - the weights come from a kmalloc'd buffer instead of mmap,
 *   - the header is checked against the file size before anything is used,
 *   - forward() is split into the attention and feed-forward halves of a
 *     layer, and no OpenMP (there is one CPU).
 * docs/LLM.md walks through forward() step by step.
 */
#include "llm/model.h"
#include "mm/kmalloc.h"
#include "lib/math.h"
#include "lib/string.h"
#include "drivers/serial.h"

#define HEADER_INTS     7           /* the 7 ints of model_config_t */
#define RMS_EPSILON     1e-5f       /* keeps rmsnorm from dividing by 0 */
#define ROPE_THETA      10000.0f    /* RoPE's base frequency, as in Llama 2 */

/* Sanity limits for the header, far above anything that fits in memory,
   so that a corrupted file is rejected before its numbers are used. */
#define MAX_DIM         4096
#define MAX_LAYERS      64
#define MAX_VOCAB       65536
#define MAX_SEQ_LEN     4096

/* ---- loading ------------------------------------------------------ */

static bool config_valid(const model_config_t* p) {
    if (p->dim <= 0 || p->dim > MAX_DIM || p->hidden_dim <= 0 || p->hidden_dim > 4 * MAX_DIM ||
        p->n_layers <= 0 || p->n_layers > MAX_LAYERS || p->n_heads <= 0 || p->n_kv_heads <= 0 ||
        p->vocab_size <= 0 || p->vocab_size > MAX_VOCAB ||
        p->seq_len <= 0 || p->seq_len > MAX_SEQ_LEN) {
        return false;
    }
    /* Heads split dim evenly, KV heads are shared evenly, and RoPE rotates
       pairs of numbers, so the head size must be even. */
    return p->dim % p->n_heads == 0 && p->n_heads % p->n_kv_heads == 0 &&
           (p->dim / p->n_heads) % 2 == 0;
}

/* How many floats of weights the header says follow it. 64-bit, so a
   corrupted header can't wrap around to a small, "valid" number. */
static uint64_t weight_count(const model_config_t* p, bool shared_weights) {
    uint64_t dim = p->dim, hidden = p->hidden_dim, layers = p->n_layers;
    uint64_t head_size = dim / p->n_heads;
    uint64_t kv_dim = head_size * p->n_kv_heads;
    uint64_t count = (uint64_t)p->vocab_size * dim;             /* embeddings */

    count += layers * (2 * dim + 2 * dim * dim + 2 * dim * kv_dim + 3 * dim * hidden);
    count += dim;                                                /* final rmsnorm */
    count += (uint64_t)p->seq_len * head_size;                   /* old RoPE tables */
    if (!shared_weights) {
        count += (uint64_t)p->vocab_size * dim;                  /* classifier */
    }
    return count;
}

static void map_weights(model_weights_t* w, const model_config_t* p, float* ptr,
                        bool shared_weights) {
    int head_size = p->dim / p->n_heads;
    int layers = p->n_layers;

    w->token_embedding_table = ptr;
    ptr += p->vocab_size * p->dim;
    w->rms_att_weight = ptr;
    ptr += layers * p->dim;
    w->wq = ptr;
    ptr += layers * p->dim * (p->n_heads * head_size);
    w->wk = ptr;
    ptr += layers * p->dim * (p->n_kv_heads * head_size);
    w->wv = ptr;
    ptr += layers * p->dim * (p->n_kv_heads * head_size);
    w->wo = ptr;
    ptr += layers * (p->n_heads * head_size) * p->dim;
    w->rms_ffn_weight = ptr;
    ptr += layers * p->dim;
    w->w1 = ptr;
    ptr += layers * p->dim * p->hidden_dim;
    w->w2 = ptr;
    ptr += layers * p->hidden_dim * p->dim;
    w->w3 = ptr;
    ptr += layers * p->dim * p->hidden_dim;
    w->rms_final_weight = ptr;
    ptr += p->dim;
    ptr += p->seq_len * head_size / 2;  /* skip the old freq_cis_real (RoPE) */
    ptr += p->seq_len * head_size / 2;  /* skip the old freq_cis_imag (RoPE) */
    w->wcls = shared_weights ? w->token_embedding_table : ptr;
}

static float* alloc_floats(int count) {
    float* p = kmalloc(count * sizeof(float));
    if (p) {
        memset(p, 0, count * sizeof(float));
    }
    return p;
}

static bool alloc_state(model_state_t* s, const model_config_t* p) {
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;

    s->x = alloc_floats(p->dim);
    s->xb = alloc_floats(p->dim);
    s->xb2 = alloc_floats(p->dim);
    s->hb = alloc_floats(p->hidden_dim);
    s->hb2 = alloc_floats(p->hidden_dim);
    s->q = alloc_floats(p->dim);
    s->key_cache = alloc_floats(p->n_layers * p->seq_len * kv_dim);
    s->value_cache = alloc_floats(p->n_layers * p->seq_len * kv_dim);
    s->att = alloc_floats(p->n_heads * p->seq_len);
    s->logits = alloc_floats(p->vocab_size);
    return s->x && s->xb && s->xb2 && s->hb && s->hb2 && s->q &&
           s->key_cache && s->value_cache && s->att && s->logits;
}

llm_status model_init(model_t* m, const uint8_t* data, uint32_t size) {
    memset(m, 0, sizeof(model_t));
    if (size < sizeof(model_config_t)) {
        serial_printf("llm: model file too short (%u bytes)\n", size);
        return LLM_BAD_MODEL;
    }
    memcpy(&m->config, data, sizeof(model_config_t));

    /* run.c's trick: a negative vocab_size means the classifier has its own
       weights instead of sharing the embedding table. */
    model_config_t* p = &m->config;
    bool shared_weights = p->vocab_size > 0;
    if (p->vocab_size < 0) {
        p->vocab_size = -p->vocab_size;
    }
    if (!config_valid(p)) {
        serial_printf("llm: bad model header\n");
        return LLM_BAD_MODEL;
    }
    uint64_t expected = sizeof(model_config_t) + weight_count(p, shared_weights) * sizeof(float);
    if (expected != size) {
        serial_printf("llm: model file is %u bytes, its header says %u\n", size, (uint32_t)expected);
        return LLM_BAD_MODEL;
    }
    map_weights(&m->weights, p, (float*)(data + HEADER_INTS * sizeof(int)), shared_weights);
    if (!alloc_state(&m->state, p)) {
        serial_printf("llm: out of memory for the model's run state\n");
        model_free(m);
        return LLM_NO_MEMORY;
    }
    return LLM_OK;
}

void model_free(model_t* m) {
    model_state_t* s = &m->state;
    kfree(s->x);
    kfree(s->xb);
    kfree(s->xb2);
    kfree(s->hb);
    kfree(s->hb2);
    kfree(s->q);
    kfree(s->key_cache);
    kfree(s->value_cache);
    kfree(s->att);
    kfree(s->logits);
    memset(s, 0, sizeof(model_state_t));
}

/* ---- the building blocks ------------------------------------------ */

/* Scale x so its root mean square is 1, then multiply by learned weights. */
static void rmsnorm(float* o, const float* x, const float* weight, int size) {
    float ss = 0.0f;
    for (int j = 0; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += RMS_EPSILON;
    ss = 1.0f / sqrtf(ss);
    for (int j = 0; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}

/* Turn scores into probabilities that add up to 1. Subtracting the max
   first keeps expf from overflowing; it doesn't change the result. */
void softmax(float* x, int size) {
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) {
            max_val = x[i];
        }
    }
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
    }
}

/* W (d,n) @ x (n,) -> xout (d,). Almost all the time is spent here. */
static void matmul(float* xout, const float* x, const float* w, int n, int d) {
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) {
            val += w[i * n + j] * x[j];
        }
        xout[i] = val;
    }
}

/* ---- forward ------------------------------------------------------ */

/* RoPE: rotate each pair of numbers in q and k by an angle that grows with
   the position, so attention scores depend on how far apart tokens are. */
static void rope(model_t* m, int pos) {
    model_config_t* p = &m->config;
    model_state_t* s = &m->state;
    int head_size = p->dim / p->n_heads;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;

    for (int i = 0; i < p->dim; i += 2) {
        int head_dim = i % head_size;
        float freq = 1.0f / powf(ROPE_THETA, head_dim / (float)head_size);
        float val = pos * freq;
        float fcr = cosf(val);
        float fci = sinf(val);
        int rotn = i < kv_dim ? 2 : 1;      /* 2 = q and k, 1 = q only */
        for (int v = 0; v < rotn; v++) {
            float* vec = v == 0 ? s->q : s->k;
            float v0 = vec[i];
            float v1 = vec[i + 1];
            vec[i]     = v0 * fcr - v1 * fci;
            vec[i + 1] = v0 * fci + v1 * fcr;
        }
    }
}

/* One attention head: score every earlier position against this query,
   softmax the scores, and mix those positions' values into xb. */
static void attention_head(model_t* m, int h, int loff, int pos) {
    model_config_t* p = &m->config;
    model_state_t* s = &m->state;
    int head_size = p->dim / p->n_heads;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    int kv_mul = p->n_heads / p->n_kv_heads;
    float* q = s->q + h * head_size;
    float* att = s->att + h * p->seq_len;

    for (int t = 0; t <= pos; t++) {
        float* k = s->key_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
        float score = 0.0f;
        for (int i = 0; i < head_size; i++) {
            score += q[i] * k[i];
        }
        att[t] = score / sqrtf(head_size);
    }
    softmax(att, pos + 1);

    float* xb = s->xb + h * head_size;
    memset(xb, 0, head_size * sizeof(float));
    for (int t = 0; t <= pos; t++) {
        float* v = s->value_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
        float a = att[t];
        for (int i = 0; i < head_size; i++) {
            xb[i] += a * v[i];
        }
    }
}

/* The attention half of layer l, added into x (the residual connection). */
static void layer_attention(model_t* m, int l, int pos) {
    model_config_t* p = &m->config;
    model_weights_t* w = &m->weights;
    model_state_t* s = &m->state;
    int dim = p->dim;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;

    rmsnorm(s->xb, s->x, w->rms_att_weight + l * dim, dim);

    /* This position's key and value go straight into the KV cache. */
    int loff = l * p->seq_len * kv_dim;
    s->k = s->key_cache + loff + pos * kv_dim;
    s->v = s->value_cache + loff + pos * kv_dim;

    matmul(s->q, s->xb, w->wq + l * dim * dim, dim, dim);
    matmul(s->k, s->xb, w->wk + l * dim * kv_dim, dim, kv_dim);
    matmul(s->v, s->xb, w->wv + l * dim * kv_dim, dim, kv_dim);
    rope(m, pos);

    for (int h = 0; h < p->n_heads; h++) {
        attention_head(m, h, loff, pos);
    }
    matmul(s->xb2, s->xb, w->wo + l * dim * dim, dim, dim);
    for (int i = 0; i < dim; i++) {
        s->x[i] += s->xb2[i];
    }
}

/* The feed-forward half of layer l: w2(silu(w1(x)) * w3(x)), added into x. */
static void layer_ffn(model_t* m, int l) {
    model_config_t* p = &m->config;
    model_weights_t* w = &m->weights;
    model_state_t* s = &m->state;
    int dim = p->dim;
    int hidden_dim = p->hidden_dim;

    rmsnorm(s->xb, s->x, w->rms_ffn_weight + l * dim, dim);
    matmul(s->hb, s->xb, w->w1 + l * dim * hidden_dim, dim, hidden_dim);
    matmul(s->hb2, s->xb, w->w3 + l * dim * hidden_dim, dim, hidden_dim);

    /* SwiGLU: silu(x) = x * sigmoid(x), times the w3 branch. */
    for (int i = 0; i < hidden_dim; i++) {
        float val = s->hb[i];
        val *= (1.0f / (1.0f + expf(-val)));
        val *= s->hb2[i];
        s->hb[i] = val;
    }
    matmul(s->xb, s->hb, w->w2 + l * dim * hidden_dim, hidden_dim, dim);
    for (int i = 0; i < dim; i++) {
        s->x[i] += s->xb[i];
    }
}

float* forward(model_t* m, int token, int pos) {
    model_config_t* p = &m->config;
    model_weights_t* w = &m->weights;
    model_state_t* s = &m->state;

    /* The token's row of the embedding table is where x starts. */
    memcpy(s->x, w->token_embedding_table + token * p->dim, p->dim * sizeof(float));

    for (int l = 0; l < p->n_layers; l++) {
        layer_attention(m, l, pos);
        layer_ffn(m, l);
    }
    rmsnorm(s->x, s->x, w->rms_final_weight, p->dim);

    /* Score every token in the vocabulary as the next one. */
    matmul(s->logits, s->x, w->wcls, p->dim, p->vocab_size);
    return s->logits;
}
