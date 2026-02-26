# xbootsplash - Installation Guide

A minimal, high-performance boot splash animation for Linux x86_64 systems.

## Quick Start

```bash
# 1. Compile
make

# 2. Install (choose method)
sudo ./splashboot_install.sh --method standard   # RECOMMENDED
# OR
sudo ./splashboot_install.sh --method custom --initramfs-dir /boot/my-initramfs

# 3. Reboot
sudo reboot
```

## Binary

- **Name**: `xbootsplash`
- **Size**: ~74 KB
- **Dependencies**: None (freestanding)

## Installation Methods

### Method 1: Standard (RECOMMENDED)

**For**: Debian/Ubuntu systems using `initramfs-tools`

**Usage**:
```bash
sudo ./splashboot_install.sh --method standard
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

### Method 2: Custom (Advanced)

**For**: Custom initramfs, non-Debian systems

**Usage**:
```bash
sudo ./splashboot_install.sh --method custom --initramfs-dir /boot/initramfs-custom
```

**Requires**:
- Manual init script configuration
- Bootloader setup
- Root mount logic

## Requirements

- Linux x86_64
- GCC compiler
- libpng (for frame generation)
- Root privileges
- `initramfs-tools` (standard method)

## Plymouth Conflict

Uninstall Plymouth first:
```bash
sudo apt remove --purge plymouth plymouth-themes
```

## Customization

### Custom Images

1. Prepare 64×64 PNG images (black background)
2. Name: `frame_00.png`, `frame_01.png`, ...
3. Generate:
   ```bash
   make frames
   make
   ```

### Animation Parameters

Edit `splash_anim_delta.c`:
```c
#define FRAME_DURATION_MS 33  // Speed
#define VERTICAL_OFFSET  80   // Position
```

## Uninstallation

```bash
sudo rm /sbin/xbootsplash
sudo rm /etc/initramfs-tools/hooks/xbootsplash
sudo rm /etc/initramfs-tools/scripts/init-top/xbootsplash
sudo rm /etc/initramfs-tools/scripts/init-bottom/xbootsplash
sudo update-initramfs -u
```
