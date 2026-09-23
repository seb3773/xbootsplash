/*
 * zx0_compress.c - Single-file optimized ZX0 compressor
 * Based on ZX0 format (c) 2021 by Einar Saukas.
 */

#include "zx0_compress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_OFFSET 1
#define FALSE 0
#define TRUE 1

static zx0_progress_fn zx0_progress_cb_global = NULL;

void zx0_set_progress_cb(zx0_progress_fn cb) {
    zx0_progress_cb_global = cb;
}

static inline void zx0_emit_progress(size_t processed, size_t total) {
    if (zx0_progress_cb_global) zx0_progress_cb_global(processed, total);
}

typedef struct block_t {
    struct block_t *chain;
    struct block_t *ghost_chain;
    int bits;
    int index;
    int offset;
    int references;
} BLOCK;

/* Memory pool */
#define QTY_BLOCKS 100000

typedef struct mem_pool_node_t {
    BLOCK *array;
    struct mem_pool_node_t *next;
} MEM_POOL_NODE;

static BLOCK *ghost_root = NULL;
static BLOCK *dead_array = NULL;
static int dead_array_size = 0;
static MEM_POOL_NODE *pool_list = NULL;

static void zx0_mem_reset(void) {
    MEM_POOL_NODE *cur = pool_list;
    while (cur) {
        MEM_POOL_NODE *nxt = cur->next;
        free(cur->array);
        free(cur);
        cur = nxt;
    }
    pool_list = NULL;
    ghost_root = NULL;
    dead_array = NULL;
    dead_array_size = 0;
}

static inline BLOCK *zx0_allocate(int bits, int index, int offset, BLOCK *chain) {
    BLOCK *ptr;

    if (ghost_root) {
        ptr = ghost_root;
        ghost_root = ptr->ghost_chain;
        if (ptr->chain && !--ptr->chain->references) {
            ptr->chain->ghost_chain = ghost_root;
            ghost_root = ptr->chain;
        }
    } else {
        if (!dead_array_size) {
            dead_array = (BLOCK *)malloc((size_t)QTY_BLOCKS * sizeof(BLOCK));
            if (!dead_array) {
                return NULL;
            }
            MEM_POOL_NODE *node = (MEM_POOL_NODE *)malloc(sizeof(MEM_POOL_NODE));
            if (node) {
                node->array = dead_array;
                node->next = pool_list;
                pool_list = node;
            }
            dead_array_size = QTY_BLOCKS;
        }
        ptr = &dead_array[--dead_array_size];
    }
    ptr->bits = bits;
    ptr->index = index;
    ptr->offset = offset;
    if (chain)
        chain->references++;
    ptr->chain = chain;
    ptr->references = 0;
    return ptr;
}

static inline void zx0_assign(BLOCK **ptr, BLOCK *chain) {
    BLOCK *old = *ptr;
    if (__builtin_expect(old == chain, 0)) {
        return;
    }
    if (chain)
        chain->references++;
    if (old && __builtin_expect(--old->references == 0, 0)) {
        old->ghost_chain = ghost_root;
        ghost_root = old;
    }
    *ptr = chain;
}

static inline int offset_ceiling(int index, int offset_limit) {
    return index > offset_limit     ? offset_limit
         : index < INITIAL_OFFSET ? INITIAL_OFFSET
                                  : index;
}

static inline int elias_gamma_bits(int value) {
#ifdef __GNUC__
    if (value <= 0)
        return 1;
    return ((32 - __builtin_clz(value)) << 1) - 1;
#else
    int bits = 1;
    while (value >>= 1)
        bits += 2;
    return bits;
#endif
}

typedef struct {
    BLOCK *last_literal;
    BLOCK *last_match;
    int match_length;
} OffsetData;

static BLOCK *zx0_optimize(unsigned char *input_data, int input_size, int skip,
                           int offset_limit) {
    unsigned char *__restrict in = input_data;
    OffsetData *offset_data;
    BLOCK **optimal;
    int *best_length;
    int best_length_size;
    int bits;
    int index;
    int offset;
    int length;
    int bits2;

    int step = input_size / 200;
    if (step <= 0)
        step = 1;
    int next_emit = step;
    zx0_emit_progress(0, (size_t)input_size);

    int max_offset = offset_ceiling(input_size - 1, offset_limit);

    offset_data = (OffsetData *)calloc((size_t)max_offset + 1, sizeof(OffsetData));
    optimal = (BLOCK **)calloc((size_t)input_size, sizeof(BLOCK *));
    best_length = (int *)malloc((size_t)input_size * sizeof(int));
    if (!offset_data || !optimal || !best_length) {
        free(offset_data);
        free(optimal);
        free(best_length);
        return NULL;
    }
    if (input_size > 2)
        best_length[2] = 2;

    BLOCK *init_block = zx0_allocate(-1, skip - 1, INITIAL_OFFSET, NULL);
    if (!init_block) {
        free(offset_data);
        free(optimal);
        free(best_length);
        return NULL;
    }
    zx0_assign(&offset_data[INITIAL_OFFSET].last_match, init_block);

    for (index = skip; index < input_size; index++) {
        if (index >= next_emit) {
            zx0_emit_progress((size_t)index, (size_t)input_size);
            next_emit += step;
        }
        unsigned char current_byte = in[index];
        const int can_match = (index != skip);
        best_length_size = 2;
        max_offset = offset_ceiling(index, offset_limit);
        for (offset = 1; offset <= max_offset; offset++) {
            OffsetData *od = &offset_data[offset];

            if (__builtin_expect(can_match && current_byte == in[index - offset], 0)) {
                if (od->last_literal) {
                    length = index - od->last_literal->index;
                    bits = od->last_literal->bits + 1 + elias_gamma_bits(length);
                    zx0_assign(&od->last_match,
                               zx0_allocate(bits, index, offset, od->last_literal));
                    if (!optimal[index] || optimal[index]->bits > bits)
                        zx0_assign(&optimal[index], od->last_match);
                }
                if (++od->match_length > 1) {
                    if (best_length_size < od->match_length) {
                        bits = optimal[index - best_length[best_length_size]]->bits +
                               elias_gamma_bits(best_length[best_length_size] - 1);
                        do {
                            best_length_size++;
                            bits2 = optimal[index - best_length_size]->bits +
                                    elias_gamma_bits(best_length_size - 1);
                            if (bits2 <= bits) {
                                best_length[best_length_size] = best_length_size;
                                bits = bits2;
                            } else {
                                best_length[best_length_size] =
                                    best_length[best_length_size - 1];
                            }
                        } while (best_length_size < od->match_length);
                    }
                    length = best_length[od->match_length];
                    int offset_msb = (offset - 1) >> 7;
                    bits = optimal[index - length]->bits + 8 +
                           elias_gamma_bits(offset_msb + 1) +
                           elias_gamma_bits(length - 1);
                    if (!od->last_match || od->last_match->index != index ||
                        od->last_match->bits > bits) {
                        zx0_assign(&od->last_match, zx0_allocate(bits, index, offset,
                                                                 optimal[index - length]));
                        if (!optimal[index] || optimal[index]->bits > bits)
                            zx0_assign(&optimal[index], od->last_match);
                    }
                }
            } else {
                od->match_length = 0;
                if (od->last_match) {
                    length = index - od->last_match->index;
                    bits = od->last_match->bits + 1 + elias_gamma_bits(length) +
                           (length << 3);
                    zx0_assign(&od->last_literal,
                               zx0_allocate(bits, index, 0, od->last_match));
                    if (!optimal[index] || optimal[index]->bits > bits)
                        zx0_assign(&optimal[index], od->last_literal);
                }
            }
        }
    }

    BLOCK *res = optimal[input_size - 1];
    free(offset_data);
    free(optimal);
    free(best_length);
    return res;
}

typedef struct {
    unsigned char *output_data;
    int output_index;
    int input_index;
    int bit_index;
    int bit_mask;
    int diff;
    int backtrack;
} CompressState;

static inline void read_bytes(CompressState *s, int n, int *delta) {
    s->input_index += n;
    s->diff += n;
    if (*delta < s->diff)
        *delta = s->diff;
}

static inline void write_byte(CompressState *s, int value) {
    s->output_data[s->output_index++] = (unsigned char)value;
    s->diff--;
}

static inline void write_bit(CompressState *s, int value) {
    if (s->backtrack) {
        if (value)
            s->output_data[s->output_index - 1] |= 1;
        s->backtrack = FALSE;
    } else {
        if (!s->bit_mask) {
            s->bit_mask = 128;
            s->bit_index = s->output_index;
            write_byte(s, 0);
        }
        if (value)
            s->output_data[s->bit_index] |= (unsigned char)s->bit_mask;
        s->bit_mask >>= 1;
    }
}

static void write_interlaced_elias_gamma(CompressState *s, int value, int backwards_mode, int invert_mode) {
    int i;
    for (i = 2; i <= value; i <<= 1)
        ;
    i >>= 1;
    while (i >>= 1) {
        write_bit(s, backwards_mode);
        write_bit(s, invert_mode ? !(value & i) : (value & i));
    }
    write_bit(s, !backwards_mode);
}

static unsigned char *zx0_compress_blocks(BLOCK *optimal, unsigned char *input_data,
                                          int input_size, int skip, int backwards_mode,
                                          int invert_mode, int *output_size, int *delta) {
    CompressState s;
    BLOCK *prev;
    BLOCK *next;
    int last_offset = INITIAL_OFFSET;
    int length;
    int i;

    *output_size = (optimal->bits + 25) / 8;
    s.output_data = (unsigned char *)malloc((size_t)*output_size);
    if (!s.output_data) {
        return NULL;
    }

    prev = NULL;
    while (optimal) {
        next = optimal->chain;
        optimal->chain = prev;
        prev = optimal;
        optimal = next;
    }

    s.diff = *output_size - input_size + skip;
    *delta = 0;
    s.input_index = skip;
    s.output_index = 0;
    s.bit_mask = 0;
    s.backtrack = TRUE;

    for (optimal = prev->chain; optimal; prev = optimal, optimal = optimal->chain) {
        length = optimal->index - prev->index;

        if (!optimal->offset) {
            write_bit(&s, 0);
            write_interlaced_elias_gamma(&s, length, backwards_mode, FALSE);
            for (i = 0; i < length; i++) {
                write_byte(&s, input_data[s.input_index]);
                read_bytes(&s, 1, delta);
            }
        } else if (optimal->offset == last_offset) {
            write_bit(&s, 0);
            write_interlaced_elias_gamma(&s, length, backwards_mode, FALSE);
            read_bytes(&s, length, delta);
        } else {
            write_bit(&s, 1);
            write_interlaced_elias_gamma(&s, (optimal->offset - 1) / 128 + 1,
                                         backwards_mode, invert_mode);
            if (backwards_mode)
                write_byte(&s, ((optimal->offset - 1) % 128) << 1);
            else
                write_byte(&s, (127 - (optimal->offset - 1) % 128) << 1);

            s.backtrack = TRUE;
            write_interlaced_elias_gamma(&s, length - 1, backwards_mode, FALSE);
            read_bytes(&s, length, delta);
            last_offset = optimal->offset;
        }
    }

    write_bit(&s, 1);
    write_interlaced_elias_gamma(&s, 256, backwards_mode, invert_mode);

    return s.output_data;
}

int zx0_compress_custom(const unsigned char *in, size_t in_sz,
                        unsigned char **out, size_t *out_sz,
                        int offset_limit) {
    if (!in || in_sz == 0 || !out || !out_sz)
        return -1;

    if (offset_limit <= 0) {
        offset_limit = (in_sz <= 65536) ? 32640 : 4096;
    }

    unsigned char *data = (unsigned char *)malloc(in_sz);
    if (!data)
        return -1;
    memcpy(data, in, in_sz);

    zx0_mem_reset();
    BLOCK *optimal_chain = zx0_optimize(data, (int)in_sz, 0, offset_limit);
    if (!optimal_chain) {
        free(data);
        zx0_mem_reset();
        return -1;
    }

    int out_size_i = 0, delta = 0;
    unsigned char *compressed = zx0_compress_blocks(optimal_chain, data, (int)in_sz,
                                                    0, 0, 1, &out_size_i, &delta);
    free(data);
    zx0_mem_reset();

    if (!compressed || out_size_i <= 0)
        return -1;

    *out_sz = (size_t)out_size_i;
    *out = compressed;
    zx0_emit_progress(in_sz, in_sz);
    return 0;
}
