/*
 * debug_fb.c - Test framebuffer access
 * Compile: gcc -o debug_fb debug_fb.c
 * Run: sudo ./debug_fb
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

int main(void) {
    int fb_fd;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    void *fbmem;
    size_t fb_size;
    
    printf("=== Framebuffer Debug ===\n\n");
    
    /* Step 1: Open device */
    printf("[1] Opening /dev/fb0... ");
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        printf("FAILED (errno=%d: %s)\n", errno, strerror(errno));
        return 1;
    }
    printf("OK (fd=%d)\n", fb_fd);
    
    /* Step 2: Get variable info */
    printf("[2] FBIOGET_VSCREENINFO... ");
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        printf("FAILED (errno=%d: %s)\n", errno, strerror(errno));
        close(fb_fd);
        return 1;
    }
    printf("OK\n");
    printf("    Resolution: %ux%u\n", vinfo.xres, vinfo.yres);
    printf("    BPP: %u\n", vinfo.bits_per_pixel);
    printf("    Red: offset=%u length=%u\n", vinfo.red.offset, vinfo.red.length);
    printf("    Green: offset=%u length=%u\n", vinfo.green.offset, vinfo.green.length);
    printf("    Blue: offset=%u length=%u\n", vinfo.blue.offset, vinfo.blue.length);
    
    /* Step 3: Get fixed info */
    printf("[3] FBIOGET_FSCREENINFO... ");
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        printf("FAILED (errno=%d: %s)\n", errno, strerror(errno));
        close(fb_fd);
        return 1;
    }
    printf("OK\n");
    printf("    smem_len: %u bytes\n", finfo.smem_len);
    printf("    line_length: %u bytes\n", finfo.line_length);
    printf("    id: %s\n", finfo.id);
    
    /* Step 4: Mmap */
    printf("[4] mmap... ");
    fb_size = finfo.smem_len;
    fbmem = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fbmem == MAP_FAILED) {
        printf("FAILED (errno=%d: %s)\n", errno, strerror(errno));
        close(fb_fd);
        return 1;
    }
    printf("OK (size=%zu)\n", fb_size);
    
    /* Step 5: Draw test pattern */
    printf("[5] Drawing red rectangle... ");
    int x = 100, y = 100;
    int w = 200, h = 200;
    
    for (int row = y; row < y + h && row < (int)vinfo.yres; row++) {
        for (int col = x; col < x + w && col < (int)vinfo.xres; col++) {
            if (vinfo.bits_per_pixel == 32) {
                uint32_t *pixel = (uint32_t *)(fbmem + row * finfo.line_length + col * 4);
                *pixel = 0x00FF0000; /* Red */
            } else if (vinfo.bits_per_pixel == 16) {
                uint16_t *pixel = (uint16_t *)(fbmem + row * finfo.line_length + col * 2);
                *pixel = 0xF800; /* Red in RGB565 */
            }
        }
    }
    printf("OK\n");
    
    printf("\n=== Test Complete ===\n");
    printf("You should see a RED rectangle at position (100,100)\n");
    
    sleep(3);
    
    /* Clear */
    memset(fbmem, 0, fb_size);
    
    munmap(fbmem, fb_size);
    close(fb_fd);
    
    return 0;
}
