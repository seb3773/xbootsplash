# xbootsplash - Installation Guide

A minimal, high-performance boot splash animation for Linux x86_64 systems.

## Quick Start

```bash
# 1. Build and install interactively
./build_anim.sh /path/to/frames

# 2. Follow the interactive menu:
#    - Select installation method
#    - Configure animation parameters
#    - Install to initramfs

# 3. Reboot
sudo reboot
```

## Binary

- **Name**: `xbootsplash` (fbdev) or `xbootsplash_drm` (DRM/KMS)
- **Size**: ~70-85 KB
- **Dependencies**: 
  - fbdev: None (freestanding, static)
  - DRM: libdrm (dynamic linking)

## Installation Methods

### Method 1: Standard (RECOMMENDED)

**For**: Debian/Ubuntu systems using `initramfs-tools`

**Usage**:
```bash
./build_anim.sh /path/to/frames
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

### Method 2: Install Existing Binary

**For**: Re-installing a previously built binary

**Usage**:
```bash
./build_anim.sh
# Select option 2: Install existing xbs_* binary
```

### Method 3: Custom Initramfs (Advanced)

**For**: Custom initramfs, non-Debian systems

**Usage**:
```bash
./build_anim.sh /path/to/frames
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
3. Run: `./build_anim.sh /path/to/frames`

### Animation Parameters

Configured interactively in build_anim.sh:
- Frame delay (FPS)
- Position offsets
- Background color
- Loop mode

## Uninstallation

```bash
./build_anim.sh
# Select option 3: Uninstall bootsplash
```

Or directly:
```bash
sudo ./build_anim.sh --uninstall-only
```

This removes:
- `/sbin/xbs_*` (all installed splash binaries)
- `/etc/initramfs-tools/hooks/xbs_*`
- `/etc/initramfs-tools/scripts/init-top/xbs_*`
- `/etc/initramfs-tools/scripts/init-bottom/xbs_*`
- Rebuilds initramfs automatically
