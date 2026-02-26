/*
 * generate_delta_v2.c - Generate compact RLE XOR delta frames
 * Build: gcc -O2 -o generate_delta_v2 generate_delta_v2.c -lpng
 * Usage: ./generate_delta_v2 <png_dir> > frames_delta.h
 * 
 * Format:
 *   - Frame 0: raw RGB565 (8KB)
 *   - Delta frames: RLE encoded
 *     - 0x00: end of frame
 *     - 0x01-0x7F: next N uint16_t values are deltas (apply XOR)
 *     - 0x80-0xFF: skip (N-0x80+1) pixels (no change)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <png.h>

#define FRAME_W 64
#define FRAME_H 64
#define FRAME_PIXELS (FRAME_W * FRAME_H)

static uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static int load_png_rgb565(const char *path, uint16_t *buffer) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    
    if (setjmp(png_jmpbuf(png))) {
        fclose(fp);
        return -1;
    }
    
    png_init_io(png, fp);
    png_read_info(png, info);
    
    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);
    
    if (width != FRAME_W || height != FRAME_H) {
        fclose(fp);
        return -1;
    }
    
    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    
    png_read_update_info(png, info);
    
    png_bytep *rows = malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++) {
        rows[y] = malloc(png_get_rowbytes(png, info));
    }
    png_read_image(png, rows);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t r = rows[y][x * 4 + 0];
            uint8_t g = rows[y][x * 4 + 1];
            uint8_t b = rows[y][x * 4 + 2];
            buffer[y * width + x] = rgb_to_rgb565(r, g, b);
        }
    }
    
    for (int y = 0; y < height; y++) free(rows[y]);
    free(rows);
    fclose(fp);
    png_destroy_read_struct(&png, &info, NULL);
    
    return 0;
}

static int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

/* Compute RLE-encoded delta and return size in bytes */
static size_t compute_delta(const uint16_t *curr, const uint16_t *prev, uint8_t *out) {
    size_t out_pos = 0;
    int i = 0;
    
    while (i < FRAME_PIXELS) {
        /* Count zeros (unchanged pixels) */
        int zeros = 0;
        while (i + zeros < FRAME_PIXELS) {
            uint16_t delta = curr[i + zeros] ^ prev[i + zeros];
            if (delta != 0) break;
            zeros++;
            if (zeros >= 128) break; /* Max skip = 128 */
        }
        
        if (zeros > 0) {
            /* Skip command: 0x80 + (zeros-1) */
            out[out_pos++] = 0x80 | (zeros - 1);
            i += zeros;
        }
        
        /* Count non-zeros (changed pixels) */
        int nonzeros = 0;
        while (i + nonzeros < FRAME_PIXELS) {
            uint16_t delta = curr[i + nonzeros] ^ prev[i + nonzeros];
            if (delta == 0) break;
            nonzeros++;
            if (nonzeros >= 127) break; /* Max run = 127 */
        }
        
        if (nonzeros > 0) {
            /* Run command: 0x00-0x7F = count */
            out[out_pos++] = nonzeros;
            
            /* Output delta values as uint16_t (little-endian) */
            for (int j = 0; j < nonzeros; j++) {
                uint16_t delta = curr[i + j] ^ prev[i + j];
                out[out_pos++] = delta & 0xFF;
                out[out_pos++] = (delta >> 8) & 0xFF;
            }
            i += nonzeros;
        }
    }
    
    /* End marker */
    out[out_pos++] = 0x00;
    
    return out_pos;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <png_dir>\n", argv[0]);
        return 1;
    }
    
    const char *dir_path = argv[1];
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("opendir");
        return 1;
    }
    
    char *files[256];
    int nfiles = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && nfiles < 256) {
        if (strstr(ent->d_name, ".png")) {
            files[nfiles] = malloc(512);
            snprintf(files[nfiles], 512, "%s/%s", dir_path, ent->d_name);
            nfiles++;
        }
    }
    closedir(dir);
    
    qsort(files, nfiles, sizeof(char *), compare_strings);
    
    /* Load all frames */
    uint16_t *frames[256];
    for (int i = 0; i < nfiles; i++) {
        frames[i] = malloc(FRAME_PIXELS * sizeof(uint16_t));
        if (load_png_rgb565(files[i], frames[i]) != 0) {
            fprintf(stderr, "Failed to load %s\n", files[i]);
            return 1;
        }
    }
    
    /* Compute deltas and store */
    uint8_t *deltas[256];
    size_t delta_sizes[256];
    size_t total_size = 0;
    
    /* Frame 0: raw */
    deltas[0] = malloc(FRAME_PIXELS * sizeof(uint16_t));
    for (int i = 0; i < FRAME_PIXELS; i++) {
        deltas[0][i * 2] = frames[0][i] & 0xFF;
        deltas[0][i * 2 + 1] = (frames[0][i] >> 8) & 0xFF;
    }
    delta_sizes[0] = FRAME_PIXELS * sizeof(uint16_t);
    total_size += delta_sizes[0];
    
    /* Delta frames */
    for (int f = 1; f < nfiles; f++) {
        deltas[f] = malloc(FRAME_PIXELS * 4); /* Max possible size */
        delta_sizes[f] = compute_delta(frames[f], frames[f-1], deltas[f]);
        total_size += delta_sizes[f];
    }
    
    /* Generate header */
    printf("/* Auto-generated delta frames - DO NOT EDIT */\n");
    printf("/* Total size: %zu bytes (%.1f KB) */\n\n", total_size, total_size / 1024.0);
    printf("#pragma once\n\n");
    printf("#define NFRAMES %d\n", nfiles);
    printf("#define FRAME_W %d\n", FRAME_W);
    printf("#define FRAME_H %d\n\n", FRAME_H);
    
    /* Frame 0: raw RGB565 */
    printf("/* Frame 0: raw RGB565 reference */\n");
    printf("static const uint8_t frame_0[%zu] = {\n", delta_sizes[0]);
    for (size_t i = 0; i < delta_sizes[0]; i++) {
        if (i % 16 == 0) printf("    ");
        printf("0x%02X", deltas[0][i]);
        if (i < delta_sizes[0] - 1) printf(",");
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n};\n\n");
    
    /* Delta frames */
    printf("/* Delta frames (RLE XOR)\n");
    printf(" * 0x00 = end\n");
    printf(" * 0x01-0x7F = next N uint16_t deltas (little-endian)\n");
    printf(" * 0x80-0xFF = skip (N-0x80+1) pixels\n");
    printf(" */\n\n");
    
    for (int f = 1; f < nfiles; f++) {
        printf("static const uint8_t frame_%d[%zu] = {\n", f, delta_sizes[f]);
        for (size_t i = 0; i < delta_sizes[f]; i++) {
            if (i % 16 == 0) printf("    ");
            printf("0x%02X", deltas[f][i]);
            if (i < delta_sizes[f] - 1) printf(",");
            if ((i + 1) % 16 == 0) printf("\n");
        }
        printf("\n};\n\n");
    }
    
    /* Frame pointer array */
    printf("static const uint8_t* const frames[NFRAMES] = {\n");
    for (int f = 0; f < nfiles; f++) {
        printf("    frame_%d,\n", f);
    }
    printf("};\n\n");
    
    /* Frame sizes */
    printf("static const uint16_t frame_sizes[NFRAMES] = {\n");
    for (int f = 0; f < nfiles; f++) {
        printf("    %zu,\n", delta_sizes[f]);
    }
    printf("};\n");
    
    /* Stats */
    fprintf(stderr, "\n=== SIZE COMPARISON ===\n");
    fprintf(stderr, "RAW RGB565: %d bytes (%.1f KB)\n", 
            nfiles * FRAME_PIXELS * 2, nfiles * FRAME_PIXELS * 2 / 1024.0);
    fprintf(stderr, "Delta RLE:  %zu bytes (%.1f KB)\n", total_size, total_size / 1024.0);
    fprintf(stderr, "Ratio: %.2fx smaller\n", (float)(nfiles * FRAME_PIXELS * 2) / total_size);
    
    /* Cleanup */
    for (int i = 0; i < nfiles; i++) {
        free(frames[i]);
        free(deltas[i]);
        free(files[i]);
    }
    
    return 0;
}
