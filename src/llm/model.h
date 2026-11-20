/*
 * model.h - a Llama 2 transformer, ported from llama2.c's run.c.
 *
 * The weights are one buffer in llama2.c's file format (see docs/LLM.md):
 * a 7-int header, then every weight matrix as 32-bit floats. The model
 * points into that buffer; nothing is copied.
 */
#ifndef model_h__
#define model_h__

#include "lib/common.h"
#include "llm/llm.h"

typedef struct {
    int dim;            /* width of the vectors that flow through the model */
    int hidden_dim;     /* width inside the feed-forward layers */
    int n_layers;
    int n_heads;        /* attention heads (queries) */
    int n_kv_heads;     /* key/value heads; fewer than n_heads = shared */
    int vocab_size;     /* number of tokens */
    int seq_len;        /* the longest sequence it can see (context length) */
} model_config_t;

typedef struct {
    float* token_embedding_table;   /* (vocab_size, dim) */
    float* rms_att_weight;          /* (layer, dim) */
    float* rms_ffn_weight;          /* (layer, dim) */
    float* wq;                      /* (layer, dim, n_heads * head_size) */
    float* wk;                      /* (layer, dim, n_kv_heads * head_size) */
    float* wv;                      /* (layer, dim, n_kv_heads * head_size) */
    float* wo;                      /* (layer, n_heads * head_size, dim) */
    float* w1;                      /* (layer, hidden_dim, dim) */
    float* w2;                      /* (layer, dim, hidden_dim) */
    float* w3;                      /* (layer, hidden_dim, dim) */
    float* rms_final_weight;        /* (dim) */
    float* wcls;                    /* (vocab_size, dim), often = embeddings */
} model_weights_t;

/* The activations of one forward pass, plus the KV cache. */
typedef struct {
    float* x;           /* (dim) the running vector of the current token */
    float* xb;          /* (dim) the same inside a residual branch */
    float* xb2;         /* (dim) */
    float* hb;          /* (hidden_dim) */
    float* hb2;         /* (hidden_dim) */
    float* q;           /* (dim) query */
    float* k;           /* points into key_cache at the current position */
    float* v;           /* points into value_cache at the current position */
    float* att;         /* (n_heads, seq_len) attention scores */
    float* logits;      /* (vocab_size) the output: one score per token */
    float* key_cache;   /* (layer, seq_len, kv_dim) */
    float* value_cache; /* (layer, seq_len, kv_dim) */
} model_state_t;

typedef struct {
    model_config_t config;
    model_weights_t weights;
    model_state_t state;
} model_t;

/* Check the header against the file size and point the weights into
   'data' (which must stay allocated). Allocates the run state. Returns
   LLM_BAD_MODEL or LLM_NO_MEMORY, with the reason on serial, on failure. */
llm_status model_init(model_t* m, const uint8_t* data, uint32_t size);
void model_free(model_t* m);

/* Run 'token' at position 'pos' through the model. Returns the logits:
   one score per vocabulary token for what comes next. */
float* forward(model_t* m, int token, int pos);

/* Exposed for the sampler. */
void softmax(float* x, int size);

#endif
