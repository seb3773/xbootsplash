# xbootsplash

A minimal boot splash animation for Linux x86_64, designed for initramfs deployment with zero runtime dependencies.

## Binary

- **Output**: `xbootsplash` (~74 KB)
- **Format**: Static ELF64, freestanding (no libc)
- **Entry point**: `_start` (no crt0)

## Architecture

```
Build Time                    Compile Time                 Runtime
─────────────────────────────────────────────────────────────────────
PNG images (64x64)            splash_anim_delta.c          /dev/fb0
       │                           │                          │
       ▼                           ▼                          ▼
generate_delta_v2            GCC -nostdlib              Frame buffer
       │                           │                          │
       ├── PNG → RGB565            ├── nolibc.h               │
       ├── XOR delta               └── frames_delta.h         │
       └── RLE encode                        │                │
       │                                     ▼                │
       ▼                           xbootsplash (74KB)         │
frames_delta.h                           │                    │
                                         ▼                    ▼
                                    Load frame 0 ───────► Blit
                                         │                    │
                                         ▼                    ▼
                                    Apply XOR delta ─────► Blit
                                         │                    │
                                         ▼                    ▼
                                    nanosleep(33ms) ◄────────┘
```

## Design Decisions

### 1. No Libc

**Problem**: Static libc → 600+ KB binary

**Solution**: Direct syscalls via inline assembly

```c
// nolibc.h - syscall wrapper
static inline long syscall3(long n, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}
```

**Result**: 74 KB binary (vs 678 KB)

### 2. RLE XOR Delta vs PNG

| Method | Size | Boot CPU |
|--------|------|----------|
| PNG + upng | 37 KB | High |
| **RLE XOR delta** | **74 KB** | **~0** |
| Raw RGB565 | 520 KB | 0 |

**Rationale**: Boot CPU is critical. PNG requires:
- DEFLATE decompression (Huffman tables)
- CRC verification
- Chunk parsing
- Branching

XOR delta: simple `xor` instruction + `memcpy`

### 3. 16-bit XOR

| Width | Size |
|-------|------|
| **16-bit** | **62.6 KB** |
| 32-bit | 70.3 KB |
| 64-bit | 85.9 KB |

Larger words reduce skip opportunities.

### 4. Single Frame Buffer

```
Memory: 8 KB frame_buffer[64×64×2]
```

- No malloc/free
- XOR updates in-place
- Deterministic

### 5. RGB565

- 2 bytes/pixel (vs 4 for RGBA)
- Direct copy to 16bpp framebuffer
- Simple expansion to 32bpp

## File Structure

```
xbootsplash/
├── README.md              # This file
├── HOW_TO_INSTALL.md      # User guide
├── Makefile               # Build system
├── nolibc.h               # Syscall wrappers
├── splash_anim_delta.c    # Main program
├── frames_delta.h         # RLE frames (generated)
├── generate_delta_v2.c    # PNG → RLE tool
├── splashboot_install.sh  # Installer
└── win10_png/             # Source images
```

## RLE Format

```
Frame 0: 8192 bytes raw RGB565 (little-endian)

Delta frames:
  0x00         = end of frame
  0x01-0x7F    = next N uint16_t XOR values
  0x80-0xFF    = skip (N-0x80+1) unchanged pixels
```

**Example**:
```
0x85          → skip 6 pixels (0x85 & 0x7F = 5, +1 = 6)
0x03, 0xABCD  → 3 XOR values: 0xABCD, ...
0x00          → end
```

## Performance

| Metric | Value |
|--------|-------|
| Load frame 0 | ~0.1 ms |
| Apply delta | ~0.05 ms |
| Blit to FB | ~0.2 ms |
| **Total/frame** | **~0.35 ms** |
| CPU usage @ 30 FPS | **~1%** |

## Memory

| Component | Size |
|-----------|------|
| Code | ~10 KB |
| Frame data | ~63 KB |
| Frame buffer | 8 KB |
| Stack | ~1 KB |
| **Total** | **~82 KB** |

## Framebuffer Support

| Mode | Handling |
|------|----------|
| 16 bpp (RGB565) | Direct memcpy |
| 32 bpp (XRGB8888) | 565→888 expansion |

## Customization

### Custom Images

```bash
# Place PNGs in directory
mkdir my_images
# ... add frame_00.png, frame_01.png, ...

# Generate frames
gcc -O2 -o generate_delta_v2 generate_delta_v2.c -lpng
./generate_delta_v2 my_images > frames_delta.h

# Rebuild
make
```

### Parameters

Edit `splash_anim_delta.c`:

```c
#define FRAME_DURATION_MS 33   // ~30 FPS
#define VERTICAL_OFFSET  80    // Pixels below center
#define FRAME_W 64            // Width
#define FRAME_H 64            // Height
```

### Frame Size

Larger frames = larger binary:
- 64×64 = 74 KB
- 128×128 = ~200 KB
- 256×256 = ~500 KB

## Limitations

1. x86_64 only (syscall numbers)
2. No alpha blending (black background)
3. No scaling
4. No audio

## Porting (ARM64)

Modify `nolibc.h`:

```c
#define SYS_read  63   // ARM64 syscall numbers
#define SYS_write 64
// ...

static inline long syscall1(long n, long a1) {
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a1;
    __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}
```

