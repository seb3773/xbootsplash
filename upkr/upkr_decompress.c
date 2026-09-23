/*
 * upkr_decompress.c - Self-contained freestanding UPKR decompressor in C99
 * Based on upkr unpack.c (Unlicense / Public Domain)
 * Freestanding, 0 malloc, nolibc compatible
 */

#include "upkr_decompress.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

static const u8* upkr_data_ptr;
static u8 upkr_probs[1 + 255 + 1 + 2*32 + 2*32];
static u32 upkr_state;

static int upkr_decode_bit(int context_index) {
    while (upkr_state < 4096) {
        upkr_state = (upkr_state << 8) | *upkr_data_ptr++;
    }

    int prob = upkr_probs[context_index];
    int bit = ((int)(upkr_state & 255) < prob) ? 1 : 0;

    if (bit) {
        upkr_state = prob * (upkr_state >> 8) + (upkr_state & 255);
        prob += (256 - prob + 8) >> 4;
    } else {
        upkr_state = (256 - prob) * (upkr_state >> 8) + (upkr_state & 255) - prob;
        prob -= (prob + 8) >> 4;
    }
    upkr_probs[context_index] = (u8)prob;

    return bit;
}

static int upkr_decode_length(int context_index) {
    int length = 0;
    int bit_pos = 0;
    while (upkr_decode_bit(context_index)) {
        length |= upkr_decode_bit(context_index + 1) << bit_pos++;
        context_index += 2;
    }
    return length | (1 << bit_pos);
}

void* upkr_unpack(void* destination, const void* compressed_data) {
    upkr_data_ptr = (const u8*)compressed_data;
    upkr_state = 0;

    for (int i = 0; i < (int)sizeof(upkr_probs); ++i) {
        upkr_probs[i] = 128;
    }

    u8* write_ptr = (u8*)destination;
    int prev_was_match = 0;
    int offset = 0;

    for (;;) {
        if (upkr_decode_bit(0)) {
            // Match
            if (prev_was_match || upkr_decode_bit(256)) {
                offset = upkr_decode_length(257) - 1;
                if (offset == 0) {
                    // Offset 0 signals EOF
                    break;
                }
            }
            int length = upkr_decode_length(257 + 64);
            while (length--) {
                *write_ptr = write_ptr[-offset];
                ++write_ptr;
            }
            prev_was_match = 1;
        } else {
            // Literal
            int byte = 1;
            while (byte < 256) {
                int bit = upkr_decode_bit(byte);
                byte = (byte << 1) + bit;
            }
            *write_ptr++ = (u8)byte;
            prev_was_match = 0;
        }
    }

    return write_ptr;
}
