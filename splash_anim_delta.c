/*
 * splash_anim_delta.c - Boot splash with RLE XOR delta frames
 * 
 * Architecture: Linux x86_64, freestanding (no libc)
 * Dependencies: nolibc.h only
 * 
 * Constraints:
 *   - Complexity: O(n) per frame where n = changed pixels
 *   - Memory: single frame buffer (8KB)
 *   - No dynamic allocation
 *   - No PNG decoding
 * 
 * Performance: XOR + memcpy only, deterministic boot time
 */

#include "nolibc.h"
#include "frames_delta.h"

/* Configuration */
#define FRAME_DURATION_MS 33  /* ~30 FPS */
#define VERTICAL_OFFSET  80   /* Pixels below center (Win10 style) */

/* Frame buffer - single buffer, updated in-place via XOR */
static uint16_t frame_buffer[FRAME_W * FRAME_H];

/* Decode RLE delta and apply XOR to frame_buffer */
static void apply_delta(const uint8_t *delta, size_t delta_size) {
    size_t pos = 0;
    int pixel_idx = 0;
    
    while (pos < delta_size) {
        uint8_t cmd = delta[pos++];
        
        if (cmd == 0x00) {
            /* End of frame */
            break;
        } else if (cmd & 0x80) {
            /* Skip: (cmd - 0x80 + 1) pixels unchanged */
            pixel_idx += (cmd & 0x7F) + 1;
        } else {
            /* Run: next cmd values are XOR deltas (little-endian uint16_t) */
            int count = cmd;
            for (int i = 0; i < count && pixel_idx < FRAME_W * FRAME_H; i++) {
                uint16_t xor_val = delta[pos] | (delta[pos + 1] << 8);
                pos += 2;
                frame_buffer[pixel_idx++] ^= xor_val;
            }
        }
    }
}

/* Load frame 0 (raw RGB565) into buffer */
static void load_frame_0(const uint8_t *raw) {
    for (int i = 0; i < FRAME_W * FRAME_H; i++) {
        frame_buffer[i] = raw[i * 2] | (raw[i * 2 + 1] << 8);
    }
}

/* Blit RGB565 frame to framebuffer (32bpp) */
static void blit_to_fb_32bpp(uint8_t *fbmem, int fb_w, int fb_h, int line_len,
                             const uint16_t *frame, int x, int y) {
    for (int row = 0; row < FRAME_H; row++) {
        if (y + row >= fb_h || y + row < 0) continue;
        
        uint32_t *dst = (uint32_t *)(fbmem + (y + row) * line_len + x * 4);
        const uint16_t *src = frame + row * FRAME_W;
        
        for (int col = 0; col < FRAME_W; col++) {
            if (x + col >= fb_w || x + col < 0) continue;
            
            uint16_t pixel = src[col];
            uint32_t r = (pixel >> 11) & 0x1F;
            uint32_t g = (pixel >> 5) & 0x3F;
            uint32_t b = pixel & 0x1F;
            
            dst[col] = (r << 3) << 16 | (g << 2) << 8 | (b << 3);
        }
    }
}

/* Blit RGB565 frame to framebuffer (16bpp) */
static void blit_to_fb_16bpp(uint8_t *fbmem, int fb_w, int fb_h, int line_len,
                             const uint16_t *frame, int x, int y) {
    for (int row = 0; row < FRAME_H; row++) {
        if (y + row >= fb_h || y + row < 0) continue;
        
        uint16_t *dst = (uint16_t *)(fbmem + (y + row) * line_len + x * 2);
        const uint16_t *src = frame + row * FRAME_W;
        
        int copy_len = FRAME_W;
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

/* High-precision sleep */
static void sleep_ms(unsigned int ms) {
    struct timespec ts = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000L
    };
    nanosleep(&ts, NULL);
}

/* Entry point */
void _start(void) {
    int fb_fd = -1;
    void *fbmem = NULL;
    size_t fb_size = 0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    
    /* Load frame 0 */
    load_frame_0(frames[0]);
    
    /* Open framebuffer */
    fb_fd = open("/dev/fb0", O_RDWR, 0);
    if (fb_fd < 0) {
        exit(1);
    }
    
    /* Get framebuffer info */
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0 ||
        ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        close(fb_fd);
        exit(1);
    }
    
    /* Map framebuffer */
    fb_size = finfo.smem_len;
    fbmem = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fbmem == MAP_FAILED) {
        close(fb_fd);
        exit(1);
    }
    
    /* Calculate position (centered horizontally, offset below center vertically) */
    int x = (vinfo.xres - FRAME_W) / 2;
    int y = (vinfo.yres - FRAME_H) / 2 + VERTICAL_OFFSET;
    
    /* Clear screen to black */
    memset(fbmem, 0, fb_size);
    
    /* Animation loop */
    int frame_idx = 0;
    while (1) {
        /* Blit current frame */
        if (vinfo.bits_per_pixel == 32) {
            blit_to_fb_32bpp(fbmem, vinfo.xres, vinfo.yres, 
                             finfo.line_length, frame_buffer, x, y);
        } else if (vinfo.bits_per_pixel == 16) {
            blit_to_fb_16bpp(fbmem, vinfo.xres, vinfo.yres,
                             finfo.line_length, frame_buffer, x, y);
        }
        
        /* Next frame */
        frame_idx = (frame_idx + 1) % NFRAMES;
        
        /* Apply delta to get next frame */
        if (frame_idx == 0) {
            /* Reset to frame 0 */
            load_frame_0(frames[0]);
        } else {
            apply_delta(frames[frame_idx], frame_sizes[frame_idx]);
        }
        
        sleep_ms(FRAME_DURATION_MS);
    }
    
    /* Unreachable, but clean for completeness */
    memset(fbmem, 0, fb_size);
    munmap(fbmem, fb_size);
    close(fb_fd);
    exit(0);
}
