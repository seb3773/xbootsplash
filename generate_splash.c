/*
 * generate_splash.c - Generate bootsplash data for multiple display modes
 * by seb3773 - https://github.com/seb3773
 * 
 * Build: gcc -O2 -o generate_splash generate_splash.c -lpng -lm
 * Usage: ./generate_splash [options] <input> > splash_data.h
 * 
 * Display modes:
 *   0 = Animation on solid background (default)
 *   1 = Animation on background image (full screen)
 *   2 = Static image on solid background
 *   3 = Static image full screen
 *
 * Options:
 *   -m <mode>      Display mode (0-3, default: 0)
 *   -x <offset>    Horizontal offset (default: 0)
 *   -y <offset>    Vertical offset (default: 0)
 *   -d <delay>     Frame delay in ms (default: 33, range: 1-1000)
 *   -l <loop>      Loop animation: 1=loop (default), 0=no loop (stay on last frame)
 *   -c <color>     Background color as RRGGBB hex (default: 000000)
 *   -b <image>     Background image for mode 1
 *   -r <w>x<h>     Target resolution for full screen modes (auto-detect if not set)
 *   -z <method>    Compression method: auto, rle_xor, rle_direct, sparse, raw (default: auto)
 *   -h             Show help
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <png.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>

/* Configuration */
static int display_mode = 0;
static int offset_x = 0;
static int offset_y = 0;
static int frame_delay_ms = 33;
static int loop = 1;  /* 1=loop, 0=stay on last frame */
static uint32_t bg_color = 0x000000;  /* RRGGBB */
static char *bg_image_path = NULL;
static int target_w = 0;
static int target_h = 0;
static int transp_warned = 0;  /* Only warn once about transparency */

/* Display modes */
#define MODE_ANIM_SOLID         0  /* Animation on solid background */
#define MODE_ANIM_IMAGE_CENTER 1  /* Animation on centered background image */
#define MODE_ANIM_IMAGE_FULL   2  /* Animation on fullscreen background image */
#define MODE_STATIC_CENTER     3  /* Static image centered on solid background */
#define MODE_STATIC_FULLSCREEN 4  /* Static image fullscreen */

/* Compression methods */
#define COMPRESS_RLE_XOR    0
#define COMPRESS_RLE_DIRECT 1
#define COMPRESS_SPARSE     2
#define COMPRESS_RAW        3
#define COMPRESS_AUTO       4
#define COMPRESS_PALETTE_LZSS 5  /* Static image: 8-bit palette + LZSS */
static int compress_method = COMPRESS_RLE_XOR;

/* Frame data */
typedef struct {
    char *path;
    char *tmp_path;
    int index;
} frame_entry_t;

/* Image buffer */
typedef struct {
    uint16_t *pixels;
    int w;
    int h;
} image_t;

static uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static void rgb565_to_rgb(uint16_t pixel, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = ((pixel >> 11) & 0x1F) << 3;
    *g = ((pixel >> 5) & 0x3F) << 2;
    *b = (pixel & 0x1F) << 3;
}

/* Extract frame index from filename using a more robust heuristic.
 * It looks for the number that changes between files.
 * If multiple numbers exist, it compares with the first frame found.
 */
static int extract_frame_index_smart(const char *filename, const char *pattern_ref) {
    if (!pattern_ref) {
        /* First frame: just pick the first number as a starting point */
        const char *p = filename;
        while (*p) {
            if (isdigit(*p)) {
                int num = 0;
                while (isdigit(*p)) {
                    num = num * 10 + (*p - '0');
                    p++;
                }
                return num;
            }
            p++;
        }
        return -1;
    }

    /* Compare with reference to find which number part changed */
    /* This is a simplified version of the logic: find the first difference in digits */
    const char *p1 = filename;
    const char *p2 = pattern_ref;
    
    while (*p1 && *p2) {
        if (isdigit(*p1) && isdigit(*p2)) {
            const char *start1 = p1;
            const char *start2 = p2;
            int num1 = 0, num2 = 0;
            while (isdigit(*p1)) num1 = num1 * 10 + (*p1++ - '0');
            while (isdigit(*p2)) num2 = num2 * 10 + (*p2++ - '0');
            
            if (num1 != num2) return num1;
            
            /* Same number, continue searching */
            continue;
        }
        if (*p1 != *p2) {
            /* Non-digit difference, just skip */
        }
        p1++; p2++;
    }
    
    /* Fallback to first number if no difference found or logic fails */
    const char *p = filename;
    while (*p) {
        if (isdigit(*p)) {
            int num = 0;
            while (isdigit(*p)) {
                num = num * 10 + (*p - '0');
                p++;
            }
            return num;
        }
        p++;
    }
    return -1;
}

static int compare_frames(const void *a, const void *b) {
    return ((frame_entry_t*)a)->index - ((frame_entry_t*)b)->index;
}

/* Escape shell metacharacters to prevent command injection */
static void shell_escape(char *dst, size_t dstsize, const char *src) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dstsize - 1; i++) {
        char c = src[i];
        /* Escape dangerous characters: $ ` " \ ' ! ; & | < > ( ) */
        if (c == '$' || c == '`' || c == '"' || c == '\\' || c == '\'' ||
            c == '!' || c == ';' || c == '&' || c == '|' || c == '<' ||
            c == '>' || c == '(' || c == ')') {
            if (j < dstsize - 2) {
                dst[j++] = '\\';
            }
        }
        dst[j++] = c;
    }
    dst[j] = '\0';
}

/* Check if PNG has transparency (alpha channel or tRNS chunk) */
static int png_has_alpha(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    
    unsigned char sig[8];
    if (fread(sig, 1, 8, fp) != 8 || png_sig_cmp(sig, 0, 8)) {
        fclose(fp);
        return 0;
    }
    
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }
    
    png_init_io(png, fp);
    png_read_info(png, info);
    
    png_byte color_type = png_get_color_type(png, info);
    int has_alpha = (color_type == PNG_COLOR_TYPE_RGBA || 
                     color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
                     png_get_valid(png, info, PNG_INFO_tRNS)) ? 1 : 0;
    
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    
    return has_alpha;
}

/* Flatten transparent PNG using imagemagick onto solid background */
static char *flatten_png(const char *path, uint32_t bg_hex) {
    static char tmp_path[512];
    static int counter = 0;
    char cmd[1024];
    char escaped_path[512];
    char escaped_tmp[512];
    
    /* Create unique temp file path per image */
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/flatten_%d_%d.png", getpid(), counter++);
    
    /* Build convert command: flatten onto background color */
    char bg_color_str[16];
    snprintf(bg_color_str, sizeof(bg_color_str), "#%06X", bg_hex);
    
    shell_escape(escaped_path, sizeof(escaped_path), path);
    shell_escape(escaped_tmp, sizeof(escaped_tmp), tmp_path);
    
    snprintf(cmd, sizeof(cmd),
             "convert \"%s\" -background \"%s\" -flatten \"%s\" 2>/dev/null",
             escaped_path, bg_color_str, escaped_tmp);
    
    if (system(cmd) != 0) {
        /* Fallback: just copy without flattening */
        snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", escaped_path, escaped_tmp);
        system(cmd);
    }
    
    return tmp_path;
}

/* Load PNG into RGB565 buffer */
static int load_png(const char *path, image_t *img) {
    const char *load_path = path;
    char *flattened_path = NULL;
    
    /* Check for transparency and flatten if needed */
    if (png_has_alpha(path)) {
        if (!transp_warned) {
            fprintf(stderr, "Warning: Transparent PNG detected in '%s'\n", path);
            fprintf(stderr, "         Flattening onto background color #%06X\n", bg_color);
            transp_warned = 1;
        }
        flattened_path = flatten_png(path, bg_color);
        load_path = flattened_path;
    }
    
    FILE *fp = fopen(load_path, "rb");
    if (!fp) return -1;
    
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    
    if (setjmp(png_jmpbuf(png))) {
        fclose(fp);
        return -1;
    }
    
    png_init_io(png, fp);
    png_read_info(png, info);
    
    int w = png_get_image_width(png, info);
    int h = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);
    
    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    
    png_read_update_info(png, info);
    
    png_bytep *rows = malloc(sizeof(png_bytep) * h);
    for (int y = 0; y < h; y++) {
        rows[y] = malloc(png_get_rowbytes(png, info));
    }
    png_read_image(png, rows);
    
    img->w = w;
    img->h = h;
    img->pixels = malloc(w * h * sizeof(uint16_t));
    
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t r = rows[y][x * 4 + 0];
            uint8_t g = rows[y][x * 4 + 1];
            uint8_t b = rows[y][x * 4 + 2];
            img->pixels[y * w + x] = rgb_to_rgb565(r, g, b);
        }
    }
    
    for (int y = 0; y < h; y++) free(rows[y]);
    free(rows);
    fclose(fp);
    png_destroy_read_struct(&png, &info, NULL);
    
    /* Clean up flattened temp file if created */
    if (flattened_path) {
        unlink(flattened_path);
    }
    
    return 0;
}

/* Bilinear interpolation for resizing */
static uint16_t sample_bilinear(const image_t *src, float x, float y) {
    int x0 = (int)x;
    int y0 = (int)y;
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= src->w) x1 = src->w - 1;
    if (y1 >= src->h) y1 = src->h - 1;
    
    float fx = x - x0;
    float fy = y - y0;
    
    uint8_t r0, g0, b0, r1, g1, b1, r2, g2, b2, r3, g3, b3;
    rgb565_to_rgb(src->pixels[y0 * src->w + x0], &r0, &g0, &b0);
    rgb565_to_rgb(src->pixels[y0 * src->w + x1], &r1, &g1, &b1);
    rgb565_to_rgb(src->pixels[y1 * src->w + x0], &r2, &g2, &b2);
    rgb565_to_rgb(src->pixels[y1 * src->w + x1], &r3, &g3, &b3);
    
    uint8_t r = (uint8_t)(r0 * (1-fx) * (1-fy) + r1 * fx * (1-fy) + 
                          r2 * (1-fx) * fy + r3 * fx * fy);
    uint8_t g = (uint8_t)(g0 * (1-fx) * (1-fy) + g1 * fx * (1-fy) + 
                          g2 * (1-fx) * fy + g3 * fx * fy);
    uint8_t b = (uint8_t)(b0 * (1-fx) * (1-fy) + b1 * fx * (1-fy) + 
                          b2 * (1-fx) * fy + b3 * fx * fy);
    
    return rgb_to_rgb565(r, g, b);
}

/* Resize image using bilinear interpolation */
static image_t* resize_image(const image_t *src, int new_w, int new_h) {
    image_t *dst = malloc(sizeof(image_t));
    dst->w = new_w;
    dst->h = new_h;
    dst->pixels = malloc(new_w * new_h * sizeof(uint16_t));
    
    float x_ratio = (float)src->w / new_w;
    float y_ratio = (float)src->h / new_h;
    
    for (int y = 0; y < new_h; y++) {
        for (int x = 0; x < new_w; x++) {
            float src_x = x * x_ratio;
            float src_y = y * y_ratio;
            dst->pixels[y * new_w + x] = sample_bilinear(src, src_x, src_y);
        }
    }
    
    return dst;
}

/* RLE Direct compression */
static size_t compress_rle_direct(const uint16_t *pixels, int count, uint8_t *out) {
    size_t pos = 0;
    int i = 0;
    
    while (i < count) {
        uint16_t val = pixels[i];
        int run = 1;
        
        while (i + run < count && run < 127) {
            if (pixels[i + run] != val) break;
            run++;
        }
        
        if (run >= 3) {
            out[pos++] = 0x80 | run;
            out[pos++] = val & 0xFF;
            out[pos++] = (val >> 8) & 0xFF;
            i += run;
        } else {
            int lit = 0;
            while (i + lit < count && lit < 127) {
                if (i + lit + 2 < count &&
                    pixels[i + lit] == pixels[i + lit + 1] &&
                    pixels[i + lit + 1] == pixels[i + lit + 2]) {
                    break;
                }
                lit++;
            }
            if (lit == 0) lit = 1;
            
            out[pos++] = lit;
            for (int j = 0; j < lit; j++) {
                out[pos++] = pixels[i + j] & 0xFF;
                out[pos++] = (pixels[i + j] >> 8) & 0xFF;
            }
            i += lit;
        }
    }
    
    out[pos++] = 0x00;
    return pos;
}

/* RLE XOR compression */
static size_t compress_rle_xor(const uint16_t *curr, const uint16_t *prev, int count, uint8_t *out) {
    size_t pos = 0;
    int i = 0;
    
    while (i < count) {
        int zeros = 0;
        while (i + zeros < count && zeros < 128) {
            if ((curr[i + zeros] ^ prev[i + zeros]) != 0) break;
            zeros++;
        }
        
        if (zeros > 0) {
            out[pos++] = 0x80 | (zeros - 1);
            i += zeros;
        }
        
        int nonzeros = 0;
        while (i + nonzeros < count && nonzeros < 127) {
            if ((curr[i + nonzeros] ^ prev[i + nonzeros]) == 0) break;
            nonzeros++;
        }
        
        if (nonzeros > 0) {
            out[pos++] = nonzeros;
            for (int j = 0; j < nonzeros; j++) {
                uint16_t delta = curr[i + j] ^ prev[i + j];
                out[pos++] = delta & 0xFF;
                out[pos++] = (delta >> 8) & 0xFF;
            }
            i += nonzeros;
        }
    }
    
    out[pos++] = 0x00;
    return pos;
}

/* Sparse XOR compression (position + value for changed pixels) */
/* WARNING: Limited to frames <= 65535 pixels (16-bit index overflow) */
static size_t compress_sparse_xor(const uint16_t *curr, const uint16_t *prev, int count, uint8_t *out) {
    /* Validate frame size for 16-bit pixel indices */
    if (count > 65535) {
        fprintf(stderr, "Warning: Sparse XOR not suitable for frames > 65535 pixels (frame has %d)\n", count);
        /* Return 0 to indicate this method should not be used */
        return 0;
    }
    
    size_t pos = 0;
    
    /* Count changed pixels first */
    int changed = 0;
    for (int i = 0; i < count; i++) {
        if ((curr[i] ^ prev[i]) != 0) changed++;
    }
    
    /* Header: number of changed pixels (16-bit) */
    out[pos++] = changed & 0xFF;
    out[pos++] = (changed >> 8) & 0xFF;
    
    /* For each changed pixel: position (16-bit) + XOR value (16-bit) */
    for (int i = 0; i < count; i++) {
        uint16_t delta = curr[i] ^ prev[i];
        if (delta != 0) {
            out[pos++] = i & 0xFF;
            out[pos++] = (i >> 8) & 0xFF;
            out[pos++] = delta & 0xFF;
            out[pos++] = (delta >> 8) & 0xFF;
        }
    }
    
    return pos;
}

/* Raw XOR compression (no compression, just XOR values) */
static size_t compress_raw_xor(const uint16_t *curr, const uint16_t *prev, int count, uint8_t *out) {
    size_t pos = 0;
    for (int i = 0; i < count; i++) {
        uint16_t delta = curr[i] ^ prev[i];
        out[pos++] = delta & 0xFF;
        out[pos++] = (delta >> 8) & 0xFF;
    }
    return pos;
}

/* Raw RGB565 (no compression, direct pixel values) */
static size_t compress_raw_direct(const uint16_t *pixels, int count, uint8_t *out) {
    for (int i = 0; i < count; i++) {
        out[i*2] = pixels[i] & 0xFF;
        out[i*2+1] = (pixels[i] >> 8) & 0xFF;
    }
    return count * 2;
}

/* --- Palette + LZSS compression for static images --- */

/* Build palette from image, return number of unique colors (max 256) */
static int build_palette(const uint16_t *pixels, int count, uint16_t *palette, uint8_t *indices) {
    int num_colors = 0;
    
    /* Simple linear search - good enough for 256 colors */
    for (int i = 0; i < count; i++) {
        uint16_t color = pixels[i];
        int found = -1;
        
        /* Search in existing palette */
        for (int j = 0; j < num_colors; j++) {
            if (palette[j] == color) {
                found = j;
                break;
            }
        }
        
        if (found >= 0) {
            indices[i] = (uint8_t)found;
        } else if (num_colors < 256) {
            palette[num_colors] = color;
            indices[i] = (uint8_t)num_colors;
            num_colors++;
        } else {
            /* Palette full - should not happen if image was quantized */
            /* Use closest color (simple: just use last) */
            indices[i] = 255;
        }
    }
    
    return num_colors;
}

/* LZSS compression parameters */
#define LZSS_WINDOW_SIZE 4096
#define LZSS_MIN_MATCH  3
#define LZSS_MAX_MATCH  18

/* LZSS compress byte array (indices) */
static size_t compress_lzss(const uint8_t *data, int count, uint8_t *out) {
    size_t out_pos = 0;
    int in_pos = 0;
    uint8_t window[LZSS_WINDOW_SIZE];
    int window_pos = 0;
    
    /* Initialize window with zeros */
    memset(window, 0, LZSS_WINDOW_SIZE);
    
    /* Flag byte: 1 = literal, 0 = back-reference */
    /* Format: [flag_byte][8 items] */
    /*   literal: 1 byte */
    /*   back-ref: 2 bytes (offset:12bits, length:4bits) */
    
    uint8_t flag_byte = 0;
    uint8_t items[16];  /* Max 8 literals or 16 bytes for 8 back-refs */
    int item_count = 0;
    int bit_pos = 0;
    
    while (in_pos < count) {
        /* Find longest match in window */
        int best_len = 0;
        int best_off = 0;
        
        int search_start = (window_pos > LZSS_WINDOW_SIZE) ? window_pos - LZSS_WINDOW_SIZE : 0;
        int max_search = (window_pos < LZSS_WINDOW_SIZE) ? window_pos : LZSS_WINDOW_SIZE;
        
        for (int off = 1; off <= max_search; off++) {
            int win_idx = (window_pos - off + LZSS_WINDOW_SIZE) % LZSS_WINDOW_SIZE;
            int len = 0;
            
            while (len < LZSS_MAX_MATCH && in_pos + len < count) {
                int w = (win_idx + len) % LZSS_WINDOW_SIZE;
                if (window[w] != data[in_pos + len]) break;
                len++;
            }
            
            if (len >= LZSS_MIN_MATCH && len > best_len) {
                best_len = len;
                best_off = off;
            }
        }
        
        if (best_len >= LZSS_MIN_MATCH) {
            /* Back-reference: flag bit = 0 */
            /* Encode: offset (12 bits) + length-3 (4 bits) */
            int encoded_len = best_len - LZSS_MIN_MATCH;
            items[item_count++] = best_off & 0xFF;
            items[item_count++] = ((best_off >> 4) & 0xF0) | (encoded_len & 0x0F);
            /* flag bit already 0 */
            
            /* Advance */
            for (int i = 0; i < best_len; i++) {
                window[window_pos] = data[in_pos];
                window_pos = (window_pos + 1) % LZSS_WINDOW_SIZE;
                in_pos++;
            }
        } else {
            /* Literal: flag bit = 1 */
            flag_byte |= (1 << bit_pos);
            items[item_count++] = data[in_pos];
            
            window[window_pos] = data[in_pos];
            window_pos = (window_pos + 1) % LZSS_WINDOW_SIZE;
            in_pos++;
        }
        
        bit_pos++;
        
        /* Flush when we have 8 bits */
        if (bit_pos == 8) {
            out[out_pos++] = flag_byte;
            for (int i = 0; i < item_count; i++) {
                out[out_pos++] = items[i];
            }
            flag_byte = 0;
            item_count = 0;
            bit_pos = 0;
        }
    }
    
    /* Flush remaining */
    if (bit_pos > 0) {
        out[out_pos++] = flag_byte;
        for (int i = 0; i < item_count; i++) {
            out[out_pos++] = items[i];
        }
    }
    
    return out_pos;
}

/* Output palette + LZSS compressed background image (for hybrid modes) */
static void output_bg_palette_lzss(const uint16_t *palette, int num_colors,
                                   const uint8_t *compressed, size_t comp_size) {
    printf("static const uint16_t bg_palette[%d] = {\n", num_colors);
    for (int i = 0; i < num_colors; i++) {
        if (i % 12 == 0) printf("    ");
        printf("0x%04X", palette[i]);
        if (i < num_colors - 1) printf(",");
        if ((i + 1) % 12 == 0) printf("\n");
    }
    printf("\n};\n\n");
    
    printf("static const uint8_t bg_compressed[%zu] = {\n", comp_size);
    for (size_t i = 0; i < comp_size; i++) {
        if (i % 16 == 0) printf("    ");
        printf("0x%02X", compressed[i]);
        if (i < comp_size - 1) printf(",");
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n};\n\n");
}

/* Output palette + LZSS compressed static image */
static void output_palette_lzss(const uint16_t *palette, int num_colors,
                                 const uint8_t *compressed, size_t comp_size,
                                 int w, int h) {
    printf("/* Palette + LZSS compressed static image */\n");
    printf("#define PALETTE_SIZE %d\n", num_colors);
    printf("#define IMG_COMPRESSED_SIZE %zu\n\n", comp_size);
    
    printf("static const uint16_t palette[%d] = {\n", num_colors);
    for (int i = 0; i < num_colors; i++) {
        if (i % 12 == 0) printf("    ");
        printf("0x%04X", palette[i]);
        if (i < num_colors - 1) printf(",");
        if ((i + 1) % 12 == 0) printf("\n");
    }
    printf("\n};\n\n");
    
    printf("static const uint8_t img_compressed[%zu] = {\n", comp_size);
    for (size_t i = 0; i < comp_size; i++) {
        if (i % 16 == 0) printf("    ");
        printf("0x%02X", compressed[i]);
        if (i < comp_size - 1) printf(",");
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n};\n\n");
}

/* Output raw image data */
static void output_image_data(const char *name, const uint16_t *pixels, int w, int h) {
    printf("static const uint16_t %s[%d] = {\n", name, w * h);
    for (int i = 0; i < w * h; i++) {
        if (i % 12 == 0) printf("    ");
        printf("0x%04X", pixels[i]);
        if (i < w * h - 1) printf(",");
        if ((i + 1) % 12 == 0) printf("\n");
    }
    printf("\n};\n\n");
}

/* Output compressed frame data */
static void output_frame_data(int frame_num, const uint8_t *data, size_t size) {
    printf("static const uint8_t frame_%d[%zu] = {\n", frame_num, size);
    for (size_t i = 0; i < size; i++) {
        if (i % 16 == 0) printf("    ");
        printf("0x%02X", data[i]);
        if (i < size - 1) printf(",");
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n};\n\n");
}

static void print_help(const char *prog) {
    fprintf(stderr, "Usage: %s [options] <input>\n", prog);
    fprintf(stderr, "\nDisplay modes:\n");
    fprintf(stderr, "  0 = Animation on solid background (default)\n");
    fprintf(stderr, "  1 = Animation on background image (centered)\n");
    fprintf(stderr, "  2 = Animation on background image (fullscreen)\n");
    fprintf(stderr, "  3 = Static image on solid background (centered)\n");
    fprintf(stderr, "  4 = Static image fullscreen\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -m <mode>      Display mode (0-4)\n");
    fprintf(stderr, "  -x <offset>    Horizontal offset (default: 0)\n");
    fprintf(stderr, "  -y <offset>    Vertical offset (default: 0)\n");
    fprintf(stderr, "  -d <delay>     Frame delay in ms (default: 33)\n");
    fprintf(stderr, "  -l <0|1>       Loop animation: 1=loop (default), 0=stay on last frame\n");
    fprintf(stderr, "  -c <color>     Background color RRGGBB hex (default: 000000)\n");
    fprintf(stderr, "  -b <image>     Background image for modes 1,2\n");
    fprintf(stderr, "  -r <W>x<H>     Target resolution for fullscreen modes\n");
    fprintf(stderr, "  -z <method>    Compression: rle_xor, rle_direct, sparse, raw, auto\n");
    fprintf(stderr, "  -h             Show help\n");
}

int main(int argc, char *argv[]) {
    char *input_path = NULL;
    
    /* Parse arguments */
    int arg_idx = 1;
    while (arg_idx < argc) {
        if (strcmp(argv[arg_idx], "-m") == 0 && arg_idx + 1 < argc) {
            display_mode = atoi(argv[++arg_idx]);
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-x") == 0 && arg_idx + 1 < argc) {
            offset_x = atoi(argv[++arg_idx]);
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-y") == 0 && arg_idx + 1 < argc) {
            offset_y = atoi(argv[++arg_idx]);
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-d") == 0 && arg_idx + 1 < argc) {
            frame_delay_ms = atoi(argv[++arg_idx]);
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-l") == 0 && arg_idx + 1 < argc) {
            loop = atoi(argv[++arg_idx]);
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-c") == 0 && arg_idx + 1 < argc) {
            bg_color = (uint32_t)strtol(argv[++arg_idx], NULL, 16);
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-b") == 0 && arg_idx + 1 < argc) {
            bg_image_path = argv[++arg_idx];
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-r") == 0 && arg_idx + 1 < argc) {
            sscanf(argv[++arg_idx], "%dx%d", &target_w, &target_h);
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-z") == 0 && arg_idx + 1 < argc) {
            const char *method = argv[++arg_idx];
            if (strcmp(method, "rle_xor") == 0) compress_method = COMPRESS_RLE_XOR;
            else if (strcmp(method, "rle_direct") == 0) compress_method = COMPRESS_RLE_DIRECT;
            else if (strcmp(method, "sparse") == 0) compress_method = COMPRESS_SPARSE;
            else if (strcmp(method, "raw") == 0) compress_method = COMPRESS_RAW;
            else if (strcmp(method, "auto") == 0) compress_method = COMPRESS_AUTO;
            else fprintf(stderr, "Warning: Unknown compression method '%s', using default\n", method);
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-h") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (argv[arg_idx][0] != '-') {
            input_path = argv[arg_idx];
            arg_idx++;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[arg_idx]);
            return 1;
        }
    }
    
    if (!input_path) {
        fprintf(stderr, "Error: No input specified\n");
        print_help(argv[0]);
        return 1;
    }
    
    const char *mode_names[] = {
        "Animation on solid background",
        "Animation on background image (centered)",
        "Animation on background image (fullscreen)",
        "Static image on solid background (centered)",
        "Static image full screen"
    };
    
    fprintf(stderr, "Display mode: %d (%s)\n", display_mode, mode_names[display_mode]);
    fprintf(stderr, "Offsets: X=%d, Y=%d\n", offset_x, offset_y);
    fprintf(stderr, "Background color: #%06X\n", bg_color);
    
    /* Output header preamble */
    printf("/* Auto-generated splash data - DO NOT EDIT */\n");
    printf("/* Mode: %s */\n\n", mode_names[display_mode]);
    printf("#pragma once\n\n");
    
    printf("#define DISPLAY_MODE %d\n", display_mode);
    printf("#define HORIZONTAL_OFFSET %d\n", offset_x);
    printf("#define VERTICAL_OFFSET %d\n", offset_y);
    printf("#define BACKGROUND_COLOR 0x%04X\n", 
           rgb_to_rgb565((bg_color >> 16) & 0xFF, (bg_color >> 8) & 0xFF, bg_color & 0xFF));
    
    if (display_mode == MODE_ANIM_SOLID || display_mode == MODE_ANIM_IMAGE_CENTER || display_mode == MODE_ANIM_IMAGE_FULL) {
        printf("#define FRAME_DURATION_MS %d\n", frame_delay_ms);
        printf("#define LOOP %d  /* 1=loop, 0=stay on last frame */\n", loop);
    }
    
    /* Handle different modes */
    if (display_mode == MODE_STATIC_CENTER || display_mode == MODE_STATIC_FULLSCREEN) {
        /* Single static image - convert to standard format first */
        char tmp_path[512];
        char cmd[1024];
        char escaped_path[512];
        char escaped_tmp[512];
        char bg_color_str[16];
        
        snprintf(bg_color_str, sizeof(bg_color_str), "#%06X", bg_color);
        snprintf(tmp_path, sizeof(tmp_path), "/tmp/splash_static_%d.png", getpid());
        
        shell_escape(escaped_path, sizeof(escaped_path), input_path);
        shell_escape(escaped_tmp, sizeof(escaped_tmp), tmp_path);
        
        /* Convert to RGB, flatten alpha - handles colormap and transparent PNGs */
        snprintf(cmd, sizeof(cmd),
                 "convert \"%s\" -background \"%s\" -flatten -type TrueColor -depth 8 PNG24:\"%s\" 2>/dev/null",
                 escaped_path, bg_color_str, escaped_tmp);
        if (system(cmd) != 0) {
            /* Fallback: try without flatten */
            snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", escaped_path, escaped_tmp);
            system(cmd);
        }
        
        image_t img;
        if (load_png(tmp_path, &img) != 0) {
            fprintf(stderr, "Error: Failed to load image: %s\n", input_path);
            unlink(tmp_path);
            return 1;
        }
        
        /* Clean up temp file */
        unlink(tmp_path);
        
        fprintf(stderr, "Image: %dx%d\n", img.w, img.h);
        
        if (display_mode == MODE_STATIC_FULLSCREEN && target_w > 0 && target_h > 0) {
            if (img.w != target_w || img.h != target_h) {
                fprintf(stderr, "Resizing to %dx%d...\n", target_w, target_h);
                image_t *resized = resize_image(&img, target_w, target_h);
                free(img.pixels);
                img = *resized;
                free(resized);
            }
            printf("#define FRAME_W %d\n", img.w);
            printf("#define FRAME_H %d\n", img.h);
        } else {
            printf("#define FRAME_W %d\n", img.w);
            printf("#define FRAME_H %d\n", img.h);
        }
        
        printf("#define NFRAMES 1\n\n");
        
        /* Compress image with palette + LZSS */
        int pixel_count = img.w * img.h;
        uint16_t *palette = malloc(256 * sizeof(uint16_t));
        uint8_t *indices = malloc(pixel_count);
        uint8_t *compressed = malloc(pixel_count * 2);  /* Worst case */
        
        /* Build palette from image */
        int num_colors = build_palette(img.pixels, pixel_count, palette, indices);
        fprintf(stderr, "Palette: %d unique colors\n", num_colors);
        
        /* Compress indices with LZSS */
        size_t comp_size = compress_lzss(indices, pixel_count, compressed);
        fprintf(stderr, "LZSS compressed: %zu bytes (%.1f%% of raw)\n", 
                comp_size, 100.0 * comp_size / (pixel_count * 2));
        
        /* Output compressed data */
        printf("#define COMPRESS_METHOD %d  /* PALETTE_LZSS */\n", COMPRESS_PALETTE_LZSS);
        output_palette_lzss(palette, num_colors, compressed, comp_size, img.w, img.h);
        
        free(palette);
        free(indices);
        free(compressed);
        free(img.pixels);
        
    } else if (display_mode == MODE_ANIM_SOLID) {
        /* Animation on solid background */
        DIR *dir = opendir(input_path);
        if (!dir) {
            fprintf(stderr, "Error: Cannot open directory: %s\n", input_path);
            return 1;
        }
        
        /* Collect frames */
        frame_entry_t *frames = malloc(sizeof(frame_entry_t) * 256);
        int nframes = 0;
        struct dirent *ent;
        char tmpdir[256];
        snprintf(tmpdir, sizeof(tmpdir), "/tmp/splash_frames_%d", getpid());
        mkdir(tmpdir, 0755);
        
        /* First pass: collect all frame filenames */
        while ((ent = readdir(dir)) != NULL && nframes < 256) {
            if (strstr(ent->d_name, ".png") || strstr(ent->d_name, ".PNG") ||
                strstr(ent->d_name, ".jpg") || strstr(ent->d_name, ".JPG") ||
                strstr(ent->d_name, ".jpeg") || strstr(ent->d_name, ".JPEG")) {
                frames[nframes].path = malloc(512);
                frames[nframes].tmp_path = malloc(512);
                snprintf(frames[nframes].path, 512, "%s/%s", input_path, ent->d_name);
                snprintf(frames[nframes].tmp_path, 512, "%s/%s", tmpdir, ent->d_name);
                frames[nframes].index = -1;  /* Will be set after sorting */
                nframes++;
                if (nframes >= 256) {
                    fprintf(stderr, "Warning: Frame limit reached (256 max). Additional frames will be ignored.\n");
                }
            }
        }
        closedir(dir);
        
        if (nframes == 0) {
            fprintf(stderr, "Error: No frames found\n");
            return 1;
        }
        
        if (nframes >= 256) {
            fprintf(stderr, "Warning: Animation truncated to 256 frames. Consider splitting into multiple sequences.\n");
        }
        
        /* Sort by filename first to ensure consistent reference frame */
        qsort(frames, nframes, sizeof(frame_entry_t), compare_frames);
        
        /* Second pass: extract frame indices using sorted first frame as reference */
        const char *reference_name = NULL;
        for (int i = 0; i < nframes; i++) {
            const char *filename = strrchr(frames[i].path, '/');
            filename = filename ? filename + 1 : frames[i].path;
            
            if (i == 0) {
                /* First frame: extract initial number */
                frames[i].index = extract_frame_index_smart(filename, NULL);
                reference_name = filename;
            } else {
                /* Subsequent frames: compare with first frame to find varying number */
                frames[i].index = extract_frame_index_smart(filename, reference_name);
            }
            
            if (frames[i].index < 0) {
                fprintf(stderr, "Warning: Could not extract index from %s, using position %d\n", filename, i);
                frames[i].index = i;
            }
        }
        
        /* Re-sort by extracted index */
        qsort(frames, nframes, sizeof(frame_entry_t), compare_frames);
        
        /* Convert frames to standard format */
        for (int i = 0; i < nframes; i++) {
            char cmd[1024];
            char escaped_path[512];
            char escaped_tmp[512];
            char bg_color_str[16];
            snprintf(bg_color_str, sizeof(bg_color_str), "#%06X", bg_color);
            shell_escape(escaped_path, sizeof(escaped_path), frames[i].path);
            shell_escape(escaped_tmp, sizeof(escaped_tmp), frames[i].tmp_path);
            snprintf(cmd, sizeof(cmd), 
                     "convert \"%s\" -background \"%s\" -flatten -type TrueColor -depth 8 PNG24:\"%s\" 2>/dev/null",
                     escaped_path, bg_color_str, escaped_tmp);
            if (system(cmd) != 0) {
                snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"",
                         escaped_path, escaped_tmp);
                system(cmd);
            }
        }
        
        /* Load all frames */
        image_t *frame_imgs = malloc(sizeof(image_t) * nframes);
        for (int i = 0; i < nframes; i++) {
            frames[i].index = i;
            if (load_png(frames[i].tmp_path, &frame_imgs[i]) != 0) {
                fprintf(stderr, "Error: Failed to load frame %d\n", i);
                return 1;
            }
        }
        
        fprintf(stderr, "Found %d frames, size %dx%d\n", nframes, frame_imgs[0].w, frame_imgs[0].h);
        
        printf("#define NFRAMES %d\n", nframes);
        printf("#define FRAME_W %d\n", frame_imgs[0].w);
        printf("#define FRAME_H %d\n\n", frame_imgs[0].h);
        
        int pixels = frame_imgs[0].w * frame_imgs[0].h;
        
        /* Auto compression: test all methods and pick best */
        if (compress_method == COMPRESS_AUTO) {
            fprintf(stderr, "Testing best compression method...\n\n");
            
            size_t best_size = SIZE_MAX;
            int best_method = COMPRESS_RLE_XOR;
            const char *method_names[] = {"RLE_XOR", "SPARSE", "RLE_DIRECT"};
            int method_ids[] = {COMPRESS_RLE_XOR, COMPRESS_SPARSE, COMPRESS_RLE_DIRECT};
            
            /* Allocate temporary buffers for testing */
            uint8_t *test_buf = malloc(pixels * 6);
            uint8_t *frame0_buf = malloc(pixels * 3);
            size_t frame0_size = compress_raw_direct(frame_imgs[0].pixels, pixels, frame0_buf);
            
            for (int m = 0; m < 3; m++) {
                size_t total = frame0_size;
                int method_valid = 1;  /* Track if method is valid for this animation */
                
                for (int f = 1; f < nframes; f++) {
                    size_t size = 0;  /* Initialize to prevent undefined behavior */
                    switch (method_ids[m]) {
                        case COMPRESS_RLE_XOR:
                            size = compress_rle_xor(frame_imgs[f].pixels, frame_imgs[f-1].pixels, pixels, test_buf);
                            break;
                        case COMPRESS_SPARSE:
                            size = compress_sparse_xor(frame_imgs[f].pixels, frame_imgs[f-1].pixels, pixels, test_buf);
                            break;
                        case COMPRESS_RLE_DIRECT:
                            size = compress_rle_direct(frame_imgs[f].pixels, pixels, test_buf);
                            break;
                        default:
                            size = 0;  /* Should never happen */
                            break;
                    }
                    /* If size is 0 (overflow/error), mark method as invalid */
                    if (size == 0 && f > 0) {
                        method_valid = 0;
                        total = SIZE_MAX;  /* Ensure this method won't be selected */
                        break;
                    }
                    total += size;
                }
                
                if (!method_valid) {
                    fprintf(stderr, "  %d/3: method %-12s ...... SKIPPED (frame too large for 16-bit indices)\n", 
                            m + 1, method_names[m]);
                } else {
                    fprintf(stderr, "  %d/3: method %-12s ...... %zu bytes (%.1f KB)\n", 
                            m + 1, method_names[m], total, total / 1024.0);
                }
                
                if (total < best_size) {
                    best_size = total;
                    best_method = method_ids[m];
                }
            }
            
            free(test_buf);
            free(frame0_buf);
            
            fprintf(stderr, "\n  ---> Best method: %s (%zu bytes)\n\n", 
                    method_names[best_method == COMPRESS_RLE_XOR ? 0 : 
                                 best_method == COMPRESS_SPARSE ? 1 : 2], best_size);
            
            compress_method = best_method;
            printf("#define COMPRESS_METHOD %d  /* Auto-selected: %s */\n", 
                   compress_method, 
                   method_names[compress_method == COMPRESS_RLE_XOR ? 0 : 
                                compress_method == COMPRESS_SPARSE ? 1 : 2]);
        } else {
            printf("#define COMPRESS_METHOD %d  /* 0=RLE_XOR, 1=RLE_DIRECT, 2=SPARSE, 3=RAW */\n", compress_method);
        }
        
        /* Compress frames with selected method */
        uint8_t *compressed[256];
        size_t comp_sizes[256];
        size_t total_size = 0;
        
        /* Frame 0: always raw RGB565 (no previous frame for XOR) */
        compressed[0] = malloc(pixels * 3);
        comp_sizes[0] = compress_raw_direct(frame_imgs[0].pixels, pixels, compressed[0]);
        total_size += comp_sizes[0];
        
        output_frame_data(0, compressed[0], comp_sizes[0]);
        
        /* Delta frames */
        for (int f = 1; f < nframes; f++) {
            compressed[f] = malloc(pixels * 6);
            switch (compress_method) {
                case COMPRESS_RLE_XOR:
                    comp_sizes[f] = compress_rle_xor(frame_imgs[f].pixels,
                                                     frame_imgs[f-1].pixels, pixels, compressed[f]);
                    break;
                case COMPRESS_RLE_DIRECT:
                    comp_sizes[f] = compress_rle_direct(frame_imgs[f].pixels, pixels, compressed[f]);
                    break;
                case COMPRESS_SPARSE:
                    comp_sizes[f] = compress_sparse_xor(frame_imgs[f].pixels,
                                                        frame_imgs[f-1].pixels, pixels, compressed[f]);
                    break;
                case COMPRESS_RAW:
                    comp_sizes[f] = compress_raw_direct(frame_imgs[f].pixels, pixels, compressed[f]);
                    break;
                default:
                    comp_sizes[f] = compress_rle_xor(frame_imgs[f].pixels,
                                                     frame_imgs[f-1].pixels, pixels, compressed[f]);
            }
            total_size += comp_sizes[f];
            output_frame_data(f, compressed[f], comp_sizes[f]);
        }
        
        /* Frame array */
        printf("static const uint8_t* const frames[NFRAMES] = {\n");
        for (int f = 0; f < nframes; f++) {
            printf("    frame_%d,\n", f);
        }
        printf("};\n\n");
        
        printf("static const uint32_t frame_sizes[NFRAMES] = {\n");
        for (int f = 0; f < nframes; f++) {
            printf("    %zu,\n", comp_sizes[f]);
        }
        printf("};\n");
        
        fprintf(stderr, "Total compressed: %zu bytes (%.1f KB)\n", total_size, total_size / 1024.0);
        
        /* Cleanup */
        for (int i = 0; i < nframes; i++) {
            free(frame_imgs[i].pixels);
            free(compressed[i]);
            free(frames[i].path);
            free(frames[i].tmp_path);
        }
        free(frame_imgs);
        free(frames);
        
        char cleanup[512];
        char escaped_tmpdir[256];
        shell_escape(escaped_tmpdir, sizeof(escaped_tmpdir), tmpdir);
        snprintf(cleanup, sizeof(cleanup), "rm -rf %s", escaped_tmpdir);
        system(cleanup);
        
    } else if (display_mode == MODE_ANIM_IMAGE_CENTER || display_mode == MODE_ANIM_IMAGE_FULL) {
        /* Animation on background image */
        if (!bg_image_path) {
            fprintf(stderr, "Error: Mode 1/2 requires background image (-b)\n");
            return 1;
        }
        
        /* Convert background to standard PNG first to handle JPEG and other formats */
        char bg_tmp_path[512];
        char bg_cmd[1024];
        char escaped_bg_path[512];
        char escaped_bg_tmp[512];
        
        snprintf(bg_tmp_path, sizeof(bg_tmp_path), "/tmp/splash_bg_%d.png", getpid());
        shell_escape(escaped_bg_path, sizeof(escaped_bg_path), bg_image_path);
        shell_escape(escaped_bg_tmp, sizeof(escaped_bg_tmp), bg_tmp_path);
        
        snprintf(bg_cmd, sizeof(bg_cmd),
                 "convert \"%s\" -type TrueColor -depth 8 PNG24:\"%s\" 2>/dev/null",
                 escaped_bg_path, escaped_bg_tmp);
        
        if (system(bg_cmd) != 0) {
            fprintf(stderr, "Error: Failed to convert background image: %s\n", bg_image_path);
            return 1;
        }

        /* Load background from the converted PNG */
        image_t bg;
        if (load_png(bg_tmp_path, &bg) != 0) {
            fprintf(stderr, "Error: Failed to load background: %s\n", bg_image_path);
            unlink(bg_tmp_path);
            return 1;
        }
        unlink(bg_tmp_path);
        
        fprintf(stderr, "Background: %dx%d\n", bg.w, bg.h);
        
        /* Fullscreen mode: resize background to target resolution */
        if (display_mode == MODE_ANIM_IMAGE_FULL && target_w > 0 && target_h > 0 && (bg.w != target_w || bg.h != target_h)) {
            fprintf(stderr, "Resizing background to %dx%d...\n", target_w, target_h);
            image_t *resized = resize_image(&bg, target_w, target_h);
            free(bg.pixels);
            bg = *resized;
            free(resized);
        }
        
        /* Load animation frames */
        DIR *dir = opendir(input_path);
        if (!dir) {
            fprintf(stderr, "Error: Cannot open directory: %s\n", input_path);
            return 1;
        }
        
        frame_entry_t *frames = malloc(sizeof(frame_entry_t) * 256);
        int nframes = 0;
        struct dirent *ent;
        char tmpdir[256];
        snprintf(tmpdir, sizeof(tmpdir), "/tmp/splash_frames_%d", getpid());
        mkdir(tmpdir, 0755);
        
        /* First pass: collect all frame filenames */
        while ((ent = readdir(dir)) != NULL && nframes < 256) {
            if (strstr(ent->d_name, ".png") || strstr(ent->d_name, ".PNG") ||
                strstr(ent->d_name, ".jpg") || strstr(ent->d_name, ".JPG") ||
                strstr(ent->d_name, ".jpeg") || strstr(ent->d_name, ".JPEG")) {
                frames[nframes].path = malloc(512);
                frames[nframes].tmp_path = malloc(512);
                snprintf(frames[nframes].path, 512, "%s/%s", input_path, ent->d_name);
                snprintf(frames[nframes].tmp_path, 512, "%s/%s", tmpdir, ent->d_name);
                frames[nframes].index = -1;  /* Will be set after sorting */
                nframes++;
                if (nframes >= 256) {
                    fprintf(stderr, "Warning: Frame limit reached (256 max). Additional frames will be ignored.\n");
                }
            }
        }
        closedir(dir);
        
        if (nframes == 0) {
            fprintf(stderr, "Error: No frames found\n");
            return 1;
        }
        
        /* Sort by filename first to ensure consistent reference frame */
        qsort(frames, nframes, sizeof(frame_entry_t), compare_frames);
        
        /* Second pass: extract frame indices using sorted first frame as reference */
        const char *reference_name = NULL;
        for (int i = 0; i < nframes; i++) {
            const char *filename = strrchr(frames[i].path, '/');
            filename = filename ? filename + 1 : frames[i].path;
            
            if (i == 0) {
                frames[i].index = extract_frame_index_smart(filename, NULL);
                reference_name = filename;
            } else {
                frames[i].index = extract_frame_index_smart(filename, reference_name);
            }
            
            if (frames[i].index < 0) {
                fprintf(stderr, "Warning: Could not extract index from %s, using position %d\n", filename, i);
                frames[i].index = i;
            }
        }
        
        /* Re-sort by extracted index */
        qsort(frames, nframes, sizeof(frame_entry_t), compare_frames);
        
        /* Convert frames to standard format */
        for (int i = 0; i < nframes; i++) {
            char cmd[1024];
            char escaped_path[512];
            char escaped_tmp[512];
            char bg_color_str[16];
            snprintf(bg_color_str, sizeof(bg_color_str), "#%06X", bg_color);
            shell_escape(escaped_path, sizeof(escaped_path), frames[i].path);
            shell_escape(escaped_tmp, sizeof(escaped_tmp), frames[i].tmp_path);
            snprintf(cmd, sizeof(cmd),
                     "convert \"%s\" -background \"%s\" -flatten -type TrueColor -depth 8 PNG24:\"%s\" 2>/dev/null",
                     escaped_path, bg_color_str, escaped_tmp);
            if (system(cmd) != 0) {
                snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"",
                         escaped_path, escaped_tmp);
                system(cmd);
            }
        }
        
        if (nframes >= 256) {
            fprintf(stderr, "Warning: Animation truncated to 256 frames. Consider splitting into multiple sequences.\n");
        }
        
        qsort(frames, nframes, sizeof(frame_entry_t), compare_frames);
        
        image_t *frame_imgs = malloc(sizeof(image_t) * nframes);
        for (int i = 0; i < nframes; i++) {
            frames[i].index = i;
            if (load_png(frames[i].tmp_path, &frame_imgs[i]) != 0) {
                fprintf(stderr, "Error: Failed to load frame %d\n", i);
                return 1;
            }
        }
        
        fprintf(stderr, "Found %d frames, size %dx%d\n", nframes, frame_imgs[0].w, frame_imgs[0].h);
        
        printf("#define NFRAMES %d\n", nframes);
        printf("#define FRAME_W %d\n", frame_imgs[0].w);
        printf("#define FRAME_H %d\n\n", frame_imgs[0].h);
        printf("#define BG_W %d\n", bg.w);
        printf("#define BG_H %d\n\n", bg.h);
        
        int pixels = frame_imgs[0].w * frame_imgs[0].h;
        
        /* Compress background with palette + LZSS */
        int bg_pixel_count = bg.w * bg.h;
        uint16_t *bg_palette = malloc(256 * sizeof(uint16_t));
        uint8_t *bg_indices = malloc(bg_pixel_count);
        uint8_t *bg_compressed = malloc(bg_pixel_count * 2);
        
        int bg_num_colors = build_palette(bg.pixels, bg_pixel_count, bg_palette, bg_indices);
        fprintf(stderr, "Background palette: %d unique colors\n", bg_num_colors);
        
        size_t bg_comp_size = compress_lzss(bg_indices, bg_pixel_count, bg_compressed);
        fprintf(stderr, "Background LZSS: %zu bytes (%.1f%% of raw)\n", 
                bg_comp_size, 100.0 * bg_comp_size / (bg_pixel_count * 2));
        
        printf("#define BG_PALETTE_SIZE %d\n", bg_num_colors);
        printf("#define BG_COMPRESSED_SIZE %zu\n\n", bg_comp_size);
        
        output_bg_palette_lzss(bg_palette, bg_num_colors, bg_compressed, bg_comp_size);
        
        free(bg_palette);
        free(bg_indices);
        free(bg_compressed);
        free(bg.pixels);
        
        /* Auto compression: test all methods and pick best */
        if (compress_method == COMPRESS_AUTO) {
            fprintf(stderr, "Testing best compression method...\n\n");
            
            size_t best_size = SIZE_MAX;
            int best_method = COMPRESS_RLE_XOR;
            const char *method_names[] = {"RLE_XOR", "SPARSE", "RLE_DIRECT"};
            int method_ids[] = {COMPRESS_RLE_XOR, COMPRESS_SPARSE, COMPRESS_RLE_DIRECT};
            
            uint8_t *test_buf = malloc(pixels * 6);
            uint8_t *frame0_buf = malloc(pixels * 3);
            size_t frame0_size = compress_raw_direct(frame_imgs[0].pixels, pixels, frame0_buf);
            
            for (int m = 0; m < 3; m++) {
                size_t total = frame0_size;
                int method_valid = 1;  /* Track if method is valid for this animation */
                
                for (int f = 1; f < nframes; f++) {
                    size_t size = 0;  /* Initialize to prevent undefined behavior */
                    switch (method_ids[m]) {
                        case COMPRESS_RLE_XOR:
                            size = compress_rle_xor(frame_imgs[f].pixels, frame_imgs[f-1].pixels, pixels, test_buf);
                            break;
                        case COMPRESS_SPARSE:
                            size = compress_sparse_xor(frame_imgs[f].pixels, frame_imgs[f-1].pixels, pixels, test_buf);
                            break;
                        case COMPRESS_RLE_DIRECT:
                            size = compress_rle_direct(frame_imgs[f].pixels, pixels, test_buf);
                            break;
                        default:
                            size = 0;  /* Should never happen */
                            break;
                    }
                    /* If size is 0 (overflow/error), mark method as invalid */
                    if (size == 0 && f > 0) {
                        method_valid = 0;
                        total = SIZE_MAX;  /* Ensure this method won't be selected */
                        break;
                    }
                    total += size;
                }
                
                if (!method_valid) {
                    fprintf(stderr, "  %d/3: method %-12s ...... SKIPPED (frame too large for 16-bit indices)\n", 
                            m + 1, method_names[m]);
                } else {
                    fprintf(stderr, "  %d/3: method %-12s ...... %zu bytes (%.1f KB)\n", 
                            m + 1, method_names[m], total, total / 1024.0);
                }
                
                if (total < best_size) {
                    best_size = total;
                    best_method = method_ids[m];
                }
            }
            
            free(test_buf);
            free(frame0_buf);
            
            fprintf(stderr, "\n  ---> Best method: %s (%zu bytes)\n\n", 
                    method_names[best_method == COMPRESS_RLE_XOR ? 0 : 
                                 best_method == COMPRESS_SPARSE ? 1 : 2], best_size);
            
            compress_method = best_method;
            printf("#define COMPRESS_METHOD %d  /* Auto-selected: %s */\n", 
                   compress_method, 
                   method_names[compress_method == COMPRESS_RLE_XOR ? 0 : 
                                compress_method == COMPRESS_SPARSE ? 1 : 2]);
        } else {
            printf("#define COMPRESS_METHOD %d  /* 0=RLE_XOR, 1=RLE_DIRECT, 2=SPARSE, 3=RAW */\n", compress_method);
        }
        
        /* Compress frames with selected method */
        uint8_t *compressed[256];
        size_t comp_sizes[256];
        size_t total_size = 0;
        
        /* Frame 0: always raw RGB565 (no previous frame for XOR) */
        compressed[0] = malloc(pixels * 3);
        comp_sizes[0] = compress_raw_direct(frame_imgs[0].pixels, pixels, compressed[0]);
        total_size += comp_sizes[0];
        output_frame_data(0, compressed[0], comp_sizes[0]);
        
        for (int f = 1; f < nframes; f++) {
            compressed[f] = malloc(pixels * 6);
            switch (compress_method) {
                case COMPRESS_RLE_XOR:
                    comp_sizes[f] = compress_rle_xor(frame_imgs[f].pixels,
                                                     frame_imgs[f-1].pixels, pixels, compressed[f]);
                    break;
                case COMPRESS_RLE_DIRECT:
                    comp_sizes[f] = compress_rle_direct(frame_imgs[f].pixels, pixels, compressed[f]);
                    break;
                case COMPRESS_SPARSE:
                    comp_sizes[f] = compress_sparse_xor(frame_imgs[f].pixels,
                                                        frame_imgs[f-1].pixels, pixels, compressed[f]);
                    break;
                case COMPRESS_RAW:
                    comp_sizes[f] = compress_raw_direct(frame_imgs[f].pixels, pixels, compressed[f]);
                    break;
                default:
                    comp_sizes[f] = compress_rle_xor(frame_imgs[f].pixels,
                                                     frame_imgs[f-1].pixels, pixels, compressed[f]);
            }
            total_size += comp_sizes[f];
            output_frame_data(f, compressed[f], comp_sizes[f]);
        }
        
        printf("static const uint8_t* const frames[NFRAMES] = {\n");
        for (int f = 0; f < nframes; f++) {
            printf("    frame_%d,\n", f);
        }
        printf("};\n\n");
        
        printf("static const uint32_t frame_sizes[NFRAMES] = {\n");
        for (int f = 0; f < nframes; f++) {
            printf("    %zu,\n", comp_sizes[f]);
        }
        printf("};\n");
        
        fprintf(stderr, "Total compressed: %zu bytes (%.1f KB)\n", total_size, total_size / 1024.0);
        
        /* Cleanup */
        for (int i = 0; i < nframes; i++) {
            free(frame_imgs[i].pixels);
            free(compressed[i]);
            free(frames[i].path);
            free(frames[i].tmp_path);
        }
        free(frame_imgs);
        free(frames);
        
        char cleanup[512];
        char escaped_tmpdir[256];
        shell_escape(escaped_tmpdir, sizeof(escaped_tmpdir), tmpdir);
        snprintf(cleanup, sizeof(cleanup), "rm -rf %s", escaped_tmpdir);
        system(cleanup);
    }
    
    return 0;
}
