/*
 * nolibc.h - Minimal libc replacement using direct Linux syscalls
 * by seb3773 - https://github.com/seb3773
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
#define O_APPEND    02000
#define O_WRONLY    1

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
#define SIGUSR1     10
#define SIGUSR2     12

/* Signal actions for rt_sigaction */
#define SA_SIGINFO  0x00000004
#define SA_RESTART  0x10000000

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
#define SYS_rt_sigaction 13
#define SYS_rt_sigprocmask 14
#define SYS_clock_gettime 228

/* Clock IDs for clock_gettime */
#define CLOCK_MONOTONIC_RAW 4

/* Inline syscall wrappers */
static inline __attribute__((always_inline)) long syscall1(long n, long a1) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline __attribute__((always_inline)) long syscall2(long n, long a1, long a2) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline __attribute__((always_inline)) long syscall3(long n, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline __attribute__((always_inline)) long syscall4(long n, long a1, long a2, long a3, long a4) {
    long ret;
    register long r10 __asm__("r10") = a4;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline __attribute__((always_inline)) long syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    register long r9 __asm__("r9") = a6;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/* libc function replacements */
static inline __attribute__((always_inline)) void exit(int status) {
    syscall1(SYS_exit, status);
    __builtin_unreachable();
}

static inline __attribute__((always_inline)) int open(const char *path, int flags, mode_t mode) {
    return (int)syscall3(SYS_open, (long)path, flags, mode);
}

static inline __attribute__((always_inline)) int close(int fd) {
    return (int)syscall1(SYS_close, fd);
}

static inline __attribute__((always_inline)) ssize_t read(int fd, void *buf, size_t count) {
    return syscall3(SYS_read, fd, (long)buf, count);
}

static inline __attribute__((always_inline)) ssize_t write(int fd, const void *buf, size_t count) {
    return syscall3(SYS_write, fd, (long)buf, count);
}

static inline __attribute__((always_inline)) void *mmap(void *addr, size_t length, int prot, int flags, int fd, long offset) {
    return (void*)syscall6(SYS_mmap, (long)addr, length, prot, flags, fd, offset);
}

static inline __attribute__((always_inline)) int munmap(void *addr, size_t length) {
    return (int)syscall2(SYS_munmap, (long)addr, length);
}

static inline __attribute__((always_inline)) int ioctl(int fd, unsigned long request, void *arg) {
    return (int)syscall3(SYS_ioctl, fd, request, (long)arg);
}

/* memset - x86_64 rep stosb for minimal binary size */
static inline __attribute__((always_inline)) void *memset(void *s, int c, size_t n) {
    void *ret = s;
    __asm__ __volatile__ (
        "rep stosb"
        : "=D" (s), "=c" (n)
        : "0" (s), "a" ((unsigned char)c), "1" (n)
        : "memory"
    );
    return ret;
}

/* memcpy - x86_64 rep movsb for minimal binary size */
static inline void *memcpy(void *dest, const void *src, size_t n) {
    void *ret = dest;
    __asm__ __volatile__ (
        "rep movsb"
        : "=D" (dest), "=S" (src), "=c" (n)
        : "0" (dest), "1" (src), "2" (n)
        : "memory"
    );
    return ret;
}

/* Simple arena allocator - no free, just bump pointer */
/* Only include if needed - set NOLIBC_NO_ARENA to disable for runtime binary */
#ifndef NOLIBC_NO_ARENA
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
#endif /* NOLIBC_NO_ARENA */

/* Nanosleep */
#ifndef _STRUCT_TIMESPEC
#define _STRUCT_TIMESPEC
struct timespec {
    long tv_sec;
    long tv_nsec;
};
#endif

static inline __attribute__((always_inline)) int nanosleep(const struct timespec *req, struct timespec *rem) {
    return (int)syscall2(SYS_nanosleep, (long)req, (long)rem);
}

/* Signal handling - proper rt_sigaction implementation */
typedef void (*sighandler_t)(int);
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

/* siginfo_t for SA_SIGINFO */
typedef struct {
    int  si_signo;
    int  si_errno;
    int  si_code;
    int  si_trapno;
    union {
        int      si_pid;
        void    *si_addr;
    } _fields;
} siginfo_t;

/* sigaction structure for rt_sigaction */
struct sigaction {
    union {
        void (*sa_handler)(int);
        void (*sa_sigaction)(int, siginfo_t *, void *);
    } __sa_handler;
    unsigned long sa_flags;
    void (*sa_restorer)(void);
    unsigned long sa_mask[16];  /* sigset_t, 64-bit on x86_64 */
};

/* Restorer function for signal handler (required on x86_64) */
static void __attribute__((noinline)) __restore_rt(void) {
    __asm__ volatile (
        "movq $15, %%rax\n\t"    /* SYS_rt_sigreturn */
        "syscall"
        :
        :
        : "memory"
    );
}

/* rt_sigaction syscall wrapper */
static inline int rt_sigaction(int sig, const struct sigaction *act, 
                                struct sigaction *oact, size_t sigsetsize) {
    return (int)syscall4(SYS_rt_sigaction, sig, (long)act, (long)oact, sigsetsize);
}

/* Simplified signal() using rt_sigaction */
static inline sighandler_t signal(int sig, sighandler_t handler) {
    struct sigaction sa = {0};
    struct sigaction old = {0};
    
    sa.__sa_handler.sa_handler = handler;
    sa.sa_flags = SA_RESTART;
    sa.sa_restorer = __restore_rt;
    
    if (rt_sigaction(sig, &sa, &old, 8) != 0) {
        return SIG_ERR;
    }
    return old.__sa_handler.sa_handler;
}

/* Framebuffer structures */
struct fb_bitfield {
    unsigned int offset;
    unsigned int length;
    unsigned int msb_right;
};

struct fb_var_screeninfo {
    unsigned int xres;
    unsigned int yres;
    unsigned int xres_virtual;
    unsigned int yres_virtual;
    unsigned int xoffset;
    unsigned int yoffset;
    unsigned int bits_per_pixel;
    unsigned int grayscale;
    struct fb_bitfield red;
    struct fb_bitfield green;
    struct fb_bitfield blue;
    struct fb_bitfield transp;
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
    unsigned short xpanstep;
    unsigned short ypanstep;
    unsigned short ywrapstep;
    unsigned int line_length;
    unsigned long mmio_start;
    unsigned int mmio_len;
    unsigned int accel;
    unsigned short capabilities;
    unsigned short reserved[2];
};

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPAN_DISPLAY     0x4606
#define FBIO_WAITFORVSYNC   0x460c
#define FBIOGET_FSCREENINFO 0x4602

#endif /* NOLIBC_H */
