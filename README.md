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
       ├── XOR delta                         │                │
       └── RLE encode                        ▼                │
       │                               xbootsplash            │
       ▼                                   │                  │
frames_delta.h                             ▼                  ▼
                                     Load frame(s) ───────► Blit
                                          │                   │
                                          ▼                   ▼
                                        Apply delta ───────► Blit
                                          │                   │
                                          ▼                   ▼
                                  nanosleep(ms) ◄─────────────┘
```

## File Structure

```
xbootsplash/
├── README.md               # This file
├── HOW_TO_INSTALL.md       # Installation guide
├── Makefile                # Build system
├── nolibc.h                # Syscall wrappers (freestanding libc)
├── start.S                 # Startup assembly (_start entry point)
├── linker.ld               # Custom linker script (minimal ELF)
├── splash_anim_delta.c     # Main program (multi-mode animation)
├── generate_splash.c       # Generator tool (PNG → compressed C header)
└── build_anim.sh           # Interactive builder + installer script
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

## Supported Resolutions

### Recommended Resolutions

| Resolution | Aspect Ratio | Use Case | Notes |
|------------|--------------|----------|-------|
| **1920×1080** | 16:9 | Most common, Full HD | Default recommendation |
| **2560×1440** | 16:9 | QHD, high-DPI | Works well |
| **3840×2160** | 16:9 | 4K UHD | Larger binary, slower blit |
| **1366×768** | 16:9 | Laptop common | Good for small screens |
| **1280×720** | 16:9 | HD ready | Minimal size |
| **1600×900** | 16:9 | WXGA++ | Good balance |

### Resolution Guidelines

- **Match your screen**: Use the native resolution for best quality
- **Frame size matters**: Animation frames are stored compressed; larger frames = larger binary
- **Background images**: Full-screen modes (2, 4) store entire image; expect 1-4 MB binaries
- **Auto-resize**: The generator can resize images to target resolution with bilinear interpolation

### Performance Impact

| Resolution | 32bpp Blit Time | Memory |
|------------|-----------------|--------|
| 1280×720 | ~0.5 ms | 1.8 MB |
| 1920×1080 | ~1.2 ms | 4 MB |
| 2560×1440 | ~2.2 ms | 7 MB |
| 3840×2160 | ~6 ms | 16 MB |

**Recommendation**: For smooth 30 FPS, stay ≤1920×1080 on older hardware.

## DRM vs FBDev Compatibility

xbootsplash now supports **two rendering backends**:

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    build_anim.sh                            │
│  - Détecte libdrm via pkg-config                            │
│  - Menu avec option T (toggle DRM/fbdev)                    │
│  - Compile le bon source selon USE_DRM                      │
└─────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
┌─────────────────────────┐     ┌─────────────────────────┐
│   fbdev mode            │     │   DRM mode              │
│   (USE_DRM=0)           │     │   (USE_DRM=1)           │
├─────────────────────────┤     ├─────────────────────────┤
│ splash_anim_delta.c     │     │ splash_anim_drm.c       │
│ + nolibc.h              │     │ + libdrm (dynamic)      │
│ + start.S + linker.ld   │     │                         │
├─────────────────────────┤     ├─────────────────────────┤
│ Static, ~280KB          │     │ Dynamic, ~285KB         │
│ /dev/fb0                │     │ /dev/dri/card0          │
│ Legacy framebuffer      │     │ DRM/KMS dumb buffers    │
└─────────────────────────┘     └─────────────────────────┘
```

### Key Differences

| Aspect | fbdev Mode | DRM Mode |
|--------|------------|----------|
| **Binary** | Static (nolibc) | Dynamic (libdrm) |
| **Device** | `/dev/fb0` | `/dev/dri/card0` |
| **Dependencies** | None | libdrm.so, libc.so |
| **Initramfs** | Simple copy | `copy_exec` for libs |
| **Modern systems** | May need emulation | Native support |
| **Multi-monitor** | No | Yes (detects connectors) |
| **V-Sync** | No | Possible via `drmWaitVBlank` |

### Framebuffer Drivers (FBDev)

xbootsplash uses the **legacy framebuffer interface** (`/dev/fb0`), which is supported by:

| Driver | System Type | Status |
|--------|-------------|--------|
| `efifb` | UEFI systems | ✅ Fully supported |
| `vesafb` | BIOS/legacy boot | ✅ Fully supported |
| `simplefb` | Device tree / ARM | ✅ Fully supported |
| `intelfb` | Intel integrated | ✅ Works |
| `nvidiafb` | NVIDIA legacy | ⚠️ May conflict with proprietary driver |
| `amdgpufb` | AMD | ✅ Works with amdgpu |

### DRM/KMS Systems

Modern systems use **DRM/KMS** (Direct Rendering Manager) instead of legacy fbdev:

- **SimpleDRM**: Newer kernels provide `simpledrm` which creates `/dev/fb0` from DRM
- **DRM drivers**: `amdgpu`, `i915`, `nouveau`, `radeon` may or may not expose `/dev/fb0`

**Compatibility Matrix:**

| Setup | `/dev/fb0` | fbdev mode | DRM mode |
|-------|------------|------------|----------|
| UEFI + efifb | ✅ Yes | ✅ Yes | ✅ Yes |
| UEFI + simpledrm | ✅ Yes | ✅ Yes | ✅ Yes |
| BIOS + vesafb | ✅ Yes | ✅ Yes | ✅ Yes |
| DRM driver with fbdev emulation | ✅ If enabled | ✅ Yes | ✅ Yes |
| DRM driver without emulation | ❌ No | ❌ No | ✅ Yes |
| Early boot (before GPU init) | ⚠️ Depends | ⚠️ May fail | ⚠️ May fail |

### DRM Implementation Details

The DRM backend (`splash_anim_drm.c`) uses **dumb buffers** for software rendering:

1. **Device Discovery**: Opens `/dev/dri/card0` (or `card1`) and checks for `DRM_CAP_DUMB_BUFFER`
2. **Connector Detection**: Finds the first connected display (HDMI, eDP, DP, etc.)
3. **CRTC Assignment**: Locates a CRTC that can drive the connected display
4. **Dumb Buffer Creation**: Allocates a linear 32bpp XRGB8888 buffer in VRAM
5. **Mode Setting**: Configures the CRTC with the native display resolution

**SSE2 Optimization for VRAM:**

DRM dumb buffers are mapped with **Write-Combining (WC)** cache mode. For optimal PCIe bandwidth:

```c
/* Process 8 pixels at once - 16-byte stores for WC efficiency */
__m128i pixels = _mm_loadu_si128((__m128i const *)(src + i));
__m128i lo = _mm_unpacklo_epi16(pixels, _mm_setzero_si128());
__m128i hi = _mm_unpackhi_epi16(pixels, _mm_setzero_si128());
/* ... RGB565 → XRGB8888 expansion with vector masks ... */
_mm_storeu_si128((__m128i *)(dst + i), result_lo);     // 16-byte write
_mm_storeu_si128((__m128i *)(dst + i + 4), result_hi); // 16-byte write
```

| Resolution | Blit Time (SSE2) | Bandwidth |
|------------|------------------|-----------|
| 1920×1080 | ~1.2 ms | ~6.5 GB/s |
| 2560×1440 | ~2.2 ms | ~7.0 GB/s |
| 3840×2160 | ~6 ms | ~5.3 GB/s |

### Checking Compatibility

```bash
# Check if framebuffer exists
ls -la /dev/fb0

# Check which driver provides it
cat /sys/class/graphics/fb0/name

# Check resolution
cat /sys/class/graphics/fb0/virtual_size

# Check if DRM is active
ls -la /sys/kernel/debug/dri/
```

### Troubleshooting DRM Issues

If `/dev/fb0` is missing on a DRM system:

1. **Enable fbdev emulation** in kernel config:
   ```
   CONFIG_DRM_FBDEV_EMULATION=y
   ```

2. **Or use simpledrm** (kernel 5.14+):
   ```
   CONFIG_DRM_SIMPLEDRM=y
   ```

3. **GRUB fallback**: Add `nomodeset` to disable DRM and use vesafb/efifb

## Failure Cases and Expected Behavior

### Normal Operation

| Event | Behavior |
|-------|----------|
| SIGTERM received | Clean exit: clear framebuffer, close resources |
| SIGINT received | Same as SIGTERM |
| Animation complete (no loop) | Stay on last frame, wait for signal |
| Loop enabled | Restart from frame 0 |

### Error Conditions

| Error | Behavior | User Visible |
|-------|----------|--------------|
| `/dev/fb0` not found | Exit(1) immediately | Black screen, boot continues |
| `/proc/cmdline` has `nosplash` | Exit(0) immediately | No splash, normal boot |
| `/proc/cmdline` has `xbootsplash=0` | Exit(0) immediately | No splash, normal boot |
| Framebuffer mmap fails | Exit(1) | Black screen |
| Invalid BPP (not 16/24/32) | Skip frame | Partial display |

### Graceful Degradation

- **No crash on error**: All errors result in clean exit
- **No hang**: Main loop always checks `terminate_requested`
- **No memory leak**: Arena allocator, no dynamic allocation at runtime
- **Clean handoff**: Framebuffer cleared to black before exit

### Known Edge Cases

| Case | Impact | Solution |
|------|--------|----------|
| Resolution mismatch | Animation offset may be off-screen | Use correct target resolution |
| 24bpp framebuffer | Works but slower (no SSE2 path) | Use 32bpp if possible |
| Sparse XOR overflow (>65535 pixels) | Method skipped in auto-selection | Use RLE XOR instead |
| Very long `/proc/cmdline` | Handled (4096 byte buffer) | No issue |

## Initramfs Integration Guide

### initramfs-tools (Debian/Ubuntu)

**Standard Installation** (recommended):
```bash
sudo ./build_anim.sh
# Select "Install" → "Standard (initramfs-tools)"
```

**How it works:**
- Binary installed to `/sbin/xbs_name`
- Hook script: `/etc/initramfs-tools/hooks/xbs_name` (copies binary to initramfs)
- Init-top script: `/etc/initramfs-tools/scripts/init-top/xbs_name` (starts splash early)
- Init-bottom script: `/etc/initramfs-tools/scripts/init-bottom/xbs_name` (stops splash before handoff)

**Manual setup:**
```bash
# Install binary
sudo cp xbs_mysplash /sbin/
sudo chmod 755 /sbin/xbs_mysplash

# Create hook
sudo tee /etc/initramfs-tools/hooks/xbs_mysplash << 'EOF'
#!/bin/sh
cp /sbin/xbs_mysplash "${DESTDIR}/sbin/"
EOF
sudo chmod 755 /etc/initramfs-tools/hooks/xbs_mysplash

# Create init-top script
sudo tee /etc/initramfs-tools/scripts/init-top/xbs_mysplash << 'EOF'
#!/bin/sh
if [ -x /sbin/xbs_mysplash ] && ! grep -q nosplash /proc/cmdline; then
    /sbin/xbs_mysplash &
    echo $! > /run/xbs_mysplash.pid
fi
EOF
sudo chmod 755 /etc/initramfs-tools/scripts/init-top/xbs_mysplash

# Create init-bottom script
sudo tee /etc/initramfs-tools/scripts/init-bottom/xbs_mysplash << 'EOF'
#!/bin/sh
if [ -f /run/xbs_mysplash.pid ]; then
    kill $(cat /run/xbs_mysplash.pid) 2>/dev/null
    rm -f /run/xbs_mysplash.pid
fi
# Clear framebuffer
dd if=/dev/zero of=/dev/fb0 2>/dev/null || true
EOF
sudo chmod 755 /etc/initramfs-tools/scripts/init-bottom/xbs_mysplash

# Rebuild initramfs
sudo update-initramfs -u
```

### mkinitcpio (Arch Linux)

**Setup:**
```bash
# Install binary
sudo cp xbs_mysplash /sbin/
sudo chmod 755 /sbin/xbs_mysplash

# Add to mkinitcpio.conf
sudo sed -i 's|^FILES=()|FILES="/sbin/xbs_mysplash"|' /etc/mkinitcpio.conf

# Or use a custom hook
sudo tee /etc/initcpio/hooks/bootsplash << 'EOF'
#!/bin/sh
run_hook() {
    if [ -x /sbin/xbs_mysplash ] && ! grep -q nosplash /proc/cmdline; then
        /sbin/xbs_mysplash &
        echo $! > /run/xbs_mysplash.pid
    fi
}

run_cleanuphook() {
    if [ -f /run/xbs_mysplash.pid ]; then
        kill $(cat /run/xbs_mysplash.pid) 2>/dev/null
        rm -f /run/xbs_mysplash.pid
    fi
    dd if=/dev/zero of=/dev/fb0 2>/dev/null || true
}
EOF
sudo chmod 755 /etc/initcpio/hooks/bootsplash

sudo tee /etc/initcpio/install/bootsplash << 'EOF'
#!/bin/sh
build() {
    add_file /sbin/xbs_mysplash
    add_runscript
}
help() {
    echo "Boot splash animation"
}
EOF
sudo chmod 755 /etc/initcpio/install/bootsplash

# Add 'bootsplash' to HOOKS in /etc/mkinitcpio.conf
# HOOKS=(base udev bootsplash autodetect ...)

# Rebuild
sudo mkinitcpio -P
```

### dracut (Fedora/RHEL)

**Setup:**
```bash
# Install binary
sudo cp xbs_mysplash /sbin/
sudo chmod 755 /sbin/xbs_mysplash

# Create dracut module
sudo mkdir -p /usr/lib/dracut/modules.d/90bootsplash

sudo tee /usr/lib/dracut/modules.d/90bootsplash/module-setup.sh << 'EOF'
#!/bin/bash
check() {
    return 0
}
depends() {
    return 0
}
install() {
    inst /sbin/xbs_mysplash
    inst_hook pre-udev 90 "$moddir/bootsplash-start.sh"
    inst_hook cleanup 90 "$moddir/bootsplash-stop.sh"
}
EOF
sudo chmod 755 /usr/lib/dracut/modules.d/90bootsplash/module-setup.sh

sudo tee /usr/lib/dracut/modules.d/90bootsplash/bootsplash-start.sh << 'EOF'
#!/bin/sh
if [ -x /sbin/xbs_mysplash ] && ! grep -q nosplash /proc/cmdline; then
    /sbin/xbs_mysplash &
    echo $! > /run/xbs_mysplash.pid
fi
EOF
sudo chmod 755 /usr/lib/dracut/modules.d/90bootsplash/bootsplash-start.sh

sudo tee /usr/lib/dracut/modules.d/90bootsplash/bootsplash-stop.sh << 'EOF'
#!/bin/sh
if [ -f /run/xbs_mysplash.pid ]; then
    kill $(cat /run/xbs_mysplash.pid) 2>/dev/null
    rm -f /run/xbs_mysplash.pid
fi
dd if=/dev/zero of=/dev/fb0 2>/dev/null || true
EOF
sudo chmod 755 /usr/lib/dracut/modules.d/90bootsplash/bootsplash-stop.sh

# Add to dracut.conf
echo 'add_dracutmodules+=" bootsplash "' | sudo tee -a /etc/dracut.conf.d/bootsplash.conf

# Rebuild
sudo dracut --force
```

### Custom initramfs

For custom init scripts:

```bash
#!/bin/sh
# In your init script

# Mount /proc first (required for cmdline check)
mount -t proc proc /proc

# Start splash (early)
if [ -x /sbin/xbs_mysplash ] && ! grep -q nosplash /proc/cmdline; then
    /sbin/xbs_mysplash &
    SPLASH_PID=$!
fi

# ... your init logic ...

# Stop splash before switch_root
if [ -n "$SPLASH_PID" ]; then
    kill $SPLASH_PID 2>/dev/null
    # Clear framebuffer
    dd if=/dev/zero of=/dev/fb0 2>/dev/null || true
fi

# Hand off to real init
exec switch_root /root /sbin/init
```

### Timing Considerations

| Phase | When | Recommended Action |
|-------|------|-------------------|
| **init-top** | Before udev | Best for early splash |
| **pre-udev** | dracut equivalent | Same as init-top |
| **init-bottom** | Before switch_root | Stop splash cleanly |
| **cleanup** | dracut equivalent | Same as init-bottom |

**Important**: Always stop the splash before `switch_root` or the process will be killed uncleanly.

