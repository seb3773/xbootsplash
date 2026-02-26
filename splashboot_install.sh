#!/bin/bash
#
# splashboot_install.sh - Install bootsplash animation into initramfs
#
# Usage: ./splashboot_install.sh --method <standard|custom> [OPTIONS]
#
# Methods:
#   standard  - Debian/Ubuntu initramfs-tools integration (RECOMMENDED)
#               Uses hooks and init-top scripts, safe with LUKS/LVM
#   custom    - Full custom initramfs (advanced users only)
#               Requires manual init script configuration
#
# Options:
#   --method METHOD        Installation method: standard or custom (required)
#   --initramfs-dir DIR    Path to initramfs directory (custom method only)
#   --binary FILE          Path to xbootsplash binary (default: ./xbootsplash)
#   --no-rebuild           Don't rebuild initramfs image
#   --help                 Show this help
#
# Requirements:
#   - Root privileges
#   - xbootsplash binary compiled
#   - For standard: initramfs-tools installed
#

set -e

# Default values
METHOD=""
INITRAMFS_DIR=""
BINARY="./xbootsplash"
REBUILD=true

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Help message
show_help() {
    sed -n '2,/^$/p' "$0" | sed 's/^# //'
    exit 0
}

# Check for Plymouth conflict
check_plymouth() {
    if command -v plymouth &>/dev/null || dpkg -l plymouth 2>/dev/null | grep -q '^ii' || rpm -q plymouth &>/dev/null 2>&1; then
        echo -e "${RED}============================================${NC}"
        echo -e "${RED}WARNING: Plymouth is installed on this system${NC}"
        echo -e "${RED}============================================${NC}"
        echo ""
        echo "Plymouth and this bootsplash system are incompatible."
        echo "Running both will cause boot issues (black screen, freezes)."
        echo ""
        echo -e "${YELLOW}You MUST uninstall Plymouth before proceeding.${NC}"
        echo ""
        echo "To uninstall Plymouth:"
        echo "  Debian/Ubuntu: sudo apt remove --purge plymouth plymouth-themes"
        echo "  RHEL/Fedora:   sudo dnf remove plymouth"
        echo "  Arch Linux:    sudo pacman -Rns plymouth"
        echo ""
        
        read -p "Do you want to uninstall Plymouth now? [y/N] " -n 1 -r
        echo ""
        
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            echo -e "${YELLOW}Uninstalling Plymouth...${NC}"
            if command -v apt &>/dev/null; then
                apt remove --purge -y plymouth plymouth-themes 2>/dev/null || true
            elif command -v dnf &>/dev/null; then
                dnf remove -y plymouth 2>/dev/null || true
            elif command -v pacman &>/dev/null; then
                pacman -Rns --noconfirm plymouth 2>/dev/null || true
            fi
            
            if command -v plymouth &>/dev/null || dpkg -l plymouth 2>/dev/null | grep -q '^ii'; then
                echo -e "${RED}Failed to uninstall Plymouth. Please remove it manually.${NC}"
                exit 1
            fi
            echo -e "${GREEN}Plymouth uninstalled successfully.${NC}"
        else
            echo -e "${RED}Plymouth must be uninstalled to continue. Aborting.${NC}"
            exit 1
        fi
    fi
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --method)
            METHOD="$2"
            shift 2
            ;;
        --initramfs-dir)
            INITRAMFS_DIR="$2"
            shift 2
            ;;
        --binary)
            BINARY="$2"
            shift 2
            ;;
        --no-rebuild)
            REBUILD=false
            shift
            ;;
        --help|-h)
            show_help
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Validate method
if [[ -z "$METHOD" ]]; then
    echo -e "${RED}ERROR: --method is required${NC}"
    echo ""
    echo "Available methods:"
    echo "  standard  - Debian/Ubuntu initramfs-tools (RECOMMENDED)"
    echo "  custom    - Full custom initramfs (advanced)"
    echo ""
    echo "Example: sudo ./splashboot_install.sh --method standard"
    exit 1
fi

if [[ "$METHOD" != "standard" && "$METHOD" != "custom" ]]; then
    echo -e "${RED}ERROR: Invalid method '$METHOD'${NC}"
    echo "Valid methods: standard, custom"
    exit 1
fi

# Check root privileges
if [[ $EUID -ne 0 ]]; then
    echo -e "${RED}This script must be run as root${NC}"
    echo "Run: sudo $0 $@"
    exit 1
fi

# Check Plymouth
check_plymouth

# Check binary exists
if [[ ! -f "$BINARY" ]]; then
    echo -e "${RED}Binary not found: $BINARY${NC}"
    echo "Compile it first with: make delta"
    exit 1
fi

# ========================================
# STANDARD METHOD (Debian initramfs-tools)
# ========================================
install_standard() {
    echo -e "${GREEN}=== Installing via initramfs-tools (Standard Method) ===${NC}"
    echo ""
    
    # Check initramfs-tools
    if [[ ! -d /etc/initramfs-tools ]]; then
        echo -e "${RED}ERROR: initramfs-tools not found${NC}"
        echo "Install with: sudo apt install initramfs-tools"
        exit 1
    fi
    
    echo -e "${BLUE}This method:${NC}"
    echo "  ✓ Safe with LUKS, LVM, mdadm, resume"
    echo "  ✓ Integrates with Debian/Ubuntu boot process"
    echo "  ✓ No modification of critical init scripts"
    echo "  ✓ Automatically included in kernel updates"
    echo ""
    
    # Create hook script
    echo -e "${GREEN}[1/4] Creating initramfs-tools hook...${NC}"
    mkdir -p /etc/initramfs-tools/hooks
    cat > /etc/initramfs-tools/hooks/splash_anim << 'HOOK_EOF'
#!/bin/sh
# initramfs-tools hook for bootsplash animation

PREREQ=""
prereqs() { echo "$PREREQ"; }
case "$1" in prereqs) prereqs; exit 0;; esac

. /usr/share/initramfs-tools/hook-functions

# Copy binary
copy_file binary /sbin/splash_anim /sbin/splash_anim

# Ensure framebuffer device
if [ ! -e "${DESTDIR}/dev/fb0" ]; then
    mknod "${DESTDIR}/dev/fb0" c 29 0 2>/dev/null || true
fi
HOOK_EOF
    chmod +x /etc/initramfs-tools/hooks/splash_anim
    echo "  -> /etc/initramfs-tools/hooks/splash_anim"
    
    # Copy binary to /sbin
    echo -e "${GREEN}[2/4] Installing binary to /sbin...${NC}"
    cp "$BINARY" /sbin/splash_anim
    chmod +x /sbin/splash_anim
    echo "  -> /sbin/splash_anim"
    
    # Create init-top script (starts splash early)
    echo -e "${GREEN}[3/4] Creating init-top script...${NC}"
    mkdir -p /etc/initramfs-tools/scripts/init-top
    cat > /etc/initramfs-tools/scripts/init-top/splash_anim << 'INIT_TOP_EOF'
#!/bin/sh
# Start bootsplash animation early in boot
# This runs after /dev is mounted, before root filesystem

PREREQ="udev"
prereqs() { echo "$PREREQ"; }
case "$1" in prereqs) prereqs; exit 0;; esac

. /scripts/functions

# Start splash in background
if [ -x /sbin/splash_anim ]; then
    /sbin/splash_anim &
    SPLASH_PID=$!
    echo "$SPLASH_PID" > /run/splash_anim.pid
fi
INIT_TOP_EOF
    chmod +x /etc/initramfs-tools/scripts/init-top/splash_anim
    echo "  -> /etc/initramfs-tools/scripts/init-top/splash_anim"
    
    # Create init-bottom script (stops splash before switch_root)
    echo -e "${GREEN}[4/4] Creating init-bottom script...${NC}"
    mkdir -p /etc/initramfs-tools/scripts/init-bottom
    cat > /etc/initramfs-tools/scripts/init-bottom/splash_anim << 'INIT_BOTTOM_EOF'
#!/bin/sh
# Stop bootsplash before switching to real root

PREREQ=""
prereqs() { echo "$PREREQ"; }
case "$1" in prereqs) prereqs; exit 0;; esac

. /scripts/functions

# Stop splash animation
if [ -f /run/splash_anim.pid ]; then
    kill $(cat /run/splash_anim.pid) 2>/dev/null || true
    rm -f /run/splash_anim.pid
fi

# Clear framebuffer to black before handoff
if [ -c /dev/fb0 ]; then
    dd if=/dev/zero of=/dev/fb0 2>/dev/null || true
fi
INIT_BOTTOM_EOF
    chmod +x /etc/initramfs-tools/scripts/init-bottom/splash_anim
    echo "  -> /etc/initramfs-tools/scripts/init-bottom/splash_anim"
    
    # Rebuild initramfs
    if [[ "$REBUILD" == true ]]; then
        echo ""
        echo -e "${GREEN}Rebuilding initramfs...${NC}"
        update-initramfs -u
        echo "  -> Done"
    else
        echo ""
        echo -e "${YELLOW}Skipping rebuild (--no-rebuild)${NC}"
        echo "Run manually: sudo update-initramfs -u"
    fi
    
    echo ""
    echo -e "${GREEN}=== Installation Complete (Standard Method) ===${NC}"
    echo ""
    echo "Files installed:"
    echo "  /sbin/splash_anim"
    echo "  /etc/initramfs-tools/hooks/splash_anim"
    echo "  /etc/initramfs-tools/scripts/init-top/splash_anim"
    echo "  /etc/initramfs-tools/scripts/init-bottom/splash_anim"
    echo ""
    echo "The splash will:"
    echo "  1. Start automatically after /dev is mounted"
    echo "  2. Stop before switch_root (clean handoff)"
    echo "  3. Persist across kernel updates"
    echo ""
    echo "No bootloader configuration required."
    echo "Reboot to see the splash animation."
}

# ========================================
# CUSTOM METHOD (Full custom initramfs)
# ========================================
install_custom() {
    echo -e "${YELLOW}=== Installing Custom Initramfs Method ===${NC}"
    echo ""
    echo -e "${RED}WARNING: This method requires manual configuration!${NC}"
    echo ""
    echo -e "${BLUE}This method:${NC}"
    echo "  • Creates a separate custom initramfs"
    echo "  • Requires bootloader configuration"
    echo "  • Requires YOU to write the init script"
    echo "  • NOT compatible with LUKS/LVM/mdadm automatically"
    echo ""
    
    # Validate initramfs directory
    if [[ -z "$INITRAMFS_DIR" ]]; then
        echo -e "${RED}ERROR: --initramfs-dir is required for custom method${NC}"
        echo ""
        echo "Example:"
        echo "  sudo ./splashboot_install.sh --method custom --initramfs-dir /boot/initramfs-custom"
        exit 1
    fi
    
    # Create directory structure
    mkdir -p "$INITRAMFS_DIR"/{sbin,dev,proc,sys,etc,run}
    
    # Copy binary
    echo -e "${GREEN}[1/3] Copying binary...${NC}"
    cp "$BINARY" "$INITRAMFS_DIR/sbin/splash_anim"
    chmod +x "$INITRAMFS_DIR/sbin/splash_anim"
    echo "  -> $INITRAMFS_DIR/sbin/splash_anim"
    
    # Create framebuffer device
    echo -e "${GREEN}[2/3] Creating device nodes...${NC}"
    if [[ ! -e "$INITRAMFS_DIR/dev/fb0" ]]; then
        mknod "$INITRAMFS_DIR/dev/fb0" c 29 0 2>/dev/null || true
    fi
    echo "  -> $INITRAMFS_DIR/dev/fb0"
    
    # Check for existing init
    INIT_SCRIPT="$INITRAMFS_DIR/init"
    
    if [[ -f "$INIT_SCRIPT" ]]; then
        echo -e "${GREEN}[3/3] Found existing init script${NC}"
        echo "  -> $INIT_SCRIPT"
        echo ""
        echo -e "${YELLOW}IMPORTANT: You must add the following to your init script:${NC}"
        echo ""
        echo "  # After mounting /dev:"
        echo "  /sbin/splash_anim &"
        echo "  SPLASH_PID=\$!"
        echo ""
        echo "  # Before switch_root:"
        echo "  kill \$SPLASH_PID"
        echo ""
    else
        echo -e "${YELLOW}[3/3] Creating minimal init script template${NC}"
        cat > "$INIT_SCRIPT" << 'INIT_EOF'
#!/bin/sh
# Minimal init script with bootsplash
# CUSTOMIZE THIS FOR YOUR SYSTEM

# Mount essential filesystems
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev 2>/dev/null || {
    # Fallback: create minimal /dev
    mknod /dev/console c 5 1
    mknod /dev/fb0 c 29 0
    mknod /dev/null c 1 3
}

# Start bootsplash
/sbin/splash_anim &
SPLASH_PID=$!

# --- CUSTOMIZE HERE ---
# Add your root mount logic:
# - Load modules (modprobe)
# - Mount root filesystem
# - Handle LUKS/LVM/mdadm if needed
#
# Example for simple root:
#   mount /dev/sda1 /mnt
#   kill $SPLASH_PID
#   exec switch_root /mnt /sbin/init
#
# Example for LUKS:
#   cryptsetup luksOpen /dev/sda2 cryptroot
#   mount /dev/mapper/cryptroot /mnt
#   kill $SPLASH_PID
#   exec switch_root /mnt /sbin/init
# -----------------------

# If no root mounted, drop to shell (for debugging)
echo "No root mounted. Dropping to shell."
exec /bin/sh
INIT_EOF
        chmod +x "$INIT_SCRIPT"
        echo "  -> $INIT_SCRIPT (template created)"
        echo ""
        echo -e "${YELLOW}IMPORTANT: Edit $INIT_SCRIPT and add your root mount logic!${NC}"
    fi
    
    # Build initramfs image
    if [[ "$REBUILD" == true ]]; then
        echo ""
        echo -e "${GREEN}Building initramfs image...${NC}"
        OUTPUT_IMG="${INITRAMFS_DIR}.img"
        
        (cd "$INITRAMFS_DIR" && find . | cpio -o -H newc 2>/dev/null | gzip -9) > "$OUTPUT_IMG"
        
        echo "  -> $OUTPUT_IMG"
        echo ""
        echo -e "${GREEN}=== Installation Complete (Custom Method) ===${NC}"
        echo ""
        echo "Next steps:"
        echo "  1. Edit $INIT_SCRIPT for your system"
        echo "  2. Configure bootloader to use $OUTPUT_IMG"
        echo ""
        echo "GRUB example:"
        echo "  Edit /etc/grub.d/40_custom:"
        echo "    menuentry 'Linux with Bootsplash' {"
        echo "      linux /vmlinuz root=/dev/sda1"
        echo "      initrd ${OUTPUT_IMG}"
        echo "    }"
        echo "  Then: sudo update-grub"
        echo ""
        echo "SYSLINUX example:"
        echo "  LABEL bootsplash"
        echo "    KERNEL vmlinuz"
        echo "    APPEND root=/dev/sda1 initrd=${OUTPUT_IMG}"
    else
        echo ""
        echo -e "${YELLOW}Skipping rebuild (--no-rebuild)${NC}"
    fi
}

# Main dispatch
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Bootsplash Animation Installer${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

case "$METHOD" in
    standard)
        install_standard
        ;;
    custom)
        install_custom
        ;;
esac
