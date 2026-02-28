# xbootsplash

A minimal boot splash animation for Linux x86_64, designed for initramfs deployment with zero runtime dependencies.

## Why xbootsplash?

**Plymouth is overkill for a boot splash.**

Plymouth was designed as a full graphical boot experience with:
- A persistent daemon running throughout boot and **after login**
- Complex DRM/KMS rendering with multiple backends
- Theme engine with scripting support
- Integration with display managers (gdm, lightdm)
- Password prompts for encrypted disks
- Multi-seat support, VT handling, etc.

This complexity comes at a cost:
- **~10+ MB** of installed size (themes, plugins, libraries)
- **Background service** continues running after boot
- **Boot overhead**: DRM initialization, theme loading, IPC
- **Potential conflicts** with graphics drivers and display managers
- **Difficult to debug**: multiple layers of abstraction

**xbootsplash takes a different approach:**

| Aspect | Plymouth | xbootsplash |
|--------|----------|-------------|
| Runtime | Daemon + service | Single binary, exits when done |
| Size | 10+ MB | 13-80 KB |
| Dependencies | libdrm, libply, plugins | **None** (freestanding) |
| Boot overhead | DRM init, theme load | ~0.5ms frame decode |
| Post-boot | Service continues | Nothing left running |
| Complexity | 50,000+ lines | ~500 lines |
| Debugging | Logs, services | Direct framebuffer |

The philosophy: **do one thing, then get out of the way.**

A boot splash should display an animation and disappear. No services, no daemons, no background processes. Once the real init takes over, xbootsplash is already gone.

## Distribution Compatibility

xbootsplash works on **any Linux distribution** that uses:
- A framebuffer device (`/dev/fb0`)
- Standard initramfs (initramfs-tools, dracut, mkinitcpio, or custom)

Tested/compatible with:
- **Debian/Ubuntu**: initramfs-tools (standard method)
- **Arch Linux**: mkinitcpio (custom method)
- **Fedora/RHEL**: dracut (custom method)
- **Gentoo**: genkernel or custom initramfs
- **Alpine**: mkinitfs (custom method)
- **Custom setups**: Any initramfs with manual init script

The only requirement is a working framebuffer. Most systems provide this via:
- `efifb` (EFI systems)
- `vesafb` (BIOS/legacy)
- `simplefb` (device tree)

## Features

- **5 Display Modes**: Animation or static image, with solid color or background image
- **Zero Dependencies**: Freestanding binary, no libc required
- **Minimal Size**: 13 KB (static image) to 80 KB (animation)
- **Auto-detection**: Frame indices, dimensions, optimal compression
- **Interactive Builder**: Guided setup with validation
- **SSE2 Optimized**: Fast RGB565→RGB8888 conversion for 32bpp framebuffers
- **Graceful Shutdown**: SIGTERM/SIGINT handler for clean exit
- **Custom Binary Names**: Install multiple splash screens with unique names (xbs_*)

## Display Modes

| Mode | Description | Use Case | Typical Size |
|------|-------------|----------|--------------|
| **0** | Animation on solid background | Boot spinner, progress indicator | 60-80 KB |
| **1** | Animation on background image (centered) | Themed boot with logo | 500 KB - 2 MB |
| **2** | Animation on background image (fullscreen) | Cinematic boot | 2-4 MB |
| **3** | Static image on solid background | Logo display | 10-20 KB |
| **4** | Static image full screen | Wallpaper/splash art | 1-4 MB |

### Mode Details

```
Mode 0: Animation on solid background
┌─────────────────────────────────────┐
│                                     │
│                                     │
│                                     │
│                                     │
│                                     │
│                                     │
│             (center)                │
│                                     │
│                                     │
│            ↓ offset_y               │
│    ┌───────────┐← offset_x          │
│    │ Animation │                    │
│    └───────────┘                    │
│         #Background color           │
└─────────────────────────────────────┘


Mode 1: Animation on background image
┌─────────────────────────────────────┐
│                                     │
│                                     │
│                  image offset_y     │
│image offset_x    ↓                  │
│     →┌────────────────────────────┐ │
│      │Background image            │ │
│      │           ↓                │ │
│      │       (center)             │ │
│      │           ↓offset_y        │ │
│      │    ┌─────────┐             │ │
│      │    │Animation│             │ │
│      │   →└─────────┘             │ │
│      │offset_x                    │ │
│      └────────────────────────────┘ │
│                   #Background color │
└─────────────────────────────────────┘

Mode 2: Animation on full screen background image
┌─────────────────────────────────────┐
│      Background image (full)        │
│                                     │
│                                     │
│                                     │
│                                     │
│                                     │
│             (center)                │
│                ↓offset_y            │
│        ┌─────────┐                  │
│        │Animation│                  │
│       →└─────────┘                  │
│offset_x                             │
│                                     │
│                                     │
└─────────────────────────────────────┘



Mode 3: Static image on solid background
┌─────────────────────────────────────┐
│                                     │
│                                     │
│                  image offset_y     │
│image offset_x    ↓                  │
│     →┌────────────────────────────┐ │
│      │Static image                │ │
│      │                            │ │
│      │       (center)             │ │
│      │                            │ │
│      │                            │ │
│      │                            │ │
│      │                            │ │
│      │                            │ │
│      └────────────────────────────┘ │
│                   #Background color │
└─────────────────────────────────────┘


Mode 4: Static image full screen
┌─────────────────────────────────────┐
│Full screen image (auto resized)     │
│                                     │
│                                     │
│                                     │
│                                     │
│                                     │
│                                     │
│             (center)                │
│                                     │
│                                     │
│                                     │
│                                     │
│                                     │
│                                     │
│                                     │
└─────────────────────────────────────┘

```

## Binary

- **Output**: `xbootsplash` (13 KB - 4 MB depending on mode and content)
- **Format**: Static ELF64, freestanding (no libc)
- **Entry point**: `_start` (no crt0)

## Architecture

```
Build Time                    Compile Time                 Runtime
─────────────────────────────────────────────────────────────────────
PNG images (any size)         splash_anim_delta.c          /dev/fb0
       │                           │                          │
       ▼                           ▼                          ▼
generate_splash              GCC -nostdlib              Frame buffer
       │                           │                          │
       ├── Convert to RGB          ├── nolibc.h               │
       ├── Resize (optional)       └── frames_delta.h         │
       ├── XOR delta                         │                 │
       └── RLE encode                        ▼                 │
       │                           xbootsplash                │
       ▼                                  │                   │
frames_delta.h                         ▼                     ▼
                                  Load frame(s) ───────► Blit
                                       │                    │
                                       ▼                    ▼
                                  Apply delta ───────► Blit
                                       │                    │
                                       ▼                    ▼
                                  nanosleep(ms) ◄──────────┘
```

## File Structure

```
xbootsplash/
├── README.md               # This file
├── HOW_TO_INSTALL.md       # Installation guide
├── Makefile                # Build system
├── nolibc.h                # Syscall wrappers
├── start.S                 # Startup assembly
├── linker.ld               # Linker script
├── splash_anim_delta.c     # Main program (multi-mode)
├── frames_delta.h          # Frame data (generated)
├── generate_splash.c       # Unified generator (all modes)
├── benchmark_compress.c    # Compression analyzer
├── build_anim.sh           # Interactive builder + installer
└── win10_png/              # Example frames
```

## Quick Start

### Interactive Build (Recommended)

```bash
./build_anim.sh
```

Follow the prompts to:
1. Select display mode
2. Configure offsets and colors
3. Choose compression method
4. Build and test

### Command Line

```bash
# Animation on black background (default)
./build_anim.sh -m 0 -y 80 my_frames/

# Animation with custom background color
./build_anim.sh -m 0 -c 1a1a2e -y 100 my_frames/

# Animation on background image
./build_anim.sh -m 1 -b wallpaper.png -r 1920x1080 my_frames/

# Static logo on colored background
./build_anim.sh -m 2 -c 0d1117 logo.png

# Full screen static image
./build_anim.sh -m 3 -r 1920x1080 wallpaper.png
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

**Result**: ~80 KB binary (vs 678 KB)

### 2. RLE XOR Delta vs PNG

| Method | Size | Boot CPU |
|--------|------|----------|
| PNG + upng | 37 KB | High |
| **RLE XOR delta** | **~75 KB** | **~0** |
| Raw RGB565 | 520 KB | 0 |

**Rationale**: Boot CPU is critical. PNG requires:
- DEFLATE decompression (Huffman tables)
- CRC verification
- Chunk parsing
- Branching

XOR delta: simple `xor` instruction + `memcpy`

### 3. Conditional Compilation

Only the code needed for the selected mode is compiled into the binary:

| Mode | Code Included |
|------|---------------|
| 0 | Animation loop + solid background fill |
| 1 | Animation loop + background image blit |
| 2 | Static display + solid background fill |
| 3 | Static display only |

### 4. Single Frame Buffer

```
Memory: frame_buffer[W×H×2] (allocated via mmap)
```

- No malloc/free
- XOR updates in-place
- Deterministic memory usage

### 5. RGB565

- 2 bytes/pixel (vs 4 for RGBA)
- Direct copy to 16bpp framebuffer
- Simple expansion to 32bpp

## Compression Methods

| Method | Best For | Description |
|--------|----------|-------------|
| **RLE XOR** | Moderate changes | RLE on XOR deltas between frames |
| **RLE Direct** | Uniform areas | RLE on direct pixel values |
| **Sparse XOR** | Minimal changes | Position + value for changed pixels only |
| **Raw** | Fastest decode | No compression, fastest runtime |

The benchmark tool automatically recommends the best method for your frames.

## RLE Format

### RLE Direct (Mode 0/1 frame 0)

```
0x00         = end of frame
0x01-0x7F    = next N uint16_t literal values
0x80-0xFF    = RLE run: (cmd & 0x7F) pixels of same value
```

### RLE XOR (Delta frames)

```
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

| Mode | Components | Size |
|------|------------|------|
| 0 | Code + frame data + buffer | ~90 KB |
| 1 | Code + frame data + bg + 2 buffers | 500 KB - 2 MB |
| 2 | Code + image + buffer | ~20 KB |
| 3 | Code + image | 1-4 MB |

## Framebuffer Support

| Mode | Handling |
|------|----------|
| 16 bpp (RGB565) | Direct memcpy |
| 32 bpp (XRGB8888) | 565→888 expansion |

## Customization

### Custom Animation

```bash
# Place PNGs in a directory (any naming with numbers)
mkdir my_frames
# Add files: anim_001.png, anim_002.png, ... or frame_00.png, ...

# Build interactively
./build_anim.sh my_frames
```

The generator automatically:
- **Detects frame index** from filename
- **Detects image dimensions** (all frames must be same size)
- **Converts any PNG format** to RGB via ImageMagick
- **Benchmarks compression** methods

### Static Image

```bash
# Single logo on colored background
./build_anim.sh -m 2 -c 1a1a2e logo.png

# Full screen wallpaper (auto-resize to 1920x1080)
./build_anim.sh -m 3 -r 1920x1080 wallpaper.png
```

### Parameters

| Option | Description | Default |
|--------|-------------|---------|
| `-m, --mode` | Display mode (0-3) | 0 |
| `-x, --offset-x` | Horizontal offset from center | 0 |
| `-y, --offset-y` | Vertical offset from center | 80 |
| `-c, --bg-color` | Background color (RRGGBB hex) | 000000 |
| `-b, --bg-image` | Background image (mode 1) | - |
| `-r, --resolution` | Target resolution WxH | auto |
| `-d, --delay` | Frame delay in ms | 33 |

### Generated Header

The generator creates `frames_delta.h` with:

```c
#define DISPLAY_MODE 0           // Selected mode
#define HORIZONTAL_OFFSET 0      // X offset
#define VERTICAL_OFFSET 80       // Y offset
#define BACKGROUND_COLOR 0x0000  // RGB565 color
#define NFRAMES 115              // Auto-detected
#define FRAME_W 64               // Auto-detected
#define FRAME_H 64               // Auto-detected
#define FRAME_DURATION_MS 33     // From -d option
// Mode 1 also includes:
#define BG_W 1920
#define BG_H 1080
```

## Compression Results

Example with 115 frames (Windows 10 spinner, 64x64):

| Metric | Value |
|--------|-------|
| Raw RGB565 | 920 KB |
| **Delta RLE** | **74 KB** |
| **Compression** | **12.44x** |

## Limitations

1. x86_64 only (syscall numbers)
2. No alpha blending
3. No audio
4. Resize uses bilinear interpolation (good quality, not best)

## Safety Features (Kill Switch)

xbootsplash reads the kernel command line (`/proc/cmdline`) at startup. You can disable it cleanly without uninstalling by adding one of these parameters to your bootloader (GRUB/Syslinux):

- `nosplash`: Standard Linux parameter, disables xbootsplash.
- `xbootsplash=0`: Specific parameter to disable only xbootsplash.

This is the **safest way** to handle a boot loop or display issue: simply edit the kernel line in GRUB and add `nosplash`.

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

## Installation

The `build_anim.sh` script includes built-in installation:

```bash
./build_anim.sh
# Follow prompts through build → test → install
```

Or install directly:
```bash
sudo ./build_anim.sh --install
```

### Installation Methods

| Method | Description |
|--------|-------------|
| **Standard** | Debian/Ubuntu initramfs-tools integration (recommended) |
| **Custom** | Full custom initramfs for advanced users |
| **Uninstall** | Remove xbootsplash from system |

### Manual Uninstall

If you need to manually remove xbootsplash:

```bash
# Remove all xbs_* binaries from /sbin
sudo rm -f /sbin/xbs_*

# Remove hooks and scripts (for each installed binary)
sudo rm -f /etc/initramfs-tools/hooks/xbs_*
sudo rm -f /etc/initramfs-tools/scripts/init-top/xbs_*
sudo rm -f /etc/initramfs-tools/scripts/init-bottom/xbs_*
sudo rm -f /run/xbs_*.pid

# Rebuild initramfs
sudo update-initramfs -u

# Update GRUB (if needed)
sudo update-grub
```

Or use the built-in uninstaller:
```bash
sudo ./build_anim.sh --uninstall-only
```

### Restore Previous Version

If a backup exists (`/sbin/xbs_*.bak`), it will be automatically restored during uninstall. To manually restore:

```bash
sudo mv /sbin/xbs_myname.bak /sbin/xbs_myname
sudo update-initramfs -u
```

### Emergency Recovery (Boot Failure)

If xbootsplash causes boot failure (black screen, freeze, or kernel panic):

**Method 1: Kill Switch (Easiest & Recommended)**
1. Hold `Shift` during boot to show GRUB menu (or press `Esc` for UEFI)
2. Press `e` to edit the selected entry
3. Find the `linux` line and append `nosplash` to the end
4. Press `Ctrl+X` or `F10` to boot
5. The system will boot without the splash screen. You can then uninstall or reconfigure it.

**Method 2: GRUB Init Override**
1. Hold `Shift` during boot to show GRUB menu
2. Press `e` to edit the selected entry
3. Find the `linux` line and add `init=/bin/bash` at the end
4. Press `Ctrl+X` or `F10` to boot
5. You'll get a root shell. Remount root as read-write:
   ```bash
   mount -o remount,rw /
   ```
6. Remove xbootsplash:
   ```bash
   # Remove all xbs_* binaries
   rm -f /sbin/xbs_*
   rm -f /sbin/xbs_*.bak
   rm -f /etc/initramfs-tools/hooks/xbs_*
   rm -f /etc/initramfs-tools/scripts/init-top/xbs_*
   rm -f /etc/initramfs-tools/scripts/init-bottom/xbs_*
   update-initramfs -u
   ```
7. Reboot:
   ```bash
   exec /sbin/init
   # or: reboot -f
   ```

**Method 3: Rescue Media (USB/CD)**
1. Boot from USB/CD rescue media (Ubuntu Live, SystemRescue, etc.)
2. Mount root filesystem:
   ```bash
   sudo mount /dev/sdXN /mnt   # Replace sdXN with your root partition
   ```
3. Chroot and remove:
   ```bash
   sudo chroot /mnt
   # Remove all xbs_* binaries
   rm -f /sbin/xbs_* /sbin/xbs_*.bak
   rm -f /etc/initramfs-tools/hooks/xbs_*
   rm -f /etc/initramfs-tools/scripts/init-top/xbs_*
   rm -f /etc/initramfs-tools/scripts/init-bottom/xbs_*
   update-initramfs -u
   exit
   reboot
   ```

### Known Limitations

| Limitation | Impact | Workaround |
|------------|--------|------------|
| Signal handler stub (previous) | No graceful shutdown | Fixed: proper rt_sigaction now |
| Pixel-by-pixel conversion | CPU usage on high-res | Fixed: SSE2 SIMD optimization |
| Infinite loop | Process persists if PID lost | Fixed: SIGTERM/SIGINT handling |

See `HOW_TO_INSTALL.md` for detailed instructions.

