/*
 * extract_frames.c - Extract frames and metadata from xbootsplash binary
 * by seb3773
 * 
 * Build: gcc -O2 -o extract_frames extract_frames.c -lpng
 * Usage: ./extract_frames <xbs_binary> [output_dir]
 * 
 * Extracts:
 *   - metadata.txt: binary metadata
 *   - frame_N.png: decompressed frames as PNG images
 *   - static/background.png: background image for anim-with-bg modes
 *   - static.png: static image for static modes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <png.h>
#include <errno.h>

#define MAGIC_KEY 0xA7F3B219

/* Watermark structure (matches splash_anim_*.c) */
struct fb_sync_state {
    uint32_t sync_key;
    uint16_t ver;
    uint16_t mode;
    uint16_t fw;
    uint16_t fh;
    uint16_t nf;
    uint16_t comp;
    uint16_t delay;
    uint16_t loop;
    uint32_t crc;
};

/* Compression methods */
static const char *comp_names[] = {
    "raw", "rle_xor", "rle_direct", "sparse", "rle_xor_opt", "palette_lzss"
};

/* Display modes */
static const char *mode_names[] = {
    "Anim solid bg", "Anim image centered", "Static centered", "Static fullscreen", "Anim image fullscreen"
};

/* LZSS decompression */
#define LZSS_WINDOW_SIZE 4096
#define LZSS_MIN_MATCH  3

static size_t decompress_lzss(const uint8_t *compressed, size_t comp_size,
                              uint8_t *out, int out_size) {
    uint8_t window[LZSS_WINDOW_SIZE];
    int window_pos = 0;
    int out_pos = 0;
    size_t in_pos = 0;
    
    memset(window, 0, LZSS_WINDOW_SIZE);
    
    while (in_pos < comp_size && out_pos < out_size) {
        if (in_pos >= comp_size) break;
        uint8_t flag = compressed[in_pos++];
        
        for (int bit = 0; bit < 8 && out_pos < out_size; bit++) {
            if (in_pos >= comp_size) break;
            
            if (flag & (1 << bit)) {
                uint8_t val = compressed[in_pos++];
                window[window_pos] = val;
                window_pos = (window_pos + 1) & (LZSS_WINDOW_SIZE - 1);
                out[out_pos++] = val;
            } else {
                if (in_pos + 1 >= comp_size) break;
                uint8_t b1 = compressed[in_pos++];
                uint8_t b2 = compressed[in_pos++];
                
                int offset = (b1 | ((b2 & 0xF0) << 4));
                int length = (b2 & 0x0F) + LZSS_MIN_MATCH;
                
                if (offset == 0) offset = 1;
                
                for (int i = 0; i < length && out_pos < out_size; i++) {
                    int win_idx = (window_pos - offset + LZSS_WINDOW_SIZE) & (LZSS_WINDOW_SIZE - 1);
                    uint8_t val = window[win_idx];
                    window[window_pos] = val;
                    window_pos = (window_pos + 1) & (LZSS_WINDOW_SIZE - 1);
                    out[out_pos++] = val;
                }
            }
        }
    }
    return out_pos;
}

/* Decode RLE XOR delta and apply to frame buffer */
static size_t apply_delta_rle_xor(const uint8_t *delta, size_t delta_size, 
                                  uint16_t *frame, int pixels) {
    size_t pos = 0;
    int pixel_idx = 0;
    
    while (pos < delta_size && pixel_idx < pixels) {
        uint8_t cmd = delta[pos++];
        
        if (cmd == 0) {
            break;
        } else if (cmd & 0x80) {
            int skip = (cmd & 0x7F) + 1;
            pixel_idx += skip;
        } else {
            int count = cmd;
            for (int i = 0; i < count && pixel_idx < pixels; i++) {
                if (pos + 1 >= delta_size) break;
                uint16_t xor_val = delta[pos] | (delta[pos+1] << 8);
                pos += 2;
                frame[pixel_idx++] ^= xor_val;
            }
        }
    }
    return pos;
}

/* Decode RLE Direct (frame 0 or standalone) */
static size_t decode_rle_direct(const uint8_t *data, size_t size,
                                uint16_t *out, int pixels) {
    size_t pos = 0;
    int pixel_idx = 0;
    
    while (pos < size && pixel_idx < pixels) {
        uint8_t cmd = data[pos++];
        
        if (cmd == 0) {
            break;
        } else if (cmd & 0x80) {
            /* RLE run */
            int count = (cmd & 0x7F) + 1;
            if (pos + 1 >= size) break;
            uint16_t val = data[pos] | (data[pos+1] << 8);
            pos += 2;
            for (int i = 0; i < count && pixel_idx < pixels; i++) {
                out[pixel_idx++] = val;
            }
        } else {
            /* Literal run */
            int count = cmd;
            for (int i = 0; i < count && pixel_idx < pixels; i++) {
                if (pos + 1 >= size) break;
                uint16_t val = data[pos] | (data[pos+1] << 8);
                pos += 2;
                out[pixel_idx++] = val;
            }
        }
    }
    return pos;
}

/* Decode sparse delta */
static size_t apply_delta_sparse(const uint8_t *delta, size_t delta_size,
                                 uint16_t *frame, int pixels) {
    size_t pos = 0;
    
    while (pos + 4 <= delta_size) {
        uint16_t offset = delta[pos] | (delta[pos+1] << 8);
        uint16_t count = delta[pos+2] | (delta[pos+3] << 8);
        pos += 4;
        
        if (offset == 0xFFFF && count == 0) break;  /* End marker */
        if (offset >= pixels) break;
        
        for (int i = 0; i < count && offset + i < pixels && pos + 1 < delta_size; i++) {
            uint16_t xor_val = delta[pos] | (delta[pos+1] << 8);
            pos += 2;
            frame[offset + i] ^= xor_val;
        }
    }
    return pos;
}

/* Save RGB565 buffer as PNG */
static int save_png(const char *path, uint16_t *pixels, int w, int h) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(fp); return -1; }
    
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_write_struct(&png, NULL); fclose(fp); return -1; }
    
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return -1;
    }
    
    png_init_io(png, fp);
    png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    
    /* Convert RGB565 to RGB888 */
    uint8_t *row = malloc(w * 3);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint16_t px = pixels[y * w + x];
            uint8_t r = (px >> 11) & 0x1F;
            uint8_t g = (px >> 5) & 0x3F;
            uint8_t b = px & 0x1F;
            row[x*3 + 0] = (r << 3) | (r >> 2);  /* 5 -> 8 bits */
            row[x*3 + 1] = (g << 2) | (g >> 4);  /* 6 -> 8 bits */
            row[x*3 + 2] = (b << 3) | (b >> 2);  /* 5 -> 8 bits */
        }
        png_write_row(png, row);
    }
    free(row);
    
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    return 0;
}

/* Find section offset and size */
static int find_section(FILE *f, const char *name, size_t *offset, size_t *size) {
    fseek(f, 0, SEEK_SET);
    
    uint8_t e_ident[16];
    if (fread(e_ident, 1, 16, f) != 16) return -1;
    
    if (e_ident[4] != 2) return -1;  /* Not 64-bit */
    
    uint64_t e_shoff;
    uint16_t e_shentsize, e_shnum, e_shstrndx;
    
    fseek(f, 40, SEEK_SET);
    if (fread(&e_shoff, 8, 1, f) != 1) return -1;
    
    fseek(f, 58, SEEK_SET);
    if (fread(&e_shentsize, 2, 1, f) != 1) return -1;
    if (fread(&e_shnum, 2, 1, f) != 1) return -1;
    if (fread(&e_shstrndx, 2, 1, f) != 1) return -1;
    
    /* Read section header string table */
    uint64_t shstr_offset = 0;
    uint64_t shstr_size = 0;
    
    fseek(f, e_shoff + e_shstrndx * e_shentsize + 24, SEEK_SET);
    if (fread(&shstr_offset, 8, 1, f) != 1) return -1;
    if (fread(&shstr_size, 8, 1, f) != 1) return -1;
    
    char *shstrtab = malloc(shstr_size);
    fseek(f, shstr_offset, SEEK_SET);
    if (fread(shstrtab, 1, shstr_size, f) != shstr_size) {
        free(shstrtab);
        return -1;
    }
    
    for (int i = 0; i < e_shnum; i++) {
        uint32_t sh_name;
        uint64_t sh_offset, sh_size;
        
        fseek(f, e_shoff + i * e_shentsize, SEEK_SET);
        if (fread(&sh_name, 4, 1, f) != 1) continue;
        
        fseek(f, e_shoff + i * e_shentsize + 24, SEEK_SET);
        if (fread(&sh_offset, 8, 1, f) != 1) continue;
        if (fread(&sh_size, 8, 1, f) != 1) continue;
        
        if (sh_name < shstr_size && strcmp(shstrtab + sh_name, name) == 0) {
            *offset = sh_offset;
            *size = sh_size;
            free(shstrtab);
            return 0;
        }
    }
    
    free(shstrtab);
    return -1;
}

/* Scan for magic key */
static int find_watermark_scan(FILE *f, size_t *offset) {
    uint32_t magic = MAGIC_KEY;
    uint8_t buf[4096];
    size_t pos = 0;
    
    fseek(f, 0, SEEK_SET);
    
    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i + 4 <= bytes_read; i++) {
            uint32_t val;
            memcpy(&val, buf + i, 4);
            if (val == magic) {
                *offset = pos + i;
                return 0;
            }
        }
        pos += bytes_read;
    }
    
    return -1;
}

/* Extract background image from palette + LZSS */
static int extract_background(const uint8_t *rodata, size_t rodata_size,
                              size_t *pos, int w, int h, const char *output_dir) {
    int pixels = w * h;
    const size_t pixels_sz = (size_t)pixels;
    
    /* Try different palette sizes (256 down to 4) */
    for (int pal_size = 256; pal_size >= 4; pal_size--) {
        size_t pal_bytes = pal_size * 2;
        
        if (*pos + pal_bytes + 10 > rodata_size) continue;
        
        /* Read potential palette */
        const uint16_t *palette = (const uint16_t *)(rodata + *pos);
        
        /* Validate palette: check if values look like RGB565 */
        int valid = 1;
        int non_zero = 0;
        for (int i = 0; i < pal_size && valid; i++) {
            uint16_t c = palette[i];
            if (c != 0) non_zero++;
            /* RGB565: R in [0,31], G in [0,63], B in [0,31] - always valid for 16-bit */
        }
        
        if (non_zero < pal_size / 4) continue;  /* Too many zeros - unlikely palette */
        
        /* Try to decompress LZSS data after palette */
        size_t comp_start = *pos + pal_bytes;
        size_t max_comp = rodata_size - comp_start;
        
        if (max_comp < pixels_sz / 8) continue;  /* Too small */
        
        uint8_t *indices = malloc(pixels_sz);
        
        /* Try different compressed sizes */
        for (size_t comp_size = pixels_sz / 8; comp_size < max_comp && comp_size < pixels_sz; comp_size += 128) {
            size_t decoded = decompress_lzss(rodata + comp_start, comp_size, indices, pixels_sz);
            
            if (decoded == pixels_sz) {
                /* Success! Convert indices to pixels via palette */
                uint16_t *img = malloc(pixels * sizeof(uint16_t));
                for (int i = 0; i < pixels; i++) {
                    img[i] = palette[indices[i] < pal_size ? indices[i] : 0];
                }
                
                /* Create static directory */
                char static_dir[512];
                snprintf(static_dir, sizeof(static_dir), "%s/static", output_dir);
                mkdir(static_dir, 0755);
                
                /* Save background */
                char bg_path[512];
                snprintf(bg_path, sizeof(bg_path), "%s/static/background.png", output_dir);
                if (save_png(bg_path, img, w, h) == 0) {
                    printf("Written: %s (palette=%d, comp=%zu)\n", bg_path, pal_size, comp_size);
                    *pos = comp_start + comp_size;
                    free(indices);
                    free(img);
                    return 1;
                }
                free(img);
            }
        }
        free(indices);
    }
    
    return 0;
}

/* Extract static image (mode 3/4) - palette + LZSS */
static int extract_static_image(const uint8_t *rodata, size_t rodata_size,
                                int w, int h, const char *output_dir) {
    int pixels = w * h;
    const size_t pixels_sz = (size_t)pixels;
    
    /* Try different palette sizes */
    for (int pal_size = 256; pal_size >= 4; pal_size--) {
        size_t pal_bytes = pal_size * 2;
        
        if (pal_bytes >= rodata_size) continue;
        
        const uint16_t *palette = (const uint16_t *)rodata;
        
        int non_zero = 0;
        for (int i = 0; i < pal_size; i++) {
            if (palette[i] != 0) non_zero++;
        }
        
        if (non_zero < pal_size / 4) continue;
        
        uint8_t *indices = malloc(pixels_sz);
        size_t comp_size = rodata_size - pal_bytes;
        
        size_t decoded = decompress_lzss(rodata + pal_bytes, comp_size, indices, pixels_sz);
        
        if (decoded == pixels_sz) {
            uint16_t *img = malloc(pixels * sizeof(uint16_t));
            for (int i = 0; i < pixels; i++) {
                img[i] = palette[indices[i] < pal_size ? indices[i] : 0];
            }
            
            char png_path[512];
            snprintf(png_path, sizeof(png_path), "%s/static.png", output_dir);
            if (save_png(png_path, img, w, h) == 0) {
                printf("Written: %s (palette=%d)\n", png_path, pal_size);
                free(indices);
                free(img);
                return 1;
            }
            free(img);
        }
        free(indices);
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <xbs_binary> [output_dir]\n", argv[0]);
        return 1;
    }
    
    const char *binary_path = argv[1];
    const char *output_dir = (argc > 2) ? argv[2] : NULL;
    
    FILE *f = fopen(binary_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open %s\n", binary_path);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Find watermark */
    size_t wm_offset = 0, wm_size = 0;
    
    if (find_section(f, ".rodata.cfg", &wm_offset, &wm_size) != 0) {
        if (find_watermark_scan(f, &wm_offset) != 0) {
            fprintf(stderr, "Error: Cannot find watermark\n");
            fclose(f);
            return 1;
        }
        wm_size = sizeof(struct fb_sync_state);
    }
    
    /* Read watermark */
    struct fb_sync_state wm;
    fseek(f, wm_offset, SEEK_SET);
    if (fread(&wm, sizeof(wm), 1, f) != 1) {
        fprintf(stderr, "Error: Cannot read watermark\n");
        fclose(f);
        return 1;
    }
    
    if (wm.sync_key != MAGIC_KEY) {
        fprintf(stderr, "Error: Invalid magic (0x%08X)\n", wm.sync_key);
        fclose(f);
        return 1;
    }
    
    /* Print metadata */
    printf("=== Xbootsplash Binary Analysis ===\n");
    printf("Binary: %s\n", binary_path);
    printf("Size: %zu bytes (%.1f KB)\n", file_size, file_size / 1024.0);
    printf("\n--- Metadata ---\n");
    printf("Watermark offset: 0x%lX\n", (unsigned long)wm_offset);
    printf("Version: %d\n", wm.ver);
    printf("Mode: %d (%s)\n", wm.mode, 
           (wm.mode < 5) ? mode_names[wm.mode] : "unknown");
    printf("Width: %d px\n", wm.fw);
    printf("Height: %d px\n", wm.fh);
    printf("Frames: %d\n", wm.nf);
    printf("Compression: %d (%s)\n", wm.comp,
           (wm.comp < 6) ? comp_names[wm.comp] : "unknown");
    printf("Delay: %d ms\n", wm.delay);
    printf("Loop: %d\n", wm.loop);
    printf("CRC: 0x%08X\n", wm.crc);
    
    /* Find .rodata section */
    size_t rodata_offset = 0, rodata_size = 0;
    if (find_section(f, ".rodata", &rodata_offset, &rodata_size) == 0) {
        printf("\n--- Frame Data ---\n");
        printf(".rodata offset: 0x%lX\n", (unsigned long)rodata_offset);
        printf(".rodata size: %zu bytes\n", rodata_size);
    }
    
    /* Extract frames if output dir specified */
    if (output_dir) {
        printf("\n--- Extracting frames ---\n");
        
        if (mkdir(output_dir, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "Error: Cannot create %s\n", output_dir);
            fclose(f);
            return 1;
        }
        
        /* Write metadata */
        char meta_path[512];
        snprintf(meta_path, sizeof(meta_path), "%s/metadata.txt", output_dir);
        FILE *mf = fopen(meta_path, "w");
        if (!mf) {
            fprintf(stderr, "Error: Cannot create %s\n", meta_path);
            fclose(f);
            return 1;
        }

        int ret = 0;
        ret |= fprintf(mf, "# Xbootsplash binary metadata\n") < 0;
        ret |= fprintf(mf, "binary=%s\n", binary_path) < 0;
        ret |= fprintf(mf, "version=%d\n", wm.ver) < 0;
        ret |= fprintf(mf, "mode=%d\n", wm.mode) < 0;
        ret |= fprintf(mf, "frame_width=%d\n", wm.fw) < 0;
        ret |= fprintf(mf, "frame_height=%d\n", wm.fh) < 0;
        ret |= fprintf(mf, "frame_count=%d\n", wm.nf) < 0;
        ret |= fprintf(mf, "compression=%d\n", wm.comp) < 0;
        ret |= fprintf(mf, "frame_delay_ms=%d\n", wm.delay) < 0;
        ret |= fprintf(mf, "loop_mode=%d\n", wm.loop) < 0;
        ret |= fprintf(mf, "crc=0x%08X\n", wm.crc) < 0;

        if (ret != 0) {
            fprintf(stderr, "Error: Failed to write metadata (disk full?)\n");
            fclose(mf);
            unlink(meta_path);
            fclose(f);
            return 1;
        }

        if (fclose(mf) != 0) {
            fprintf(stderr, "Error: Failed to close file (data may be lost)\n");
            unlink(meta_path);
            fclose(f);
            return 1;
        }

        printf("Written: %s\n", meta_path);
        
        /* Read frame data */
        if (rodata_size > 0 && wm.nf > 0) {
            uint8_t *rodata = malloc(rodata_size);
            fseek(f, rodata_offset, SEEK_SET);
            if (fread(rodata, 1, rodata_size, f) != rodata_size) {
                fprintf(stderr, "Error: Cannot read .rodata\n");
                free(rodata);
                fclose(f);
                return 1;
            }
            
            int pixels = wm.fw * wm.fh;
            uint16_t *frame = malloc((size_t)pixels * sizeof(uint16_t));
            uint16_t *prev_frame = malloc((size_t)pixels * sizeof(uint16_t));
            
            /* Static modes (3,4): single palette+LZSS image */
            if (wm.mode == 3 || wm.mode == 4) {
                if (!extract_static_image(rodata, rodata_size, wm.fw, wm.fh, output_dir)) {
                    fprintf(stderr, "Warning: Could not extract static image\n");
                }
            } else {
                /* Animation modes */
                size_t pos = 0;
                
                /* For modes 1 and 2, extract background first */
                if (wm.mode == 1 || wm.mode == 2) {
                    if (!extract_background(rodata, rodata_size, &pos, wm.fw, wm.fh, output_dir)) {
                        char static_dir[512];
                        snprintf(static_dir, sizeof(static_dir), "%s/static", output_dir);
                        mkdir(static_dir, 0755);
                        printf("Warning: Could not extract background image\n");
                        pos = 0;
                    }
                }
                
                /* Extract animation frames */
                int extracted = 0;
                
                for (int i = 0; i < wm.nf && pos < rodata_size; i++) {
                    memset(frame, 0, (size_t)pixels * sizeof(uint16_t));
                    
                    if (i == 0) {
                        /* First frame */
                        if (wm.comp == 0) {
                            /* Raw RGB565 */
                            if (pos + (size_t)pixels * 2 <= rodata_size) {
                                for (int p = 0; p < pixels; p++) {
                                    frame[p] = rodata[pos + (size_t)p * 2] | (rodata[pos + (size_t)p * 2 + 1] << 8);
                                }
                                pos += (size_t)pixels * 2;
                                extracted++;
                            }
                        } else if (wm.comp == 2) {
                            /* RLE Direct */
                            size_t consumed = decode_rle_direct(rodata + pos, rodata_size - pos, frame, pixels);
                            if (consumed > 0) {
                                pos += consumed;
                                extracted++;
                            }
                        } else {
                            /* Try raw as fallback */
                            if (pos + (size_t)pixels * 2 <= rodata_size) {
                                for (int p = 0; p < pixels; p++) {
                                    frame[p] = rodata[pos + (size_t)p * 2] | (rodata[pos + (size_t)p * 2 + 1] << 8);
                                }
                                pos += (size_t)pixels * 2;
                                extracted++;
                            }
                        }
                    } else {
                        /* Delta frames */
                        memcpy(frame, prev_frame, (size_t)pixels * sizeof(uint16_t));
                        
                        if (wm.comp == 1 || wm.comp == 4) {
                            /* RLE XOR */
                            size_t consumed = apply_delta_rle_xor(rodata + pos, rodata_size - pos, frame, pixels);
                            pos += consumed > 0 ? consumed : (size_t)pixels / 4;
                            extracted++;
                        } else if (wm.comp == 3) {
                            /* Sparse */
                            size_t consumed = apply_delta_sparse(rodata + pos, rodata_size - pos, frame, pixels);
                            pos += consumed > 0 ? consumed : (size_t)pixels / 8;
                            extracted++;
                        }
                    }
                    
                    /* Save frame */
                    if (extracted > i) {
                        char png_path[512];
                        snprintf(png_path, sizeof(png_path), "%s/frame_%d.png", output_dir, i);
                        if (save_png(png_path, frame, wm.fw, wm.fh) == 0) {
                            printf("Written: %s\n", png_path);
                        }
                    }
                    
                    memcpy(prev_frame, frame, (size_t)pixels * sizeof(uint16_t));
                }
                
                printf("\nExtracted %d frames\n", extracted);
            }
            
            free(frame);
            free(prev_frame);
            free(rodata);
        }
        
        printf("\nExtraction complete: %s/\n", output_dir);
    }
    
    fclose(f);
    return 0;
}
