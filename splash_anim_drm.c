/*
 * splash_anim_drm.c - Boot splash animation using DRM/KMS
 * 
 * DRM/KMS version using dumb buffers for simple software rendering.
 * Requires libdrm at build time and runtime (dynamic linking).
 * 
 * This is the modern path for systems without /dev/fb0 support.
 * Uses the same frames_delta.h format as the fbdev version.
 * 
 * Special mode: --restore-crtc restores CRTC from saved state file.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

/* Generated frame data */
#include "frames_delta.h"

/* CRTC state file for external restore after SIGKILL */
#define CRTC_STATE_FILE "/run/xbs_drm_crtc.info"

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
    
    /* Card index for restore */
    int card_idx;
    
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

/* Decode Raw RGB565 delta (direct pixel values, overwrites buffer) */
static void apply_delta_raw(const uint8_t *raw, size_t size) {
    int pixels = size / 2;
    if (pixels > FRAME_W * FRAME_H) pixels = FRAME_W * FRAME_H;
    
    for (int i = 0; i < pixels; i++) {
        frame_buffer[i] = raw[i * 2] | (raw[i * 2 + 1] << 8);
    }
}

/* Forward declaration */
static void decode_raw(const uint8_t *src, size_t src_len, uint16_t *dst, int dst_count);

/* Apply delta based on compression method */
static void apply_delta(const uint8_t *src, size_t src_len) {
#if COMPRESS_METHOD == 0
    apply_delta_rle_xor(src, src_len);
#elif COMPRESS_METHOD == 1
    apply_delta_rle_direct(src, src_len);
#elif COMPRESS_METHOD == 2
    apply_delta_sparse_xor(src, src_len);
#elif COMPRESS_METHOD == 3
    apply_delta_raw(src, src_len);
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
/* LZSS format: flag byte + 8 items (literal=1byte or back-ref=2bytes) */
#define LZSS_WINDOW_SIZE 4096
#define LZSS_MIN_MATCH  3

static void decompress_palette_lzss(const uint8_t *compressed, size_t comp_size,
                                     const uint16_t *palette, size_t palette_size,
                                     uint16_t *dst, int pixel_count) {
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
                dst[out_pos++] = palette[val < palette_size ? val : 0];
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
                    dst[out_pos++] = palette[val < palette_size ? val : 0];
                }
            }
        }
    }
}

/* ============================================================================
 * DRM/KMS Initialization
 * ============================================================================ */

static void drm_cleanup(xbs_drm_ctx_t *ctx);  /* forward declaration */
static void save_crtc_state(xbs_drm_ctx_t *ctx);  /* forward declaration */

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
    /* Open DRM device and find connected connector */
    int fd = -1;
    int found_card_idx = -1;
    
    /* Scan all DRI cards (handles multi-GPU, eGPU systems)
     * CRITICAL: Must check for CONNECTED connector, not just dumb buffer capability!
     * On hybrid laptops (Optimus: iGPU + dGPU), both cards may have dumb buffers,
     * but only the iGPU has the physical display connected. Selecting the dGPU
     * would result in splash displayed to nowhere (black screen for user).
     */
    for (int card_idx = 0; card_idx < 16; card_idx++) {
        char card_path[32];
        snprintf(card_path, sizeof(card_path), "/dev/dri/card%d", card_idx);
        
        fd = open(card_path, O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;
        
        /* Check for dumb buffer capability */
        uint64_t has_dumb;
        if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || !has_dumb) {
            close(fd);
            fd = -1;
            continue;
        }
        
        /* Check for CONNECTED connector - this is the critical fix for multi-GPU */
        drmModeRes *res = drmModeGetResources(fd);
        if (!res) {
            close(fd);
            fd = -1;
            continue;
        }
        
        /* Scan connectors for a connected one */
        int has_connected = 0;
        for (int i = 0; i < res->count_connectors; i++) {
            drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
            if (conn && conn->connection == DRM_MODE_CONNECTED) {
                has_connected = 1;
                drmModeFreeConnector(conn);
                break;
            }
            if (conn) drmModeFreeConnector(conn);
        }
        
        drmModeFreeResources(res);
        
        if (has_connected) {
            found_card_idx = card_idx;  /* Remember which card we're using */
            break;  /* Found usable device with connected display */
        }
        
        /* No connected display on this card - try next */
        close(fd);
        fd = -1;
    }
    
    if (fd < 0) {
        write(2, "DRM: No device with connected display\n", 38);
        return -ENODEV;
    }
    
    ctx->fd = fd;
    ctx->card_idx = found_card_idx;  /* Save for restore */
    
    /* Try to become DRM master (required for modesetting on older kernels)
     * On newer kernels (5.8+), modesetting works without master if CAP_SYS_ADMIN
     * So failure here is not fatal - we'll try modesetting anyway */
    if (drmSetMaster(fd) < 0) {
        write(2, "DRM: Cannot become master (continuing anyway)\n", 46);
        /* Don't fail - try to proceed with modesetting */
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
    
    /* Save to file for external restore after SIGKILL */
    save_crtc_state(ctx);
    
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
    
    /* Mark framebuffer as dirty for virtual GPUs (VMware/QXL) before cleanup */
    /* This ensures the virtual GPU releases all resources before handoff to tty1 */
    if (ctx->fb_id) {
        struct drm_mode_rect clip = {
            .x1 = 0,
            .y1 = 0,
            .x2 = ctx->width,
            .y2 = ctx->height
        };
        drmModeDirtyFB(ctx->fd, ctx->fb_id, &clip, 1);
    }
    
    /* Restore previous CRTC state */
    if (ctx->saved_crtc) {
        drmModeSetCrtc(ctx->fd, ctx->saved_crtc->crtc_id,
                       ctx->saved_crtc->buffer_id,
                       ctx->saved_crtc->x, ctx->saved_crtc->y,
                       &ctx->conn_id, 1, &ctx->saved_crtc->mode);
        drmModeFreeCrtc(ctx->saved_crtc);
        ctx->saved_crtc = NULL;
    }
    
    /* Remove CRTC state file */
    unlink(CRTC_STATE_FILE);
    
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

/* Save CRTC state to file for external restore (after SIGKILL) */
static void save_crtc_state(xbs_drm_ctx_t *ctx) {
    if (!ctx->saved_crtc) return;
    
    FILE *f = fopen(CRTC_STATE_FILE, "w");
    if (!f) return;
    
    fprintf(f, "%u %u %u %u %u %d ", 
            ctx->saved_crtc->crtc_id,
            ctx->saved_crtc->buffer_id,
            ctx->saved_crtc->x,
            ctx->saved_crtc->y,
            ctx->conn_id,
            ctx->card_idx);
    /* Write mode info */
    fwrite(&ctx->saved_crtc->mode, sizeof(drmModeModeInfo), 1, f);
    fclose(f);
}

/* Restore CRTC from saved state file (standalone mode) */
static int restore_crtc_from_file(void) {
    FILE *f = fopen(CRTC_STATE_FILE, "r");
    if (!f) return -1;
    
    uint32_t crtc_id, buffer_id, x, y, conn_id;
    int card_idx;
    drmModeModeInfo mode;
    
    if (fscanf(f, "%u %u %u %u %u %d ", &crtc_id, &buffer_id, &x, &y, &conn_id, &card_idx) != 6) {
        fclose(f);
        return -1;
    }
    
    if (fread(&mode, sizeof(drmModeModeInfo), 1, f) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    
    /* Open the SAME DRM device that was used during splash */
    char card_path[32];
    snprintf(card_path, sizeof(card_path), "/dev/dri/card%d", card_idx);
    int fd = open(card_path, O_RDWR);
    if (fd < 0) return -1;
    
    /* Try to become DRM master for modesetting */
    drmSetMaster(fd);  /* May fail if already master, that's OK */
    
    /* Restore CRTC */
    int ret = drmModeSetCrtc(fd, crtc_id, buffer_id, x, y, &conn_id, 1, &mode);
    drmDropMaster(fd);
    close(fd);
    
    /* Remove state file */
    unlink(CRTC_STATE_FILE);
    
    return ret;
}

/* ============================================================================
 * Blitting Functions (SSE2 optimized for VRAM Write-Combining)
 * ============================================================================ */

#include <emmintrin.h>

/* SSE2 optimized RGB565 to XRGB8888 conversion - processes 8 pixels at once
 * Uses non-temporal stores when aligned for optimal PCIe bandwidth to VRAM
 * Non-temporal stores bypass cache - ideal for Write-Combining VRAM memory */
static void blit_rgb565_to_xrgb8888_sse2(uint32_t *dst, const uint16_t *src, int count) {
    /* Hoist mask constants outside loop */
    const __m128i r_mask = _mm_set1_epi32(0x0000F800);  /* bits 11-15 */
    const __m128i g_mask = _mm_set1_epi32(0x000007E0);  /* bits 5-10 */
    const __m128i b_mask = _mm_set1_epi32(0x0000001F);  /* bits 0-4 */
    
    /* Check if destination is 16-byte aligned for streaming stores */
    int use_streaming = (((uintptr_t)dst & 0xF) == 0);
    
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
        __m128i r_lo = _mm_and_si128(lo, r_mask);
        __m128i r_hi = _mm_and_si128(hi, r_mask);
        r_lo = _mm_slli_epi32(r_lo, 5 + 3);  /* shift to bit 16, then expand 5->8 bits */
        r_hi = _mm_slli_epi32(r_hi, 5 + 3);
        
        /* G6 (bits 5-10) -> G8 (bits 8-15): shift left by 3 (to bit 8), then 2 more for expansion */
        __m128i g_lo = _mm_and_si128(lo, g_mask);
        __m128i g_hi = _mm_and_si128(hi, g_mask);
        g_lo = _mm_slli_epi32(g_lo, 3 + 2);  /* shift to bit 8, then expand 6->8 bits */
        g_hi = _mm_slli_epi32(g_hi, 3 + 2);
        
        /* B5 (bits 0-4) -> B8 (bits 0-7): no shift needed, just expand by 3 */
        __m128i b_lo = _mm_and_si128(lo, b_mask);
        __m128i b_hi = _mm_and_si128(hi, b_mask);
        b_lo = _mm_slli_epi32(b_lo, 3);  /* expand 5->8 bits */
        b_hi = _mm_slli_epi32(b_hi, 3);
        
        /* Combine R, G, B */
        __m128i result_lo = _mm_or_si128(_mm_or_si128(r_lo, g_lo), b_lo);
        __m128i result_hi = _mm_or_si128(_mm_or_si128(r_hi, g_hi), b_hi);
        
        /* Use streaming stores for aligned VRAM writes (2x bandwidth), unaligned for safety */
        if (use_streaming) {
            _mm_stream_si128((__m128i *)(dst + i), result_lo);
            _mm_stream_si128((__m128i *)(dst + i + 4), result_hi);
        } else {
            _mm_storeu_si128((__m128i *)(dst + i), result_lo);
            _mm_storeu_si128((__m128i *)(dst + i + 4), result_hi);
        }
    }
    
    /* SFENCE required after streaming stores to ensure completion */
    if (use_streaming) _mm_sfence();
    
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
    struct timespec rem;
    while (nanosleep(&req, &rem) != 0 && !terminate_requested) {
        req = rem;
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char **argv) {
    /* Check for restore mode */
    if (argc > 1 && strcmp(argv[1], "--restore-crtc") == 0) {
        int ret = restore_crtc_from_file();
        return ret < 0 ? 1 : 0;
    }
    
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
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (frame_buffer == MAP_FAILED) {
        write(2, "DRM: No memory\n", 14);
        drm_cleanup(&drm_ctx);
        return 1;
    }
    
    /* Load initial frame */
#if DISPLAY_MODE == 3 || DISPLAY_MODE == 4
    /* Static image - decompress or copy to frame_buffer */
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
#endif
    
#if DISPLAY_MODE == 1 || DISPLAY_MODE == 2
    /* Allocate and decompress background */
    bg_buffer = mmap(NULL, BG_W * BG_H * 2, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
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
        
        /* Mark framebuffer as dirty to trigger refresh - lighter than drmModeSetCrtc */
        struct drm_clip_rect clip = {
            .x1 = 0,
            .y1 = 0,
            .x2 = drm_ctx.width,
            .y2 = drm_ctx.height
        };
        drmModeDirtyFB(drm_ctx.fd, drm_ctx.fb_id, &clip, 1);
        
        if (terminate_requested) break;
        
        /* Next frame */
        frame_idx++;
        
        if (frame_idx >= NFRAMES) {
#if defined(LOOP_MODE) && LOOP_MODE == 0
            /* No loop: stay on last frame until terminated */
            while (!terminate_requested) sleep_ms(1000);
            break;
#elif defined(LOOP_MODE) && LOOP_MODE == 2
            /* Partial loop: replay frames 0 to LOOP_START-1 to restore buffer state */
            /* XOR deltas are chained, so we need correct base state before LOOP_START */
            load_frame_0(frames[0], frame_sizes[0]);
            for (int f = 1; f < LOOP_START && !terminate_requested; f++) {
                apply_delta(frames[f], frame_sizes[f]);
            }
            frame_idx = LOOP_START;
#else
            /* Default or LOOP_MODE=1: full loop from frame 0 */
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
