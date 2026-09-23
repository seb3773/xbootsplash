/*
 * test_upkr_c.c - Prototype and validation of native C UPKR compressor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include "../upkr/upkr_decompress.h"

#define UPKR_MAX_CONTEXTS 385

typedef struct {
    uint16_t *steps;
    size_t count;
    size_t capacity;
    uint8_t probs[UPKR_MAX_CONTEXTS];
} UpkrEncoder;

static void enc_init(UpkrEncoder *enc) {
    enc->count = 0;
    enc->capacity = 65536;
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
    enc_bit(enc, 0, 0); // is_match = 0
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
    enc_bit(enc, 0, 1); // is_match = 1
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
    enc_bit(enc, 0, 1); // is_match = 1
    if (!*prev_was_match) {
        enc_bit(enc, 256, 1); // new_offset = 1
    }
    enc_length(enc, 257, 1); // offset = 1 -> unpacker offset = 1 - 1 = 0 (EOF)
}

static size_t enc_finish(UpkrEncoder *enc, uint8_t *out, size_t out_max) {
    size_t temp_cap = enc->count + 1024;
    uint8_t *temp = (uint8_t*)malloc(temp_cap);
    size_t temp_len = 0;
    
    uint32_t state = 1 << 12; // 4096
    
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
    
    // Reverse into output buffer
    for (size_t i = 0; i < temp_len; i++) {
        out[i] = temp[temp_len - 1 - i];
    }
    
    free(temp);
    return temp_len;
}

// Simple test compressor using greedy matcher
static size_t test_compress_greedy(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_max) {
    UpkrEncoder enc;
    enc_init(&enc);
    
    int prev_was_match = 0;
    uint32_t last_offset = 0;
    
    // 64K hash table for 2-byte prefix
    int head[65536];
    memset(head, -1, sizeof(head));
    int *prev = (int*)malloc(in_size * sizeof(int));
    
    size_t pos = 0;
    while (pos < in_size) {
        uint32_t best_len = 0;
        uint32_t best_off = 0;
        
        // 1. Check repeated offset
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
        
        // 2. Check hash matches if at least 2 bytes left
        if (pos + 1 < in_size) {
            uint16_t h = ((uint16_t)in[pos] << 8) | in[pos + 1];
            int cur = head[h];
            int chain_len = 0;
            while (cur >= 0 && chain_len < 64) {
                uint32_t off = (uint32_t)(pos - cur);
                size_t mlen = 0;
                while (pos + mlen < in_size && in[pos + mlen] == in[cur + mlen]) {
                    mlen++;
                }
                if (mlen > best_len) {
                    best_len = (uint32_t)mlen;
                    best_off = off;
                }
                cur = prev[cur];
                chain_len++;
            }
            prev[pos] = head[h];
            head[h] = (int)pos;
        }
        
        // Heuristic: only use match if len >= 2 (or len >= 1 with repeated offset)
        if (best_len >= 2 || (best_len >= 1 && best_off == last_offset)) {
            enc_match(&enc, best_off, best_len, &prev_was_match, &last_offset);
            // update hash table for skipped bytes
            for (size_t k = 1; k < best_len && pos + k + 1 < in_size; k++) {
                uint16_t h = ((uint16_t)in[pos + k] << 8) | in[pos + k + 1];
                prev[pos + k] = head[h];
                head[h] = (int)(pos + k);
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

int main() {
    printf("=== Testing Native C UPKR Encoder ===\n");
    
    const char *test_str = "Hello, World! Yellow world! Testing UPKR native C compression and decompression. "
                           "Repeat repeat repeat repeat repeat! 1234567890 1234567890 1234567890.";
    size_t in_size = strlen(test_str);
    
    uint8_t comp_buf[4096];
    size_t comp_sz = test_compress_greedy((const uint8_t*)test_str, in_size, comp_buf, sizeof(comp_buf));
    
    printf("Input size:  %zu bytes\n", in_size);
    printf("Packed size: %zu bytes (ratio: %.1f%%)\n", comp_sz, (double)comp_sz * 100.0 / in_size);
    
    assert(comp_sz > 0);
    
    // Decompress with C upkr_unpack
    uint8_t decomp_buf[4096];
    memset(decomp_buf, 0, sizeof(decomp_buf));
    void *end_ptr = upkr_unpack(decomp_buf, comp_buf);
    size_t decomp_sz = (uint8_t*)end_ptr - decomp_buf;
    
    printf("Decompressed size: %zu bytes\n", decomp_sz);
    assert(decomp_sz == in_size);
    assert(memcmp(test_str, decomp_buf, in_size) == 0);
    printf(">> SUCCESS: Decoded string matches input 100%% bit-exact!\n");
    
    return 0;
}
