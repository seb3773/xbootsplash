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

| Aspect | Plymouth | xbootsplash (fbdev) | xbootsplash (DRM) |
|--------|----------|---------------------|-------------------|
| Runtime | Daemon + service | Single binary, exits when done | Single binary, exits when done |
| Size | 10+ MB | 13-80 KB (static) | ~285 KB (dynamic) |
| Dependencies | libdrm, libply, plugins | **None** (freestanding) | libdrm, libc (dynamic) |
| Boot overhead | DRM init, theme load | ~0.5ms frame decode | ~0.5ms frame decode |
| Post-boot | Service continues | Nothing left running | Nothing left running |
| Complexity | 50,000+ lines | ~500 lines | ~600 lines |
| Debugging | Logs, services | Direct framebuffer | DRM/KMS dumb buffer |

The philosophy: **do one thing, then get out of the way.**

A boot splash should display an animation and disappear. No services, no daemons, no background processes. Once the real init takes over, xbootsplash is already gone.

## Distribution Compatibility

xbootsplash works on **any Linux distribution** that uses:
- **fbdev mode**: A framebuffer device (`/dev/fb0`)
- **DRM mode**: A DRM/KMS device (`/dev/dri/card0`)
- Standard initramfs (initramfs-tools, dracut, mkinitcpio, or custom)

Tested/compatible with:
- **Debian/Ubuntu**: initramfs-tools (standard method)
- **Arch Linux**: mkinitcpio (custom method)
- **Fedora/RHEL**: dracut (custom method)
- **Gentoo**: genkernel or custom initramfs
- **Alpine**: mkinitfs (custom method)
- **Custom setups**: Any initramfs with manual init script

**fbdev mode** works on systems with:
- `efifb` (EFI systems)
- `vesafb` (BIOS/legacy)
- `simplefb` (device tree)
- DRM drivers with fbdev emulation enabled

**DRM mode** works on systems with:
- Any DRM/KMS driver (amdgpu, i915, nouveau, radeon, etc.)
- No fbdev emulation required

## Features

- **5 Display Modes**: Animation or static image, with solid color or background image
- **Zero Dependencies** (fbdev): Freestanding binary, no libc required
- **Minimal Size**: 13 KB (fbdev static) to 29–80 KB (animations) or 35–285 KB (DRM dynamic). Rich or cinematic boot animations with dozens of complex frames or full-screen backgrounds can comfortably reach ~1 MB if desired, while remaining completely negligible in memory and initramfs footprint.
- **Dual Backend**: fbdev (legacy) and DRM/KMS (modern) support
- **Live Hardware VT Preview**: Test splash screens directly on physical display hardware (`/dev/fb0` or DRM/KMS) at native refresh rates without rebooting or altering initramfs, backed by an automated 10-second fail-safe watchdog and instant keyboard exit
- **Autonomous GUI Studio**: Fast and lightweight TQt3-based visual studio (`xbootsplash-gui`) with real-time composite canvas, eyedropper color picker, package inspector, and an embedded hardware test runner (100% self-contained single executable)
- **Dual Super-Compression (ZX0 & UPKR)**:
  - **ZX0 Super-pack**: Elias-gamma variable-length coding, ultra-fast microsecond decode rate (>340 MB/s), 0-byte RAM state
  - **UPKR Super-pack**: Modern LZ + rANS adaptive entropy coding, delivering an additional **15% to 25% size reduction vs ZX0**
- **Native Debian (.deb) Packaging**: 1-click generation of installable `.deb` packages with automatic `update-initramfs` triggers on installation (`apt install ./splash.deb`) and removal
- **Auto-detection**: Frame indices, dimensions, optimal compression
- **Interactive Builder & Visual Studio**: Guided CLI setup with validation or full graphical suite
- **SSE2 Optimized**: Fast RGB565→RGB8888 conversion for 32bpp framebuffers
- **Graceful Shutdown**: SIGTERM/SIGINT handler for clean exit
- **Custom Binary Names**: Install multiple splash screens with unique names (xbs_*)
- **Package System (.xbs)**: Export and install distributable theme packages with smart visual previews (lossless PNG for static images, animated GIF for animations)
- **Visual Package Inspector**: Browse `.xbs` packages with fluid, uncropped 16:9 real-time animated preview, inspect metadata, test on hardware, install directly, or convert to `.deb`
- **Binary Extraction**: Extract frames and images from compiled xbs_* binaries
- **Dual Coordinate Systems**: Position elements by offsets from center or absolute coordinates

## Package System (.xbs & .deb)

xbootsplash provides two distribution formats: universal `.xbs` archives and native Debian `.deb` packages.

### 1. Universal Theme Package (.xbs)

A `.xbs` file is a portable tar.gz archive containing a pre-compiled splash binary and metadata, ready for instant inspection, testing, or installation.

```
theme_drm_1920x1080.xbs
├── splash_bin                 # Pre-compiled binary
├── metadata.conf              # Theme information (Bash-parseable)
└── preview.gif / preview.png  # Visual preview (animated GIF or static PNG)
```

> **Smart Visual Previews**: Static image themes generate a lossless `preview.png`, while animated splashes generate a fluid `preview.gif`. The GUI Package Inspector plays these previews in real-time with automatic aspect-ratio preservation and zero flicker.

#### Creating a Package (.xbs)
- **In GUI Studio**: Click **Export .xbs Package...**
- **In CLI Builder**:
  ```bash
  ./CLI_src/build_anim.sh
  # At the end, choose "Export as .xbs package"
  # Output saved in packages/ directory
  ```

#### Installing a Package (.xbs)
Install a downloaded `.xbs` package without needing gcc, ImageMagick, or source files:
- **In GUI Studio**: Open `File -> Inspect / Install .xbs Package...` and click **Install Bootsplash**.
- **In CLI**:
  ```bash
  sudo ./CLI_src/build_anim.sh --install-package packages/theme_drm_1920x1080.xbs
  ```

### 2. Native Debian Package (.deb)

xbootsplash can generate fully compliant `.deb` packages for Debian, Ubuntu, Linux Mint, Q4OS, and all Debian-derived distributions:

```
xbs-mytheme_1.0_amd64.deb
├── debian-binary              # Package format version (2.0)
├── control.tar.gz             # Package metadata, architecture, dependencies
└── data.tar.gz
    ├── /usr/bin/xbs_mytheme                          # Pre-compiled bootsplash binary
    ├── /etc/initramfs-tools/hooks/xbs_mytheme        # Initramfs hook (copies binary to initrd)
    ├── /etc/initramfs-tools/scripts/init-top/xbs_mytheme # Early-boot launch script
    └── /usr/share/doc/xbs-mytheme/copyright
```

#### Exporting a `.deb`
- **From GUI Menu**: Select **Tools &rarr; Build Debian Package (.deb)...**
- **From Package Inspector**: Open any `.xbs` package and click the **Export as .deb...** button.

#### Installing and Removing `.deb` Packages
Debian packages integrate natively with `apt` and `dpkg`. Maintainer scripts (`postinst` and `prerm`) automatically invoke `update-initramfs -u`:

```bash
# Install package and rebuild initramfs automatically
sudo apt install ./xbs-mytheme_1.0_amd64.deb

# Remove bootsplash and restore default boot cleanly
sudo apt remove xbs-mytheme
```

### Package Metadata

The `metadata.conf` file contains theme information:

```ini
# XBootsplash Package Metadata
XBS_PKG_VERSION="1.0"
SPLASH_NAME="mytheme"
BACKEND="drm"
RESOLUTION="1920x1080"
DISPLAY_MODE="0"
BG_COLOR="0x0000"
FRAME_W="600"
FRAME_H="338"
NFRAMES="60"
FPS="30"
BINARY_SHA256="e3b0c44298fc1c149afbf4c8996fb924..."
```

### Naming Convention

Packages follow the format: `<name>_<backend>_<resolution>.xbs`

Examples:
- `win11_drm_1920x1080.xbs` - Windows 11 style, DRM backend, 1080p
- `amiga_fbdev_1920x1080.xbs` - Amiga Workbench style, fbdev backend, 1080p
- `fox_fbdev_1920x1080.xbs` - Minimalist running fox, fbdev backend, 1080p

## Display Modes

| Mode | Description | Use Case | Typical Size |
|------|-------------|----------|--------------|
| **0** | Animation on solid background | Boot spinner, progress indicator | 60-80 KB |
| **1** | Animation on background image (centered) | Themed boot with logo | 500 KB - 2 MB |
| **2** | Animation on background image (fullscreen) | Cinematic boot | 2-4 MB |
| **3** | Static image on solid background | Logo display | 10-20 KB |
| **4** | Static image full screen | Wallpaper/splash art | 1-4 MB |

### Mode Details

**Coordinate Systems:** During configuration (STEP 4), you can choose between:
- **Offsets mode** (default): Each element is automatically centered, and you specify offsets from the centered position
- **Coordinates mode**: You specify absolute X,Y coordinates for each element (top-left corner)

Offsets are recommended for most use cases as they automatically adapt to resolution changes.

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

### Resolution Handling & Screen Size Discrepancies

A common operational question is: **what happens if a binary is compiled for a specific resolution (e.g. 1920x1080) but executed on a display with a different native resolution (e.g. 1024x768, 2560x1440, 4K, or vice versa)?**

#### Core Philosophy: Zero Runtime Dynamic Scaling
To guarantee instantaneous startup and maintain a freestanding binary with zero libc dependencies, **`xbootsplash` never performs real-time software bicubic or bilinear image scaling on the CPU**. Applying software interpolation on every frame at 60 FPS in early kernel boot would introduce measurable CPU overhead and delay desktop handoff.

Instead:
1. **Dynamic Hardware Query**: At boot time, `xbootsplash` queries the exact active screen resolution directly from the display hardware (`ioctl(FBIOGET_VSCREENINFO)` under fbdev, or KMS connector modes under DRM).
2. **1:1 Native Pixel Ratio**: Pixels are rendered at native 1:1 scale without blurring or geometric distortion.
3. **Hardware Boundary Clipping**: Low-level blitters (`blit_to_fb_32bpp`, `blit_frame_dblbuf`) enforce strict bounding guards on all four borders ($x < 0$, $y < 0$, $x + w > \text{xres}$, $y + h > \text{yres}$), preventing buffer overflows and crashes under any resolution mismatch.

#### Behavior by Display Mode

##### 1. Mode 0: Animation on Solid Background (e.g., Blackhole)
* **Visual Result**: **Flawless on all displays.**
* **Centering**: The animation origin is calculated dynamically at runtime relative to the active display:
  $$x = \frac{\text{xres} - \text{FRAME\_W}}{2} + \text{offset\_x}, \quad y = \frac{\text{yres} - \text{FRAME\_H}}{2} + \text{offset\_y}$$
* **Background**: The solid background color (`#RRGGBB`) automatically fills 100% of the screen, whether running on 800x600, 1080p, 1440p, or 4K.
* **Deformation**: **None.** The animation remains sharp, unstretched, and perfectly centered.

##### 2. Mode 1: Animation on Background Image (e.g., Amiga)
* **Visual Result**: **Seamless integration** when matching background margin colors.
* **Centering**: Both the background graphic and the animation frames are centered dynamically against the active screen dimensions.
* **If Native Screen > Background Image**: The background graphic rests in the center, and outer borders are filled with `BACKGROUND_COLOR`. If the background color matches the perimeter of your artwork, the transition is invisible.
* **If Native Screen < Background Image**: The artwork is clipped at the screen perimeter without crashing.

##### 3. Mode 2 (Fullscreen Background) & Modes 3/4 (Fullscreen Static Image)
In these modes, the artwork is prepared to `target_resolution` at compile time.
* **Compiled Resolution < Native Screen (e.g., 1280x720 binary on a 1920x1080 monitor)**:
  * **No distortion or stretching blur**: Pixels remain 1:1.
  * The image occupies its original dimensions ($1280 \times 720$), and the remaining horizontal and vertical borders display the configured `BACKGROUND_COLOR` (letterbox / pillarbox margins).
* **Compiled Resolution > Native Screen (e.g., 1920x1080 binary on a 1024x768 monitor)**:
  * **No crash or memory corruption**: Boundary clipping prevents out-of-bounds writes.
  * **Cropped**: The right and bottom regions of the image that exceed 1024x768 fall outside the viewport and are clipped.

#### Hibernation Resume Notification Banner
The *"Resume from hibernation..."* status banner is dynamically centered horizontally using the live screen width:
$$\text{banner\_x} = \frac{\text{xres} - \text{BANNER\_W}}{2}$$
It is therefore **always centered at the top of the physical screen**, regardless of the compile-time target resolution.

#### Summary Comparison Table

| Display Mode | Native Screen vs Compiled Target | Deformation / Blur? | Crash / Buffer Overflow? | Visual Output |
| :--- | :--- | :---: | :---: | :--- |
| **Mode 0 (Solid BG)** | Any resolution difference | **No** | **No** | **Perfect**: Auto-centered, solid color fills screen |
| **Mode 1 (Centered BG)** | Any resolution difference | **No** | **No** | **Seamless**: Auto-centered, solid margins fill remaining space |
| **Mode 2/4 (Fullscreen)** | Native screen **> compiled** | **No** | **No** | 1:1 image with solid letterbox/pillarbox borders |
| **Mode 2/4 (Fullscreen)** | Native screen **< compiled** | **No** | **No** | Image safely cropped at right/bottom edges |

## Binary

- **Output**: `xbootsplash` (13 KB - 4 MB depending on mode and content)
- **Format (fbdev)**: Static ELF64, freestanding (no libc)
- **Format (DRM)**: Dynamic ELF64, linked with libdrm
- **Entry point**: `_start` (fbdev) or `main` (DRM)

## Architecture

```
   Build Time                      Compile Time                Runtime
─────────────────────────────────────────────---------────────────────────────
PNG images (any size)         ┌─ splash_anim_delta.c       ┌─ /dev/fb0
       │                      │  + nolibc.h (static)       │  Frame buffer
       ▼                      │  OR                        │
generate_splash               │  splash_anim_drm.c         │
       │                      │  + libdrm (dynamic)        ▼
       ├── Convert to RGB     │  DRM dumb buffer           └─ /dev/dri/card0
       ├── Resize (optional)  │             
       ├── XOR delta          │
       └── RLE encode         └─ frames_delta.h
       │                              │
       ▼                              ▼
frames_delta.h                   xbootsplash
                                      │
                                      ▼
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
├── README.md               # Main documentation
├── Makefile                # Root build system
├── engine_src/             # Standalone C engine & compiler
│   ├── splash_anim_delta.c # Freestanding fbdev runtime engine
│   ├── splash_anim_drm.c   # Freestanding DRM/KMS runtime engine
│   ├── generate_splash.c   # Generator tool (PNG → compressed C header)
│   ├── nolibc.h            # Syscall wrappers (freestanding libc)
│   ├── start.S             # Minimal startup assembly (_start)
│   └── linker.ld           # Custom linker script
├── CLI_src/                # Interactive builder & extraction tools
│   ├── build_anim.sh       # Interactive builder + installer script
│   └── extract_frames.c    # Extraction tool (binary → PNG frames)
├── GUI_src/                # Graphical Studio (TQt3 source, embedded runner & assets)
├── zx0/                    # ZX0 compressor (C) & zero-libc freestanding decompressor
├── upkr/                   # UPKR compressor (C) & zero-libc freestanding decompressor
├── datas/                  # Library of 50+ sample animation frame sequences (PNG)
├── gif_previews/           # Animated GIF previews of sample themes
├── packages/               # Ready-to-install boot splash packages (.xbs)
├── projects/               # Pre-configured Studio project profiles (.xbsp)
├── docs/                   # Detailed guides & specifications
│   ├── HOW_TO_INSTALL.md   # Installation guide
│   └── COMPRESSION_FORMAT.md # Compression formats documentation
└── tests/                  # Automated test, benchmark & QEMU sandbox scripts
    ├── bench_compression.c # Compression ratio & throughput benchmark suite
    ├── test.sh             # Mode verification script
    └── test_qemu.sh        # Isolated QEMU/KVM sandbox runner
```

## Quick Start

### Graphical Studio (xbootsplash-gui)

Built with **Trinity Qt3 (TQt3)** for fast startup, responsive rendering, and zero bloat. For an interactive visual workflow with real-time positioning, eyedropper color selection, theme browsing, live hardware preview, and 1-click `.deb` package generation:

```bash
# Build the GUI binary
./GUI_src/build.sh
# Or from root: make gui

# Build portable standalone AppImage (zero-dependency on host TDE/TQt3)
./build_appimage.sh
# Or: make appimage  -> produces xbootsplash-gui-x86_64.AppImage

# Build native Debian/Ubuntu installer package
./build_deb.sh
# Or: make deb       -> produces xbootsplash-gui_1.0.0_amd64.deb

# Launch the visual studio
./GUI_src/build/xbootsplash-gui
# Or run the AppImage anywhere:
./xbootsplash-gui-x86_64.AppImage
```

Key Studio Features:
- **Interactive Canvas**: Real-time virtual composite preview with zoom, center crosshair guides, and playback controls.
- **Eyedropper Tool**: Sample exact RGB565 / hex colors directly from images or anywhere on your screen.
- **Package Inspector**: Visually browse `.xbs` packages with fluid uncropped 16:9 animated preview, inspect specs, install directly, or export to `.deb`.
- **Debian (.deb) Generator**: Export production-ready `.deb` packages with automatic initramfs update hooks.
- **Super-Pack Selector**: Choose between standard, ZX0, or UPKR compression for up to -65% binary size.
- **[▶ Test Live] (F6)**: Test your bootsplash on the physical screen without rebooting!
- **100% Autonomous Portable Executable**: The entire engine build toolchain (generator, freestanding nolibc runtime, linker script, ZX0 & UPKR compressors, and Makefile) is super-compressed into the GUI binary (~68 KB payload). Compilations occur purely in volatile RAM (`/run/user/<uid>/` tmpfs) with automatic cleanup—run `xbootsplash-gui` anywhere with zero Git repository dependency.

### Interactive CLI Build (Terminal)

```bash
./CLI_src/build_anim.sh
```

Follow the prompts to:
1. Select display mode
2. Configure offsets and colors
3. Choose compression method
4. Build and test

### Command Line Examples

```bash
# Animation on black background (default)
./CLI_src/build_anim.sh -m 0 -y 80 my_frames/

# Animation with custom background color
./CLI_src/build_anim.sh -m 0 -c 1a1a2e -y 100 my_frames/

# Animation on background image
./CLI_src/build_anim.sh -m 1 -b wallpaper.png -r 1920x1080 my_frames/

# Static logo on colored background
./CLI_src/build_anim.sh -m 2 -c 0d1117 logo.png

# Full screen static image
./CLI_src/build_anim.sh -m 3 -r 1920x1080 wallpaper.png

# Animation with ZX0 super-compression (ultra-compact binary)
./CLI_src/build_anim.sh -m 0 -Z my_frames/
```

## Included Themes, Projects & Animation Library

The repository includes a comprehensive set of ready-to-use animations, project profiles, and pre-compiled packages:

### 1. Animation Sequences (`datas/`)
Over 50 ready-to-build animation sequences (lossless PNG frames) curated for boot screens:
- **Retro Gaming & Computing**: `amiga`, `atari`, `neogeo`, `sega`, `msx`, `pong`, `pacman`
- **Sci-Fi & Cyberpunk**: `fallout`, `blackhole`, `eventhorizon`, `skynet`, `knightrider`, `sauron`, `scifi`, `rog`
- **Operating Systems**: `q4os`, `win95`, `win7`, `win10`, `win11`, `android`, `tdealien`
- **Modern Geometry & Minimal Loaders**: `fox`, `redsphere`, `cube3dcolors`, `diamond`, `racing`, `structure`, `infinite`, `rider`, `hexa`, `dots`, `spinner1`–`spinner5`, `loadingcircle`...

You can use these directly as frame sources in the CLI builder:
```bash
./CLI_src/build_anim.sh -m 0 datas/fox/
```
Or import them into GUI Studio by dragging or selecting the folder.

### 2. Studio Project Profiles (`projects/`)
Pre-tuned `.xbsp` project configuration files for XBootsplash Studio (`xbootsplash-gui`):
- Double-click or load via **File &rarr; Open Project...** (`Ctrl+O`).
- Instantly loads resolution, background color, centered/custom offsets, playback speed, and frame delays.
- Allows 1-click recompilation, live hardware testing, and `.deb` packaging.

### 3. Pre-Compiled Packages (`packages/`)
Over 30 ready-to-install `.xbs` theme packages containing pre-compiled binaries and embedded visual previews:
- **Inspect visually**: Open **File &rarr; Inspect / Install .xbs Package...** in GUI Studio to see the animated preview playing in real-time.
- **Hardware Test**: Click **Test Live on Hardware** to view the theme directly on your physical monitor without rebooting.
- **Debian Conversion**: Click **Export as .deb...** to turn any `.xbs` into an installable Debian package.
- **CLI Installation**:
  ```bash
  sudo ./CLI_src/build_anim.sh --install-package packages/amiga_fbdev_1920x1080.xbs
  ```

### 4. Animated GIF Previews (`gif_previews/`)
A collection of standalone animated GIFs showing the sample themes in action (`xbs_amiga_preview.gif`, `xbs_fallout_preview.gif`, `xbs_fox_preview.gif`, `xbs_redsphere_preview.gif`, `xbs_win95_preview.gif`, etc.). These provide an immediate visual preview in file managers, image viewers, and web browsers without needing to launch the studio or boot the system.

## Design Decisions

### 1. No Libc (fbdev mode)

**Problem**: Static libc → 600+ KB binary

**Solution**: Direct syscalls via inline assembly (fbdev mode only)

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
| 1 | Animation loop + background image blit (centered) |
| 2 | Animation loop + background image blit (fullscreen) |
| 3 | Static display + solid background fill |
| 4 | Static display + fullscreen image |

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

### Compression Modes: Normal vs Super-Packing (ZX0 & UPKR)

xbootsplash provides three compression tiers, selectable in the GUI Studio ("Enable maximum compression" dropdown) or via CLI flags:

| Mode | Compression Algorithm | Entropy / Match Model | Decompression Speed | Binary Footprint Gain | Typical Boot RAM |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Normal** *(default)* | RLE XOR / Palette LZSS | Run-length + 4 KB sliding window | **Instant** (direct blit) | Baseline (60–85 KB) | **0 KB** (direct streaming) |
| **ZX0 Super-pack** | Optimal LZ77 + Elias-gamma | Backwards match + bitstream Elias-gamma | **> 340 MB/s** (~2–20 µs/frame) | **20% to 50% smaller** | ~50 to 300 KB temporary buffer |
| **UPKR Super-pack** | Optimal LZ77 + Adaptive rANS | Asymmetric Numeral Systems (rANS) entropy | **> 120 MB/s** (~15–80 µs/frame) | **30% to 65% smaller** (*-15% to -25% vs ZX0*) | ~50 to 300 KB temporary buffer |

#### Why UPKR Achieves Maximum Compression
While ZX0 encodes match lengths and offsets using static Elias-gamma bit prefixes, **UPKR** couples optimal LZ77 parse trees with an **adaptive rANS (Asymmetric Numeral Systems)** entropy coder. As the stream is parsed, symbol probability tables continuously adapt to the local distribution of XOR deltas and zero-runs, squeezing an extra **15% to 25% out of streams that are already heavily compressed by ZX0**.

#### Benchmark on Real-World Boot Splash Themes

Compression benchmark conducted on 1080p (1920x1080) themes comparing raw frame buffers, RLE XOR baseline, ZX0, and UPKR:

| Theme | Frames | Uncompressed (Raw 1080p) | RLE XOR Baseline | ZX0 Super-pack | UPKR Super-pack | Net Gain vs ZX0 |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **amiga** (Workbench 3.1) | 32 | 132.7 MB | 500 KB | 332 KB | **272 KB** | **-18.1%** |
| **fox** (Running Fox) | 15 | 62.2 MB | 87 KB | 58 KB | **47 KB** | **-18.9%** |
| **fallout** (Vault Boy Pip-Boy) | 48 | 199.1 MB | 3.4 MB | 1.8 MB | **1.5 MB** | **-16.7%** |
| **redsphere** (3D Raytraced Sphere) | 30 | 124.4 MB | 162 KB | 79 KB | **66 KB** | **-16.5%** |
| **pacman** (Retro Arcade) | 30 | 124.4 MB | 1.1 MB | 620 KB | **512 KB** | **-17.4%** |

- **Zero-Libc Freestanding Decompressors**: Both the ZX0 unpacker (`zx0/zx0_decompress.c`) and UPKR unpacker (`upkr/upkr_decompress.c`) are written in 100% freestanding C without any libc or dynamic allocator (`malloc`) dependency. They decompress directly into temporary anonymous `mmap` RAM at boot time.
- **Decompression Budget**: At 30–60 FPS, a single frame budget is 16.6 to 33.3 milliseconds. Decoding a UPKR frame takes between 15 and 80 microseconds, utilizing **less than 0.3% of the available frame time budget**.

### The Compression Ouroboros: 4 Levels of Recursive Packing

An architectural hallmark of xbootsplash is its self-referential "Ouroboros" compression pipeline, recursively nesting compression algorithms across four lifecycle stages:

```text
[Level 1 : GUI Build Time (CMake)]
   CMake uses the compiled ZX0 host tool (pack_engine)
   to pack and compress the entire engine... including the ZX0 and UPKR C sources!
        ↓
[Level 2 : GUI Runtime (Click on "Build")]
   The standalone GUI binary uses its embedded C++ decompressor
   to extract into volatile RAM (tmpfs) the C sources of both ZX0 and UPKR compressors!
        ↓
[Level 3 : Splash Payload Generation]
   The ephemeral generator compiles and runs the chosen compressor (ZX0 or UPKR)
   to super-pack the animation frames and delta stream into frames_delta.h!
        ↓
[Level 4 : Linux Kernel Boot Time]
   The resulting minimal freestanding binary (xbootsplash) executes
   the matching zero-libc decompressor in nolibc to unpack frames in real time on the framebuffer!
```

| Lifecycle Level | Role | Mechanism | Memory Footprint / Performance |
| :--- | :--- | :--- | :--- |
| **Level 1** (CMake) | Pack engine sources | `pack_engine` with `zx0_compress_custom()` | Raw: 275.4 KB → ZX0: **74.1 KB** (**26.9%** ratio) |
| **Level 2** (GUI RAM) | Deploy build environment | `EmbeddedEngineExtractor` with `zx0_decompress_to()` | **< 2 ms** extraction to `/run/user/<uid>/` tmpfs |
| **Level 3** (Generator) | Super-pack frames & deltas | `generate_splash` with embedded ZX0 / UPKR | **20% to 65%** size reduction on animation streams |
| **Level 4** (Boot Time) | Stream display at kernel boot | Freestanding `zx0_decompress.c` or `upkr_decompress.c` | **> 120–340 MB/s** decode rate, 0 libc dependency |

### Delta Compression Algorithms (Animations)

| Method | Best For | Description |
|--------|----------|-------------|
| **RLE XOR** | Moderate changes | RLE on XOR deltas between frames |
| **RLE Direct** | Uniform areas | RLE on direct pixel values |
| **Sparse XOR** | Minimal changes | Position + value for changed pixels only |
| **Raw** | Fastest decode | No compression, fastest runtime |

The generator automatically analyzes each frame and selects the optimal delta strategy. When Super-packing (ZX0 or UPKR) is enabled, the resulting optimal delta stream is further compressed into a single unified stream.

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
| 2 | Code + frame data + bg (fullscreen) + buffer | 2-4 MB |
| 3 | Code + image + buffer | ~20 KB |
| 4 | Code + image (fullscreen) | 1-4 MB |

## Framebuffer Support

**fbdev mode:**

| Mode | Handling |
|------|----------|
| 16 bpp (RGB565) | Direct memcpy |
| 32 bpp (XRGB8888) | 565→888 expansion (SSE2) |

**Double buffering (fbdev):**

When available, double buffering is automatically enabled for tear-free animation:
- **Detection**: Runtime check for `yres_virtual >= 2 * yres`
- **Mechanism**: `FBIOPAN_DISPLAY` + `FBIO_WAITFORVSYNC` for page flipping
- **Fallback**: Single buffer mode if not supported
- **Zero overhead**: Function pointer dispatch (no per-frame test)

To check if your system supports double buffering:
```bash
cat /sys/class/graphics/fb0/virtual_size
# Example: 1920,2160 means double buffer available (yres_virtual=2160 for yres=1080)
```

**DRM mode:**

| Format | Handling |
|--------|----------|
| XRGB8888 | 565→888 expansion (SSE2 optimized for VRAM WC) |

## Customization

### Custom Animation

```bash
# Place PNGs in a directory (any naming with numbers)
mkdir my_frames
# Add files: anim_001.png, anim_002.png, ... or frame_00.png, ...

# Build interactively
./CLI_src/build_anim.sh my_frames
```

The generator automatically:
- **Detects frame index** from filename
- **Detects image dimensions** (all frames must be same size)
- **Converts any PNG format** to RGB via ImageMagick
- **Benchmarks compression** methods

### Static Image

```bash
# Mode 3: Static logo on colored background
./CLI_src/build_anim.sh -m 3 -c 0d1117 logo.png

# Mode 4: Full screen wallpaper (auto-resize to 1920x1080)
./CLI_src/build_anim.sh -m 4 -r 1920x1080 wallpaper.png
```

### Parameters

| Option | Description | Default |
|--------|-------------|---------|
| `-m, --mode` | Display mode (0-4) | 0 |
| `-x, --offset-x` | Horizontal offset from center | 0 |
| `-y, --offset-y` | Vertical offset from center | 80 |
| `-X, --bg-offset-x` | Background image X offset (mode 1) | 0 |
| `-Y, --bg-offset-y` | Background image Y offset (mode 1) | 0 |
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

## Compression Results & Benchmarks

Real-world size comparisons between Normal Mode and ZX0 Super-Compression:

| Test Set | Asset / Binary Type | Normal Size (without ZX0) | ZX0 Size (with `-Z`) | Size Reduction / Gain |
| :--- | :--- | :--- | :--- | :--- |
| **Win10 Spinner** (115 frames) | Standalone DRM Binary | **80,568 bytes** (78 KB) | **35,432 bytes** (34 KB) | **-56%** *(Binary divided by 2.3x!)* |
| **Win10 Spinner** (115 frames) | Standalone fbdev Binary | **74,200 bytes** (72 KB) | **29,000 bytes** (28 KB) | **-61%** *(Binary divided by 2.5x!)* |
| **Amiga Floppy** (32 frames, 397x427) | Animation Delta Stream | **1,293,121 bytes** (1.26 MB) | **259,249 bytes** (253 KB) | **-80%** (**1,033,872 bytes saved!**) |
| **Atari Logo** (214x77) | Static Image (Palette) | **8,408 bytes** | **6,714 bytes** | **-20.1%** |
| **Progress-0** (64x64) | Static Image (Palette) | **582 bytes** | **198 bytes** | **-66.0%** |

## Binary Extraction

xbootsplash includes a tool to extract frames and images from compiled `xbs_*` binaries. This is useful for:
- Recovering frames from a compiled splash
- Analyzing existing splash binaries
- Converting between splash configurations

### Using the Extraction Tool

```bash
# Build the extraction tool
make extract_frames

# Extract from a binary
./extract_frames xbs_mytheme mytheme_extracted/

# Output structure:
mytheme_extracted/
├── metadata.txt           # Binary metadata (mode, resolution, etc.)
├── frame_0.png            # Animation frames (modes 0, 1, 2)
├── frame_1.png
├── ...
├── static/                # For hybrid modes (1, 2)
│   └── background.png     # Background image
└── static.png             # For static modes (3, 4)
```

### Extraction from build_anim.sh

Use option 3 in the main menu:

```bash
./CLI_src/build_anim.sh
# Select: 3) Extract xbs_* binary
```

The script will:
1. Search for `xbs_*` binaries in the specified directory
2. Display metadata (mode, resolution, frame count, compression)
3. Extract frames as PNG images

### Supported Compression Methods

The extraction tool decompresses all compression methods:

| Method | Description |
|--------|-------------|
| Raw RGB565 | Uncompressed frames |
| RLE XOR | RLE-encoded XOR deltas |
| RLE Direct | RLE-encoded direct pixels |
| Sparse XOR | Position + value for changed pixels |
| Palette + LZSS | Palette-indexed LZSS compressed (static images, backgrounds) |

### Extraction Output by Mode

| Mode | Output Structure |
|------|-----------------|
| 0 (anim solid) | `frame_N.png` files |
| 1 (anim + bg) | `static/background.png` + `frame_N.png` |
| 2 (anim fullscreen bg) | `static/background.png` + `frame_N.png` |
| 3 (static centered) | `static.png` |
| 4 (static fullscreen) | `static.png` |

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
./CLI_src/build_anim.sh
# Follow prompts through build → test → install
```

Or install directly:
```bash
sudo ./CLI_src/build_anim.sh --install
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
sudo ./CLI_src/build_anim.sh --uninstall-only
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
3. Find the `linux` line and add `init=/bin/sh` at the end
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

See `docs/HOW_TO_INSTALL.md` for detailed instructions.

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

xbootsplash supports **two rendering backends**:

### Architecture Overview

```
┌──────────────────────────────────────────────────────────────┐
│                    build_anim.sh                             │
│  - libdrm edtected with pkg-config                           │
│  - Menu with "T" option (toggle DRM/fbdev)                   │
│  - Compiling corresponding source depending on selon USE_DRM │
└──────────────────────────────────────────────────────────────┘
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

### Why fbdev for Shutdown Splash?

Even on DRM-capable systems, the **shutdown splash always uses fbdev mode**. This is because:

1. **libdrm may be unavailable at shutdown time:**
   - Filesystem teardown is in progress
   - `/usr` may be a separate partition (already unmounted)
   - `libdrm.so` typically resides in `/usr/lib/` → inaccessible
   - Result: `error while loading shared libraries: libdrm.so.2`

2. **DRM drivers provide fbdev fallback:**
   - Modern DRM drivers expose `/dev/fb0` via `simpledrm` (kernel 5.14+)
   - Or via fbdev emulation (`CONFIG_DRM_FBDEV_EMULATION=y`)
   - The fbdev interface remains available even during shutdown

3. **Static fbdev binary has zero dependencies:**
   - No library loading required
   - Works in degraded environments (filesystems unmounted)
   - Guaranteed to execute successfully

**Summary:** At shutdown, the environment is degraded. DRM dynamic binaries risk crash due to missing libraries. fbdev static binaries always work.

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

1. **Use DRM mode**: Build with `make drm` or toggle to DRM in `build_anim.sh` - no fbdev emulation needed

2. **Or enable fbdev emulation** in kernel config (for fbdev mode):
   ```
   CONFIG_DRM_FBDEV_EMULATION=y
   ```

3. **Or use simpledrm** (kernel 5.14+):
   ```
   CONFIG_DRM_SIMPLEDRM=y
   ```

4. **GRUB fallback**: Add `nomodeset` to disable DRM and use vesafb/efifb

## Failure Cases and Expected Behavior

### Normal Operation

| Event | Behavior |
|-------|----------|
| SIGTERM received | Clean exit: clear framebuffer, close resources |
| SIGINT received | Same as SIGTERM |
| Animation complete (no loop) | Stay on last frame, wait for signal |
| Full loop enabled | Restart from frame 0 |
| Partial loop enabled | Restart from LOOP_START frame |
| Ping-pong loop enabled | Play smoothly forward then backward (0→N→1→0...) |

## Loop Modes

xbootsplash supports 4 loop modes for animations:

| Mode | Behavior | Use Case |
|------|----------|----------|
| **No loop** (`0`) | Play once, stay on last frame | Intro animation, boot complete indicator |
| **Full loop** (`1`) | Play 0→N, restart from 0 | Continuous animation |
| **Partial loop** (`2`) | Play 0→N once, then loop from frame X to N | Intro + seamless loop |
| **Ping-Pong** (`3`) | Play forward (0→N-1), then smoothly reverse (N-2→1) | Pendulum, breathing, bouncing, oscillating effects |

**Partial loop example** (11 frames, loop from frame 7):
```
First pass:  0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10
Then loop:                     ↑←←←←←←←←←←←←←←←←←←←←←←←←|
             7 → 8 → 9 → 10 → 7 → 8 → 9 → 10 → ...
```

**Ping-Pong loop example** (5 frames, indices 0..4):
```
Sequence:    0 → 1 → 2 → 3 → 4 → 3 → 2 → 1 → [loops back to 0]
```

> [!TIP]
> In Ping-Pong mode, turning point frames (`0` and `N-1`) are never repeated consecutively, ensuring consistent, fluid frame pacing without visual freeze or stutter. Furthermore, the symmetrical frame deltas compress with exceptional ratios under ZX0 super-compression!

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

## Live Hardware VT Testing Architecture

### The Problem: Why Live Hardware Testing?

Historically, developing and testing a custom boot splash under Linux has been tedious and fraught with risks:

1. **Blind Rebooting**: Validating changes required regenerating the initramfs (`update-initramfs -u`, `dracut --force`, or `mkinitcpio -P`) and rebooting the entire host machine. If an image offset was wrong, a background color mismatched, or the binary crashed, you had to reboot repeatedly. A severe crash could even leave early boot hanging.
2. **QEMU / Virtual Machine Discrepancies**: Testing inside a VM or emulator cannot accurately reproduce bare-metal GPU driver behaviors, physical display panel timings, native EDID resolutions, or kernel framebuffer emulation idiosyncrasies (such as Intel `i915drmfb`, `amdgpu`, or `nouveau`).
3. **Desktop Session Conflicts**: Attempting to write directly to `/dev/fb0` or claiming DRM master privileges (`DRM_IOCTL_SET_MASTER`) from inside an active X11 or Wayland desktop either gets blocked by the display server or destroys active desktop windows with catastrophic graphical glitches.

The **Live Hardware VT Preview** completely solves this workflow. With a single click (`[▶ Test Live]` or `F6`) in `xbootsplash-gui` or from the Package Inspector, physical screen control is temporarily delegated to the compiled bootsplash binary on a dedicated virtual console, running at full hardware refresh rate, before seamlessly and automatically returning to your unmodified desktop session within 10 seconds (or immediately upon pressing any key).

```
┌────────────────────────┐         ┌────────────────────────┐         ┌────────────────────────┐
│     Active Desktop     │  ─────> │      Isolated VT       │  ─────> │     Active Desktop     │
│     (X11 / Wayland)    │         │  (Direct FB/DRM Scan)  │         │  (Restored 1:1 State)  │
└────────────────────────┘         └────────────────────────┘         └────────────────────────┘
            │                                   │                                  ▲
            │ 1. Query current VT (VT_GETSTATE) │ 3. Enter KD_GRAPHICS             │ 6. KD_TEXT & termios reset
            │ 2. Allocate free VT (VT_OPENQRY)  │ 4. Direct evdev keyboard poll    │ 7. VT_ACTIVATE original VT
            └─────────────────────────────────> │ 5. Execute bootsplash binary     │ 8. Unsilence klogctl
                                                └──────────────────────────────────┘
```

---

### Low-Level Mechanisms & Fail-Safe Guarantees

The live hardware test is driven by an autonomous standalone runner (`xbs_vt_runner`), built around direct Linux kernel interfaces with zero tolerance for system lockouts:

#### 1. Dynamic VT Query & Seamless Acquisition
- **Origin VT Discovery**: The runner queries the console multiplexer `/dev/tty0` using `ioctl(tty0, VT_GETSTATE, &vts)`. This captures the exact virtual terminal hosting the user's active graphical session (e.g. VT 1 on systemd/GDM or VT 7 on legacy X11).
- **Free VT Allocation**: Instead of hardcoding a virtual console, it queries the kernel for the next unused Virtual Terminal via `ioctl(tty0, VT_OPENQRY, &free_vt)` (typically allocating VT 12).
- **Atomic Console Switch**: It opens `/dev/tty<free_vt>` and switches the physical display using `ioctl(tty0, VT_ACTIVATE, free_vt)` followed by `ioctl(tty0, VT_WAITACTIVE, free_vt)`.

#### 2. Visual Console Isolation
- **Graphics Mode Switch**: Transitions the target VT to graphics mode using `ioctl(vt_fd, KDSETMODE, KD_GRAPHICS)`. This immediately suppresses the blinking text cursor, disables kernel virtual terminal font rendering, and prevents console text artifacts from corrupting the display.
- **Display Unblanking (DPMS)**: Invokes `TIOCLINUX` subcode 4 (`TIOCL_UNBLANKSCREEN`) to force the physical display controller out of power-saving sleep.
- **Kernel Log Silencing**: Temporarily suppresses kernel console printk messages via `klogctl(6, NULL, 0)` (`SYSLOG_ACTION_CONSOLE_OFF`) so asynchronous dmesg warnings or device driver logs cannot draw over the bootsplash frames.

#### 3. Direct Hardware Event Monitoring (`evdev`)
- In `KD_GRAPHICS` mode, standard tty input processing (termios line discipline, `stdin`) is disabled by the Linux kernel, meaning the runner process cannot receive regular keyboard characters via standard input.
- To ensure the user can instantly exit the preview at any moment, the runner scans `/dev/input/event*` devices, inspects hardware capabilities via `ioctl(fd, EVIOCGBIT(0), evbit)` to identify all physical keyboards (`EV_KEY`), and monitors them non-blocking with `poll()`.
- Pressing **any key** (e.g. Esc, Space, Enter, or any alphanumeric key) immediately breaks the event loop and initiates instantaneous desktop restoration.

#### 4. Dual Safety Nets & Async-Signal-Safe Emergency Recovery
- **Dedicated Alternate Signal Stack (`sigaltstack`)**: Allocates a dedicated 64 KB memory arena (`SS_ONSTACK`) for signal execution. If the child process crashes, or if stack overflow or memory exhaustion occurs, signal handlers are guaranteed to execute without hitting stack boundaries.
- **Direct Kernel Syscall Emergency Handler**: Catches all critical termination and fault signals (`SIGSEGV`, `SIGABRT`, `SIGBUS`, `SIGILL`, `SIGFPE`, `SIGTERM`, `SIGINT`, `SIGQUIT`, `SIGALRM`). Standard C library functions (`printf`, `malloc`, `exit`) are not async-signal-safe and can deadlock if heap state is poisoned. The emergency recovery handler relies **strictly on raw direct Linux syscalls**:
  ```c
  /* Emergency async-signal-safe recovery */
  syscall(SYS_kill, g_child_pid, SIGKILL);
  syscall(SYS_ioctl, g_test_tty_fd, KDSETMODE, KD_TEXT);
  syscall(SYS_ioctl, g_tty0_fd, VT_ACTIVATE, g_original_vt);
  _exit(4);
  ```
- **Hardware Watchdog Timer**: Arms a kernel alarm timer (`alarm(duration + 2)`) *before* initiating `VT_ACTIVATE`. Even if an unexpected kernel freeze or deadlock were to occur inside VT switching ioctls, the hardware timer forces a `SIGALRM` and recovers the session.
- **Atomic Single-Execution Lock**: Guarded by `__atomic_test_and_set(&g_vt_restored, __ATOMIC_SEQ_CST)`, ensuring the cleanup sequence executes exactly once regardless of concurrency between timer expiration, keyboard events, or signals.

#### 5. Complete Desktop Environment Restoration
When the preview concludes (via duration timeout, keypress, or signal), the restoration routine performs full cleanup:
1. Sends `SIGTERM` to the splash process, allowing 300ms for graceful cleanup before escalating to `SIGKILL`.
2. Closes all open evdev keyboard file descriptors.
3. Restores console text mode: `ioctl(vt_fd, KDSETMODE, KD_TEXT)`.
4. Restores keyboard translation: `ioctl(vt_fd, KDSKBMODE, K_XLATE)`.
5. Restores canonical termios flags (`ICANON`, `ECHO`, `ISIG`, `CS8`, etc.).
6. Emits the ANSI hardware terminal reset sequence (`\033c`) to reset terminal fonts and clear display state.
7. Re-enables kernel console logging: `klogctl(7, NULL, 0)`.
8. Reactivates and waits for the original desktop virtual terminal: `ioctl(tty0, VT_ACTIVATE, orig_vt)` and `ioctl(tty0, VT_WAITACTIVE, orig_vt)`.

#### 6. 100% Self-Contained In-Memory Deployment
To keep `xbootsplash-gui` completely standalone:
- The compiled C runner (`xbs_vt_runner`, stripped down to ~9.8 KB) is converted at build time into a C byte array (`embedded_vt_runner.h`) via `xxd -i` and embedded directly in the `xbootsplash-gui` binary rodata.
- At runtime, the GUI extracts the runner on-the-fly into volatile RAM `tmpfs` (`/run/user/<uid>/xbs_vt_runner`), verifies its byte length, marks it executable (`0755`), and invokes it with `sudo`.
- No companion binaries or helper scripts need to be distributed, installed, or kept on disk.

#### 7. DRM fbdev Shadow Buffer Flushing (`FBIOPAN_DISPLAY`)
On modern Linux DRM drivers operating in fbdev emulation (such as `i915drmfb` on Intel GPUs), the kernel maintains an in-memory shadow buffer. In single-buffer mode (`!fb_has_dblbuf`), writing pixels directly to mmap'd framebuffer memory does not trigger an immediate hardware scanout update without explicit display panning.
By invoking `ioctl(fb_fd, FBIOPAN_DISPLAY, &fb_vinfo)` on every rendered frame, `xbootsplash` forces the DRM subsystem to flush dirty memory to the display controller, ensuring seamless 60 FPS playback on all DRM fbdev emulations without frame freezing.

## Initramfs Integration Guide

### initramfs-tools (Debian/Ubuntu)

**Recommended: Native Debian Package**
```bash
sudo apt install ./xbs-mytheme_1.0_amd64.deb
# Automatically registers hooks and runs update-initramfs -u
```

**Alternative: GUI Studio or CLI Builder**
- In GUI Studio: Click **Install Bootsplash** in the Package Inspector.
- In CLI:
  ```bash
  sudo ./CLI_src/build_anim.sh
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

## Future Roadmap: Pre-compiled ELF Stubs (Zero-Compiler Generation)

Currently, creating an `xbootsplash` binary invokes `gcc` to compile the runtime engine (`splash_anim_delta.c` or `splash_anim_drm.c`) together with the generated payload header (`frames_delta.h`).

However, the executable machine code itself (direct kernel syscalls, framebuffer discovery and mmap, SSE2 blitter, 60 FPS animation loop, and ZX0/LZSS decompression engine) is completely static and identical across all themes. Only the data payload (compressed frames/deltas) and theme configuration metadata (dimensions, display mode, loop mode, frame delay) change.

A planned future evolution is to offer **pre-compiled ELF stub injection** (similar to the stub-based architecture utilized in ELF packers like [zELF](https://github.com/seb3773)):

- **Pre-compiled Stubs**: Ship minimal, pre-compiled template binaries (`xbs_stub_fbdev.bin` and `xbs_stub_drm.bin`, ~5–15 KB).
- **Payload Patching**: Instead of invoking an external C compiler, the generator or GUI directly patches the pre-compiled ELF stub (by overwriting a reserved payload section or appending a new ELF section and updating the ELF headers).
- **Key Advantages**:
  1. **Zero Build Dependencies**: Users do not need `gcc`, `make`, `binutils`, or any C compiler toolchain installed on their system to generate custom bootsplashes.
  2. **Instant Generation (< 10 ms)**: Splash binaries are produced almost instantaneously without compiler invocation overhead.
  3. **Deterministic & Portable**: Guarantees identical, predictable machine binaries across all distributions regardless of local GCC versions or system libraries.
  4. **Self-Contained GUI**: The GUI can generate final, production-ready boot executables natively out-of-the-box on systems lacking development packages.

## Future Roadmap: Unified Hybrid Binary (Direct Kernel KMS + fbdev Fallback, Zero libdrm)

Another planned architectural evolution is the creation of a **single, unified, freestanding hybrid binary**:

- **DRM/KMS without `libdrm.so`**:
  - Direct Rendering Manager (DRM) does not strictly require the userspace library `libdrm.so`.
  - The necessary Linux DRM ioctls (`DRM_IOCTL_MODE_GETRESOURCES`, `DRM_IOCTL_MODE_GETCONNECTOR`, `DRM_IOCTL_MODE_CREATE_DUMB`, `DRM_IOCTL_MODE_MAP_DUMB`, `DRM_IOCTL_MODE_ADDFB`, `DRM_IOCTL_MODE_SETCRTC`, `DRM_IOCTL_MODE_PAGE_FLIP`) are part of the standard Linux kernel UAPI (`<drm/drm.h>` and `<drm/drm_mode.h>`).
  - Because `xbootsplash` runs freestanding under `nolibc`, these ioctls can be invoked via direct raw kernel `ioctl()` syscalls without linking to `libdrm` or `libc`.

- **Automatic Runtime Fallback (`/dev/dri/card0` → `/dev/fb0`)**:
  - At startup, the unified binary attempts to open `/dev/dri/card0` and initialize native KMS with hardware page-flipping (tear-free VSync).
  - If DRM initialization fails (e.g. at early boot in `initramfs` before `udev` has loaded the GPU driver, or on systems without DRM/KMS support), the binary instantly and seamlessly falls back to the Framebuffer `/dev/fb0`.

- **Key Advantages**:
  1. **Single Universal Binary**: One ~40 KB static executable for both early Boot (with automatic fallback) and late Shutdown (no risk of shared library unmount crashes).
  2. **100% Freestanding & Static**: Zero external library dependencies (`libc`, `libdrm`), eliminating package management complexity.
  3. **Best of Both Worlds**: Tear-free hardware VSync when KMS is ready, and guaranteed universal display compatibility everywhere else.

## Credits & Acknowledgments

- **Dennis Ranke** (exoticorn): Creator of the [UPKR compression format](https://github.com/exoticorn/upkr), providing state-of-the-art LZ+rANS compression ratios and lightweight decoding.
- **Einar Saukas**: Creator of the [ZX0 compression format](https://github.com/einar-saukas/ZX0), enabling ultra-compact payloads with zero-allocation freestanding decompression.
- **seb3773**: Project creator, architecture, freestanding engine, and XBootsplash Studio implementation ([https://github.com/seb3773](https://github.com/seb3773)).


