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
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <png.h>
#include <fcntl.h>

/* CRC32 table for polynomial 0xEDB88320 */
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBBBD6, 0xACBCCB40, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

static uint32_t calc_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/* Configuration */
static int display_mode = 0;
static int offset_x = 0;
static int offset_y = 0;
static int bg_offset_x = 0;
static int bg_offset_y = 0;
static int frame_delay_ms = 33;
static int loop_mode = 1;      /* 0=no loop, 1=full loop, 2=partial loop */
static int loop_start = 0;     /* Start frame for partial loop */
static int invert_frames = 0;  /* 0=normal order (0->N), 1=inverted order (N->0) */
static uint32_t bg_color = 0x000000;  /* RRGGBB */
static char *bg_image_path = NULL;

/* Secure temporary directory - created with mkdtemp to prevent symlink attacks */
static char secure_tmpdir[256] = "";
static int tmpdir_created = 0;

/* Create secure temp directory on first use */
static const char *get_secure_tmpdir(void) {
    if (!tmpdir_created) {
        snprintf(secure_tmpdir, sizeof(secure_tmpdir), "/tmp/xbs_XXXXXX");
        if (mkdtemp(secure_tmpdir) == NULL) {
            fprintf(stderr, "Error: Failed to create secure temp directory\n");
            exit(1);
        }
        tmpdir_created = 1;
    }
    return secure_tmpdir;
}

/* Clean up temp directory at exit */
static void cleanup_tmpdir(void) {
    if (tmpdir_created && secure_tmpdir[0]) {
        /* Remove all files in directory */
        DIR *dir = opendir(secure_tmpdir);
        if (dir) {
            struct dirent *ent;
            while ((ent = readdir(dir)) != NULL) {
                if (ent->d_name[0] == '.') continue;
                char filepath[512];
                snprintf(filepath, sizeof(filepath), "%s/%s", secure_tmpdir, ent->d_name);
                unlink(filepath);
            }
            closedir(dir);
        }
        rmdir(secure_tmpdir);
        tmpdir_created = 0;
    }
}
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

#define MAX_FRAMES 1000
#define MAX_IMAGE_WIDTH  8192
#define MAX_IMAGE_HEIGHT 8192
#define MAX_IMAGE_PIXELS ((size_t)MAX_IMAGE_WIDTH * (size_t)MAX_IMAGE_HEIGHT)

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

/* Compare frames by filename (path) - used before index extraction */
static int compare_frames_by_path(const void *a, const void *b) {
    const char *path_a = ((frame_entry_t*)a)->path;
    const char *path_b = ((frame_entry_t*)b)->path;
    if (!path_a || !path_b) return 0;
    /* Compare just the filename part, not the full path */
    const char *name_a = strrchr(path_a, '/');
    const char *name_b = strrchr(path_b, '/');
    name_a = name_a ? name_a + 1 : path_a;
    name_b = name_b ? name_b + 1 : path_b;
    return strcmp(name_a, name_b);
}

/* Absolute paths for external tools (prevents PATH hijacking) */
#define CMD_CONVERT "/usr/bin/convert"
#define CMD_CP      "/bin/cp"
#define CMD_RM      "/bin/rm"

/* Execute command without shell interpretation and with clean environment */
static int exec_cmd_safe(const char *cmd_path, char *const argv[]) {
    if (access(cmd_path, X_OK) != 0) return -1;

    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        char *const clean_env[] = {
            (char *)"PATH=/usr/bin:/bin",
            (char *)"HOME=/root",
            (char *)"LANG=C",
            NULL
        };
        execve(cmd_path, argv, clean_env);
        _exit(127);
    }

    int status = 0;
    for (;;) {
        pid_t w = waitpid(pid, &status, 0);
        if (w == pid) break;
        if (w < 0 && errno == EINTR) continue;
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
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
/* Returns pointer to tmp_path buffer provided by caller (must be at least 512 bytes) */
static char *flatten_png(const char *path, uint32_t bg_hex, char *tmp_path, size_t tmp_path_size) {
    static int counter = 0;
    
    /* Create unique temp file path in secure directory */
    const char *tmpdir = get_secure_tmpdir();
    snprintf(tmp_path, tmp_path_size, "%s/flatten_%d.png", tmpdir, counter++);
    
    /* Build convert command: flatten onto background color */
    char bg_color_str[16];
    snprintf(bg_color_str, sizeof(bg_color_str), "#%06X", bg_hex);
    
    /* Use exec_cmd to avoid shell injection */
    char *argv[] = {
        (char *)"convert",
        (char *)path,
        (char *)"-background",
        bg_color_str,
        (char *)"-flatten",
        tmp_path,
        NULL
    };
    
    if (exec_cmd_safe(CMD_CONVERT, argv) != 0) {
        /* Fallback: just copy without flattening */
        /* WARNING: This will cause visual distortion if PNG has alpha channel!
         * The 32bpp RGBA will be interpreted as 24bpp RGB, shifting all pixels.
         * User should ensure ImageMagick is properly installed for transparent PNGs.
         */
        fprintf(stderr, "Warning: ImageMagick convert failed for '%s'\n", path);
        fprintf(stderr, "         Transparent PNG will not be flattened - visual distortion may occur!\n");
        char *cp_argv[] = {(char *)"cp", (char *)path, tmp_path, NULL};
        exec_cmd_safe(CMD_CP, cp_argv);
    }
    
    return tmp_path;
}

/* Load PNG into RGB565 buffer */
static int load_png(const char *path, image_t *img) {
    const char *load_path = path;
    char flattened_buf[512];
    char *flattened_path = NULL;
    png_bytep *rows = NULL;
    int w = 0;
    int h = 0;
    
    /* Check for transparency and flatten if needed */
    if (png_has_alpha(path)) {
        if (!transp_warned) {
            fprintf(stderr, "Warning: Transparent PNG detected in '%s'\n", path);
            fprintf(stderr, "         Flattening onto background color #%06X\n", bg_color);
            transp_warned = 1;
        }
        flattened_path = flatten_png(path, bg_color, flattened_buf, sizeof(flattened_buf));
        load_path = flattened_path;
    }
    
    FILE *fp = fopen(load_path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open PNG: %s (%s)\n", load_path, strerror(errno));
        if (flattened_path) unlink(flattened_path);
        return -1;
    }
    
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fprintf(stderr, "Error: libpng init failed for: %s\n", load_path);
        fclose(fp);
        if (flattened_path) unlink(flattened_path);
        return -1;
    }
    png_infop info = png_create_info_struct(png);
    if (!info) {
        fprintf(stderr, "Error: libpng info init failed for: %s\n", load_path);
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        if (flattened_path) unlink(flattened_path);
        return -1;
    }
    
    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "Error: libpng read failed for: %s\n", load_path);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        if (flattened_path) unlink(flattened_path);
        return -1;
    }
    
    png_init_io(png, fp);
    png_read_info(png, info);

    {
        const png_uint_32 w32 = png_get_image_width(png, info);
        const png_uint_32 h32 = png_get_image_height(png, info);
        if (w32 == 0 || h32 == 0 || w32 > MAX_IMAGE_WIDTH || h32 > MAX_IMAGE_HEIGHT) {
            fprintf(stderr, "Error: Image too large: %ux%u (max %dx%d)\n",
                    (unsigned)w32, (unsigned)h32, MAX_IMAGE_WIDTH, MAX_IMAGE_HEIGHT);
            png_destroy_read_struct(&png, &info, NULL);
            fclose(fp);
            if (flattened_path) unlink(flattened_path);
            return -1;
        }
        if ((size_t)w32 * (size_t)h32 > MAX_IMAGE_PIXELS) {
            fprintf(stderr, "Error: Image has too many pixels: %zu (max %zu)\n",
                    (size_t)w32 * (size_t)h32, (size_t)MAX_IMAGE_PIXELS);
            png_destroy_read_struct(&png, &info, NULL);
            fclose(fp);
            if (flattened_path) unlink(flattened_path);
            return -1;
        }
        w = (int)w32;
        h = (int)h32;
    }
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
    
    rows = malloc(sizeof(png_bytep) * (size_t)h);
    if (!rows) {
        fprintf(stderr, "Error: Out of memory for PNG rows\n");
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        if (flattened_path) unlink(flattened_path);
        return -1;
    }
    for (int y = 0; y < h; y++) rows[y] = NULL;
    const size_t rowbytes = (size_t)png_get_rowbytes(png, info);
    for (int y = 0; y < h; y++) {
        rows[y] = malloc(rowbytes);
        if (!rows[y]) {
            fprintf(stderr, "Error: Out of memory for PNG row buffers\n");
            for (int yy = 0; yy < y; yy++) free(rows[yy]);
            free(rows);
            png_destroy_read_struct(&png, &info, NULL);
            fclose(fp);
            if (flattened_path) unlink(flattened_path);
            return -1;
        }
    }
    png_read_image(png, rows);
    
    img->w = w;
    img->h = h;
    img->pixels = malloc((size_t)w * (size_t)h * sizeof(uint16_t));
    if (!img->pixels) {
        fprintf(stderr, "Error: Out of memory for image %dx%d\n", w, h);
        for (int y = 0; y < h; y++) free(rows[y]);
        free(rows);
        fclose(fp);
        png_destroy_read_struct(&png, &info, NULL);
        if (flattened_path) unlink(flattened_path);
        return -1;
    }
    
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
    
    if (flattened_path) unlink(flattened_path);
    
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
/* Worst case: count * 3 + 1 bytes (each pixel unique = 1 + 2 bytes, plus terminator) */
static size_t compress_rle_direct(const uint16_t *pixels, int count, uint8_t *out, size_t out_size) {
    size_t pos = 0;
    int i = 0;
    const size_t max_pos = out_size > 0 ? out_size - 1 : 0;  /* Reserve space for terminator */
    
    while (i < count) {
        uint16_t val = pixels[i];
        int run = 1;
        
        while (i + run < count && run < 127) {
            if (pixels[i + run] != val) break;
            run++;
        }
        
        if (run >= 3) {
            /* RLE: 3 bytes */
            if (pos + 3 > max_pos) goto overflow;
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
            
            /* Literal: 1 + lit*2 bytes */
            if (pos + 1 + lit * 2 > max_pos) goto overflow;
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
    
overflow:
    fprintf(stderr, "Error: RLE direct buffer overflow at pixel %d\n", i);
    return 0;  /* Signal error */
}

/* RLE XOR compression */
/* Worst case: count * 3 + 1 bytes (all nonzeros = 1 + 2 bytes each, plus terminator) */
static size_t compress_rle_xor(const uint16_t *curr, const uint16_t *prev, int count, uint8_t *out, size_t out_size) {
    size_t pos = 0;
    int i = 0;
    const size_t max_pos = out_size > 0 ? out_size - 1 : 0;
    
    while (i < count) {
        int zeros = 0;
        while (i + zeros < count && zeros < 128) {
            if ((curr[i + zeros] ^ prev[i + zeros]) != 0) break;
            zeros++;
        }
        
        if (zeros > 0) {
            if (pos + 1 > max_pos) goto overflow;
            out[pos++] = 0x80 | (zeros - 1);
            i += zeros;
        }
        
        int nonzeros = 0;
        while (i + nonzeros < count && nonzeros < 127) {
            if ((curr[i + nonzeros] ^ prev[i + nonzeros]) == 0) break;
            nonzeros++;
        }
        
        if (nonzeros > 0) {
            if (pos + 1 + nonzeros * 2 > max_pos) goto overflow;
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
    
overflow:
    fprintf(stderr, "Error: RLE XOR buffer overflow at pixel %d\n", i);
    return 0;
}

/* Sparse XOR compression (position + value for changed pixels) */
/* Format: 4-byte header (changed count, 32-bit) + N * (4-byte position + 2-byte XOR) */
/* Supports frames up to 4 billion pixels with minimal RAM overhead during boot */
static size_t compress_sparse_xor(const uint16_t *curr, const uint16_t *prev, int count, uint8_t *out, size_t out_size) {
    /* Validate frame size for 32-bit pixel indices (practical limit: 2^31) */
    if (count > 2147483647) {
        fprintf(stderr, "Warning: Sparse XOR not suitable for frames > 2^31 pixels (frame has %d)\n", count);
        return 0;
    }
    
    size_t pos = 0;
    const size_t max_pos = out_size;
    
    /* Count changed pixels first */
    int changed = 0;
    for (int i = 0; i < count; i++) {
        if ((curr[i] ^ prev[i]) != 0) changed++;
    }
    
    /* Header: number of changed pixels (32-bit) */
    if (pos + 4 > max_pos) goto overflow;
    out[pos++] = changed & 0xFF;
    out[pos++] = (changed >> 8) & 0xFF;
    out[pos++] = (changed >> 16) & 0xFF;
    out[pos++] = (changed >> 24) & 0xFF;
    
    /* For each changed pixel: position (32-bit) + XOR value (16-bit) */
    for (int i = 0; i < count; i++) {
        uint16_t delta = curr[i] ^ prev[i];
        if (delta != 0) {
            if (pos + 6 > max_pos) goto overflow;
            /* Position: 32-bit little-endian */
            out[pos++] = i & 0xFF;
            out[pos++] = (i >> 8) & 0xFF;
            out[pos++] = (i >> 16) & 0xFF;
            out[pos++] = (i >> 24) & 0xFF;
            /* XOR value: 16-bit little-endian */
            out[pos++] = delta & 0xFF;
            out[pos++] = (delta >> 8) & 0xFF;
        }
    }
    
    return pos;
    
overflow:
    fprintf(stderr, "Error: Sparse XOR buffer overflow\n");
    return 0;
}

/* Raw XOR compression (no compression, just XOR values) */
/* Output size: count * 2 bytes */
static size_t compress_raw_xor(const uint16_t *curr, const uint16_t *prev, int count, uint8_t *out, size_t out_size) {
    size_t required = (size_t)count * 2;
    if (out_size < required) {
        fprintf(stderr, "Error: Raw XOR buffer overflow (need %zu, have %zu)\n", required, out_size);
        return 0;
    }
    for (int i = 0; i < count; i++) {
        uint16_t delta = curr[i] ^ prev[i];
        out[i*2] = delta & 0xFF;
        out[i*2+1] = (delta >> 8) & 0xFF;
    }
    return required;
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
    int overflow = 0;
    
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
            /* Palette full: refuse silently producing wrong output. */
            overflow = 1;
            indices[i] = 255;
        }
    }

    if (overflow) {
        return -1;
    }
    
    return num_colors;
}

/* LZSS compression parameters */
#define LZSS_WINDOW_SIZE 4096
#define LZSS_MIN_MATCH  3
#define LZSS_MAX_MATCH  18

/* LZSS compress byte array (indices) */
static size_t compress_lzss(const uint8_t *data, int count, uint8_t *out, size_t out_size) {
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
            /* Bounds check: flag byte + items */
            if (out_pos + 1 + item_count > out_size) {
                fprintf(stderr, "LZSS overflow: output buffer too small\n");
                return 0;  /* Return 0 to indicate failure */
            }
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
        /* Bounds check */
        if (out_pos + 1 + item_count > out_size) {
            fprintf(stderr, "LZSS overflow: output buffer too small\n");
            return 0;
        }
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
    fprintf(stderr, "  -X <offset>    Background horizontal offset for mode 1 (default: 0)\n");
    fprintf(stderr, "  -Y <offset>    Background vertical offset for mode 1 (default: 0)\n");
    fprintf(stderr, "  -d <delay>     Frame delay in ms (default: 33)\n");
    fprintf(stderr, "  -l <mode>      Loop mode: 0=no loop, 1=full loop (default), 2=partial loop\n");
    fprintf(stderr, "  -L <frame>     Loop start frame for partial loop mode (default: 0)\n");
    fprintf(stderr, "  -I             Invert frame order (play N->0 instead of 0->N)\n");
    fprintf(stderr, "  -c <color>     Background color RRGGBB hex (default: 000000)\n");
    fprintf(stderr, "  -b <image>     Background image for modes 1,2\n");
    fprintf(stderr, "  -r <W>x<H>     Target resolution for fullscreen modes\n");
    fprintf(stderr, "  -z <method>    Compression: rle_xor, rle_direct, sparse, raw, auto\n");
    fprintf(stderr, "  -h             Show help\n");
}

int main(int argc, char *argv[]) {
    char *input_path = NULL;
    
    /* Register cleanup handler for secure temp directory */
    atexit(cleanup_tmpdir);
    
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
        } else if (strcmp(argv[arg_idx], "-X") == 0 && arg_idx + 1 < argc) {
            bg_offset_x = atoi(argv[++arg_idx]);
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-Y") == 0 && arg_idx + 1 < argc) {
            bg_offset_y = atoi(argv[++arg_idx]);
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-d") == 0 && arg_idx + 1 < argc) {
            frame_delay_ms = atoi(argv[++arg_idx]);
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-l") == 0 && arg_idx + 1 < argc) {
            loop_mode = atoi(argv[++arg_idx]);
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-L") == 0 && arg_idx + 1 < argc) {
            loop_start = atoi(argv[++arg_idx]);
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-I") == 0) {
            invert_frames = 1;
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
    
    /* Validate display_mode range */
    if (display_mode < 0 || display_mode > 4) {
        fprintf(stderr, "Error: Invalid display mode %d (must be 0-4)\n", display_mode);
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
    printf("#define BG_OFFSET_X %d\n", bg_offset_x);
    printf("#define BG_OFFSET_Y %d\n", bg_offset_y);
    printf("#define BACKGROUND_COLOR 0x%04X\n", 
           rgb_to_rgb565((bg_color >> 16) & 0xFF, (bg_color >> 8) & 0xFF, bg_color & 0xFF));
    
    if (display_mode == MODE_ANIM_SOLID || display_mode == MODE_ANIM_IMAGE_CENTER || display_mode == MODE_ANIM_IMAGE_FULL) {
        printf("#define FRAME_DURATION_MS %d\n", frame_delay_ms);
        printf("#define LOOP_MODE %d  /* 0=no loop, 1=full loop, 2=partial loop */\n", loop_mode);
        if (loop_mode == 2) {
            printf("#define LOOP_START %d  /* Loop restart frame for partial loop */\n", loop_start);
        }
    }
    
    /* Handle different modes */
    if (display_mode == MODE_STATIC_CENTER || display_mode == MODE_STATIC_FULLSCREEN) {
        /* Single static image - convert to standard format first */
        char tmp_path[512];
        char cmd[1024];
        char bg_color_str[16];
        
        snprintf(bg_color_str, sizeof(bg_color_str), "#%06X", bg_color);
        const char *tmpdir = get_secure_tmpdir();
        snprintf(tmp_path, sizeof(tmp_path), "%s/splash_static.png", tmpdir);
        
        /* Convert to RGB, flatten alpha - handles colormap and transparent PNGs */
        char png24_arg[512];
        snprintf(png24_arg, sizeof(png24_arg), "PNG24:%s", tmp_path);
        
        char *argv[] = {
            (char *)"convert",
            (char *)input_path,
            (char *)"-background",
            bg_color_str,
            (char *)"-flatten",
            (char *)"-type", (char *)"TrueColor",
            (char *)"-depth", (char *)"8",
            png24_arg,
            NULL
        };
        
        if (exec_cmd_safe(CMD_CONVERT, argv) != 0) {
            /* Fallback: try without flatten */
            char *cp_argv[] = {(char *)"cp", (char *)input_path, tmp_path, NULL};
            exec_cmd_safe(CMD_CP, cp_argv);
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
        if (num_colors < 0) {
            fprintf(stderr, "Error: Image has more than 256 unique colors. Please quantize it to 256 colors (or less).\n");
            free(palette);
            free(indices);
            free(compressed);
            free(img.pixels);
            return 1;
        }
        fprintf(stderr, "Palette: %d unique colors\n", num_colors);
        
        /* Compress indices with LZSS */
        size_t comp_size = compress_lzss(indices, pixel_count, compressed, pixel_count * 2);
        if (comp_size == 0) {
            fprintf(stderr, "Error: LZSS compression failed\n");
            free(palette);
            free(indices);
            free(compressed);
            free(img.pixels);
            return 1;
        }
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
        /* First pass: count frames */
        int frame_count = 0;
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (strstr(ent->d_name, ".png") || strstr(ent->d_name, ".PNG") ||
                strstr(ent->d_name, ".jpg") || strstr(ent->d_name, ".JPG") ||
                strstr(ent->d_name, ".jpeg") || strstr(ent->d_name, ".JPEG")) {
                frame_count++;
            }
        }
        rewinddir(dir);
        
        if (frame_count == 0) {
            fprintf(stderr, "Error: No frames found\n");
            closedir(dir);
            return 1;
        }
        
        if (frame_count > MAX_FRAMES) {
            fprintf(stderr, "Error: Too many frames (%d). Maximum is %d.\n",
                    frame_count, MAX_FRAMES);
            fprintf(stderr, "For a 10-second animation at 30 FPS, you need 300 frames.\n");
            fprintf(stderr, "Consider reducing frame count or increasing frame delay.\n");
            closedir(dir);
            return 1;
        }
        
        fprintf(stderr, "Found %d frames\n", frame_count);
        
        /* Allocate arrays */
        frame_entry_t *frames = malloc(sizeof(frame_entry_t) * frame_count);
        int nframes = 0;
        const char *tmpdir = get_secure_tmpdir();
        
        /* Second pass: collect frame filenames */
        while ((ent = readdir(dir)) != NULL && nframes < frame_count) {
            if (strstr(ent->d_name, ".png") || strstr(ent->d_name, ".PNG") ||
                strstr(ent->d_name, ".jpg") || strstr(ent->d_name, ".JPG") ||
                strstr(ent->d_name, ".jpeg") || strstr(ent->d_name, ".JPEG")) {
                frames[nframes].path = malloc(512);
                frames[nframes].tmp_path = malloc(512);
                snprintf(frames[nframes].path, 512, "%s/%s", input_path, ent->d_name);
                snprintf(frames[nframes].tmp_path, 512, "%s/%s", tmpdir, ent->d_name);
                frames[nframes].index = -1;  /* Will be set after sorting */
                nframes++;
            }
        }
        closedir(dir);
        
        /* Sort by filename first to ensure consistent reference frame */
        qsort(frames, nframes, sizeof(frame_entry_t), compare_frames_by_path);
        
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
        
        /* Reverse frame order if invert_frames is set */
        if (invert_frames && nframes > 1) {
            for (int i = 0; i < nframes / 2; i++) {
                frame_entry_t tmp = frames[i];
                frames[i] = frames[nframes - 1 - i];
                frames[nframes - 1 - i] = tmp;
            }
            fprintf(stderr, "Frame order inverted: playing N->0\n");
        }
        
        /* Convert frames to standard format */
        for (int i = 0; i < nframes; i++) {
            char bg_color_str[16];
            snprintf(bg_color_str, sizeof(bg_color_str), "#%06X", bg_color);
            
            char png24_arg[512];
            snprintf(png24_arg, sizeof(png24_arg), "PNG24:%s", frames[i].tmp_path);
            
            char *argv[] = {
                (char *)"convert",
                frames[i].path,
                (char *)"-background",
                bg_color_str,
                (char *)"-flatten",
                (char *)"-type", (char *)"TrueColor",
                (char *)"-depth", (char *)"8",
                png24_arg,
                NULL
            };
            
            if (exec_cmd_safe(CMD_CONVERT, argv) != 0) {
                char *cp_argv[] = {(char *)"cp", frames[i].path, frames[i].tmp_path, NULL};
                exec_cmd_safe(CMD_CP, cp_argv);
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
        
        /* Validate all frames have consistent size */
        int ref_w = frame_imgs[0].w;
        int ref_h = frame_imgs[0].h;
        for (int i = 1; i < nframes; i++) {
            if (frame_imgs[i].w != ref_w || frame_imgs[i].h != ref_h) {
                fprintf(stderr, "Error: Frame size mismatch!\n");
                fprintf(stderr, "  Frame 0: %dx%d\n", ref_w, ref_h);
                fprintf(stderr, "  Frame %d: %dx%d (file: %s)\n", 
                        i, frame_imgs[i].w, frame_imgs[i].h, frames[i].path);
                fprintf(stderr, "All frames must have the same dimensions.\n");
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

            uint8_t *frame0_buf = malloc((size_t)pixels * 2);
            if (!frame0_buf) {
                fprintf(stderr, "Error: Out of memory for frame0 buffer\n");
                for (int i = 0; i < nframes; i++) {
                    free(frame_imgs[i].pixels);
                    free(frames[i].path);
                    free(frames[i].tmp_path);
                }
                free(frame_imgs);
                free(frames);
                return 1;
            }
            size_t frame0_size = compress_raw_direct(frame_imgs[0].pixels, pixels, frame0_buf);

            uint8_t **best_comp = NULL;
            size_t *best_sizes = NULL;

            for (int m = 0; m < 3; m++) {
                size_t total = frame0_size;
                int method_valid = 1;

                uint8_t **method_comp = calloc((size_t)nframes, sizeof(uint8_t *));
                size_t *method_sizes = calloc((size_t)nframes, sizeof(size_t));
                if (!method_comp || !method_sizes) {
                    fprintf(stderr, "  %d/3: method %-12s ...... SKIPPED (out of memory)\n",
                            m + 1, method_names[m]);
                    free(method_comp);
                    free(method_sizes);
                    continue;
                }

                for (int f = 1; f < nframes; f++) {
                    size_t out_cap = 0;
                    if (method_ids[m] == COMPRESS_RLE_XOR || method_ids[m] == COMPRESS_RLE_DIRECT) {
                        out_cap = (size_t)pixels * 3 + 1;
                    } else {
                        size_t changed = 0;
                        const uint16_t *c = frame_imgs[f].pixels;
                        const uint16_t *p = frame_imgs[f - 1].pixels;
                        for (int i = 0; i < pixels; i++) changed += (c[i] != p[i]);
                        out_cap = 4 + changed * 6;
                    }

                    method_comp[f] = malloc(out_cap);
                    if (!method_comp[f]) {
                        method_valid = 0;
                        total = SIZE_MAX;
                        break;
                    }

                    size_t size = 0;
                    switch (method_ids[m]) {
                        case COMPRESS_RLE_XOR:
                            size = compress_rle_xor(frame_imgs[f].pixels, frame_imgs[f - 1].pixels,
                                                    pixels, method_comp[f], out_cap);
                            break;
                        case COMPRESS_SPARSE:
                            size = compress_sparse_xor(frame_imgs[f].pixels, frame_imgs[f - 1].pixels,
                                                       pixels, method_comp[f], out_cap);
                            break;
                        case COMPRESS_RLE_DIRECT:
                            size = compress_rle_direct(frame_imgs[f].pixels, pixels, method_comp[f], out_cap);
                            break;
                        default:
                            size = 0;
                            break;
                    }

                    if (size == 0) {
                        method_valid = 0;
                        total = SIZE_MAX;
                        break;
                    }

                    method_sizes[f] = size;
                    total += size;
                }

                if (!method_valid) {
                    fprintf(stderr, "  %d/3: method %-12s ...... SKIPPED (compression failed)\n",
                            m + 1, method_names[m]);
                } else {
                    fprintf(stderr, "  %d/3: method %-12s ...... %zu bytes (%.1f KB)\n",
                            m + 1, method_names[m], total, total / 1024.0);
                }

                if (total < best_size) {
                    if (best_comp) {
                        for (int f = 1; f < nframes; f++) free(best_comp[f]);
                        free(best_comp);
                        free(best_sizes);
                    }
                    best_size = total;
                    best_method = method_ids[m];
                    best_comp = method_comp;
                    best_sizes = method_sizes;
                    method_comp = NULL;
                    method_sizes = NULL;
                }

                if (method_comp) {
                    for (int f = 1; f < nframes; f++) free(method_comp[f]);
                    free(method_comp);
                }
                free(method_sizes);
            }
            
            /* Verify at least one method succeeded */
            if (best_size == SIZE_MAX || !best_comp) {
                fprintf(stderr, "Error: All compression methods failed\n");
                free(frame0_buf);
                for (int i = 0; i < nframes; i++) {
                    free(frame_imgs[i].pixels);
                    free(frames[i].path);
                    free(frames[i].tmp_path);
                }
                free(frame_imgs);
                free(frames);
                return 1;
            }
            
            fprintf(stderr, "\n  ---> Best method: %s (%zu bytes)\n\n", 
                    method_names[best_method == COMPRESS_RLE_XOR ? 0 : 
                                 best_method == COMPRESS_SPARSE ? 1 : 2], best_size);
            
            compress_method = best_method;
            printf("#define COMPRESS_METHOD %d  /* Auto-selected: %s */\n", 
                   compress_method, 
                   method_names[compress_method == COMPRESS_RLE_XOR ? 0 : 
                                compress_method == COMPRESS_SPARSE ? 1 : 2]);

            /* Compress frames with selected method (reuse best buffers from auto-test) */
            uint8_t **compressed = malloc(sizeof(uint8_t*) * nframes);
            size_t *comp_sizes = malloc(sizeof(size_t) * nframes);
            if (!compressed || !comp_sizes) {
                fprintf(stderr, "Error: Out of memory for compressed arrays\n");
                free(frame0_buf);
                for (int f = 1; f < nframes; f++) free(best_comp[f]);
                free(best_comp);
                free(best_sizes);
                for (int i = 0; i < nframes; i++) {
                    free(frame_imgs[i].pixels);
                    free(frames[i].path);
                    free(frames[i].tmp_path);
                }
                free(frame_imgs);
                free(frames);
                free(compressed);
                free(comp_sizes);
                return 1;
            }
            size_t total_size = 0;

            compressed[0] = frame0_buf;
            comp_sizes[0] = frame0_size;
            total_size += comp_sizes[0];
            output_frame_data(0, compressed[0], comp_sizes[0]);
            
            /* CRC of first 1024 bytes of frame_0 (or less if smaller) */
            size_t crc_len = comp_sizes[0] < 1024 ? comp_sizes[0] : 1024;
            uint32_t frame_crc = calc_crc32(compressed[0], crc_len);
            printf("#define FRAME_CRC 0x%08X\n\n", frame_crc);

            for (int f = 1; f < nframes; f++) {
                compressed[f] = best_comp ? best_comp[f] : NULL;
                comp_sizes[f] = best_sizes ? best_sizes[f] : 0;
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

            for (int i = 0; i < nframes; i++) {
                free(frame_imgs[i].pixels);
                free(compressed[i]);
                free(frames[i].path);
                free(frames[i].tmp_path);
            }
            free(frame_imgs);
            free(frames);
            free(compressed);
            free(comp_sizes);

            free(best_comp);
            free(best_sizes);

            /* Cleanup temp directory */
            char *rm_argv[] = {(char *)"rm", (char *)"-rf", (char *)tmpdir, NULL};
            exec_cmd_safe(CMD_RM, rm_argv);
            return 0;
        } else {
            printf("#define COMPRESS_METHOD %d  /* 0=RLE_XOR, 1=RLE_DIRECT, 2=SPARSE, 3=RAW */\n", compress_method);
        }
        
        /* Compress frames with selected method */
        uint8_t **compressed = malloc(sizeof(uint8_t*) * nframes);
        size_t *comp_sizes = malloc(sizeof(size_t) * nframes);
        size_t total_size = 0;
        
        /* Frame 0: always raw RGB565 (no previous frame for XOR) */
        compressed[0] = malloc(pixels * 3);
        comp_sizes[0] = compress_raw_direct(frame_imgs[0].pixels, pixels, compressed[0]);
        total_size += comp_sizes[0];
        
        output_frame_data(0, compressed[0], comp_sizes[0]);
        
        /* CRC of first 1024 bytes of frame_0 (or less if smaller) */
        size_t crc_len = comp_sizes[0] < 1024 ? comp_sizes[0] : 1024;
        uint32_t frame_crc = calc_crc32(compressed[0], crc_len);
        printf("#define FRAME_CRC 0x%08X\n\n", frame_crc);
        
        /* Delta frames */
        for (int f = 1; f < nframes; f++) {
            size_t comp_buf_size;
            if (compress_method == COMPRESS_RLE_XOR || compress_method == COMPRESS_RLE_DIRECT) {
                comp_buf_size = (size_t)pixels * 3 + 1;
            } else if (compress_method == COMPRESS_RAW) {
                comp_buf_size = (size_t)pixels * 2;
            } else if (compress_method == COMPRESS_SPARSE) {
                comp_buf_size = (size_t)pixels * 6 + 4;
            } else {
                comp_buf_size = (size_t)pixels * 3 + 1;
            }
            compressed[f] = malloc(comp_buf_size);
            switch (compress_method) {
                case COMPRESS_RLE_XOR:
                    comp_sizes[f] = compress_rle_xor(frame_imgs[f].pixels,
                                                     frame_imgs[f-1].pixels, pixels, compressed[f], comp_buf_size);
                    break;
                case COMPRESS_RLE_DIRECT:
                    comp_sizes[f] = compress_rle_direct(frame_imgs[f].pixels, pixels, compressed[f], comp_buf_size);
                    break;
                case COMPRESS_SPARSE:
                    comp_sizes[f] = compress_sparse_xor(frame_imgs[f].pixels,
                                                        frame_imgs[f-1].pixels, pixels, compressed[f], comp_buf_size);
                    break;
                case COMPRESS_RAW:
                    /* RAW XOR for delta frames - no compression, just XOR values */
                    comp_sizes[f] = compress_raw_xor(frame_imgs[f].pixels,
                                                        frame_imgs[f-1].pixels, pixels, compressed[f], comp_buf_size);
                    break;
                default:
                    comp_sizes[f] = compress_rle_xor(frame_imgs[f].pixels,
                                                     frame_imgs[f-1].pixels, pixels, compressed[f], comp_buf_size);
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
        free(compressed);
        free(comp_sizes);
        
        /* Cleanup temp directory */
        char *rm_argv[] = {(char *)"rm", (char *)"-rf", (char *)tmpdir, NULL};
        exec_cmd_safe(CMD_RM, rm_argv);
        
    } else if (display_mode == MODE_ANIM_IMAGE_CENTER || display_mode == MODE_ANIM_IMAGE_FULL) {
        /* Animation on background image */
        if (!bg_image_path) {
            fprintf(stderr, "Error: Mode 1/2 requires background image (-b)\n");
            return 1;
        }
        
        /* Convert background to standard PNG first to handle JPEG and other formats */
        char bg_tmp_path[512];
        const char *tmpdir = get_secure_tmpdir();
        snprintf(bg_tmp_path, sizeof(bg_tmp_path), "%s/splash_bg.png", tmpdir);
        
        char png24_arg[512];
        snprintf(png24_arg, sizeof(png24_arg), "PNG24:%s", bg_tmp_path);
        
        char *bg_argv[] = {
            (char *)"convert",
            (char *)bg_image_path,
            (char *)"-type", (char *)"TrueColor",
            (char *)"-depth", (char *)"8",
            png24_arg,
            NULL
        };
        
        if (exec_cmd_safe(CMD_CONVERT, bg_argv) != 0) {
            fprintf(stderr, "Error: Failed to convert background image: %s\n", bg_image_path);
            return 1;
        }

        if (access(bg_tmp_path, R_OK) != 0) {
            fprintf(stderr, "Error: Background conversion produced no output: %s (%s)\n",
                    bg_tmp_path, strerror(errno));
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
        
        frame_entry_t *frames = NULL;
        int nframes = 0;
        struct dirent *ent;
        tmpdir = get_secure_tmpdir();
        
        /* First pass: count frames */
        int frame_count = 0;
        while ((ent = readdir(dir)) != NULL) {
            if (strstr(ent->d_name, ".png") || strstr(ent->d_name, ".PNG") ||
                strstr(ent->d_name, ".jpg") || strstr(ent->d_name, ".JPG") ||
                strstr(ent->d_name, ".jpeg") || strstr(ent->d_name, ".JPEG")) {
                frame_count++;
            }
        }
        rewinddir(dir);
        
        if (frame_count == 0) {
            fprintf(stderr, "Error: No frames found\n");
            closedir(dir);
            return 1;
        }
        
        if (frame_count > MAX_FRAMES) {
            fprintf(stderr, "Error: Too many frames (%d). Maximum is %d.\n",
                    frame_count, MAX_FRAMES);
            fprintf(stderr, "For a 10-second animation at 30 FPS, you need 300 frames.\n");
            fprintf(stderr, "Consider reducing frame count or increasing frame delay.\n");
            closedir(dir);
            return 1;
        }
        
        fprintf(stderr, "Found %d frames\n", frame_count);
        
        /* Allocate arrays */
        frames = malloc(sizeof(frame_entry_t) * frame_count);
        
        /* Second pass: collect frame filenames */
        while ((ent = readdir(dir)) != NULL && nframes < frame_count) {
            if (strstr(ent->d_name, ".png") || strstr(ent->d_name, ".PNG") ||
                strstr(ent->d_name, ".jpg") || strstr(ent->d_name, ".JPG") ||
                strstr(ent->d_name, ".jpeg") || strstr(ent->d_name, ".JPEG")) {
                frames[nframes].path = malloc(512);
                frames[nframes].tmp_path = malloc(512);
                snprintf(frames[nframes].path, 512, "%s/%s", input_path, ent->d_name);
                snprintf(frames[nframes].tmp_path, 512, "%s/%s", tmpdir, ent->d_name);
                frames[nframes].index = -1;  /* Will be set after sorting */
                nframes++;
            }
        }
        closedir(dir);
        
        if (nframes == 0) {
            fprintf(stderr, "Error: No frames found\n");
            return 1;
        }
        
        /* Sort by filename first to ensure consistent reference frame */
        qsort(frames, nframes, sizeof(frame_entry_t), compare_frames_by_path);
        
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
        
        /* Reverse frame order if invert_frames is set */
        if (invert_frames && nframes > 1) {
            for (int i = 0; i < nframes / 2; i++) {
                frame_entry_t tmp = frames[i];
                frames[i] = frames[nframes - 1 - i];
                frames[nframes - 1 - i] = tmp;
            }
            fprintf(stderr, "Frame order inverted: playing N->0\n");
        }
        
        /* Convert frames to standard format */
        for (int i = 0; i < nframes; i++) {
            char bg_color_str[16];
            snprintf(bg_color_str, sizeof(bg_color_str), "#%06X", bg_color);
            
            char png24_arg[512];
            snprintf(png24_arg, sizeof(png24_arg), "PNG24:%s", frames[i].tmp_path);
            
            char *argv[] = {
                (char *)"convert",
                frames[i].path,
                (char *)"-background",
                bg_color_str,
                (char *)"-flatten",
                (char *)"-type", (char *)"TrueColor",
                (char *)"-depth", (char *)"8",
                png24_arg,
                NULL
            };
            
            if (exec_cmd_safe(CMD_CONVERT, argv) != 0) {
                char *cp_argv[] = {(char *)"cp", frames[i].path, frames[i].tmp_path, NULL};
                exec_cmd_safe(CMD_CP, cp_argv);
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
        
        /* Validate all frames have consistent size */
        int ref_w = frame_imgs[0].w;
        int ref_h = frame_imgs[0].h;
        for (int i = 1; i < nframes; i++) {
            if (frame_imgs[i].w != ref_w || frame_imgs[i].h != ref_h) {
                fprintf(stderr, "Error: Frame size mismatch!\n");
                fprintf(stderr, "  Frame 0: %dx%d\n", ref_w, ref_h);
                fprintf(stderr, "  Frame %d: %dx%d (file: %s)\n", 
                        i, frame_imgs[i].w, frame_imgs[i].h, frames[i].path);
                fprintf(stderr, "All frames must have the same dimensions.\n");
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
        if (bg_num_colors < 0) {
            fprintf(stderr, "Error: Background has more than 256 unique colors. Please quantize it to 256 colors (or less).\n");
            free(bg_palette);
            free(bg_indices);
            free(bg_compressed);
            free(bg.pixels);
            return 1;
        }
        fprintf(stderr, "Background palette: %d unique colors\n", bg_num_colors);
        
        size_t bg_comp_size = compress_lzss(bg_indices, bg_pixel_count, bg_compressed, bg_pixel_count * 2);
        if (bg_comp_size == 0) {
            fprintf(stderr, "Error: Background LZSS compression failed\n");
            free(bg_palette);
            free(bg_indices);
            free(bg_compressed);
            free(bg.pixels);
            return 1;
        }
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

            uint8_t *frame0_buf = malloc((size_t)pixels * 2);
            if (!frame0_buf) {
                fprintf(stderr, "Error: Out of memory for frame0 buffer\n");
                for (int i = 0; i < nframes; i++) {
                    free(frame_imgs[i].pixels);
                    free(frames[i].path);
                    free(frames[i].tmp_path);
                }
                free(frame_imgs);
                free(frames);
                return 1;
            }
            size_t frame0_size = compress_raw_direct(frame_imgs[0].pixels, pixels, frame0_buf);

            uint8_t **best_comp = NULL;
            size_t *best_sizes = NULL;

            for (int m = 0; m < 3; m++) {
                size_t total = frame0_size;
                int method_valid = 1;

                uint8_t **method_comp = calloc((size_t)nframes, sizeof(uint8_t *));
                size_t *method_sizes = calloc((size_t)nframes, sizeof(size_t));
                if (!method_comp || !method_sizes) {
                    fprintf(stderr, "  %d/3: method %-12s ...... SKIPPED (out of memory)\n",
                            m + 1, method_names[m]);
                    free(method_comp);
                    free(method_sizes);
                    continue;
                }

                for (int f = 1; f < nframes; f++) {
                    size_t out_cap = 0;
                    if (method_ids[m] == COMPRESS_RLE_XOR || method_ids[m] == COMPRESS_RLE_DIRECT) {
                        out_cap = (size_t)pixels * 3 + 1;
                    } else {
                        size_t changed = 0;
                        const uint16_t *c = frame_imgs[f].pixels;
                        const uint16_t *p = frame_imgs[f - 1].pixels;
                        for (int i = 0; i < pixels; i++) changed += (c[i] != p[i]);
                        out_cap = 4 + changed * 6;
                    }

                    method_comp[f] = malloc(out_cap);
                    if (!method_comp[f]) {
                        method_valid = 0;
                        total = SIZE_MAX;
                        break;
                    }

                    size_t size = 0;
                    switch (method_ids[m]) {
                        case COMPRESS_RLE_XOR:
                            size = compress_rle_xor(frame_imgs[f].pixels, frame_imgs[f - 1].pixels,
                                                    pixels, method_comp[f], out_cap);
                            break;
                        case COMPRESS_SPARSE:
                            size = compress_sparse_xor(frame_imgs[f].pixels, frame_imgs[f - 1].pixels,
                                                       pixels, method_comp[f], out_cap);
                            break;
                        case COMPRESS_RLE_DIRECT:
                            size = compress_rle_direct(frame_imgs[f].pixels, pixels, method_comp[f], out_cap);
                            break;
                        default:
                            size = 0;
                            break;
                    }

                    if (size == 0) {
                        method_valid = 0;
                        total = SIZE_MAX;
                        break;
                    }

                    method_sizes[f] = size;
                    total += size;
                }

                if (!method_valid) {
                    fprintf(stderr, "  %d/3: method %-12s ...... SKIPPED (compression failed)\n",
                            m + 1, method_names[m]);
                } else {
                    fprintf(stderr, "  %d/3: method %-12s ...... %zu bytes (%.1f KB)\n",
                            m + 1, method_names[m], total, total / 1024.0);
                }

                if (total < best_size) {
                    if (best_comp) {
                        for (int f = 1; f < nframes; f++) free(best_comp[f]);
                        free(best_comp);
                        free(best_sizes);
                    }
                    best_size = total;
                    best_method = method_ids[m];
                    best_comp = method_comp;
                    best_sizes = method_sizes;
                    method_comp = NULL;
                    method_sizes = NULL;
                }

                if (method_comp) {
                    for (int f = 1; f < nframes; f++) free(method_comp[f]);
                    free(method_comp);
                }
                free(method_sizes);
            }
            
            /* Verify at least one method succeeded */
            if (best_size == SIZE_MAX || !best_comp) {
                fprintf(stderr, "Error: All compression methods failed\n");
                free(frame0_buf);
                for (int i = 0; i < nframes; i++) {
                    free(frame_imgs[i].pixels);
                    free(frames[i].path);
                    free(frames[i].tmp_path);
                }
                free(frame_imgs);
                free(frames);
                return 1;
            }
            
            fprintf(stderr, "\n  ---> Best method: %s (%zu bytes)\n\n", 
                    method_names[best_method == COMPRESS_RLE_XOR ? 0 : 
                                 best_method == COMPRESS_SPARSE ? 1 : 2], best_size);
            
            compress_method = best_method;
            printf("#define COMPRESS_METHOD %d  /* Auto-selected: %s */\n", 
                   compress_method, 
                   method_names[compress_method == COMPRESS_RLE_XOR ? 0 : 
                                compress_method == COMPRESS_SPARSE ? 1 : 2]);

            uint8_t **compressed = malloc(sizeof(uint8_t*) * nframes);
            size_t *comp_sizes = malloc(sizeof(size_t) * nframes);
            if (!compressed || !comp_sizes) {
                fprintf(stderr, "Error: Out of memory for compressed arrays\n");
                free(frame0_buf);
                for (int f = 1; f < nframes; f++) free(best_comp[f]);
                free(best_comp);
                free(best_sizes);
                for (int i = 0; i < nframes; i++) {
                    free(frame_imgs[i].pixels);
                    free(frames[i].path);
                    free(frames[i].tmp_path);
                }
                free(frame_imgs);
                free(frames);
                free(compressed);
                free(comp_sizes);
                return 1;
            }
            size_t total_size = 0;

            compressed[0] = frame0_buf;
            comp_sizes[0] = frame0_size;
            total_size += comp_sizes[0];
            output_frame_data(0, compressed[0], comp_sizes[0]);
            
            /* CRC of first 1024 bytes of frame_0 (or less if smaller) */
            size_t crc_len = comp_sizes[0] < 1024 ? comp_sizes[0] : 1024;
            uint32_t frame_crc = calc_crc32(compressed[0], crc_len);
            printf("#define FRAME_CRC 0x%08X\n\n", frame_crc);

            for (int f = 1; f < nframes; f++) {
                compressed[f] = best_comp ? best_comp[f] : NULL;
                comp_sizes[f] = best_sizes ? best_sizes[f] : 0;
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

            for (int i = 0; i < nframes; i++) {
                free(frame_imgs[i].pixels);
                free(compressed[i]);
                free(frames[i].path);
                free(frames[i].tmp_path);
            }
            free(frame_imgs);
            free(frames);
            free(compressed);
            free(comp_sizes);

            free(best_comp);
            free(best_sizes);

            char *rm_argv[] = {(char *)"rm", (char *)"-rf", (char *)tmpdir, NULL};
            exec_cmd_safe(CMD_RM, rm_argv);
            return 0;
        } else {
            printf("#define COMPRESS_METHOD %d  /* 0=RLE_XOR, 1=RLE_DIRECT, 2=SPARSE, 3=RAW */\n", compress_method);
        }
        
        /* Compress frames with selected method */
        uint8_t **compressed = malloc(sizeof(uint8_t*) * nframes);
        size_t *comp_sizes = malloc(sizeof(size_t) * nframes);
        size_t total_size = 0;
        
        /* Frame 0: always raw RGB565 (no previous frame for XOR) */
        compressed[0] = malloc(pixels * 3);
        comp_sizes[0] = compress_raw_direct(frame_imgs[0].pixels, pixels, compressed[0]);
        total_size += comp_sizes[0];
        output_frame_data(0, compressed[0], comp_sizes[0]);
        
        /* CRC of first 1024 bytes of frame_0 (or less if smaller) */
        size_t crc_len = comp_sizes[0] < 1024 ? comp_sizes[0] : 1024;
        uint32_t frame_crc = calc_crc32(compressed[0], crc_len);
        printf("#define FRAME_CRC 0x%08X\n\n", frame_crc);
        
        for (int f = 1; f < nframes; f++) {
            size_t comp_buf_size;
            if (compress_method == COMPRESS_RLE_XOR || compress_method == COMPRESS_RLE_DIRECT) {
                comp_buf_size = (size_t)pixels * 3 + 1;
            } else if (compress_method == COMPRESS_RAW) {
                comp_buf_size = (size_t)pixels * 2;
            } else if (compress_method == COMPRESS_SPARSE) {
                comp_buf_size = (size_t)pixels * 6 + 4;
            } else {
                comp_buf_size = (size_t)pixels * 3 + 1;
            }
            compressed[f] = malloc(comp_buf_size);
            switch (compress_method) {
                case COMPRESS_RLE_XOR:
                    comp_sizes[f] = compress_rle_xor(frame_imgs[f].pixels,
                                                     frame_imgs[f-1].pixels, pixels, compressed[f], comp_buf_size);
                    break;
                case COMPRESS_RLE_DIRECT:
                    comp_sizes[f] = compress_rle_direct(frame_imgs[f].pixels, pixels, compressed[f], comp_buf_size);
                    break;
                case COMPRESS_SPARSE:
                    comp_sizes[f] = compress_sparse_xor(frame_imgs[f].pixels,
                                                        frame_imgs[f-1].pixels, pixels, compressed[f], comp_buf_size);
                    break;
                case COMPRESS_RAW:
                    /* RAW XOR for delta frames - no compression, just XOR values */
                    comp_sizes[f] = compress_raw_xor(frame_imgs[f].pixels,
                                                        frame_imgs[f-1].pixels, pixels, compressed[f], comp_buf_size);
                    break;
                default:
                    comp_sizes[f] = compress_rle_xor(frame_imgs[f].pixels,
                                                     frame_imgs[f-1].pixels, pixels, compressed[f], comp_buf_size);
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
        free(compressed);
        free(comp_sizes);
        
        /* Cleanup temp directory */
        char *rm_argv[] = {(char *)"rm", (char *)"-rf", (char *)tmpdir, NULL};
        exec_cmd_safe(CMD_RM, rm_argv);
    }
    
    return 0;
}
