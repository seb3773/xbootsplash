/*
 * splash_anim_drm.c - Boot splash animation using DRM/KMS
 * 
 * DRM/KMS version using dumb buffers for simple software rendering.
 * Requires libdrm at build time and runtime (dynamic linking).
 * 
 * This is the modern path for systems without /dev/fb0 support.
 * Uses the same frames_delta.h format as the fbdev version.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

/* Generated frame data */
#include "frames_delta.h"

/* ============================================================================
 * DRM/KMS Context
 * ============================================================================ */

typedef struct {
    int fd;
    
    /* Connector and CRTC */
    uint32_t conn_id;
    uint32_t crtc_id;
    drmModeModeInfo mode;
    drmModeCrtc *saved_crtc;
    
    /* Framebuffer */
    uint32_t fb_id;
    uint32_t handle;
    uint32_t pitch;
    uint32_t size;
    uint8_t *map;
    
    /* Dimensions */
    uint32_t width;
    uint32_t height;
} xbs_drm_ctx_t;

/* Global for signal handler */
static volatile sig_atomic_t terminate_requested = 0;
static xbs_drm_ctx_t *g_drm_ctx = NULL;

/* Frame buffer for animation (RGB565) */
static uint16_t *frame_buffer = NULL;

#if DISPLAY_MODE == 1 || DISPLAY_MODE == 2
static uint16_t *bg_buffer = NULL;
#endif

/* ============================================================================
 * Signal Handling
 * ============================================================================ */

static void signal_handler(int sig) {
    (void)sig;
    terminate_requested = 1;
}

/* ============================================================================
 * Frame Decompression (same as fbdev version)
 * ============================================================================ */

/* RLE decode for frame 0 (RLE direct format)
 * 0x00 = end of frame
 * 0x01-0x7F = next N uint16_t literal values
 * 0x80-0xFF = repeat next uint16_t value (cmd & 0x7F) times */
static int decode_rle(const uint8_t *src, size_t src_len, uint16_t *dst, int dst_count) {
    int pos = 0;
    size_t i = 0;
    
    while (i < src_len && pos < dst_count) {
        uint8_t cmd = src[i++];
        
        if (cmd == 0) {
            /* End of frame */
            break;
        } else if (cmd <= 0x7F) {
            /* Literal run: next N uint16_t values */
            int n = cmd;
            if (i + n * 2 > src_len) n = (src_len - i) / 2;
            for (int j = 0; j < n && pos < dst_count; j++) {
                dst[pos++] = src[i] | (src[i+1] << 8);
                i += 2;
            }
        } else {
            /* RLE run: repeat next value (cmd & 0x7F) times */
            int count = cmd & 0x7F;
            if (i + 1 >= src_len) break;
            uint16_t value = src[i] | (src[i+1] << 8);
            i += 2;
            for (int j = 0; j < count && pos < dst_count; j++) {
                dst[pos++] = value;
            }
        }
    }
    
    return pos;
}

/* Apply XOR delta (RLE XOR format - for COMPRESS_METHOD 0) */
static void apply_delta_rle_xor(const uint8_t *src, size_t src_len) {
    int pos = 0;
    size_t i = 0;
    int count = FRAME_W * FRAME_H;
    
    while (i < src_len && pos < count) {
        uint8_t cmd = src[i++];
        
        if (cmd == 0) {
            break;
        } else if (cmd <= 0x7F) {
            /* XOR values */
            int n = cmd;
            if (i + n * 2 > src_len) n = (src_len - i) / 2;
            for (int j = 0; j < n && pos < count; j++) {
                uint16_t delta = src[i] | (src[i+1] << 8);
                frame_buffer[pos++] ^= delta;
                i += 2;
            }
        } else {
            /* Skip unchanged pixels */
            pos += (cmd & 0x7F) + 1;
        }
    }
}

/* Apply RLE Direct delta (COMPRESS_METHOD 1) 
 * Same format as RLE decode: 0x01-0x7F=literal, 0x80-0xFF=repeat */
static void apply_delta_rle_direct(const uint8_t *src, size_t src_len) {
    int pos = 0;
    size_t i = 0;
    int count = FRAME_W * FRAME_H;
    
    while (i < src_len && pos < count) {
        uint8_t cmd = src[i++];
        
        if (cmd == 0) {
            break;
        } else if (cmd <= 0x7F) {
            /* Literal run: next N uint16_t values */
            int n = cmd;
            if (i + n * 2 > src_len) n = (src_len - i) / 2;
            for (int j = 0; j < n && pos < count; j++) {
                frame_buffer[pos++] = src[i] | (src[i+1] << 8);
                i += 2;
            }
        } else {
            /* RLE run: repeat next value (cmd & 0x7F) times */
            int repeat = cmd & 0x7F;
            if (i + 1 >= src_len) break;
            uint16_t value = src[i] | (src[i+1] << 8);
            i += 2;
            for (int j = 0; j < repeat && pos < count; j++) {
                frame_buffer[pos++] = value;
            }
        }
    }
}

/* Apply delta based on compression method */
static void apply_delta(const uint8_t *src, size_t src_len) {
#if COMPRESS_METHOD == 0
    apply_delta_rle_xor(src, src_len);
#elif COMPRESS_METHOD == 1
    apply_delta_rle_direct(src, src_len);
#else
    /* Fallback: treat as raw */
    decode_raw(src, src_len, frame_buffer, FRAME_W * FRAME_H);
#endif
}

/* Decode Raw RGB565 (direct pixel values, no compression) */
static void decode_raw(const uint8_t *src, size_t src_len, uint16_t *dst, int dst_count) {
    int pixels = src_len / 2;
    if (pixels > dst_count) pixels = dst_count;
    
    for (int i = 0; i < pixels; i++) {
        dst[i] = src[i * 2] | (src[i * 2 + 1] << 8);
    }
}

/* Load frame 0 - always stored as raw RGB565 */
static void load_frame_0(const uint8_t *data, size_t size) {
    decode_raw(data, size, frame_buffer, FRAME_W * FRAME_H);
}

/* Palette + LZSS decompression */
static void decompress_palette_lzss(const uint8_t *compressed, size_t comp_size,
                                     const uint16_t *palette, size_t palette_size,
                                     uint16_t *dst, int pixel_count) {
    /* Simple LZSS: copy palette indices */
    int pos = 0;
    size_t i = 0;
    
    while (i < comp_size && pos < pixel_count) {
        uint8_t b = compressed[i++];
        
        if (b < 0x80) {
            /* Literal palette index */
            if (b < palette_size) {
                dst[pos++] = palette[b];
            }
        } else if (b >= 0x80 && b <= 0xBF) {
            /* Short match: (b - 0x80) + 2 bytes */
            /* Simplified: just copy next N bytes as palette indices */
            int n = (b - 0x80) + 2;
            for (int j = 0; j < n && pos < pixel_count && i < comp_size; j++) {
                uint8_t idx = compressed[i++];
                if (idx < palette_size) {
                    dst[pos++] = palette[idx];
                }
            }
        } else {
            /* Long match or RLE */
            if (i < comp_size) {
                int n = compressed[i++];
                for (int j = 0; j < n && pos < pixel_count; j++) {
                    /* Use previous pixel or palette[0] */
                    if (pos > 0) {
                        dst[pos] = dst[pos - 1];
                    } else if (palette_size > 0) {
                        dst[pos] = palette[0];
                    }
                    pos++;
                }
            }
        }
    }
}

/* ============================================================================
 * DRM/KMS Initialization
 * ============================================================================ */

static void drm_cleanup(xbs_drm_ctx_t *ctx);  /* forward declaration */

static int drm_find_connector(int fd, drmModeRes *res, xbs_drm_ctx_t *ctx) {
    drmModeConnector *conn = NULL;
    
    for (int i = 0; i < res->count_connectors; i++) {
        conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn) continue;
        
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            ctx->conn_id = conn->connector_id;
            memcpy(&ctx->mode, &conn->modes[0], sizeof(ctx->mode));
            ctx->width = conn->modes[0].hdisplay;
            ctx->height = conn->modes[0].vdisplay;
            drmModeFreeConnector(conn);
            return 0;
        }
        
        drmModeFreeConnector(conn);
    }
    
    return -ENOENT;
}

static int drm_find_crtc(int fd, drmModeRes *res, xbs_drm_ctx_t *ctx) {
    drmModeConnector *conn = drmModeGetConnector(fd, ctx->conn_id);
    if (!conn) return -ENOENT;
    
    /* Try currently attached encoder first */
    if (conn->encoder_id) {
        drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoder_id);
        if (enc && enc->crtc_id) {
            ctx->crtc_id = enc->crtc_id;
            drmModeFreeEncoder(enc);
            drmModeFreeConnector(conn);
            return 0;
        }
        if (enc) drmModeFreeEncoder(enc);
    }
    
    /* Find a CRTC that works with this connector */
    for (int i = 0; i < conn->count_encoders; i++) {
        drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoders[i]);
        if (!enc) continue;
        
        for (int j = 0; j < res->count_crtcs; j++) {
            if (enc->possible_crtcs & (1 << j)) {
                ctx->crtc_id = res->crtcs[j];
                drmModeFreeEncoder(enc);
                drmModeFreeConnector(conn);
                return 0;
            }
        }
        
        drmModeFreeEncoder(enc);
    }
    
    drmModeFreeConnector(conn);
    return -ENOENT;
}

static int drm_create_fb(int fd, xbs_drm_ctx_t *ctx) {
    struct drm_mode_create_dumb creq = {
        .width = ctx->width,
        .height = ctx->height,
        .bpp = 32
    };
    
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
        return -errno;
    }
    
    ctx->handle = creq.handle;
    ctx->pitch = creq.pitch;
    ctx->size = creq.size;
    
    /* Create framebuffer */
    if (drmModeAddFB(fd, ctx->width, ctx->height, 24, 32, ctx->pitch, 
                     ctx->handle, &ctx->fb_id) < 0) {
        struct drm_mode_destroy_dumb dreq = { .handle = ctx->handle };
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        return -errno;
    }
    
    /* Map dumb buffer */
    struct drm_mode_map_dumb mreq = { .handle = ctx->handle };
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) < 0) {
        drmModeRmFB(fd, ctx->fb_id);
        struct drm_mode_destroy_dumb dreq = { .handle = ctx->handle };
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        return -errno;
    }
    
    ctx->map = mmap(NULL, ctx->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
    if (ctx->map == MAP_FAILED) {
        drmModeRmFB(fd, ctx->fb_id);
        struct drm_mode_destroy_dumb dreq = { .handle = ctx->handle };
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        return -errno;
    }
    
    /* Clear to black */
    memset(ctx->map, 0, ctx->size);
    
    return 0;
}

static int drm_init(xbs_drm_ctx_t *ctx) {
    int ret;
    
    /* Open DRM device */
    const char *cards[] = { "/dev/dri/card0", "/dev/dri/card1", NULL };
    int fd = -1;
    
    for (int i = 0; cards[i]; i++) {
        fd = open(cards[i], O_RDWR | O_CLOEXEC);
        if (fd >= 0) {
            uint64_t has_dumb;
            if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) >= 0 && has_dumb) {
                break;
            }
            close(fd);
            fd = -1;
        }
    }
    
    if (fd < 0) {
        write(2, "DRM: No device\n", 15);
        return -ENODEV;
    }
    
    ctx->fd = fd;
    
    /* Try to become DRM master (required for modesetting)
     * This will fail if X11/Wayland is already running */
    if (drmSetMaster(fd) < 0) {
        write(2, "DRM: Cannot become master\n", 26);
        close(fd);
        return -EBUSY;
    }
    
    /* Get resources */
    drmModeRes *res = drmModeGetResources(fd);
    if (!res) {
        close(fd);
        return -errno;
    }
    
    /* Find connected connector */
    ret = drm_find_connector(fd, res, ctx);
    if (ret < 0) {
        write(2, "DRM: No connector\n", 18);
        drmModeFreeResources(res);
        close(fd);
        return ret;
    }
    
    /* Find CRTC */
    ret = drm_find_crtc(fd, res, ctx);
    if (ret < 0) {
        write(2, "DRM: No CRTC\n", 13);
        drmModeFreeResources(res);
        close(fd);
        return ret;
    }
    
    drmModeFreeResources(res);
    
    /* Create framebuffer */
    ret = drm_create_fb(fd, ctx);
    if (ret < 0) {
        write(2, "DRM: No framebuffer\n", 20);
        close(fd);
        return ret;
    }
    
    /* Save current CRTC state */
    ctx->saved_crtc = drmModeGetCrtc(fd, ctx->crtc_id);
    
    /* Set mode */
    ret = drmModeSetCrtc(fd, ctx->crtc_id, ctx->fb_id, 0, 0, 
                         &ctx->conn_id, 1, &ctx->mode);
    if (ret < 0) {
        write(2, "DRM: CRTC failed\n", 17);
        drm_cleanup(ctx);
        return ret;
    }
    
    return 0;
}

static void drm_cleanup(xbs_drm_ctx_t *ctx) {
    if (!ctx || ctx->fd < 0) return;
    
    /* Restore previous CRTC state */
    if (ctx->saved_crtc) {
        drmModeSetCrtc(ctx->fd, ctx->saved_crtc->crtc_id,
                       ctx->saved_crtc->buffer_id,
                       ctx->saved_crtc->x, ctx->saved_crtc->y,
                       &ctx->conn_id, 1, &ctx->saved_crtc->mode);
        drmModeFreeCrtc(ctx->saved_crtc);
    }
    
    /* Release DRM master */
    drmDropMaster(ctx->fd);
    
    /* Unmap buffer */
    if (ctx->map && ctx->map != MAP_FAILED) {
        munmap(ctx->map, ctx->size);
    }
    
    /* Remove framebuffer */
    if (ctx->fb_id) {
        drmModeRmFB(ctx->fd, ctx->fb_id);
    }
    
    /* Destroy dumb buffer */
    if (ctx->handle) {
        struct drm_mode_destroy_dumb dreq = { .handle = ctx->handle };
        drmIoctl(ctx->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
    }
    
    close(ctx->fd);
    ctx->fd = -1;
}

/* ============================================================================
 * Blitting Functions (SSE2 optimized for VRAM Write-Combining)
 * ============================================================================ */

#include <emmintrin.h>

/* SSE2 optimized RGB565 to XRGB8888 conversion - processes 8 pixels at once
 * Critical for VRAM: uses _mm_storeu_si128 (16-byte writes) for optimal PCIe bandwidth
 * Write-Combining requires contiguous 64-byte blocks for best performance */
static void blit_rgb565_to_xrgb8888_sse2(uint32_t *dst, const uint16_t *src, int count) {
    int i = 0;
    
    /* Process 8 pixels at a time */
    for (; i + 7 < count; i += 8) {
        /* Load 8 RGB565 pixels (16 bytes) */
        __m128i pixels = _mm_loadu_si128((__m128i const *)(src + i));
        
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
        
        /* Store 8 XRGB8888 pixels (32 bytes) - 16-byte aligned stores for WC efficiency */
        _mm_storeu_si128((__m128i *)(dst + i), result_lo);
        _mm_storeu_si128((__m128i *)(dst + i + 4), result_hi);
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

/* Blit RGB565 frame to XRGB8888 DRM framebuffer */
static void blit_to_drm(uint8_t *fb, int fb_w, int fb_h, int fb_pitch,
                        const uint16_t *frame, int fw, int fh, int x, int y) {
    /* Clip to framebuffer bounds */
    if (x < 0) { fw += x; frame -= x; x = 0; }
    if (y < 0) { fh += y; frame -= y * FRAME_W; y = 0; }
    if (x + fw > fb_w) fw = fb_w - x;
    if (y + fh > fb_h) fh = fb_h - y;
    if (fw <= 0 || fh <= 0) return;
    
    /* Convert RGB565 to XRGB8888 with SSE2 - row by row */
    for (int row = 0; row < fh; row++) {
        uint32_t *dst = (uint32_t *)(fb + (y + row) * fb_pitch + x * 4);
        const uint16_t *src = frame + row * FRAME_W;
        blit_rgb565_to_xrgb8888_sse2(dst, src, fw);
    }
}

/* Fill framebuffer with solid color - SSE2 optimized for VRAM */
static void fill_fb_color(uint8_t *fb, int fb_w, int fb_h, int fb_pitch, uint16_t color) {
    uint32_t r = (color >> 11) & 0x1F;
    uint32_t g = (color >> 5) & 0x3F;
    uint32_t b = color & 0x1F;
    uint32_t pixel = (r << 19) | (g << 10) | (b << 3);
    
    /* Use SSE2 for optimal VRAM write-combining */
    __m128i vpixel = _mm_set1_epi32((int)pixel);
    
    for (int y = 0; y < fb_h; y++) {
        uint32_t *dst = (uint32_t *)(fb + y * fb_pitch);
        int x = 0;
        
        /* Fill 4 pixels (16 bytes) at a time - optimal for WC */
        for (; x + 3 < fb_w; x += 4) {
            _mm_storeu_si128((__m128i *)(dst + x), vpixel);
        }
        
        /* Handle remaining pixels */
        for (; x < fb_w; x++) {
            dst[x] = pixel;
        }
    }
}

/* ============================================================================
 * Kill Switch - Check /proc/cmdline
 * ============================================================================ */

static int check_cmdline_disable(void) {
    int fd = open("/proc/cmdline", O_RDONLY);
    if (fd < 0) return 0;
    
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    
    if (n <= 0) return 0;
    buf[n] = '\0';
    
    char *p = buf;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (!*p) break;
        
        /* Check for nosplash */
        if (strncmp(p, "nosplash", 8) == 0) {
            char next = p[8];
            if (next == ' ' || next == '\t' || next == '\n' || next == '\0') {
                return 1;
            }
        }
        
        /* Check for xbootsplash=0 */
        if (strncmp(p, "xbootsplash=0", 13) == 0) {
            char next = p[13];
            if (next == ' ' || next == '\t' || next == '\n' || next == '\0') {
                return 1;
            }
        }
        
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
    }
    
    return 0;
}

/* ============================================================================
 * Timing
 * ============================================================================ */

static long get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void sleep_ms(unsigned int ms) {
    struct timespec req = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000L
    };
    nanosleep(&req, NULL);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    xbs_drm_ctx_t drm_ctx = {0};
    int ret;
    
    /* Kill switch */
    if (check_cmdline_disable()) {
        return 0;
    }
    
    /* Setup signal handlers */
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    /* Initialize DRM */
    ret = drm_init(&drm_ctx);
    if (ret < 0) {
        write(2, "DRM: Init failed\n", 17);
        return 1;
    }
    
    g_drm_ctx = &drm_ctx;
    
    /* Allocate frame buffer */
    frame_buffer = mmap(NULL, FRAME_W * FRAME_H * 2, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (frame_buffer == MAP_FAILED) {
        write(2, "DRM: No memory\n", 14);
        drm_cleanup(&drm_ctx);
        return 1;
    }
    
#if DISPLAY_MODE == 1 || DISPLAY_MODE == 2
    /* Allocate and decompress background */
    bg_buffer = mmap(NULL, BG_W * BG_H * 2, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (bg_buffer == MAP_FAILED) {
        munmap(frame_buffer, FRAME_W * FRAME_H * 2);
        drm_cleanup(&drm_ctx);
        return 1;
    }
    decompress_palette_lzss(bg_compressed, BG_COMPRESSED_SIZE, bg_palette, BG_PALETTE_SIZE,
                            bg_buffer, BG_W * BG_H);
#endif
    
    /* Calculate position */
    int x, y;
    
#if DISPLAY_MODE == 4
    x = (drm_ctx.width - FRAME_W) / 2 + HORIZONTAL_OFFSET;
    y = (drm_ctx.height - FRAME_H) / 2 + VERTICAL_OFFSET;
    fill_fb_color(drm_ctx.map, drm_ctx.width, drm_ctx.height, drm_ctx.pitch, BACKGROUND_COLOR);
#elif DISPLAY_MODE == 1 || DISPLAY_MODE == 2
    x = (drm_ctx.width - FRAME_W) / 2 + HORIZONTAL_OFFSET;
    y = (drm_ctx.height - FRAME_H) / 2 + VERTICAL_OFFSET;
    blit_to_drm(drm_ctx.map, drm_ctx.width, drm_ctx.height, drm_ctx.pitch,
                bg_buffer, BG_W, BG_H, 0, 0);
#else
    x = (drm_ctx.width - FRAME_W) / 2 + HORIZONTAL_OFFSET;
    y = (drm_ctx.height - FRAME_H) / 2 + VERTICAL_OFFSET;
    fill_fb_color(drm_ctx.map, drm_ctx.width, drm_ctx.height, drm_ctx.pitch, BACKGROUND_COLOR);
#endif
    
    /* Main loop */
#if DISPLAY_MODE == 3 || DISPLAY_MODE == 4
    /* Static image */
    blit_to_drm(drm_ctx.map, drm_ctx.width, drm_ctx.height, drm_ctx.pitch,
                frame_buffer, FRAME_W, FRAME_H, x, y);
    
    while (!terminate_requested) {
        sleep_ms(1000);
    }
#else
    /* Animation */
    int frame_idx = 0;
    
    /* Load first frame */
    load_frame_0(frames[0], frame_sizes[0]);
    
    while (!terminate_requested) {
        long frame_start = get_time_ms();
        
        /* Blit current frame */
        blit_to_drm(drm_ctx.map, drm_ctx.width, drm_ctx.height, drm_ctx.pitch,
                    frame_buffer, FRAME_W, FRAME_H, x, y);
        
        /* Force CRTC refresh - required for dumb buffer updates to be visible */
        drmModeSetCrtc(drm_ctx.fd, drm_ctx.crtc_id, drm_ctx.fb_id, 0, 0,
                      &drm_ctx.conn_id, 1, &drm_ctx.mode);
        
        if (terminate_requested) break;
        
        /* Next frame */
        frame_idx++;
        
        if (frame_idx >= NFRAMES) {
#ifdef LOOP
            if (LOOP) {
                frame_idx = 0;
                load_frame_0(frames[0], frame_sizes[0]);
            } else {
                while (!terminate_requested) sleep_ms(1000);
                break;
            }
#else
            frame_idx = 0;
            load_frame_0(frames[0], frame_sizes[0]);
#endif
        }
        
        /* Apply delta */
        if (frame_idx > 0) {
            apply_delta(frames[frame_idx], frame_sizes[frame_idx]);
        }
        
        /* Delta-time sleep */
        long elapsed = get_time_ms() - frame_start;
        long sleep_time = FRAME_DURATION_MS - elapsed;
        if (sleep_time > 0) {
            sleep_ms((unsigned int)sleep_time);
        }
    }
#endif
    
    /* Cleanup */
    memset(drm_ctx.map, 0, drm_ctx.size);  /* Clear to black */
    munmap(frame_buffer, FRAME_W * FRAME_H * 2);
#if DISPLAY_MODE == 1 || DISPLAY_MODE == 2
    munmap(bg_buffer, BG_W * BG_H * 2);
#endif
    drm_cleanup(&drm_ctx);
    
    return 0;
}
