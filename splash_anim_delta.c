/*
 * splash_anim_delta.c - Boot splash with multiple display modes
 * by seb3773 - https://github.com/seb3773
 * 
 * Architecture: Linux x86_64, freestanding (no libc)
 * Dependencies: nolibc.h only
 * 
 * Display modes (selected at compile time via DISPLAY_MODE):
 *   0 = Animation on solid background
 *   1 = Animation on background image (full screen)
 *   2 = Static image on solid background
 *   3 = Static image full screen
 * 
 * Compression (selected at compile time via COMPRESS_METHOD):
 *   0 = RLE XOR    - RLE on XOR deltas
 *   1 = RLE Direct - RLE on pixel values
 *   2 = Sparse XOR - Position + value for changed pixels
 *   3 = Raw XOR    - No compression
 *   5 = Palette LZSS - 8-bit palette + LZSS (static images only)
 * 
 * Constraints:
 *   - Complexity: O(n) per frame where n = changed pixels
 *   - Memory: single frame buffer
 *   - No dynamic allocation
 *   - No PNG decoding
 */

#include "nolibc.h"
#include <emmintrin.h>  /* SSE2 intrinsics for x86_64 */

/* Frame buffer - allocated via mmap at runtime */
static uint16_t *frame_buffer = NULL;
static uint16_t *bg_buffer = NULL;  /* For mode 1 */

/* Volatile flag for signal handling */
static volatile int terminate_requested = 0;

/* Signal handler for graceful termination */
static void signal_handler(int sig) {
    (void)sig;
    terminate_requested = 1;
}

#include "frames_delta.h"

/* --- LZSS decompression for palette+LZSS compressed data --- */
#if (defined(COMPRESS_METHOD) && COMPRESS_METHOD == 5) || (defined(DISPLAY_MODE) && (DISPLAY_MODE == 1 || DISPLAY_MODE == 2))

#define LZSS_WINDOW_SIZE 4096
#define LZSS_MIN_MATCH  3

/* Decompress LZSS data to indices, then expand via palette */
static void decompress_palette_lzss(const uint8_t *compressed, size_t comp_size,
                                     const uint16_t *pal, int num_colors,
                                     uint16_t *out, int pixel_count) {
    uint8_t window[LZSS_WINDOW_SIZE];
    int window_pos = 0;
    int out_pos = 0;
    size_t in_pos = 0;
    
    /* Initialize window */
    for (int i = 0; i < LZSS_WINDOW_SIZE; i++) window[i] = 0;
    
    while (in_pos < comp_size && out_pos < pixel_count) {
        uint8_t flag = compressed[in_pos++];
        
        for (int bit = 0; bit < 8 && out_pos < pixel_count; bit++) {
            if (in_pos >= comp_size) break;
            
            if (flag & (1 << bit)) {
                /* Literal byte */
                uint8_t val = compressed[in_pos++];
                window[window_pos] = val;
                window_pos = (window_pos + 1) % LZSS_WINDOW_SIZE;
                
                /* Expand via palette */
                out[out_pos++] = pal[val < num_colors ? val : 0];
            } else {
                /* Back-reference: 2 bytes */
                if (in_pos + 1 >= comp_size) break;
                uint8_t b1 = compressed[in_pos++];
                uint8_t b2 = compressed[in_pos++];
                
                int offset = (b1 | ((b2 & 0xF0) << 4));
                int length = (b2 & 0x0F) + LZSS_MIN_MATCH;
                
                /* Copy from window */
                for (int i = 0; i < length && out_pos < pixel_count; i++) {
                    int win_idx = (window_pos - offset + LZSS_WINDOW_SIZE) % LZSS_WINDOW_SIZE;
                    uint8_t val = window[win_idx];
                    window[window_pos] = val;
                    window_pos = (window_pos + 1) % LZSS_WINDOW_SIZE;
                    
                    /* Expand via palette */
                    out[out_pos++] = pal[val < num_colors ? val : 0];
                }
            }
        }
    }
}
#endif

/* Decode RLE XOR delta and apply to frame_buffer */
static void apply_delta_rle_xor(const uint8_t *delta, size_t delta_size) {
    size_t pos = 0;
    int pixel_idx = 0;
    int max_pixels = FRAME_W * FRAME_H;
    
    while (pos < delta_size) {
        uint8_t cmd = delta[pos++];
        
        if (cmd == 0x00) {
            break;
        } else if (cmd & 0x80) {
            /* Skip: advance pixel index */
            int skip = (cmd & 0x7F) + 1;
            pixel_idx += skip;
            /* Clamp to max to prevent overflow in next iteration */
            if (pixel_idx > max_pixels) pixel_idx = max_pixels;
        } else {
            int count = cmd;
            for (int i = 0; i < count && pixel_idx < max_pixels; i++) {
                /* Bounds check before reading 2 bytes */
                if (pos + 1 >= delta_size) break;
                uint16_t xor_val = delta[pos] | (delta[pos + 1] << 8);
                pos += 2;
                frame_buffer[pixel_idx++] ^= xor_val;
            }
        }
    }
}

/* Decode RLE Direct (no XOR, direct pixel values) */
static void apply_delta_rle_direct(const uint8_t *delta, size_t delta_size) {
    size_t pos = 0;
    int pixel_idx = 0;
    int max_pixels = FRAME_W * FRAME_H;
    
    while (pos < delta_size) {
        uint8_t cmd = delta[pos++];
        
        if (cmd == 0x00) {
            break;
        } else if (cmd & 0x80) {
            int count = cmd & 0x7F;
            /* Bounds check before reading 2 bytes */
            if (pos + 1 >= delta_size) break;
            uint16_t val = delta[pos] | (delta[pos + 1] << 8);
            pos += 2;
            for (int i = 0; i < count && pixel_idx < max_pixels; i++) {
                frame_buffer[pixel_idx++] = val;
            }
        } else {
            int count = cmd;
            for (int i = 0; i < count && pixel_idx < max_pixels; i++) {
                /* Bounds check before reading 2 bytes */
                if (pos + 1 >= delta_size) break;
                frame_buffer[pixel_idx++] = delta[pos] | (delta[pos + 1] << 8);
                pos += 2;
            }
        }
    }
}

/* Decode Sparse XOR delta (position + value for changed pixels) */
static void apply_delta_sparse_xor(const uint8_t *delta, size_t delta_size) {
    if (delta_size < 2) return;
    
    size_t pos = 0;
    int changed = delta[pos] | (delta[pos + 1] << 8);
    pos += 2;
    
    for (int i = 0; i < changed && pos + 3 < delta_size; i++) {
        int idx = delta[pos] | (delta[pos + 1] << 8);
        uint16_t xor_val = delta[pos + 2] | (delta[pos + 3] << 8);
        pos += 4;
        
        if (idx < FRAME_W * FRAME_H) {
            frame_buffer[idx] ^= xor_val;
        }
    }
}

/* Decode Raw RGB565 (direct pixel values, no compression) */
static void apply_delta_raw(const uint8_t *raw, size_t size) {
    int pixels = size / 2;
    if (pixels > FRAME_W * FRAME_H) pixels = FRAME_W * FRAME_H;
    
    for (int i = 0; i < pixels; i++) {
        frame_buffer[i] = raw[i * 2] | (raw[i * 2 + 1] << 8);
    }
}

/* Apply delta based on compression method */
static void apply_delta(const uint8_t *delta, size_t delta_size) {
#if COMPRESS_METHOD == 0
    apply_delta_rle_xor(delta, delta_size);
#elif COMPRESS_METHOD == 1
    apply_delta_rle_direct(delta, delta_size);
#elif COMPRESS_METHOD == 2
    apply_delta_sparse_xor(delta, delta_size);
#elif COMPRESS_METHOD == 3
    apply_delta_raw(delta, delta_size);
#endif
}

/* Load frame 0 based on compression method */
static void load_frame_0(const uint8_t *raw, size_t size) {
    /* Frame 0 is always stored as raw RGB565 for all methods */
    apply_delta_raw(raw, size);
}

/* Fill framebuffer with solid color - optimized */
static void fill_fb_color(uint8_t *fbmem, int fb_w, int fb_h, int line_len, 
                          int bpp, uint16_t color, int r_off, int g_off, int b_off) {
    /* Fast path for black (0x0000) - use memset */
    if (color == 0x0000) {
        for (int y = 0; y < fb_h; y++) {
            memset(fbmem + y * line_len, 0, line_len);
        }
        return;
    }
    
    /* For 32bpp with standard layout, use SSE2 */
    if (bpp == 32 && r_off == 16 && g_off == 8 && b_off == 0) {
        /* XRGB8888 standard layout */
        uint32_t r = (color >> 11) & 0x1F;
        uint32_t g = (color >> 5) & 0x3F;
        uint32_t b = color & 0x1F;
        uint32_t pixel = (r << 19) | (g << 10) | (b << 3);
        
        /* Fill with SSE2 (4 pixels at a time) */
        __m128i fill_val = _mm_set1_epi32(pixel);
        
        for (int y = 0; y < fb_h; y++) {
            uint32_t *dst = (uint32_t *)(fbmem + y * line_len);
            int x = 0;
            for (; x + 3 < fb_w; x += 4) {
                _mm_storeu_si128((__m128i*)(dst + x), fill_val);
            }
            for (; x < fb_w; x++) dst[x] = pixel;
        }
        return;
    }
    
    /* Fallback for other cases */
    for (int y = 0; y < fb_h; y++) {
        if (bpp == 32) {
            uint32_t *dst = (uint32_t *)(fbmem + y * line_len);
            uint32_t r = (color >> 11) & 0x1F;
            uint32_t g = (color >> 5) & 0x3F;
            uint32_t b = color & 0x1F;
            uint32_t pixel = ((r << 3) << r_off) | ((g << 2) << g_off) | ((b << 3) << b_off);
            for (int x = 0; x < fb_w; x++) dst[x] = pixel;
        } else if (bpp == 16) {
            uint16_t *dst = (uint16_t *)(fbmem + y * line_len);
            uint16_t pattern = color;
            for (int x = 0; x < fb_w; x++) dst[x] = pattern;
        } else if (bpp == 24) {
            uint8_t *dst = fbmem + y * line_len;
            uint32_t r = (color >> 11) & 0x1F;
            uint32_t g = (color >> 5) & 0x3F;
            uint32_t b = color & 0x1F;
            uint8_t r8 = r << 3;
            uint8_t g8 = g << 2;
            uint8_t b8 = b << 3;
            for (int x = 0; x < fb_w; x++) {
                uint8_t *pixel = dst + x * 3;
                pixel[r_off] = r8;
                pixel[g_off] = g8;
                pixel[b_off] = b8;
            }
        }
    }
}

/* Fill a rectangular area with solid color - for partial clearing */
/* Only used when frames don't fully cover their area (future optimization) */
#if DISPLAY_MODE == 0
static void fill_rect(uint8_t *fbmem, int fb_w, int fb_h, int line_len, int bpp,
                      int rect_x, int rect_y, int rect_w, int rect_h,
                      uint16_t color, int r_off, int g_off, int b_off) {
    /* Clip to framebuffer bounds */
    if (rect_x < 0) { rect_w += rect_x; rect_x = 0; }
    if (rect_y < 0) { rect_h += rect_y; rect_y = 0; }
    if (rect_x + rect_w > fb_w) rect_w = fb_w - rect_x;
    if (rect_y + rect_h > fb_h) rect_h = fb_h - rect_y;
    if (rect_w <= 0 || rect_h <= 0) return;
    
    /* Fast path for black */
    if (color == 0x0000) {
        for (int y = rect_y; y < rect_y + rect_h; y++) {
            if (bpp == 32) {
                memset(fbmem + y * line_len + rect_x * 4, 0, rect_w * 4);
            } else if (bpp == 16) {
                memset(fbmem + y * line_len + rect_x * 2, 0, rect_w * 2);
            }
        }
        return;
    }
    
    /* Fill rect with color */
    for (int y = rect_y; y < rect_y + rect_h; y++) {
        if (bpp == 32) {
            uint32_t *dst = (uint32_t *)(fbmem + y * line_len + rect_x * 4);
            uint32_t r = (color >> 11) & 0x1F;
            uint32_t g = (color >> 5) & 0x3F;
            uint32_t b = color & 0x1F;
            uint32_t pixel = ((r << 3) << r_off) | ((g << 2) << g_off) | ((b << 3) << b_off);
            for (int x = 0; x < rect_w; x++) dst[x] = pixel;
        } else if (bpp == 16) {
            uint16_t *dst = (uint16_t *)(fbmem + y * line_len + rect_x * 2);
            for (int x = 0; x < rect_w; x++) dst[x] = color;
        }
    }
}
#endif

/* SSE2 optimized RGB565 to XRGB8888 conversion (standard layout: R=16, G=8, B=0) */
/* Processes 8 pixels at once. Requires SSE2 capable CPU (all x86_64 have it). */
static void blit_to_fb_32bpp_sse2(uint32_t *dst, const uint16_t *src, int count) {
    int i = 0;
    
    /* Process 8 pixels at a time */
    for (; i + 7 < count; i += 8) {
        /* Load 8 RGB565 pixels (16 bytes) */
        __m128i pixels = _mm_loadu_si128((__m128i*)(src + i));
        
        /* Expand to 32-bit: unpack low 4 pixels */
        __m128i lo = _mm_unpacklo_epi16(pixels, _mm_setzero_si128());
        /* Expand to 32-bit: unpack high 4 pixels */
        __m128i hi = _mm_unpackhi_epi16(pixels, _mm_setzero_si128());
        
        /* RGB565 layout: RRRRRGGGGGGBBBBB (R5@11-15, G6@5-10, B5@0-4) */
        /* XRGB8888 target: R@16-23, G@8-15, B@0-7 */
        
        /* R5 (bits 11-15) -> R8 (bits 16-23): shift left by 5 (to bit 16), then 3 more for expansion */
        __m128i r_mask = _mm_set1_epi32(0x0000F800);  /* bits 11-15 */
        __m128i r_lo = _mm_and_si128(lo, r_mask);
        __m128i r_hi = _mm_and_si128(hi, r_mask);
        r_lo = _mm_slli_epi32(r_lo, 5 + 3);  /* shift to bit 16, then expand 5->8 bits */
        r_hi = _mm_slli_epi32(r_hi, 5 + 3);
        
        /* G6 (bits 5-10) -> G8 (bits 8-15): shift left by 3 (to bit 8), then 2 more for expansion */
        __m128i g_mask = _mm_set1_epi32(0x000007E0);  /* bits 5-10 */
        __m128i g_lo = _mm_and_si128(lo, g_mask);
        __m128i g_hi = _mm_and_si128(hi, g_mask);
        g_lo = _mm_slli_epi32(g_lo, 3 + 2);  /* shift to bit 8, then expand 6->8 bits */
        g_hi = _mm_slli_epi32(g_hi, 3 + 2);
        
        /* B5 (bits 0-4) -> B8 (bits 0-7): no shift needed, just expand by 3 */
        __m128i b_mask = _mm_set1_epi32(0x0000001F);  /* bits 0-4 */
        __m128i b_lo = _mm_and_si128(lo, b_mask);
        __m128i b_hi = _mm_and_si128(hi, b_mask);
        b_lo = _mm_slli_epi32(b_lo, 3);  /* expand 5->8 bits */
        b_hi = _mm_slli_epi32(b_hi, 3);
        
        /* Combine R, G, B */
        __m128i result_lo = _mm_or_si128(_mm_or_si128(r_lo, g_lo), b_lo);
        __m128i result_hi = _mm_or_si128(_mm_or_si128(r_hi, g_hi), b_hi);
        
        /* Store 8 XRGB8888 pixels (32 bytes) */
        _mm_storeu_si128((__m128i*)(dst + i), result_lo);
        _mm_storeu_si128((__m128i*)(dst + i + 4), result_hi);
    }
    
    /* Handle remaining pixels with scalar code */
    for (; i < count; i++) {
        uint16_t pixel = src[i];
        uint32_t r = (pixel >> 11) & 0x1F;
        uint32_t g = (pixel >> 5) & 0x3F;
        uint32_t b = pixel & 0x1F;
        dst[i] = (r << 19) | (g << 10) | (b << 3);  /* XRGB8888: R@16, G@8, B@0 */
    }
}

/* SSE2 optimized RGB565 to BGRX8888 conversion (BGR layout: R=0, G=8, B=16) */
static void blit_to_fb_32bpp_sse2_bgr(uint32_t *dst, const uint16_t *src, int count) {
    int i = 0;
    
    for (; i + 7 < count; i += 8) {
        __m128i pixels = _mm_loadu_si128((__m128i*)(src + i));
        __m128i lo = _mm_unpacklo_epi16(pixels, _mm_setzero_si128());
        __m128i hi = _mm_unpackhi_epi16(pixels, _mm_setzero_si128());
        
        /* RGB565 -> BGRX8888: R@0-7, G@8-15, B@16-23 */
        
        /* R5 (bits 11-15) -> R8 (bits 0-7): shift right by 11, then left 3 for expansion */
        __m128i r_mask = _mm_set1_epi32(0x0000F800);
        __m128i r_lo = _mm_and_si128(lo, r_mask);
        __m128i r_hi = _mm_and_si128(hi, r_mask);
        r_lo = _mm_srli_epi32(r_lo, 11 - 3);  /* shift right 8 to get to bit 0, effectively >>11 then <<3 */
        r_hi = _mm_srli_epi32(r_hi, 11 - 3);
        
        /* G6 (bits 5-10) -> G8 (bits 8-15): same as XRGB */
        __m128i g_mask = _mm_set1_epi32(0x000007E0);
        __m128i g_lo = _mm_and_si128(lo, g_mask);
        __m128i g_hi = _mm_and_si128(hi, g_mask);
        g_lo = _mm_slli_epi32(g_lo, 3 + 2);
        g_hi = _mm_slli_epi32(g_hi, 3 + 2);
        
        /* B5 (bits 0-4) -> B8 (bits 16-23): shift left 16, then expand */
        __m128i b_mask = _mm_set1_epi32(0x0000001F);
        __m128i b_lo = _mm_and_si128(lo, b_mask);
        __m128i b_hi = _mm_and_si128(hi, b_mask);
        b_lo = _mm_slli_epi32(b_lo, 16 + 3);
        b_hi = _mm_slli_epi32(b_hi, 16 + 3);
        
        __m128i result_lo = _mm_or_si128(_mm_or_si128(r_lo, g_lo), b_lo);
        __m128i result_hi = _mm_or_si128(_mm_or_si128(r_hi, g_hi), b_hi);
        
        _mm_storeu_si128((__m128i*)(dst + i), result_lo);
        _mm_storeu_si128((__m128i*)(dst + i + 4), result_hi);
    }
    
    for (; i < count; i++) {
        uint16_t pixel = src[i];
        uint32_t r = (pixel >> 11) & 0x1F;
        uint32_t g = (pixel >> 5) & 0x3F;
        uint32_t b = pixel & 0x1F;
        dst[i] = (b << 19) | (g << 10) | (r << 3);  /* BGRX8888: B@16, G@8, R@0 */
    }
}

/* Blit RGB565 frame to framebuffer (32bpp) - dispatches to SSE2 or scalar */
static void blit_to_fb_32bpp(uint8_t *fbmem, int fb_w, int fb_h, int line_len,
                             const uint16_t *frame, int fw, int fh, int x, int y,
                             int r_off, int g_off, int b_off) {
    /* Detect standard layouts for SSE2 optimization */
    int use_sse2_rgb = (r_off == 16 && g_off == 8 && b_off == 0);   /* XRGB8888 */
    int use_sse2_bgr = (r_off == 0 && g_off == 8 && b_off == 16);   /* BGRX8888 */
    
    for (int row = 0; row < fh; row++) {
        if (y + row >= fb_h || y + row < 0) continue;
        
        uint32_t *dst = (uint32_t *)(fbmem + (y + row) * line_len + x * 4);
        const uint16_t *src = frame + row * fw;
        
        /* Calculate visible columns */
        int col_start = (x < 0) ? -x : 0;
        int col_end = fw;
        if (x + col_end > fb_w) col_end = fb_w - x;
        int visible_count = col_end - col_start;
        
        if (visible_count <= 0) continue;
        
        dst += col_start;
        src += col_start;
        
        /* Use SSE2 for standard layouts, scalar for exotic configs */
        if (use_sse2_rgb) {
            blit_to_fb_32bpp_sse2(dst, src, visible_count);
        } else if (use_sse2_bgr) {
            blit_to_fb_32bpp_sse2_bgr(dst, src, visible_count);
        } else {
            /* Scalar fallback for exotic RGB layouts */
            for (int col = 0; col < visible_count; col++) {
                uint16_t pixel = src[col];
                uint32_t r = (pixel >> 11) & 0x1F;
                uint32_t g = (pixel >> 5) & 0x3F;
                uint32_t b = pixel & 0x1F;
                dst[col] = ((r << 3) << r_off) | ((g << 2) << g_off) | ((b << 3) << b_off);
            }
        }
    }
}

/* Blit RGB565 frame to framebuffer (16bpp) */
static void blit_to_fb_16bpp(uint8_t *fbmem, int fb_w, int fb_h, int line_len,
                             const uint16_t *frame, int fw, int fh, int x, int y) {
    for (int row = 0; row < fh; row++) {
        if (y + row >= fb_h || y + row < 0) continue;
        
        uint16_t *dst = (uint16_t *)(fbmem + (y + row) * line_len + x * 2);
        const uint16_t *src = frame + row * fw;
        
        int copy_len = fw;
        if (x + copy_len > fb_w) copy_len = fb_w - x;
        if (x < 0) {
            src -= x;
            copy_len += x;
            dst += x;
        }
        
        if (copy_len > 0) {
            memcpy(dst, src, copy_len * sizeof(uint16_t));
        }
    }
}

/* Blit RGB565 frame to framebuffer (24bpp - RGB888 packed) */
/* Note: 24bpp is rare but some hardware uses it. Format is typically RGB in memory order. */
static void blit_to_fb_24bpp(uint8_t *fbmem, int fb_w, int fb_h, int line_len,
                             const uint16_t *frame, int fw, int fh, int x, int y,
                             int r_off, int g_off, int b_off) {
    /* 24bpp: typically 3 bytes per pixel, RGB or BGR order */
    /* r_off/g_off/b_off indicate byte position (0, 1, or 2) for each component */
    for (int row = 0; row < fh; row++) {
        if (y + row >= fb_h || y + row < 0) continue;
        
        uint8_t *dst = fbmem + (y + row) * line_len + x * 3;
        const uint16_t *src = frame + row * fw;
        
        for (int col = 0; col < fw; col++) {
            if (x + col >= fb_w || x + col < 0) continue;
            
            uint16_t pixel = src[col];
            uint8_t r = (pixel >> 11) & 0x1F;
            uint8_t g = (pixel >> 5) & 0x3F;
            uint8_t b = pixel & 0x1F;
            
            /* Expand to 8-bit */
            r <<= 3;
            g <<= 2;
            b <<= 3;
            
            /* Use byte positions from offsets (0, 1, or 2) */
            uint8_t *pixel_dst = dst + col * 3;
            pixel_dst[r_off] = r;
            pixel_dst[g_off] = g;
            pixel_dst[b_off] = b;
        }
    }
}

/* Blit frame with position handling */
static void blit_frame(uint8_t *fbmem, int fb_w, int fb_h, int line_len, int bpp,
                       const uint16_t *frame, int fw, int fh, int x, int y,
                       int r_off, int g_off, int b_off) {
    if (bpp == 32) {
        blit_to_fb_32bpp(fbmem, fb_w, fb_h, line_len, frame, fw, fh, x, y, r_off, g_off, b_off);
    } else if (bpp == 16) {
        blit_to_fb_16bpp(fbmem, fb_w, fb_h, line_len, frame, fw, fh, x, y);
    } else if (bpp == 24) {
        blit_to_fb_24bpp(fbmem, fb_w, fb_h, line_len, frame, fw, fh, x, y, r_off, g_off, b_off);
    }
}

/* High-precision sleep with EINTR handling */
static void sleep_ms(unsigned int ms) {
    struct timespec req = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000L
    };
    struct timespec rem;
    while (nanosleep(&req, &rem) != 0) {
        /* Retry with remaining time if interrupted by signal */
        req = rem;
    }
}

/* Check if splash is disabled via kernel cmdline */
/* Returns 1 if disabled (should exit), 0 if enabled */
static int check_cmdline_disable(void) {
    int fd = open("/proc/cmdline", 0, 0);  /* O_RDONLY = 0 */
    if (fd < 0) return 0;  /* Can't read cmdline, assume enabled */
    
    /* 4096 bytes (page size) - standard for cmdline on modern systems */
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    
    if (n <= 0) return 0;
    buf[n] = '\0';
    
    /* Check for nosplash parameter */
    char *p = buf;
    while (*p) {
        /* Skip whitespace (including \n sometimes present at end) */
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (!*p) break;
        
        /* Check for nosplash */
        if (p[0] == 'n' && p[1] == 'o' && p[2] == 's' && p[3] == 'p' && 
            p[4] == 'l' && p[5] == 'a' && p[6] == 's' && p[7] == 'h') {
            char next = p[8];
            if (next == ' ' || next == '\t' || next == '\n' || next == '\0') {
                return 1;  /* Found nosplash */
            }
        }
        
        /* Check for xbootsplash=0 */
        if (p[0] == 'x' && p[1] == 'b' && p[2] == 'o' && p[3] == 'o' && 
            p[4] == 't' && p[5] == 's' && p[6] == 'p' && p[7] == 'l' &&
            p[8] == 'a' && p[9] == 's' && p[10] == 'h' && p[11] == '=') {
            if (p[12] == '0') {
                char next = p[13];
                if (next == ' ' || next == '\t' || next == '\n' || next == '\0') {
                    return 1;  /* Found xbootsplash=0 */
                }
            }
        }
        
        /* Skip to next parameter */
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
    }
    
    return 0;  /* Not found, splash enabled */
}

/* Get current time in milliseconds using CLOCK_MONOTONIC_RAW */
static long get_time_ms(void) {
    struct timespec ts;
    /* Use syscall directly for clock_gettime */
    syscall2(228, (long)CLOCK_MONOTONIC_RAW, (long)&ts);  /* SYS_clock_gettime = 228 on x86_64 */
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* Entry point */
int main(void) {
    int fb_fd = -1;
    void *fbmem = NULL;
    size_t fb_size = 0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    
    /* Kill switch: check kernel cmdline for nosplash or xbootsplash=0 */
    if (check_cmdline_disable()) {
        return 0;  /* Splash disabled, exit cleanly */
    }
    
    /* Setup signal handlers for graceful termination */
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    /* Allocate frame buffer */
    frame_buffer = mmap(NULL, FRAME_W * FRAME_H * 2, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (frame_buffer == MAP_FAILED) {
        return 1;
    }
    
#if DISPLAY_MODE == 1 || DISPLAY_MODE == 2
    /* Allocate background buffer for animation-on-image modes */
    bg_buffer = mmap(NULL, BG_W * BG_H * 2, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (bg_buffer == MAP_FAILED) {
        return 1;
    }
    /* Decompress background from palette + LZSS */
    decompress_palette_lzss(bg_compressed, BG_COMPRESSED_SIZE, bg_palette, BG_PALETTE_SIZE,
                            bg_buffer, BG_W * BG_H);
#endif
    
    /* Load initial frame */
#if DISPLAY_MODE == 3 || DISPLAY_MODE == 4
    /* Static image */
#if defined(COMPRESS_METHOD) && COMPRESS_METHOD == 5
    /* Palette + LZSS compressed */
    decompress_palette_lzss(img_compressed, IMG_COMPRESSED_SIZE, palette, PALETTE_SIZE,
                            frame_buffer, FRAME_W * FRAME_H);
#else
    /* Raw RGB565 */
    for (int i = 0; i < FRAME_W * FRAME_H; i++) {
        frame_buffer[i] = frame_0[i];
    }
#endif
#else
    /* Animation: load frame 0 compressed */
    load_frame_0(frames[0], frame_sizes[0]);
#endif
    
    /* Open framebuffer */
    fb_fd = open("/dev/fb0", O_RDWR, 0);
    if (fb_fd < 0) {
        return 1;
    }
    
    /* Get framebuffer info */
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0 ||
        ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        close(fb_fd);
        return 1;
    }
    
    /* Map framebuffer */
    fb_size = finfo.smem_len;
    fbmem = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fbmem == MAP_FAILED) {
        close(fb_fd);
        return 1;
    }
    
    /* Detect RGB bit offsets for 32bpp hardware compatibility */
    /* Standard XRGB8888: R=16, G=8, B=0. Some hardware uses BGRX: R=0, G=8, B=16 */
    /* For 24bpp, we need byte positions (0, 1, 2) instead of bit offsets */
    int r_off, g_off, b_off;
    
    if (vinfo.bits_per_pixel == 32) {
        r_off = vinfo.red.offset;
        g_off = vinfo.green.offset;
        b_off = vinfo.blue.offset;
    } else if (vinfo.bits_per_pixel == 24) {
        /* 24bpp: determine byte order from vinfo offsets */
        /* vinfo.{red,green,blue}.offset gives bit position: 0, 8, or 16 */
        /* Convert to byte position: bit/8 = 0, 1, or 2 */
        r_off = vinfo.red.offset / 8;
        g_off = vinfo.green.offset / 8;
        b_off = vinfo.blue.offset / 8;
    } else {
        /* 16bpp or other - use defaults (not used for these modes anyway) */
        r_off = 16;
        g_off = 8;
        b_off = 0;
    }
    
    /* Calculate position */
    int x, y;
    
#if DISPLAY_MODE == 4
    /* Full screen static: center the image with offset */
    x = (vinfo.xres - FRAME_W) / 2 + HORIZONTAL_OFFSET;
    y = (vinfo.yres - FRAME_H) / 2 + VERTICAL_OFFSET;
    /* Clear to background color first */
    fill_fb_color(fbmem, vinfo.xres, vinfo.yres, finfo.line_length,
                  vinfo.bits_per_pixel, BACKGROUND_COLOR, r_off, g_off, b_off);
#elif DISPLAY_MODE == 1 || DISPLAY_MODE == 2
    /* Animation on background image (centered or fullscreen) */
    x = (vinfo.xres - FRAME_W) / 2 + HORIZONTAL_OFFSET;
    y = (vinfo.yres - FRAME_H) / 2 + VERTICAL_OFFSET;
    /* Draw background first */
    blit_frame(fbmem, vinfo.xres, vinfo.yres, finfo.line_length, vinfo.bits_per_pixel,
               bg_buffer, BG_W, BG_H, 0, 0, r_off, g_off, b_off);
#else
    /* Animation or static on solid background */
    x = (vinfo.xres - FRAME_W) / 2 + HORIZONTAL_OFFSET;
    y = (vinfo.yres - FRAME_H) / 2 + VERTICAL_OFFSET;
    /* Clear to background color */
    fill_fb_color(fbmem, vinfo.xres, vinfo.yres, finfo.line_length,
                  vinfo.bits_per_pixel, BACKGROUND_COLOR, r_off, g_off, b_off);
#endif
    
    /* Main loop */
#if DISPLAY_MODE == 3 || DISPLAY_MODE == 4
    /* Static image: just display and wait for termination signal */
    blit_frame(fbmem, vinfo.xres, vinfo.yres, finfo.line_length, vinfo.bits_per_pixel,
               frame_buffer, FRAME_W, FRAME_H, x, y, r_off, g_off, b_off);
    
    /* Sleep until signal received */
    while (!terminate_requested) {
        sleep_ms(1000);
    }
#else
    /* Animation loop */
    /* Draw background once - animation frames completely cover their area */
#if DISPLAY_MODE == 1 || DISPLAY_MODE == 2
    blit_frame(fbmem, vinfo.xres, vinfo.yres, finfo.line_length, vinfo.bits_per_pixel,
               bg_buffer, BG_W, BG_H, 0, 0, r_off, g_off, b_off);
#endif
    
    int frame_idx = 0;
    while (!terminate_requested) {
        /* Measure frame start time */
        long frame_start = get_time_ms();
        
        /* Blit current frame */
        blit_frame(fbmem, vinfo.xres, vinfo.yres, finfo.line_length, vinfo.bits_per_pixel,
                   frame_buffer, FRAME_W, FRAME_H, x, y, r_off, g_off, b_off);
        
        /* Check for termination signal */
        if (terminate_requested) break;
        
        /* Next frame */
        frame_idx++;
        
        /* Check loop mode */
        if (frame_idx >= NFRAMES) {
#ifdef LOOP
            if (LOOP) {
                frame_idx = 0;
            } else {
                /* Stay on last frame until terminated */
                while (!terminate_requested) {
                    sleep_ms(1000);
                }
                break;
            }
#else
            /* Default: loop */
            frame_idx = 0;
#endif
        }
        
        /* Apply delta to get next frame */
        if (frame_idx == 0) {
            load_frame_0(frames[0], frame_sizes[0]);
        } else {
            apply_delta(frames[frame_idx], frame_sizes[frame_idx]);
        }
        
        /* Delta-time: subtract processing time from sleep duration */
        long frame_end = get_time_ms();
        long elapsed = frame_end - frame_start;
        long sleep_time = FRAME_DURATION_MS - elapsed;
        
        if (sleep_time > 0) {
            sleep_ms((unsigned int)sleep_time);
        }
    }
#endif
    
    /* Graceful cleanup: clear framebuffer to black */
    memset(fbmem, 0, fb_size);
    munmap(fbmem, fb_size);
    close(fb_fd);
    return 0;
}
