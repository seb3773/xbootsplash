/*
 * bench_compression.c - Empirical benchmark of frame compression algorithms
 * Evaluates:
 *   - Baseline RLE XOR & Sparse XOR
 *   - Golomb-Rice (LOCO-I / JPEG-LS residual coding)
 *   - Elias-gamma LZ77 (ZX0)
 *   - Adaptive rANS LZ77 (upkr)
 *   - Channel Decorrelation (Split RGB RLE) + ZX0 / upkr
 *   - Raw XOR direct vs RLE intermediate
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <png.h>
#include <dirent.h>
#include <ctype.h>
#include <assert.h>

#include "zx0/zx0_compress.h"
#include "zx0/zx0_decompress.h"
#include "_work/upkr/c_library/upkr.h"

void* upkr_unpack(void* destination, void* compressed_data);

/* Structure to hold an RGB565 frame */
typedef struct {
    uint16_t *pixels;
    int width;
    int height;
    int count;
} FrameRGB565;

/* Load a PNG file and convert to RGB565 */
static int load_png_rgb565(const char *filename, FrameRGB565 *frame) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(fp); return -1; }

    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, NULL, NULL); fclose(fp); return -1; }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return -1;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png, info));
    }
    png_read_image(png, row_pointers);
    fclose(fp);

    frame->width = width;
    frame->height = height;
    frame->count = width * height;
    frame->pixels = (uint16_t *)malloc(width * height * sizeof(uint16_t));

    for (int y = 0; y < height; y++) {
        png_bytep row = row_pointers[y];
        for (int x = 0; x < width; x++) {
            png_bytep px = &(row[x * 4]);
            uint16_t r = (px[0] >> 3) & 0x1F;
            uint16_t g = (px[1] >> 2) & 0x3F;
            uint16_t b = (px[2] >> 3) & 0x1F;
            frame->pixels[y * width + x] = (r << 11) | (g << 5) | b;
        }
        free(row_pointers[y]);
    }
    free(row_pointers);
    png_destroy_read_struct(&png, &info, NULL);
    return 0;
}

static void free_frame(FrameRGB565 *frame) {
    if (frame->pixels) {
        free(frame->pixels);
        frame->pixels = NULL;
    }
}

static double get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec * 1e-3;
}

/* ========================================================================= */
/* ALGORITHM 1: Baseline RLE XOR (current method 0)                          */
/* ========================================================================= */

static size_t encode_rle_xor(const uint16_t *curr, const uint16_t *prev, int count, uint8_t *out, size_t out_max) {
    size_t out_pos = 0;
    int i = 0;
    while (i < count) {
        int same_run = 0;
        while (i + same_run < count && same_run < 128 && curr[i + same_run] == prev[i + same_run]) {
            same_run++;
        }
        if (same_run > 0) {
            if (out_pos >= out_max) return 0;
            out[out_pos++] = (uint8_t)(0x80 | (same_run - 1));
            i += same_run;
            continue;
        }

        int diff_run = 0;
        while (i + diff_run < count && diff_run < 127 && curr[i + diff_run] != prev[i + diff_run]) {
            diff_run++;
        }
        if (diff_run > 0) {
            if (out_pos + 1 + diff_run * 2 > out_max) return 0;
            out[out_pos++] = (uint8_t)diff_run;
            for (int j = 0; j < diff_run; j++) {
                uint16_t d = curr[i + j] ^ prev[i + j];
                out[out_pos++] = d & 0xFF;
                out[out_pos++] = (d >> 8) & 0xFF;
            }
            i += diff_run;
        }
    }
    if (out_pos < out_max) out[out_pos++] = 0;
    return out_pos;
}

static void decode_rle_xor(const uint8_t *in, uint16_t *frame_buffer, int count) {
    size_t in_pos = 0;
    int out_pos = 0;
    while (out_pos < count) {
        uint8_t cmd = in[in_pos++];
        if (cmd == 0) break;
        if (cmd & 0x80) {
            out_pos += (cmd & 0x7F) + 1;
        } else {
            int len = cmd;
            for (int j = 0; j < len; j++) {
                uint16_t val = in[in_pos] | ((uint16_t)in[in_pos + 1] << 8);
                in_pos += 2;
                frame_buffer[out_pos++] ^= val;
            }
        }
    }
}

/* ========================================================================= */
/* ALGORITHM 2: Baseline Sparse XOR (current method 2)                       */
/* ========================================================================= */

static size_t encode_sparse_xor(const uint16_t *curr, const uint16_t *prev, int count, uint8_t *out, size_t out_max) {
    int changed = 0;
    for (int i = 0; i < count; i++) {
        if ((curr[i] ^ prev[i]) != 0) changed++;
    }
    size_t needed = 4 + (size_t)changed * 6;
    if (needed > out_max) return 0;

    size_t pos = 0;
    out[pos++] = changed & 0xFF;
    out[pos++] = (changed >> 8) & 0xFF;
    out[pos++] = (changed >> 16) & 0xFF;
    out[pos++] = (changed >> 24) & 0xFF;

    for (int i = 0; i < count; i++) {
        uint16_t d = curr[i] ^ prev[i];
        if (d != 0) {
            out[pos++] = i & 0xFF;
            out[pos++] = (i >> 8) & 0xFF;
            out[pos++] = (i >> 16) & 0xFF;
            out[pos++] = (i >> 24) & 0xFF;
            out[pos++] = d & 0xFF;
            out[pos++] = (d >> 8) & 0xFF;
        }
    }
    return pos;
}

static void decode_sparse_xor(const uint8_t *in, uint16_t *frame_buffer, int count) {
    (void)count;
    uint32_t changed = in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
    size_t pos = 4;
    for (uint32_t c = 0; c < changed; c++) {
        uint32_t idx = in[pos] | ((uint32_t)in[pos+1] << 8) | ((uint32_t)in[pos+2] << 16) | ((uint32_t)in[pos+3] << 24);
        uint16_t val = in[pos+4] | ((uint16_t)in[pos+5] << 8);
        pos += 6;
        frame_buffer[idx] ^= val;
    }
}

/* ========================================================================= */
/* ALGORITHM 3: Channel-Split RLE XOR (Decoupled R5, G6, B5 color planes)     */
/* ========================================================================= */

static size_t encode_split_rgb_rle(const uint16_t *curr, const uint16_t *prev, int count, uint8_t *out) {
    size_t out_pos = 0;
    // R channel (5 bits: 0..31)
    int i = 0;
    while (i < count) {
        int same = 0;
        while (i + same < count && same < 128 && ((curr[i + same] ^ prev[i + same]) >> 11) == 0) same++;
        if (same > 0) { out[out_pos++] = 0x80 | (same - 1); i += same; continue; }
        int diff = 0;
        while (i + diff < count && diff < 127 && ((curr[i + diff] ^ prev[i + diff]) >> 11) != 0) diff++;
        if (diff > 0) {
            out[out_pos++] = diff;
            for (int j = 0; j < diff; j++) out[out_pos++] = (curr[i + j] ^ prev[i + j]) >> 11;
            i += diff;
        }
    }
    out[out_pos++] = 0;

    // G channel (6 bits: 0..63)
    i = 0;
    while (i < count) {
        int same = 0;
        while (i + same < count && same < 128 && (((curr[i + same] ^ prev[i + same]) >> 5) & 0x3F) == 0) same++;
        if (same > 0) { out[out_pos++] = 0x80 | (same - 1); i += same; continue; }
        int diff = 0;
        while (i + diff < count && diff < 127 && (((curr[i + diff] ^ prev[i + diff]) >> 5) & 0x3F) != 0) diff++;
        if (diff > 0) {
            out[out_pos++] = diff;
            for (int j = 0; j < diff; j++) out[out_pos++] = ((curr[i + j] ^ prev[i + j]) >> 5) & 0x3F;
            i += diff;
        }
    }
    out[out_pos++] = 0;

    // B channel (5 bits: 0..31)
    i = 0;
    while (i < count) {
        int same = 0;
        while (i + same < count && same < 128 && ((curr[i + same] ^ prev[i + same]) & 0x1F) == 0) same++;
        if (same > 0) { out[out_pos++] = 0x80 | (same - 1); i += same; continue; }
        int diff = 0;
        while (i + diff < count && diff < 127 && ((curr[i + diff] ^ prev[i + diff]) & 0x1F) != 0) diff++;
        if (diff > 0) {
            out[out_pos++] = diff;
            for (int j = 0; j < diff; j++) out[out_pos++] = (curr[i + j] ^ prev[i + j]) & 0x1F;
            i += diff;
        }
    }
    out[out_pos++] = 0;
    return out_pos;
}

static void decode_split_rgb_rle(const uint8_t *in, uint16_t *frame_buffer, int count) {
    size_t in_pos = 0;
    // R channel (bits 11..15)
    int pos = 0;
    while (pos < count) {
        uint8_t cmd = in[in_pos++];
        if (cmd == 0) break;
        if (cmd & 0x80) {
            pos += (cmd & 0x7F) + 1;
        } else {
            int len = cmd;
            for (int j = 0; j < len; j++) {
                frame_buffer[pos++] ^= ((uint16_t)in[in_pos++] << 11);
            }
        }
    }
    if (pos == count && in[in_pos] == 0) in_pos++;

    // G channel (bits 5..10)
    pos = 0;
    while (pos < count) {
        uint8_t cmd = in[in_pos++];
        if (cmd == 0) break;
        if (cmd & 0x80) {
            pos += (cmd & 0x7F) + 1;
        } else {
            int len = cmd;
            for (int j = 0; j < len; j++) {
                frame_buffer[pos++] ^= ((uint16_t)in[in_pos++] << 5);
            }
        }
    }
    if (pos == count && in[in_pos] == 0) in_pos++;

    // B channel (bits 0..4)
    pos = 0;
    while (pos < count) {
        uint8_t cmd = in[in_pos++];
        if (cmd == 0) break;
        if (cmd & 0x80) {
            pos += (cmd & 0x7F) + 1;
        } else {
            int len = cmd;
            for (int j = 0; j < len; j++) {
                frame_buffer[pos++] ^= (uint16_t)in[in_pos++];
            }
        }
    }
    if (pos == count && in[in_pos] == 0) in_pos++;
}

/* ========================================================================= */
/* ALGORITHM 4: Hybrid RLE + Golomb-Rice (LOCO-I residual model)             */
/* ========================================================================= */

typedef struct {
    uint8_t *buf;
    size_t max_size;
    size_t byte_pos;
    int bit_pos;
} BitWriter;

static inline void bw_init(BitWriter *bw, uint8_t *buf, size_t max_size) {
    bw->buf = buf;
    bw->max_size = max_size;
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    if (max_size > 0) bw->buf[0] = 0;
}

static inline void bw_write_bit(BitWriter *bw, int bit) {
    if (bw->byte_pos >= bw->max_size) return;
    if (bit) bw->buf[bw->byte_pos] |= (1 << (7 - bw->bit_pos));
    bw->bit_pos++;
    if (bw->bit_pos == 8) {
        bw->bit_pos = 0;
        bw->byte_pos++;
        if (bw->byte_pos < bw->max_size) bw->buf[bw->byte_pos] = 0;
    }
}

static inline void bw_write_bits(BitWriter *bw, uint32_t val, int nbits) {
    for (int i = nbits - 1; i >= 0; i--) bw_write_bit(bw, (val >> i) & 1);
}

static inline size_t bw_finish(BitWriter *bw) {
    return (bw->bit_pos > 0) ? (bw->byte_pos + 1) : bw->byte_pos;
}

typedef struct {
    const uint8_t *buf;
    size_t size;
    size_t byte_pos;
    int bit_pos;
} BitReader;

static inline void br_init(BitReader *br, const uint8_t *buf, size_t size) {
    br->buf = buf;
    br->size = size;
    br->byte_pos = 0;
    br->bit_pos = 0;
}

static inline int br_read_bit(BitReader *br) {
    if (br->byte_pos >= br->size) return 1;
    int bit = (br->buf[br->byte_pos] >> (7 - br->bit_pos)) & 1;
    br->bit_pos++;
    if (br->bit_pos == 8) {
        br->bit_pos = 0;
        br->byte_pos++;
    }
    return bit;
}

static inline uint32_t br_read_bits(BitReader *br, int nbits) {
    uint32_t val = 0;
    for (int i = 0; i < nbits; i++) val = (val << 1) | br_read_bit(br);
    return val;
}

#define RICE_ESCAPE_LIMIT 31

static inline void bw_write_rice(BitWriter *bw, uint32_t val, int k, int raw_bits) {
    uint32_t q = val >> k;
    if (q < RICE_ESCAPE_LIMIT) {
        for (uint32_t i = 0; i < q; i++) bw_write_bit(bw, 0);
        bw_write_bit(bw, 1);
        if (k > 0) {
            uint32_t r = val & ((1U << k) - 1);
            bw_write_bits(bw, r, k);
        }
    } else {
        for (int i = 0; i < RICE_ESCAPE_LIMIT; i++) bw_write_bit(bw, 0);
        bw_write_bit(bw, 1);
        bw_write_bits(bw, val, raw_bits);
    }
}

static inline uint32_t br_read_rice(BitReader *br, int k, int raw_bits) {
    uint32_t q = 0;
    while (!br_read_bit(br)) {
        q++;
        if (q == RICE_ESCAPE_LIMIT) {
            br_read_bit(br);
            return br_read_bits(br, raw_bits);
        }
    }
    uint32_t r = (k > 0) ? br_read_bits(br, k) : 0;
    return (q << k) | r;
}

static size_t encode_rle_rice(const uint16_t *curr, const uint16_t *prev, int count, uint8_t *out, size_t out_max) {
    int best_k = 3;
    size_t min_bits = (size_t)-1;
    for (int k = 0; k <= 8; k++) {
        size_t bits = 0;
        for (int i = 0; i < count; i++) {
            int32_t d = (int32_t)curr[i] - (int32_t)prev[i];
            if (d != 0) {
                uint32_t mag = (uint32_t)(abs(d) - 1);
                uint32_t q = mag >> k;
                if (q < RICE_ESCAPE_LIMIT) bits += q + 1 + k + 1;
                else bits += RICE_ESCAPE_LIMIT + 1 + 17 + 1;
            }
        }
        if (bits < min_bits) {
            min_bits = bits;
            best_k = k;
        }
    }

    BitWriter bw;
    bw_init(&bw, out, out_max);
    bw_write_bits(&bw, best_k, 4);

    int i = 0;
    while (i < count) {
        int zeros = 0;
        while (i + zeros < count && curr[i + zeros] == prev[i + zeros]) zeros++;
        if (zeros > 0) {
            bw_write_bit(&bw, 0);
            bw_write_rice(&bw, zeros - 1, 6, 24);
            i += zeros;
            if (i >= count) break;
        }

        bw_write_bit(&bw, 1);
        int32_t d = (int32_t)curr[i] - (int32_t)prev[i];
        uint32_t mag = (uint32_t)(abs(d) - 1);
        int sign = (d < 0) ? 1 : 0;
        bw_write_rice(&bw, mag, best_k, 17);
        bw_write_bit(&bw, sign);
        i++;
    }
    return bw_finish(&bw);
}

static void decode_rle_rice(const uint8_t *in, size_t in_size, uint16_t *frame_buffer, int count) {
    BitReader br;
    br_init(&br, in, in_size);
    int k = br_read_bits(&br, 4);

    int pos = 0;
    while (pos < count) {
        int tag = br_read_bit(&br);
        if (tag == 0) {
            uint32_t zeros = br_read_rice(&br, 6, 24) + 1;
            pos += zeros;
        } else {
            uint32_t mag = br_read_rice(&br, k, 17);
            int sign = br_read_bit(&br);
            int32_t delta = (int32_t)(mag + 1);
            if (sign) delta = -delta;
            frame_buffer[pos] = (uint16_t)((int32_t)frame_buffer[pos] + delta);
            pos++;
        }
    }
}

/* ========================================================================= */
/* BENCHMARK RUNNER                                                          */
/* ========================================================================= */

static void run_dataset_benchmark(const char *dataset_name, FrameRGB565 *frames, int nframes, FILE *out_md) {
    int pixels = frames[0].count;
    size_t raw_total = (size_t)nframes * pixels * sizeof(uint16_t);
    size_t raw_delta_total = (size_t)(nframes - 1) * pixels * sizeof(uint16_t);

    printf("\n================================================================================\n");
    printf(" DATASET: %s\n", dataset_name);
    printf(" Frames: %d | Resolution: %dx%d (%d pixels/frame)\n", nframes, frames[0].width, frames[0].height, pixels);
    printf(" Raw Delta Stream: %zu bytes (%.1f KB)\n", raw_delta_total, raw_delta_total / 1024.0);
    printf("================================================================================\n");

    size_t buf_max = (size_t)pixels * 4 + 65536;
    uint8_t *tmp_buf = (uint8_t *)malloc(buf_max);
    uint16_t *work_frame = (uint16_t *)malloc(pixels * sizeof(uint16_t));

    // Per-frame stream stores
    uint8_t **rle_xor_streams = (uint8_t **)malloc(sizeof(uint8_t *) * nframes);
    size_t *rle_xor_sizes = (size_t *)malloc(sizeof(size_t) * nframes);

    uint8_t **sparse_streams = (uint8_t **)malloc(sizeof(uint8_t *) * nframes);
    size_t *sparse_sizes = (size_t *)malloc(sizeof(size_t) * nframes);

    uint8_t **rle_rice_streams = (uint8_t **)malloc(sizeof(uint8_t *) * nframes);
    size_t *rle_rice_sizes = (size_t *)malloc(sizeof(size_t) * nframes);

    uint8_t **split_rle_streams = (uint8_t **)malloc(sizeof(uint8_t *) * nframes);
    size_t *split_rle_sizes = (size_t *)malloc(sizeof(size_t) * nframes);

    size_t total_rle_xor = 0;
    size_t total_sparse = 0;
    size_t total_rle_rice = 0;
    size_t total_split_rle = 0;

    for (int f = 1; f < nframes; f++) {
        rle_xor_sizes[f] = encode_rle_xor(frames[f].pixels, frames[f - 1].pixels, pixels, tmp_buf, buf_max);
        rle_xor_streams[f] = (uint8_t *)malloc(rle_xor_sizes[f]);
        memcpy(rle_xor_streams[f], tmp_buf, rle_xor_sizes[f]);
        total_rle_xor += rle_xor_sizes[f];

        sparse_sizes[f] = encode_sparse_xor(frames[f].pixels, frames[f - 1].pixels, pixels, tmp_buf, buf_max);
        sparse_streams[f] = (uint8_t *)malloc(sparse_sizes[f]);
        memcpy(sparse_streams[f], tmp_buf, sparse_sizes[f]);
        total_sparse += sparse_sizes[f];

        rle_rice_sizes[f] = encode_rle_rice(frames[f].pixels, frames[f - 1].pixels, pixels, tmp_buf, buf_max);
        rle_rice_streams[f] = (uint8_t *)malloc(rle_rice_sizes[f]);
        memcpy(rle_rice_streams[f], tmp_buf, rle_rice_sizes[f]);
        total_rle_rice += rle_rice_sizes[f];

        split_rle_sizes[f] = encode_split_rgb_rle(frames[f].pixels, frames[f - 1].pixels, pixels, tmp_buf);
        split_rle_streams[f] = (uint8_t *)malloc(split_rle_sizes[f]);
        memcpy(split_rle_streams[f], tmp_buf, split_rle_sizes[f]);
        total_split_rle += split_rle_sizes[f];
    }

    // Prepare concatenated streams for backends
    uint8_t *cat_rle_xor = (uint8_t *)malloc(total_rle_xor);
    size_t cat_pos = 0;
    for (int f = 1; f < nframes; f++) {
        memcpy(cat_rle_xor + cat_pos, rle_xor_streams[f], rle_xor_sizes[f]);
        cat_pos += rle_xor_sizes[f];
    }

    uint8_t *cat_split_rle = (uint8_t *)malloc(total_split_rle);
    cat_pos = 0;
    for (int f = 1; f < nframes; f++) {
        memcpy(cat_split_rle + cat_pos, split_rle_streams[f], split_rle_sizes[f]);
        cat_pos += split_rle_sizes[f];
    }

    // 1. ZX0 on RLE XOR
    unsigned char *zx0_out_rle = NULL;
    size_t zx0_size_rle = 0;
    zx0_compress_custom(cat_rle_xor, total_rle_xor, &zx0_out_rle, &zx0_size_rle, 2048);

    // 2. upkr (level 6) on RLE XOR
    uint8_t *upkr_out_rle = (uint8_t *)malloc(total_rle_xor * 2 + 1024);
    size_t upkr_size_rle = upkr_compress(upkr_out_rle, total_rle_xor * 2 + 1024, cat_rle_xor, total_rle_xor, 6);

    // 3. ZX0 on Split RGB RLE
    unsigned char *zx0_out_split = NULL;
    size_t zx0_size_split = 0;
    zx0_compress_custom(cat_split_rle, total_split_rle, &zx0_out_split, &zx0_size_split, 2048);

    // 4. upkr (level 6) on Split RGB RLE
    uint8_t *upkr_out_split = (uint8_t *)malloc(total_split_rle * 2 + 1024);
    size_t upkr_size_split = upkr_compress(upkr_out_split, total_split_rle * 2 + 1024, cat_split_rle, total_split_rle, 6);

    // Timing & Correctness verification
    int ok_rle_xor = 1, ok_sparse = 1, ok_rle_rice = 1, ok_split_rle = 1;
    int ok_zx0_rle = 0, ok_upkr_rle = 0, ok_zx0_split = 0, ok_upkr_split = 0;

    // Verify RLE XOR
    memcpy(work_frame, frames[0].pixels, pixels * sizeof(uint16_t));
    double t0 = get_time_us();
    for (int f = 1; f < nframes; f++) {
        decode_rle_xor(rle_xor_streams[f], work_frame, pixels);
        if (memcmp(work_frame, frames[f].pixels, pixels * sizeof(uint16_t)) != 0) ok_rle_xor = 0;
    }
    double t1 = get_time_us();
    double time_rle_xor = (t1 - t0) / (nframes - 1);

    // Verify Sparse XOR
    memcpy(work_frame, frames[0].pixels, pixels * sizeof(uint16_t));
    t0 = get_time_us();
    for (int f = 1; f < nframes; f++) {
        decode_sparse_xor(sparse_streams[f], work_frame, pixels);
        if (memcmp(work_frame, frames[f].pixels, pixels * sizeof(uint16_t)) != 0) ok_sparse = 0;
    }
    t1 = get_time_us();
    double time_sparse = (t1 - t0) / (nframes - 1);

    // Verify Hybrid RLE Rice
    memcpy(work_frame, frames[0].pixels, pixels * sizeof(uint16_t));
    t0 = get_time_us();
    for (int f = 1; f < nframes; f++) {
        decode_rle_rice(rle_rice_streams[f], rle_rice_sizes[f], work_frame, pixels);
        if (memcmp(work_frame, frames[f].pixels, pixels * sizeof(uint16_t)) != 0) ok_rle_rice = 0;
    }
    t1 = get_time_us();
    double time_rle_rice = (t1 - t0) / (nframes - 1);

    // Verify Split RGB RLE
    memcpy(work_frame, frames[0].pixels, pixels * sizeof(uint16_t));
    t0 = get_time_us();
    for (int f = 1; f < nframes; f++) {
        decode_split_rgb_rle(split_rle_streams[f], work_frame, pixels);
        if (memcmp(work_frame, frames[f].pixels, pixels * sizeof(uint16_t)) != 0) ok_split_rle = 0;
    }
    t1 = get_time_us();
    double time_split_rle = (t1 - t0) / (nframes - 1);

    // Backend decompression timing & verification
    uint8_t *decomp_buf = (uint8_t *)malloc(raw_delta_total * 2 + 65536);

    // Decode ZX0 RLE
    t0 = get_time_us();
    for (int rep = 0; rep < 10; rep++) {
        zx0_decompress_to(zx0_out_rle, (int)zx0_size_rle, decomp_buf, (int)total_rle_xor);
    }
    t1 = get_time_us();
    double time_zx0_rle = (t1 - t0) / 10.0 / (nframes - 1);
    ok_zx0_rle = (memcmp(decomp_buf, cat_rle_xor, total_rle_xor) == 0);

    // Decode upkr RLE
    t0 = get_time_us();
    for (int rep = 0; rep < 10; rep++) {
        upkr_unpack(decomp_buf, upkr_out_rle);
    }
    t1 = get_time_us();
    double time_upkr_rle = (t1 - t0) / 10.0 / (nframes - 1);
    ok_upkr_rle = (memcmp(decomp_buf, cat_rle_xor, total_rle_xor) == 0);

    // Decode ZX0 Split
    t0 = get_time_us();
    for (int rep = 0; rep < 10; rep++) {
        zx0_decompress_to(zx0_out_split, (int)zx0_size_split, decomp_buf, (int)total_split_rle);
    }
    t1 = get_time_us();
    double time_zx0_split = (t1 - t0) / 10.0 / (nframes - 1);
    ok_zx0_split = (memcmp(decomp_buf, cat_split_rle, total_split_rle) == 0);

    // Decode upkr Split
    t0 = get_time_us();
    for (int rep = 0; rep < 10; rep++) {
        upkr_unpack(decomp_buf, upkr_out_split);
    }
    t1 = get_time_us();
    double time_upkr_split = (t1 - t0) / 10.0 / (nframes - 1);
    ok_upkr_split = (memcmp(decomp_buf, cat_split_rle, total_split_rle) == 0);

    printf("%-36s | %10s | %7s | %12s | %s\n", "Pipeline / Algorithm", "Delta Size", "Ratio", "Decode (us/f)", "Status");
    printf("-------------------------------------+------------+---------+--------------+---------\n");
    printf("%-36s | %8.1f KB | %6.1f%% | %12s | MATCH\n", "0. Uncompressed Raw RGB565", (double)raw_delta_total / 1024.0, 100.0, "N/A");
    printf("%-36s | %8.1f KB | %6.1f%% | %9.1f us | %s\n", "1. Baseline RLE XOR", (double)total_rle_xor / 1024.0, 100.0 * total_rle_xor / raw_delta_total, time_rle_xor, ok_rle_xor ? "PASS" : "FAIL");
    printf("%-36s | %8.1f KB | %6.1f%% | %9.1f us | %s\n", "2. Baseline Sparse XOR", (double)total_sparse / 1024.0, 100.0 * total_sparse / raw_delta_total, time_sparse, ok_sparse ? "PASS" : "FAIL");
    printf("%-36s | %8.1f KB | %6.1f%% | %9.1f us | %s\n", "3. Hybrid RLE + Golomb-Rice", (double)total_rle_rice / 1024.0, 100.0 * total_rle_rice / raw_delta_total, time_rle_rice, ok_rle_rice ? "PASS" : "FAIL");
    printf("%-36s | %8.1f KB | %6.1f%% | %9.1f us | %s\n", "4. Split RGB RLE (No backend)", (double)total_split_rle / 1024.0, 100.0 * total_split_rle / raw_delta_total, time_split_rle, ok_split_rle ? "PASS" : "FAIL");
    printf("%-36s | %8.1f KB | %6.1f%% | %9.1f us | %s\n", "5. RLE XOR + ZX0 (Current best)", (double)zx0_size_rle / 1024.0, 100.0 * zx0_size_rle / raw_delta_total, time_zx0_rle + time_rle_xor, ok_zx0_rle ? "PASS" : "FAIL");
    printf("%-36s | %8.1f KB | %6.1f%% | %9.1f us | %s\n", "6. RLE XOR + upkr (lvl 6)", (double)upkr_size_rle / 1024.0, 100.0 * upkr_size_rle / raw_delta_total, time_upkr_rle + time_rle_xor, ok_upkr_rle ? "PASS" : "FAIL");
    printf("%-36s | %8.1f KB | %6.1f%% | %9.1f us | %s\n", "7. Split RGB RLE + ZX0", (double)zx0_size_split / 1024.0, 100.0 * zx0_size_split / raw_delta_total, time_zx0_split + time_split_rle, ok_zx0_split ? "PASS" : "FAIL");
    printf("%-36s | %8.1f KB | %6.1f%% | %9.1f us | %s\n", "8. Split RGB RLE + upkr (lvl 6)", (double)upkr_size_split / 1024.0, 100.0 * upkr_size_split / raw_delta_total, time_upkr_split + time_split_rle, ok_upkr_split ? "PASS" : "FAIL");

    if (out_md) {
        fprintf(out_md, "### Dataset: %s\n\n", dataset_name);
        fprintf(out_md, "- **Frames**: %d | **Resolution**: %dx%d (%d px/frame)\n", nframes, frames[0].width, frames[0].height, pixels);
        fprintf(out_md, "- **Raw Delta Stream**: %.1f KB\n\n", (double)raw_delta_total / 1024.0);
        fprintf(out_md, "| Pipeline / Algorithm | Compressed Delta | Ratio vs Raw | Decode Time/Frame | Memory Needed | Status |\n");
        fprintf(out_md, "| :--- | :---: | :---: | :---: | :---: | :---: |\n");
        fprintf(out_md, "| **Raw RGB565** | %.1f KB | 100.0%% | 0 µs | None | Baseline |\n", (double)raw_delta_total / 1024.0);
        fprintf(out_md, "| **Baseline RLE XOR** | %.1f KB | %.1f%% | %.1f µs | Zero-alloc | %s |\n", (double)total_rle_xor / 1024.0, 100.0 * total_rle_xor / raw_delta_total, time_rle_xor, ok_rle_xor ? "PASS" : "FAIL");
        fprintf(out_md, "| **Baseline Sparse XOR** | %.1f KB | %.1f%% | %.1f µs | Zero-alloc | %s |\n", (double)total_sparse / 1024.0, 100.0 * total_sparse / raw_delta_total, time_sparse, ok_sparse ? "PASS" : "FAIL");
        fprintf(out_md, "| **Hybrid RLE + Golomb-Rice** | %.1f KB | %.1f%% | %.1f µs | Zero-alloc | %s |\n", (double)total_rle_rice / 1024.0, 100.0 * total_rle_rice / raw_delta_total, time_rle_rice, ok_rle_rice ? "PASS" : "FAIL");
        fprintf(out_md, "| **Split RGB RLE (No backend)** | %.1f KB | %.1f%% | %.1f µs | Zero-alloc | %s |\n", (double)total_split_rle / 1024.0, 100.0 * total_split_rle / raw_delta_total, time_split_rle, ok_split_rle ? "PASS" : "FAIL");
        fprintf(out_md, "| **Current: RLE XOR + ZX0** | **%.1f KB** | **%.1f%%** | **%.1f µs** | **Buffer mmap** | **%s** |\n", (double)zx0_size_rle / 1024.0, 100.0 * zx0_size_rle / raw_delta_total, time_zx0_rle + time_rle_xor, ok_zx0_rle ? "PASS" : "FAIL");
        fprintf(out_md, "| **Candidate: RLE XOR + upkr** | **%.1f KB** | **%.1f%%** | **%.1f µs** | **Buffer mmap** | **%s** |\n", (double)upkr_size_rle / 1024.0, 100.0 * upkr_size_rle / raw_delta_total, time_upkr_rle + time_rle_xor, ok_upkr_rle ? "PASS" : "FAIL");
        fprintf(out_md, "| **Split RGB RLE + ZX0** | %.1f KB | %.1f%% | %.1f µs | Buffer mmap | %s |\n", (double)zx0_size_split / 1024.0, 100.0 * zx0_size_split / raw_delta_total, time_zx0_split + time_split_rle, ok_zx0_split ? "PASS" : "FAIL");
        fprintf(out_md, "| **Split RGB RLE + upkr** | %.1f KB | %.1f%% | %.1f µs | Buffer mmap | %s |\n\n", (double)upkr_size_split / 1024.0, 100.0 * upkr_size_split / raw_delta_total, time_upkr_split + time_split_rle, ok_upkr_split ? "PASS" : "FAIL");
    }

    free(tmp_buf);
    free(work_frame);
    free(cat_rle_xor);
    free(cat_split_rle);
    free(zx0_out_rle);
    free(upkr_out_rle);
    free(zx0_out_split);
    free(upkr_out_split);
    free(decomp_buf);

    for (int f = 1; f < nframes; f++) {
        free(rle_xor_streams[f]);
        free(sparse_streams[f]);
        free(rle_rice_streams[f]);
        free(split_rle_streams[f]);
    }
    free(rle_xor_streams);
    free(rle_xor_sizes);
    free(sparse_streams);
    free(sparse_sizes);
    free(rle_rice_streams);
    free(rle_rice_sizes);
    free(split_rle_streams);
    free(split_rle_sizes);
}

int main(int argc, char **argv) {
    printf("================================================================================\n");
    printf("   XBOOTSPLASH FRAME COMPRESSION: RICE vs RLE vs ZX0 vs UPKR EMPIRICAL SUITE    \n");
    printf("================================================================================\n");

    FILE *out_md = fopen("tests/BENCHMARK_RICE_COMPRESSION.md", "w");
    if (out_md) {
        fprintf(out_md, "# XBootsplash Frame Compression Benchmark: Rice vs RLE vs ZX0 vs upkr\n\n");
        fprintf(out_md, "Date: September 2026  \n");
        fprintf(out_md, "Context: Empirical evaluation comparing Golomb-Rice entropy coding (LOCO-I / JPEG-LS style), Elias-gamma LZ77 (ZX0), and adaptive rANS LZ77 (upkr) across packed and channel-split frame deltas for freestanding Linux boot splashes.\n\n");
        fprintf(out_md, "## Detailed Empirical Results\n\n");
    }

    // 1. Dataset 1: redsphere (3D smooth shaded sphere, 25 frames)
    {
        const char *dir = "/tmp/redsphere_extracted";
        FrameRGB565 frames[25];
        int loaded = 0;
        for (int i = 0; i < 25; i++) {
            char path[256];
            snprintf(path, sizeof(path), "%s/frame_%d.png", dir, i);
            if (load_png_rgb565(path, &frames[i]) == 0) loaded++;
        }
        if (loaded == 25) {
            run_dataset_benchmark("redsphere (Smooth 3D Spherical Shading)", frames, loaded, out_md);
            for (int i = 0; i < loaded; i++) free_frame(&frames[i]);
        }
    }

    // 2. Dataset 2: pong (Discrete 2D retro sprite, 25 frames)
    {
        const char *dir = "/tmp/pong_gif";
        FrameRGB565 frames[25];
        int loaded = 0;
        for (int i = 0; i < 25; i++) {
            char path[256];
            snprintf(path, sizeof(path), "%s/frame_%03d.png", dir, i);
            if (load_png_rgb565(path, &frames[i]) == 0) loaded++;
        }
        if (loaded == 25) {
            run_dataset_benchmark("pong (Flat 2D High-Contrast Sprite)", frames, loaded, out_md);
            for (int i = 0; i < loaded; i++) free_frame(&frames[i]);
        }
    }

    // 3. Dataset 3: fallout (Complex Dense Animation, 20 frames sample)
    {
        const char *dir = "/tmp/fallout_gif";
        FrameRGB565 frames[20];
        int loaded = 0;
        for (int i = 0; i < 20; i++) {
            char path[256];
            snprintf(path, sizeof(path), "%s/frame_%03d.png", dir, i);
            if (load_png_rgb565(path, &frames[i]) == 0) loaded++;
        }
        if (loaded == 20) {
            run_dataset_benchmark("fallout (Complex Dense Animation, 20 frames sample)", frames, loaded, out_md);
            for (int i = 0; i < loaded; i++) free_frame(&frames[i]);
        }
    }

    if (out_md) {
        fprintf(out_md, "## Comprehensive Architectural Synthesis\n\n");
        fprintf(out_md, "See detailed analysis in the executive summary below.\n");
        fclose(out_md);
        printf("\nDetailed Markdown report written to tests/BENCHMARK_RICE_COMPRESSION.md\n");
    }

    return 0;
}
