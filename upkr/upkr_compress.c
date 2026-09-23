/*
 * upkr_compress.c - Standalone C99 UPKR compressor (Public Domain / Unlicense)
 *
 * Implements:
 *   - 385-context probability model matching upkr specification
 *   - Reverse rANS byte-stream entropy encoder
 *   - Fast LZ match finder with 3-byte hash table and lazy parsing
 *   - Zero external dependencies beyond standard C library
 */

#include "upkr_compress.h"
#include <stdlib.h>
#include <string.h>

#define UPKR_MAX_CONTEXTS 385

typedef struct {
    uint16_t *steps;
    size_t count;
    size_t capacity;
    uint8_t probs[UPKR_MAX_CONTEXTS];
} UpkrEncoder;

static void enc_init(UpkrEncoder *enc) {
    enc->count = 0;
    enc->capacity = 131072;
    enc->steps = (uint16_t*)malloc(enc->capacity * sizeof(uint16_t));
    for (int i = 0; i < UPKR_MAX_CONTEXTS; i++) {
        enc->probs[i] = 128;
    }
}

static void enc_free(UpkrEncoder *enc) {
    if (enc->steps) free(enc->steps);
    enc->steps = NULL;
}

static inline void enc_bit(UpkrEncoder *enc, int ctx, int bit) {
    if (enc->count >= enc->capacity) {
        enc->capacity *= 2;
        enc->steps = (uint16_t*)realloc(enc->steps, enc->capacity * sizeof(uint16_t));
    }
    int prob = enc->probs[ctx];
    enc->steps[enc->count++] = (uint16_t)(prob | (bit ? 0x8000 : 0));
    if (bit) {
        prob += (256 - prob + 8) >> 4;
    } else {
        prob -= (prob + 8) >> 4;
    }
    enc->probs[ctx] = (uint8_t)prob;
}

static void enc_length(UpkrEncoder *enc, int ctx_start, uint32_t val) {
    int ctx = ctx_start;
    while (val >= 2) {
        enc_bit(enc, ctx, 1);
        enc_bit(enc, ctx + 1, val & 1);
        ctx += 2;
        val >>= 1;
    }
    enc_bit(enc, ctx, 0);
}

static void enc_literal(UpkrEncoder *enc, uint8_t byte, int *prev_was_match) {
    enc_bit(enc, 0, 0);
    int ctx = 1;
    for (int i = 7; i >= 0; i--) {
        int bit = (byte >> i) & 1;
        enc_bit(enc, ctx, bit);
        ctx = (ctx << 1) | bit;
    }
    *prev_was_match = 0;
}

static void enc_match(UpkrEncoder *enc, uint32_t offset, uint32_t len,
                      int *prev_was_match, uint32_t *last_offset) {
    enc_bit(enc, 0, 1);
    int new_offset = 1;
    if (!*prev_was_match) {
        new_offset = (offset != *last_offset);
        enc_bit(enc, 256, new_offset);
    }
    if (new_offset || *prev_was_match) {
        enc_length(enc, 257, offset + 1);
        *last_offset = offset;
    }
    enc_length(enc, 257 + 64, len);
    *prev_was_match = 1;
}

static void enc_eof(UpkrEncoder *enc, int *prev_was_match) {
    enc_bit(enc, 0, 1);
    if (!*prev_was_match) enc_bit(enc, 256, 1);
    enc_length(enc, 257, 1);
}

static size_t enc_finish(UpkrEncoder *enc, uint8_t *out, size_t out_max) {
    size_t temp_cap = enc->count + 1024;
    uint8_t *temp = (uint8_t*)malloc(temp_cap);
    if (!temp) return 0;
    size_t temp_len = 0;
    uint32_t state = 1 << 12;
    
    for (size_t i = enc->count; i > 0; i--) {
        uint16_t step = enc->steps[i - 1];
        uint32_t prob = step & 0x7FFF;
        uint32_t bit = (step >> 15) & 1;
        uint32_t start, p;
        if (bit) {
            start = 0;
            p = prob;
        } else {
            start = prob;
            p = 256 - prob;
        }
        uint32_t max_state = 4096 * p;
        while (state >= max_state) {
            if (temp_len >= temp_cap) {
                temp_cap *= 2;
                temp = (uint8_t*)realloc(temp, temp_cap);
            }
            temp[temp_len++] = (uint8_t)(state & 0xFF);
            state >>= 8;
        }
        state = ((state / p) << 8) + (state % p) + start;
    }
    
    while (state > 0) {
        if (temp_len >= temp_cap) {
            temp_cap *= 2;
            temp = (uint8_t*)realloc(temp, temp_cap);
        }
        temp[temp_len++] = (uint8_t)(state & 0xFF);
        state >>= 8;
    }
    
    if (temp_len > out_max) {
        free(temp);
        return 0;
    }
    
    for (size_t i = 0; i < temp_len; i++) {
        out[i] = temp[temp_len - 1 - i];
    }
    free(temp);
    return temp_len;
}

size_t upkr_compress(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_max, int level) {
    if (!in || in_size == 0 || !out || out_max == 0) return 0;
    
    UpkrEncoder enc;
    enc_init(&enc);
    
    int prev_was_match = 0;
    uint32_t last_offset = 0;
    
    #define UPKR_HASH_SIZE 65536
    int head[UPKR_HASH_SIZE];
    memset(head, -1, sizeof(head));
    int *prev = (int*)malloc(in_size * sizeof(int));
    if (!prev) {
        enc_free(&enc);
        return 0;
    }
    
    int max_depth = (level >= 6) ? 128 : (level >= 3 ? 64 : 32);
    
    size_t pos = 0;
    while (pos < in_size) {
        uint32_t best_len = 0;
        uint32_t best_off = 0;
        
        // 1. Check repeated offset match
        if (last_offset > 0 && last_offset <= pos) {
            size_t mlen = 0;
            while (pos + mlen < in_size && in[pos + mlen] == in[pos - last_offset + mlen]) {
                mlen++;
            }
            if (mlen >= 1) {
                best_len = (uint32_t)mlen;
                best_off = last_offset;
            }
        }
        
        // 2. Check 3-byte hash matches
        if (pos + 2 < in_size) {
            uint32_t h = (((uint32_t)in[pos] << 10) ^ ((uint32_t)in[pos+1] << 5) ^ in[pos+2]) & (UPKR_HASH_SIZE - 1);
            int cur = head[h];
            int chain = 0;
            while (cur >= 0 && chain < max_depth) {
                uint32_t off = (uint32_t)(pos - cur);
                if (in[cur] == in[pos] && in[cur+1] == in[pos+1] && in[cur+2] == in[pos+2]) {
                    size_t mlen = 3;
                    while (pos + mlen < in_size && in[pos + mlen] == in[cur + mlen]) {
                        mlen++;
                    }
                    if (mlen > best_len || (mlen == best_len && off < best_off)) {
                        best_len = (uint32_t)mlen;
                        best_off = off;
                    }
                }
                cur = prev[cur];
                chain++;
            }
            prev[pos] = head[h];
            head[h] = (int)pos;
        }
        
        // 3. Lazy evaluation for level >= 3
        if (level >= 3 && best_len >= 2 && pos + 1 < in_size) {
            uint32_t next_best_len = 0;
            if (pos + 3 < in_size) {
                uint32_t h1 = (((uint32_t)in[pos+1] << 10) ^ ((uint32_t)in[pos+2] << 5) ^ in[pos+3]) & (UPKR_HASH_SIZE - 1);
                int cur1 = head[h1];
                int chain = 0;
                while (cur1 >= 0 && chain < 32) {
                    if (in[cur1] == in[pos+1] && in[cur1+1] == in[pos+2] && in[cur1+2] == in[pos+3]) {
                        size_t mlen = 3;
                        while (pos + 1 + mlen < in_size && in[pos + 1 + mlen] == in[cur1 + mlen]) mlen++;
                        if (mlen > next_best_len) next_best_len = (uint32_t)mlen;
                    }
                    cur1 = prev[cur1];
                    chain++;
                }
            }
            if (next_best_len > best_len + 1) {
                best_len = 0; // Take literal instead
            }
        }
        
        if (best_len >= 2 || (best_len >= 1 && best_off == last_offset)) {
            enc_match(&enc, best_off, best_len, &prev_was_match, &last_offset);
            for (size_t k = 1; k < best_len && pos + k + 2 < in_size; k++) {
                uint32_t h = (((uint32_t)in[pos+k] << 10) ^ ((uint32_t)in[pos+k+1] << 5) ^ in[pos+k+2]) & (UPKR_HASH_SIZE - 1);
                prev[pos+k] = head[h];
                head[h] = (int)(pos+k);
            }
            pos += best_len;
        } else {
            enc_literal(&enc, in[pos], &prev_was_match);
            pos++;
        }
    }
    
    enc_eof(&enc, &prev_was_match);
    size_t comp_sz = enc_finish(&enc, out, out_max);
    
    free(prev);
    enc_free(&enc);
    return comp_sz;
}
