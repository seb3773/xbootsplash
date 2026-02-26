/*
 * nolibc.h - Minimal libc replacement using direct Linux syscalls
 * Target: Linux x86_64
 * 
 * Constraints:
 *   - No external dependencies
 *   - Inline syscalls via int 0x80 or syscall instruction
 *   - Simple arena allocator (no free)
 */

#ifndef NOLIBC_H
#define NOLIBC_H

typedef unsigned long size_t;
typedef long ssize_t;
typedef unsigned int mode_t;

/* Fixed-width integer types */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long int64_t;

#define NULL ((void*)0)
#define MAP_FAILED ((void*)-1)

/* File open flags */
#define O_RDONLY    0
#define O_RDWR      2
#define O_CREAT     0100
#define O_TRUNC     01000

/* Memory protection */
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

/* Memory mapping */
#define MAP_SHARED  0x01
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20

/* Signal */
#define SIGTERM     15
#define SIGINT      2
#define SIGKILL     9

/* Syscall numbers for x86_64 */
#define SYS_read    0
#define SYS_write   1
#define SYS_open    2
#define SYS_close   3
#define SYS_mmap    9
#define SYS_munmap  11
#define SYS_exit    60
#define SYS_nanosleep 35
#define SYS_ioctl   16

/* Inline syscall wrappers */
static inline long syscall1(long n, long a1) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long syscall2(long n, long a1, long a2) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long syscall3(long n, long a1, long a2, long a3) {
    long ret;
    register long r10 __asm__("r10") = a3;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long syscall4(long n, long a1, long a2, long a3, long a4) {
    long ret;
    register long r10 __asm__("r10") = a3;
    register long r8 __asm__("r8") = a4;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a3;
    register long r8 __asm__("r8") = a4;
    register long r9 __asm__("r9") = a5;
    register long r12 __asm__("r12") = a6;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "r"(r10), "r"(r8), "r"(r9), "r"(r12)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/* libc function replacements */
static inline void exit(int status) {
    syscall1(SYS_exit, status);
    __builtin_unreachable();
}

static inline int open(const char *path, int flags, mode_t mode) {
    return (int)syscall3(SYS_open, (long)path, flags, mode);
}

static inline int close(int fd) {
    return (int)syscall1(SYS_close, fd);
}

static inline ssize_t read(int fd, void *buf, size_t count) {
    return syscall3(SYS_read, fd, (long)buf, count);
}

static inline ssize_t write(int fd, const void *buf, size_t count) {
    return syscall3(SYS_write, fd, (long)buf, count);
}

static inline void *mmap(void *addr, size_t length, int prot, int flags, int fd, long offset) {
    return (void*)syscall6(SYS_mmap, (long)addr, length, prot, flags, fd, offset);
}

static inline int munmap(void *addr, size_t length) {
    return (int)syscall2(SYS_munmap, (long)addr, length);
}

static inline int ioctl(int fd, unsigned long request, void *arg) {
    return (int)syscall3(SYS_ioctl, fd, request, (long)arg);
}

/* memset - optimized for size */
static inline void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

/* memcpy - optimized for size */
static inline void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

/* Simple arena allocator - no free, just bump pointer */
/* Pre-allocate 2MB arena - enough for all decoded frames + working memory */
#define ARENA_SIZE (2 * 1024 * 1024)

static char arena[ARENA_SIZE] __attribute__((aligned(16)));
static size_t arena_offset = 0;

static inline void *malloc(size_t size) {
    /* Align to 16 bytes */
    size = (size + 15) & ~15;
    
    if (arena_offset + size > ARENA_SIZE) {
        return NULL;
    }
    
    void *ptr = arena + arena_offset;
    arena_offset += size;
    return ptr;
}

static inline void free(void *ptr) {
    /* No-op for arena allocator */
    (void)ptr;
}

static inline void *realloc(void *ptr, size_t size) {
    /* Simple: allocate new, don't free old */
    (void)ptr;
    return malloc(size);
}

/* Nanosleep */
struct timespec {
    long tv_sec;
    long tv_nsec;
};

static inline int nanosleep(const struct timespec *req, struct timespec *rem) {
    return (int)syscall2(SYS_nanosleep, (long)req, (long)rem);
}

/* Signal handling - minimal */
typedef void (*sighandler_t)(int);
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)

/* We'll use a simpler approach: just check a volatile flag */
static inline sighandler_t signal(int sig, sighandler_t handler) {
    /* Minimal: we'll handle SIGTERM/SIGINT via checking */
    (void)sig;
    (void)handler;
    return SIG_DFL;
}

/* Framebuffer structures */
struct fb_var_screeninfo {
    unsigned int xres;
    unsigned int yres;
    unsigned int xres_virtual;
    unsigned int yres_virtual;
    unsigned int xoffset;
    unsigned int yoffset;
    unsigned int bits_per_pixel;
    unsigned int grayscale;
    unsigned int red_offset;
    unsigned int red_length;
    unsigned int green_offset;
    unsigned int green_length;
    unsigned int blue_offset;
    unsigned int blue_length;
    unsigned int transp_offset;
    unsigned int transp_length;
    unsigned int nonstd;
    unsigned int activate;
    unsigned int height;
    unsigned int width;
    unsigned int accel_flags;
    unsigned int pixclock;
    unsigned int left_margin;
    unsigned int right_margin;
    unsigned int upper_margin;
    unsigned int lower_margin;
    unsigned int hsync_len;
    unsigned int vsync_len;
    unsigned int sync;
    unsigned int vmode;
    unsigned int rotate;
    unsigned int colorspace;
    unsigned int reserved[4];
};

struct fb_fix_screeninfo {
    char id[16];
    unsigned long smem_start;
    unsigned int smem_len;
    unsigned int type;
    unsigned int type_aux;
    unsigned int visual;
    unsigned int xpanstep;
    unsigned int ypanstep;
    unsigned int ywrapstep;
    unsigned int line_length;
    unsigned int mmio_start;
    unsigned int mmio_len;
    unsigned int accel;
    unsigned short capabilities;
    unsigned short reserved[2];
};

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOGET_FSCREENINFO 0x4601

#endif /* NOLIBC_H */
