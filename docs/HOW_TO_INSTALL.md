# xbootsplash - Installation Guide

A minimal, high-performance boot splash animation for Linux x86_64 systems.

## Quick Start

### Option A: Native Debian Package (.deb) — Recommended
```bash
sudo apt install ./xbs-mytheme_1.0_amd64.deb
```
This automatically registers the initramfs hooks and rebuilds the initramfs via `update-initramfs -u`.

### Option B: Visual Studio (xbootsplash-gui)
1. Launch `./GUI_src/build/xbootsplash-gui` (or the portable `./xbootsplash-gui-x86_64.AppImage`)
2. Open the Package Inspector (`File -> Inspect / Install .xbs Package...`)
3. Click **Install Bootsplash**

### Option C: Interactive CLI Script
```bash
# 1. Build and install interactively
./CLI_src/build_anim.sh /path/to/frames

# 2. Follow the interactive menu:
#    - Select installation method
#    - Configure animation parameters
#    - Install to initramfs

# 3. Reboot
sudo reboot
```

## Binary

- **Name**: `xbootsplash` (fbdev) or `xbootsplash_drm` (DRM/KMS)
- **Size**: ~35-85 KB (down to ~25 KB with ZX0 or UPKR super-compression)
- **Dependencies**: 
  - fbdev: None (freestanding, static, nolibc)
  - DRM: libdrm (dynamic linking)

## Installation Methods

### Method 1: Native Debian Package (.deb)
Exported from GUI Studio (**Tools &rarr; Build Debian Package (.deb)...** or from the Package Inspector):
- Clean native installation: `sudo apt install ./xbs-mytheme_1.0_amd64.deb`
- Clean removal: `sudo apt remove xbs-mytheme`

### Method 2: Standard initramfs-tools (CLI)

**For**: Debian/Ubuntu systems using `initramfs-tools`

**Usage**:
```bash
./CLI_src/build_anim.sh /path/to/frames
# Select option 1 in the installation menu
```

**What it does**:
1. Installs binary to `/sbin/xbootsplash`
2. Creates initramfs-tools hook
3. Creates init-top and init-bottom scripts
4. Rebuilds initramfs

**Advantages**:
- ✓ Safe with LUKS, LVM, mdadm
- ✓ Persists across kernel updates
- ✓ No bootloader configuration needed

### Method 3: Install from .xbs Package
```bash
sudo ./CLI_src/build_anim.sh --install-package packages/theme_drm_1920x1080.xbs
```

### Method 4: Custom Initramfs (Advanced)

**For**: Custom initramfs, non-Debian systems

**Usage**:
```bash
./CLI_src/build_anim.sh /path/to/frames
# Select option 2 (Custom) in installation menu
```

**Requires**:
- Manual init script configuration
- Bootloader setup
- Root mount logic

## Requirements

- Linux x86_64
- GCC compiler
- libpng (for frame generation)
- Root privileges (for installation)
- `initramfs-tools` (standard method)

## Plymouth Conflict

Uninstall Plymouth first:
```bash
sudo apt remove --purge plymouth plymouth-themes
```

## Customization

### Custom Images

1. Prepare PNG images (any resolution, black background recommended)
2. Name: `frame_00.png`, `frame_01.png`, ...
3. Run: `./CLI_src/build_anim.sh /path/to/frames`

### Animation Parameters

Configured interactively in `build_anim.sh`:
- Frame delay (FPS)
- Position offsets
- Background color
- Loop mode

## Uninstallation

### For Debian Packages (.deb)
```bash
sudo apt remove xbs-mytheme
```

### For CLI or Package Inspector Installs
```bash
./CLI_src/build_anim.sh
# Select option 3: Uninstall bootsplash
```

Or directly:
```bash
sudo ./CLI_src/build_anim.sh --uninstall-only
```

This removes:
- `/sbin/xbs_*` (all installed splash binaries)
- `/etc/initramfs-tools/hooks/xbs_*`
- `/etc/initramfs-tools/scripts/init-top/xbs_*`
- `/etc/initramfs-tools/scripts/init-bottom/xbs_*`
- Rebuilds initramfs automatically
