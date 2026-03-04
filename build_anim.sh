#!/bin/bash
#
# build_anim.sh - Interactive splash builder for xbootsplash
# by seb3773 - https://github.com/seb3773
#
# Usage: ./build_anim.sh [options] [input]
#
# Display modes:
#   1 = Animation on solid background (default)
#   2 = Animation on background image (centered)
#   3 = Animation on background image (full screen)
#   4 = Static image on solid background
#   5 = Static image full screen
#
# Build options:
#   -h, --help       Show this help message
#   -m, --mode       Display mode (1-5)
#   -x, --offset-x   Horizontal offset from center (default: 0)
#   -y, --offset-y   Vertical offset from center (default: 80)
#   -d, --delay      Frame delay in ms (default: 33)
#   -c, --bg-color   Background color as RRGGBB hex (default: 000000)
#   -b, --bg-image   Background image for mode 1
#   -r, --resolution Target resolution WxH for full screen modes
#
# Install/Uninstall options:
#   --install-package <file.xbs>  Install from .xbs package file
#   --install-existing <binary>   Install existing binary (internal use)
#   --install-type <type>         Installation type: boot|shutdown|both
#   --uninstall-only              Uninstall current splash (internal use)
#   --uninstall-all               Uninstall ALL splash systems (internal use)
#
# Input:
#   - Animation modes: directory containing PNG frames
#   - Static modes: single PNG image file
#
# If no input specified, will prompt interactively.
#

set -e

RED='\033[31m'
GREEN='\033[38;5;46m'
YELLOW='\033[1;33m'
BLUE='\033[34m'
CYAN='\033[36m'
BOLD='\033[1m'
WHITE='\033[37m'
BLACK='\033[30m'
YELLOW_GRAD1='\033[38;5;226m'
YELLOW_GRAD2='\033[38;5;220m'
YELLOW_GRAD3='\033[38;5;214m'
YELLOW_GRAD4='\033[38;5;208m'
YELLOW_GRAD5='\033[38;5;202m'
BLUE_GRAD2='\033[38;5;45m'
BLUE_GRAD4='\033[38;5;33m'
BLUE_GRAD5='\033[38;5;27m'
NC='\033[0m' # No Color

BLACK_BG='\033[40m'
RED_BG='\033[41m'
GREEN_BG='\033[42m'
YELLOW_BG='\033[43m'
BLUE_BG='\033[44m'
MAGENTA_BG='\033[45m'
CYAN_BG='\033[46m'
GRAY_BG='\033[100m'
WHITE_BG='\033[47m'
BG_RESET='\033[49m'

# Default parameters
FRAME_DIR=""
FRAME_OFFSET_X=0
FRAME_OFFSET_Y=0
FRAME_DELAY=33
BINARY="xbootsplash"
COMPRESS_METHOD=""
DISPLAY_MODE=0
MODE_ARG=""
MODE_ARG_PROVIDED=0
BG_COLOR="000000"
BG_IMAGE=""
BG_OFFSET_X=0
BG_OFFSET_Y=0
TARGET_RES=""
LOOP_MODE=1  # 0=no loop, 1=full loop, 2=partial loop
LOOP_START=0  # Start frame for partial loop

# If build-related CLI args are provided, we can bypass the interactive menu.
NONINTERACTIVE_BUILD=0

# DRM vs fbdev mode (can be overridden via environment: USE_DRM=1 ./build_anim.sh)
USE_DRM_EXPLICIT=0
if [ -n "${USE_DRM+x}" ]; then
    USE_DRM_EXPLICIT=1
fi
USE_DRM="${USE_DRM:-0}"  # 0=fbdev, 1=DRM
LIBDRM_AVAILABLE=0

# Screen dimensions (detected or specified)
SCREEN_W=0
SCREEN_H=0
OBJECT_W=0
OBJECT_H=0

# Check for libdrm availability
check_libdrm() {
    if pkg-config --exists libdrm 2>/dev/null; then
        LIBDRM_AVAILABLE=1
        return 0
    else
        LIBDRM_AVAILABLE=0
        return 1
    fi
}

# Show help
show_help() {
    sed -n '2,/^$/p' "$0" | sed 's/^# //'
    exit 0
}

# Parse options
INSTALL_EXISTING=""
UNINSTALL_ONLY=""
INSTALL_PACKAGE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            ;;
        --install-package)
            # Install from .xbs package file
            INSTALL_PACKAGE="$2"
            shift 2
            ;;
        --install-existing)
            # Internal: install existing binary (called with sudo)
            INSTALL_EXISTING="$2"
            shift 2
            ;;
        --install-type)
            # Internal: preserve installation type across sudo re-exec
            INSTALL_TYPE="$2"
            shift 2
            ;;
        --uninstall-only)
            # Internal: uninstall only (called with sudo)
            UNINSTALL_ONLY="1"
            shift
            ;;
        --uninstall-all)
            # Internal: uninstall all splash systems (called with sudo)
            UNINSTALL_ALL="1"
            shift
            ;;
        -m|--mode)
            MODE_ARG="$2"
            MODE_ARG_PROVIDED=1
            NONINTERACTIVE_BUILD=1
            shift 2
            ;;
        -x|--offset-x)
            FRAME_OFFSET_X="$2"
            NONINTERACTIVE_BUILD=1
            shift 2
            ;;
        -y|--offset-y)
            FRAME_OFFSET_Y="$2"
            NONINTERACTIVE_BUILD=1
            shift 2
            ;;
        -o|--offset)
            FRAME_OFFSET_Y="$2"
            NONINTERACTIVE_BUILD=1
            shift 2
            ;;
        -d|--delay)
            FRAME_DELAY="$2"
            NONINTERACTIVE_BUILD=1
            shift 2
            ;;
        -n|--binary)
            BINARY="$2"
            NONINTERACTIVE_BUILD=1
            shift 2
            ;;
        -c|--bg-color)
            BG_COLOR="$2"
            NONINTERACTIVE_BUILD=1
            shift 2
            ;;
        -b|--bg-image)
            BG_IMAGE="$2"
            NONINTERACTIVE_BUILD=1
            shift 2
            ;;
        -X|--bg-offset-x)
            BG_OFFSET_X="$2"
            NONINTERACTIVE_BUILD=1
            shift 2
            ;;
        -Y|--bg-offset-y)
            BG_OFFSET_Y="$2"
            NONINTERACTIVE_BUILD=1
            shift 2
            ;;
        -l|--loop)
            LOOP_MODE="$2"
            NONINTERACTIVE_BUILD=1
            shift 2
            ;;
        -L|--loop-start)
            LOOP_START="$2"
            NONINTERACTIVE_BUILD=1
            shift 2
            ;;
        -r|--resolution)
            TARGET_RES="$2"
            NONINTERACTIVE_BUILD=1
            shift 2
            ;;
        -*)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Run: $0 --help"
            exit 1
            ;;
        *)
            FRAME_DIR="$1"
            NONINTERACTIVE_BUILD=1
            shift
            ;;
    esac
done

# CLI mode harmonization:
# - Accept menu numbering (1-5) and map to internal generator numbering (0-4)
# - Keep backward compatibility with old 0-4 values
if [[ $MODE_ARG_PROVIDED -eq 1 ]]; then
    if [[ "$MODE_ARG" =~ ^[0-9]+$ ]]; then
        if [[ "$MODE_ARG" -ge 1 && "$MODE_ARG" -le 5 ]]; then
            DISPLAY_MODE=$((MODE_ARG - 1))
        elif [[ "$MODE_ARG" -ge 0 && "$MODE_ARG" -le 4 ]]; then
            DISPLAY_MODE="$MODE_ARG"
        else
            echo -e "${RED}ERROR: Invalid mode: $MODE_ARG${NC}"
            echo "Allowed: 1-5 (see --help for mode descriptions)"
            exit 1
        fi
    else
        echo -e "${RED}ERROR: Invalid mode: $MODE_ARG${NC}"
        echo "Allowed: 1-5 (see --help for mode descriptions)"
        exit 1
    fi
fi

# Minimal non-interactive build runner (no prompts)
run_noninteractive_build() {
    # Expand path
    FRAME_DIR="${FRAME_DIR/#\~/$HOME}"

    # Validate input based on mode
    if [ "$DISPLAY_MODE" -eq 3 ] || [ "$DISPLAY_MODE" -eq 4 ]; then
        if [ ! -f "$FRAME_DIR" ]; then
            print_error "Image file not found: $FRAME_DIR"
            exit 1
        fi
    else
        if [ ! -d "$FRAME_DIR" ]; then
            print_error "Directory not found: $FRAME_DIR"
            exit 1
        fi
        if ! analyze_frames "$FRAME_DIR"; then
            print_error "Frame analysis failed"
            exit 1
        fi
    fi

    # Validate background image requirements
    if [ "$DISPLAY_MODE" -eq 1 ] || [ "$DISPLAY_MODE" -eq 2 ]; then
        if [ -z "$BG_IMAGE" ] || [ ! -f "$BG_IMAGE" ]; then
            print_error "Background image not found: $BG_IMAGE"
            exit 1
        fi
    fi

    build_animation
    echo -e "\n${GREEN}${BOLD}✓ All done!${NC}\n"
}

# Print functions
print_header() {
  echo -e "${CYAN}${BOLD}  ╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}${BOLD}  ║${NC}${YELLOW_GRAD1}          ___             _    ___       _           _        ${CYAN}${BOLD}║${NC}"
    echo -e "${CYAN}${BOLD}  ║${NC}${YELLOW_GRAD2}     __  | . > ___  ___ _| |_ / __> ___ | | ___  ___| |_      ${CYAN}${BOLD}║${NC}"
    echo -e "${CYAN}${BOLD}  ║${NC}${YELLOW_GRAD3}     \\ \\/| . \\/ . \\/ . \\ | |  \\__ \\| . \\| |<_> |<_-<| . |     ${CYAN}${BOLD}║${NC}"
    echo -e "${CYAN}${BOLD}  ║${NC}${YELLOW_GRAD4}     /\\_\\|___/\\___/\\___/ |_|  <___/|  _/|_|<___|/__/|_|_|     ${CYAN}${BOLD}║${NC}"
    echo -e "${CYAN}${BOLD}  ║${NC}${BLUE_GRAD4}         SPLASH ANIMATION BUILDER${YELLOW_GRAD5}  |_| ${BLUE_GRAD4}by seb3773 - ${BLUE_GRAD2}v1        ${CYAN}${BOLD}║${NC}"
    echo -e "${CYAN}${BOLD}  ╚══════════════════════════════════════════════════════════════╝${NC}"
}

print_step() {
    echo -e "\n  ${BLUE}${BOLD}━━━ ${WHITE}STEP ${BOLD}$1${BLUE} ━━━━━━━━━━━━━━━━━━━━━━━━━━ ${YELLOW}$2${NC}"
}
print_success() {
    echo -e "    ${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "   ${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${CYAN}  $1${NC}"
}

print_warning() {
    echo -e "   ${YELLOW}⚠ $1${NC}"
}

ask_continue() {
    echo -e "\n${BOLD}$1${NC} ${YELLOW}[${CYAN}Y${YELLOW}/n]${NC}"
    read -r response
    case "$response" in
        [nN][oO]|[nN])
            echo -e "\n${YELLOW}✖ Operation cancelled by user ✖${NC}"
            exit 0
            ;;
        *)
            return 0
            ;;
    esac
}

# Detect screen dimensions
detect_screen_size() {
    if [ -f /sys/class/graphics/fb0/virtual_size ]; then
        local fb_size=$(cat /sys/class/graphics/fb0/virtual_size)
        SCREEN_W=$(echo "$fb_size" | cut -d',' -f1)
        SCREEN_H=$(echo "$fb_size" | cut -d',' -f2)
    elif [ -n "$TARGET_RES" ]; then
        SCREEN_W=$(echo "$TARGET_RES" | cut -d'x' -f1)
        SCREEN_H=$(echo "$TARGET_RES" | cut -d'x' -f2)
    else
        # Default fallback
        SCREEN_W=1920
        SCREEN_H=1080
    fi
}

# Get object dimensions (animation frame or static image)
get_object_size() {
    local file="$1"
    if [ -f "$file" ]; then
        local size=$(identify -format "%wx%h" "$file" 2>/dev/null | head -1) || return 1
        if [ -n "$size" ]; then
            OBJECT_W=$(echo "$size" | cut -d'x' -f1)
            OBJECT_H=$(echo "$size" | cut -d'x' -f2)
        fi
    fi
}

# Calculate valid offset range
calc_offset_range() {
    local obj_w="$1"
    local obj_h="$2"
    
    if [ "$SCREEN_W" -gt 0 ] && [ "$obj_w" -gt 0 ]; then
        local max_x=$((SCREEN_W - obj_w))
        local max_y=$((SCREEN_H - obj_h))
        echo "0-$max_x"
    fi
}

# Ask yes/no without exiting on no (returns 0 for yes, 1 for no)
ask_yes_no() {
    echo -e "\n➤${YELLOW} ${BOLD}$1${NC} ${YELLOW}[${CYAN}Y${YELLOW}/n]${NC}:"
    read -r response
    case "$response" in
        [nN][oO]|[nN])
            return 1
            ;;
        *)
            return 0
            ;;
    esac
}

# Detect if running under GUI (X11/Wayland) or pure TTY
is_gui_session() {
    # Check for X11 or Wayland session
    if [ -n "$DISPLAY" ] || [ -n "$WAYLAND_DISPLAY" ]; then
        return 0
    fi
    # Check if $XDG_SESSION_TYPE is set to x11 or wayland
    if [ "$XDG_SESSION_TYPE" = "x11" ] || [ "$XDG_SESSION_TYPE" = "wayland" ]; then
        return 0
    fi
    return 1
}

# Detect initramfs system
detect_initramfs_system() {
    print_info "  ${WHITE}☉${CYAN} Detecting initramfs system..."
    
    local initramfs_type=""
    local initramfs_valid=false
    
    # Check for initramfs-tools (Debian/Ubuntu)
    if [ -d /etc/initramfs-tools ]; then
        initramfs_type="initramfs-tools"
        initramfs_valid=true
        print_success "Detected: initramfs-tools (Debian/Ubuntu)"
        print_info "  ❯ Standard installation method available"
    # Check for dracut (Fedora/RHEL/Arch)
    elif [ -d /etc/dracut.conf ] || command -v dracut &>/dev/null; then
        initramfs_type="dracut"
        initramfs_valid=true
        print_success "Detected: dracut (Fedora/RHEL/Arch)"
        print_info "  ❯ Custom installation method required"
    # Check for mkinitcpio (Arch)
    elif [ -f /etc/mkinitcpio.conf ]; then
        initramfs_type="mkinitcpio"
        initramfs_valid=true
        print_success "Detected: mkinitcpio (Arch Linux)"
        print_info "  ❯ Custom installation method required"
    # Check for mkinitfs (Alpine)
    elif [ -f /etc/mkinitfs/mkinitfs.conf ]; then
        initramfs_type="mkinitfs"
        initramfs_valid=true
        print_success "Detected: mkinitfs (Alpine)"
        print_info "  ❯ Custom installation method required"
    # Check for genkernel (Gentoo)
    elif command -v genkernel &>/dev/null; then
        initramfs_type="genkernel"
        initramfs_valid=true
        print_success "Detected: genkernel (Gentoo)"
        print_info "  ❯ Custom installation method required"
    else
        print_warning "No standard initramfs system detected."
        print_info "  ❯ Custom installation method will be required"
        initramfs_type="custom"
        initramfs_valid=true
    fi
    
    # Check for framebuffer device
    print_info "  ${WHITE}☉${CYAN} Checking framebuffer..."
    if [ -e /dev/fb0 ]; then
        print_success "/dev/fb0 found"
    else
        print_warning "/dev/fb0 not found (may not be available in current environment)"
        print_info "  Framebuffer typically available during boot via efifb/vesafb"
    fi
    
    # Export for later use
    INITRAMFS_TYPE="$initramfs_type"
    INITRAMFS_VALID="$initramfs_valid"
}

# Check kernel cmdline for quiet boot options
check_kernel_cmdline() {
    print_info "  ${WHITE}☉${CYAN} Checking kernel command line..."
    
    local cmdline=""
    if [ -f /proc/cmdline ]; then
        cmdline=$(cat /proc/cmdline)
    fi
    
    # Recommended options for clean boot splash
    local recommended_opts="quiet splash loglevel=0 vt.cur_default=1 systemd.show_status=0 rd.udev.log_level=0"
    local missing_opts=()
    
    # Check each recommended option
    for opt in quiet splash loglevel=0 vt.cur_default=1; do
        if ! echo "$cmdline" | grep -qw "$opt"; then
            missing_opts+=("$opt")
        fi
    done
    
    # Check systemd options (only if systemd is running)
    if pidof systemd &>/dev/null; then
        for opt in systemd.show_status=0 rd.udev.log_level=0; do
            if ! echo "$cmdline" | grep -qw "$opt"; then
                missing_opts+=("$opt")
            fi
        done
    fi
    
    if [ ${#missing_opts[@]} -eq 0 ]; then
        print_success "Kernel cmdline already configured for quiet boot"
        return 0
    fi
    
    print_warning "Kernel cmdline missing quiet boot options:"
    for opt in "${missing_opts[@]}"; do
        print_info "  - $opt"
    done
    
    echo ""
    echo -e "${YELLOW}These options hide boot messages that would appear over the splash:${NC}"
    print_info "  quiet             - Suppress most kernel messages"
    print_info "  splash            - Indicate splash screen in use"
    print_info "  loglevel=0        - Only critical kernel messages"
    print_info "  vt.cur_default=1  - Hide cursor on VT"
    print_info "  systemd.show_status=0 - Hide systemd status"
    print_info "  rd.udev.log_level=0   - Hide udev messages"
    
    echo ""
    echo -e "${YELLOW}✜ Recommended full cmdline addition:${NC}"
    print_info "  quiet splash loglevel=0 vt.cur_default=1 systemd.show_status=0 rd.udev.log_level=0"
    
    echo ""
    if ask_continue "Show how to add these options to GRUB?"; then
        show_grub_config_guide
    fi
}

# Show GRUB configuration guide
show_grub_config_guide() {
    echo ""
    echo -e "${CYAN}${BOLD}══════════════════════════════════════════════════════════════${NC}"
    echo -e "${CYAN}${BOLD}           GRUB CONFIGURATION GUIDE                              ${NC}"
    echo -e "${CYAN}${BOLD}══════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "${GREEN}Option 1: Edit /etc/default/grub${NC}"
    echo ""
    echo "  1. Edit the file:"
    echo "     sudo nano /etc/default/grub"
    echo ""
    echo "  2. Find the GRUB_CMDLINE_LINUX_DEFAULT line and add options:"
    echo "     GRUB_CMDLINE_LINUX_DEFAULT=\"quiet splash loglevel=0 vt.cur_default=1 systemd.show_status=0\""
    echo ""
    echo "  3. Update GRUB:"
    echo "     sudo update-grub"
    echo ""
    echo -e "${GREEN}Option 2: Add to existing cmdline${NC}"
    echo ""
    echo "  If you already have options, append to them:"
    echo "  GRUB_CMDLINE_LINUX_DEFAULT=\"quiet splash loglevel=0 vt.cur_default=1 systemd.show_status=0 rd.udev.log_level=0\""
    echo ""
    echo -e "${YELLOW}Note about fbcon=nodefer:${NC}"
    echo "  This option is NOT recommended - it can cause issues with some"
    echo "  graphics drivers. The splash will work without it."
    echo ""
    echo -e "${CYAN}${BOLD}══════════════════════════════════════════════════════════════${NC}"
}

# Check dependencies
check_dependencies() {
    print_step "0" " Checking dependencies"
    
    # Detect initramfs system first
    detect_initramfs_system
    
    # Check kernel cmdline for quiet boot
    check_kernel_cmdline
    
    local missing=()
    local missing_pkgs=()
    
    # Essential build tools
    if ! command -v gcc &> /dev/null; then
        missing+=("gcc")
        missing_pkgs+=("gcc")
    fi
    
    if ! command -v ld &> /dev/null && ! command -v ld.bfd &> /dev/null && ! command -v ld.gold &> /dev/null; then
        missing+=("linker (ld)")
        missing_pkgs+=("binutils")
    fi
    
    if ! command -v make &> /dev/null; then
        missing+=("make")
        missing_pkgs+=("make")
    fi
    
    # ImageMagick for PNG conversion
    if ! command -v convert &> /dev/null; then
        missing+=("ImageMagick (convert)")
        missing_pkgs+=("imagemagick")
    fi
    
    if ! command -v identify &> /dev/null; then
        missing+=("ImageMagick (identify)")
        if [[ ! " ${missing_pkgs[@]} " =~ " imagemagick " ]]; then
            missing_pkgs+=("imagemagick")
        fi
    fi
    
    # libpng for generator (cross-distro: use ld check instead of dpkg)
    if ! ld -lpng -o /dev/null 2>/dev/null; then
        missing+=("libpng-dev")
        missing_pkgs+=("libpng-dev")
    fi
    
    # Check for missing essential dependencies
    if [ ${#missing[@]} -gt 0 ]; then
        print_error "✘ Missing essential dependencies:"
        for dep in "${missing[@]}"; do
            print_info "  - $dep"
        done
        
        echo -e "\n${YELLOW}Required packages:${NC} ${missing_pkgs[*]}"
        
        if ask_continue "Install missing dependencies now?"; then
            print_info "Installing dependencies..."
            
            # Detect package manager
            if command -v apt &> /dev/null; then
                sudo apt update
                sudo apt install -y "${missing_pkgs[@]}"
            elif command -v dnf &> /dev/null; then
                sudo dnf install -y "${missing_pkgs[@]}"
            elif command -v pacman &> /dev/null; then
                sudo pacman -S --noconfirm "${missing_pkgs[@]}"
            else
                print_error "✖ No supported package manager found"
                print_info "Please install manually: ${missing_pkgs[*]}"
                exit 1
            fi
            
            # Verify installation
            local still_missing=()
            for dep in "${missing[@]}"; do
                case "$dep" in
                    gcc) command -v gcc &> /dev/null || still_missing+=("$dep") ;;
                    linker*) (command -v ld || command -v ld.bfd || command -v ld.gold) &> /dev/null || still_missing+=("$dep") ;;
                    make) command -v make &> /dev/null || still_missing+=("$dep") ;;
                    ImageMagick*|convert) command -v convert &> /dev/null || still_missing+=("$dep") ;;
                    identify) command -v identify &> /dev/null || still_missing+=("$dep") ;;
                    libpng-dev) ld -lpng -o /dev/null 2>/dev/null || still_missing+=("$dep") ;;
                    *) still_missing+=("$dep") ;;
                esac
            done
            
            if [ ${#still_missing[@]} -gt 0 ]; then
                print_error "✖ Failed to install: ${still_missing[*]}"
                exit 1
            fi
            
            print_success "Dependencies installed"
        else
            print_error "✖✖ Cannot continue without dependencies"
            exit 1
        fi
    else
        print_success "All essential dependencies found"
    fi
    
    # Optional: sstrip for smaller binary
    check_sstrip
}

# Check and offer to install sstrip (optional)
check_sstrip() {
    print_info "  ${WHITE}☉${CYAN} Checking for sstrip (optional, reduces binary size)..."
    
    if command -v sstrip &> /dev/null; then
        print_success "sstrip found"
        return 0
    fi
    
    # Check common locations
    if [ -x "/usr/local/bin/sstrip" ] || [ -x "$HOME/.local/bin/sstrip" ]; then
        print_success "sstrip found"
        return 0
    fi
    
    print_warning "sstrip not found"
    print_info "sstrip can reduce binary size by stripping additional sections"
    print_info "Source: https://github.com/aunali1/super-strip"
    
    if ask_continue "Install sstrip now? (recommended for production)"; then
        install_sstrip
    else
        print_info "Continuing without sstrip (binary will be slightly larger)"
    fi
}

# Install sstrip
install_sstrip() {
    local tmpdir=$(mktemp -d)
    local sstrip_bin=""
    
    print_info "⚙ Downloading and building sstrip..."
    
    # Check for git or wget
    if command -v git &> /dev/null; then
        git clone https://github.com/aunali1/super-strip.git "$tmpdir/super-strip" 2>/dev/null || true
        sstrip_bin="$tmpdir/super-strip/sstrip"
    elif command -v wget &> /dev/null; then
        # Download prebuilt if available, or source
        wget -q "https://github.com/aunali1/super-strip/archive/refs/heads/master.tar.gz" -O "$tmpdir/sstrip.tar.gz" 2>/dev/null || true
        tar -xzf "$tmpdir/sstrip.tar.gz" -C "$tmpdir" 2>/dev/null || true
        sstrip_bin="$tmpdir/super-strip-master/sstrip"
    elif command -v curl &> /dev/null; then
        curl -sL "https://github.com/aunali1/super-strip/archive/refs/heads/master.tar.gz" -o "$tmpdir/sstrip.tar.gz" 2>/dev/null || true
        tar -xzf "$tmpdir/sstrip.tar.gz" -C "$tmpdir" 2>/dev/null || true
        sstrip_bin="$tmpdir/super-strip-master/sstrip"
    else
        print_error "Need git, wget, or curl to download sstrip"
        print_info "Install manually from: https://github.com/aunali1/super-strip"
        rm -rf "$tmpdir"
        return 1
    fi
    
    if [ -d "$tmpdir/super-strip" ] || [ -d "$tmpdir/super-strip-master" ]; then
        local srcdir="$tmpdir/super-strip"
        [ -d "$tmpdir/super-strip-master" ] && srcdir="$tmpdir/super-strip-master"
        
        # Build sstrip in subshell to preserve working directory
        (
            cd "$srcdir" || exit 1
            if make 2>/dev/null; then
                # Install to /usr/local/bin
                sudo mkdir -p /usr/local/bin
                sudo cp sstrip /usr/local/bin/sstrip
                sudo chmod +x /usr/local/bin/sstrip
                
                # Verify
                if command -v sstrip &> /dev/null || [ -x "/usr/local/bin/sstrip" ]; then
                    echo "SUCCESS: sstrip installed to /usr/local/bin/sstrip"
                else
                    echo "WARNING: sstrip built but installation failed"
                fi
            else
                echo "ERROR: Failed to build sstrip"
                echo "INFO: You may need to install build-essential or base-devel"
                exit 1
            fi
        )
        local build_result=$?
        
        if [ $build_result -eq 0 ]; then
            print_success "sstrip installed to /usr/local/bin/sstrip"
        else
            print_error "Failed to build sstrip"
            print_info "You may need to install build-essential or base-devel"
        fi
    else
        print_error "Failed to download sstrip source"
    fi
    
    rm -rf "$tmpdir"
}

# Analyze frames directory
analyze_frames() {
    local dir="$1"
    echo ""
    print_step "3" "Analyzing frames directory"
    print_info "Directory: $dir"
    
    # Find image files (PNG, JPG, JPEG)
    local img_count=$(find "$dir" -maxdepth 1 -type f \( -name "*.png" -o -name "*.PNG" -o -name "*.jpg" -o -name "*.JPG" -o -name "*.jpeg" -o -name "*.JPEG" \) 2>/dev/null | wc -l)
    
    if [ "$img_count" -eq 0 ]; then
        print_warning "No PNG/JPG images found in $dir"
        return 1
    fi
    
    print_success "Found $img_count image files"
    
    # Extract frame indices and detect pattern
    print_info "☉ Detecting frame numbering pattern..."
    
    local first_frame=""
    local last_frame=""
    local first_index=999999
    local last_index=-1
    local frame_size=""
    
    # Detect frame numbering pattern intelligently
    local first_basename=""
    while IFS= read -r file; do
        local basename=$(basename "$file")
        if [ -z "$first_basename" ]; then
            first_basename="$basename"
            first_index=$(echo "$basename" | grep -oP '\d+' | head -n 1)
            first_frame="$basename"
        else
            # Find which number changed compared to the first frame
            local current_nums=($(echo "$basename" | grep -oP '\d+'))
            local first_nums=($(echo "$first_basename" | grep -oP '\d+'))
            
            local found_diff=0
            for i in "${!current_nums[@]}"; do
                if [ "${current_nums[$i]}" != "${first_nums[$i]}" ]; then
                    local index=$((10#${current_nums[$i]}))
                    [ $index -lt $first_index ] && first_index=$index && first_frame="$basename"
                    [ $index -gt $last_index ] && last_index=$index && last_frame="$basename"
                    found_diff=1
                    break
                fi
            done
            
            # Fallback to last number if no difference found in existing positions
            if [ $found_diff -eq 0 ]; then
                local nums=$(echo "$basename" | grep -oE '[0-9]+' | tail -1)
                if [ -n "$nums" ]; then
                    local index=$((10#$nums))
                    [ $index -lt $first_index ] && first_index=$index && first_frame="$basename"
                    [ $index -gt $last_index ] && last_index=$index && last_frame="$basename"
                fi
            fi
        fi
        
        # Get image size from first file with valid image
        if [ -z "$frame_size" ] && identify "$file" &>/dev/null; then
            frame_size=$(identify -format "%wx%h" "$file" 2>/dev/null) || true
        fi
        
    done < <(find "$dir" -maxdepth 1 -type f \( -name "*.png" -o -name "*.PNG" -o -name "*.jpg" -o -name "*.JPG" -o -name "*.jpeg" -o -name "*.JPEG" \) 2>/dev/null | sort)
    
    # Store frame dimensions globally for summary
    if [ -n "$frame_size" ]; then
        FRAME_W=$(echo "$frame_size" | cut -d'x' -f1)
        FRAME_H=$(echo "$frame_size" | cut -d'x' -f2)
    fi
    
    if [ $last_index -lt 0 ]; then
        print_error "Could not detect frame numbering pattern"
        print_warning "Filenames should contain numbers (e.g., frame_00.png, spin001.jpg)"
        return 1
    fi
    
    # Validate filename consistency - all frames should follow the same pattern
    local inconsistent_files=()
    local pattern_prefix=$(echo "$first_frame" | sed 's/[0-9]/X/g' | sed 's/X.*//')
    local pattern_suffix=$(echo "$first_frame" | sed 's/[0-9]/X/g' | sed 's/.*X//')
    
    while IFS= read -r file; do
        local basename=$(basename "$file")
        local check_prefix=$(echo "$basename" | sed 's/[0-9]/X/g' | sed 's/X.*//')
        local check_suffix=$(echo "$basename" | sed 's/[0-9]/X/g' | sed 's/.*X//')
        
        # Check if prefix/suffix pattern matches (allowing for different number lengths)
        if [ "$check_prefix" != "$pattern_prefix" ] || [ "$check_suffix" != "$pattern_suffix" ]; then
            inconsistent_files+=("$basename")
        fi
    done < <(find "$dir" -maxdepth 1 -type f \( -name "*.png" -o -name "*.PNG" -o -name "*.jpg" -o -name "*.JPG" -o -name "*.jpeg" -o -name "*.JPEG" \) 2>/dev/null | sort)
    
    if [ ${#inconsistent_files[@]} -gt 0 ]; then
        print_error "Inconsistent filename pattern detected!"
        echo -e "  ${YELLOW}Expected pattern:${NC} ${pattern_prefix}<number>${pattern_suffix}"
        echo -e "  ${YELLOW}Inconsistent files:${NC}"
        for f in "${inconsistent_files[@]:0:5}"; do
            echo -e "    ${RED}✗ $f${NC}"
        done
        if [ ${#inconsistent_files[@]} -gt 5 ]; then
            echo -e "    ${YELLOW}... and $((${#inconsistent_files[@]} - 5)) more${NC}"
        fi
        echo -e "  ${CYAN}All frames must follow the same naming convention.${NC}"
        print_warning "Please rename or remove inconsistent files, then select a directory again."
        return 1
    fi
    
    # Display analysis results
    echo -e "     ${BOLD}${GREEN}════════════════════════════════════════════════════════════════${NC}"
    echo -e "     ${BOLD}${GREEN}                     ANALYSIS RESULTS                           ${NC}"
    echo -e "     ${BOLD}${GREEN}════════════════════════════════════════════════════════════════${NC}"
    echo -e "     ${CYAN}Total frames:${NC}    $img_count"
    echo -e "     ${CYAN}Frame size:${NC}      $frame_size pixels"
    echo -e "     ${CYAN}First frame:${NC}     $first_frame (index $first_index)"
    echo -e "     ${CYAN}Last frame:${NC}      $last_frame (index $last_index)"
    echo -e "     ${CYAN}Index range:${NC}     $first_index → $last_index"
    echo -e "     ${BOLD}${GREEN}════════════════════════════════════════════════════════════════${NC}"
    
    # Check for missing frames
    local expected_count=$((last_index - first_index + 1))
    if [ $expected_count -ne $img_count ]; then
        print_warning "Frame count mismatch: expected $expected_count, found $img_count"
        print_warning "Some frame indices may be missing"
    fi
    
    # Store for later
    FRAME_DIR="$dir"
}

# Select display mode
select_mode() {
    echo ""
    print_step "2" "Select splash mode"
    echo -e "   ╭──────────────────────────────────────────────────────────────────────────────╮"
    echo -e "   │  ${CYAN}1)${NC} Animation on solid background (default)                                  │"
    echo -e "   │      ${YELLOW}→ Animation frames on uniform color background${NC}                          │"
    echo -e "   │  ${CYAN}2)${NC} Animation on background image                                            │"
    echo -e "   │      ${YELLOW}→ Animation frames overlaid on a static image centered on uniform color${NC} │"
    echo -e "   │  ${CYAN}3)${NC} Animation on background image (full screen)                              │"
    echo -e "   │      ${YELLOW}→ Animation frames overlaid on a full-screen image${NC}                      │"
    echo -e "   │  ${CYAN}4)${NC} Static image on solid background                                         │"
    echo -e "   │      ${YELLOW}→ Single static image centered on uniform color${NC}                         │"
    echo -e "   │  ${CYAN}5)${NC} Static image full screen                                                 │"
    echo -e "   │      ${YELLOW}→ Single static image filling the screen${NC}                                │"
    echo -e "   ╰──────────────────────────────────────────────────────────────────────────────╯"
    echo -en "   ${YELLOW}➤ Select mode [${CYAN}1${YELLOW}]: ${NC}"
    read -r mode_choice
    
    case "$mode_choice" in
        2) DISPLAY_MODE=1 ;;
        3) DISPLAY_MODE=2 ;;
        4) DISPLAY_MODE=3 ;;
        5) DISPLAY_MODE=4 ;;
        *) DISPLAY_MODE=0 ;;
    esac
    
    local mode_names=("Animation on solid background" "Animation on background image (centered)" 
                      "Animation on background image (fullscreen)" "Static image on solid background" 
                      "Static image full screen")
    print_info "Selected: [${mode_names[$DISPLAY_MODE]}]"
    
    # Mode-specific prompts
    # Modes 1 and 2: need background image
    if [ $DISPLAY_MODE -eq 1 ] || [ $DISPLAY_MODE -eq 2 ]; then
        if [ -z "$BG_IMAGE" ]; then
            echo -e "\n${YELLOW}Enter background image path (PNG/JPG): ${NC}"
            read -r BG_IMAGE
            if [ ! -f "$BG_IMAGE" ]; then
                print_error "Background image not found: $BG_IMAGE"
                exit 1
            fi
        fi
    fi
    
    # Mode 2 (fullscreen anim): target resolution for background
    if [ $DISPLAY_MODE -eq 2 ] && [ -z "$TARGET_RES" ]; then
        echo -e "${YELLOW}Target resolution (e.g., 1920x1080) or ENTER for auto-detect: ${NC}"
        read -r TARGET_RES
        
        if [ -z "$TARGET_RES" ]; then
            if [ -f /sys/class/graphics/fb0/virtual_size ]; then
                local fb_size=$(cat /sys/class/graphics/fb0/virtual_size)
                TARGET_RES="${fb_size/,/x}"
                print_info "Detected framebuffer: $TARGET_RES"
            fi
        fi
    fi
    
    # Mode 4 (static fullscreen): target resolution
    if [ $DISPLAY_MODE -eq 4 ] && [ -z "$TARGET_RES" ]; then
        echo -e "${YELLOW}Target resolution (e.g., 1920x1080) or ENTER for auto-detect: ${NC}"
        read -r TARGET_RES
        
        if [ -z "$TARGET_RES" ]; then
            if [ -f /sys/class/graphics/fb0/virtual_size ]; then
                local fb_size=$(cat /sys/class/graphics/fb0/virtual_size)
                TARGET_RES="${fb_size/,/x}"
                print_info "Detected framebuffer: $TARGET_RES"
            fi
        fi
    fi
}

# Get build parameters
get_parameters() {
    print_step "4" "Configure display parameters"
    
    # Show mode-specific diagram
    case $DISPLAY_MODE in
        0)
            echo -e "${CYAN}"
            echo "        Animation on solid background"
            echo "   ┌─────────────────────────────────────┐"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}             (center)                ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}            ${BLACK}↓ offset_y${CYAN}               ${BG_RESET}│"
            echo -e "   │${GRAY_BG}    ┌───────────┐${BLACK}← offset_x${CYAN}          ${BG_RESET}│"
            echo -e "   │${GRAY_BG}    │${CYAN_BG}${WHITE} Animation ${BG_RESET}${CYAN}${GRAY_BG}│                    ${BG_RESET}${CYAN}│"
            echo -e "   │${GRAY_BG}    └───────────┘                    ${BG_RESET}│"
            echo -e "   │${GRAY_BG}         ${YELLOW}#Background color${CYAN}${GRAY_BG}           ${BG_RESET}${CYAN}│"
            echo -e "   └─────────────────────────────────────┘"
            echo -e "${NC}"
            ;;
        1)
            echo -e "${CYAN}"
            echo "        Animation on background image"
            echo "   ┌──────────────────────────────────────┐"
            echo -e "   │${GRAY_BG}                                      ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                      ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                   ${BLACK}image offset_y${CYAN}     ${BG_RESET}│"
            echo -e "   │${GRAY_BG} ${BLACK}image offset_x${CYAN}    ${BLACK}↓${CYAN}                  ${BG_RESET}│"
            echo -e "   │${GRAY_BG}      ${BLACK}→${CYAN}┌────────────────────────────┐ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}       │${GREEN_BG}Background image            ${BG_RESET}${GRAY_BG}│ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}       │${GREEN_BG}           ${BLACK}↓${CYAN}                ${BG_RESET}${GRAY_BG}│ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}       │${GREEN_BG}       (center)             ${BG_RESET}${GRAY_BG}│ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}       │${GREEN_BG}           ${BLACK}↓offset_y${CYAN}        ${BG_RESET}${GRAY_BG}│ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}       │${GREEN_BG}    ┌─────────┐             ${BG_RESET}${GRAY_BG}│ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}       │${GREEN_BG}    │${CYAN_BG}${WHITE}Animation${BG_RESET}${CYAN}${GREEN_BG}│             ${BG_RESET}${GRAY_BG}│ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}       │${GREEN_BG}   ${BLACK}→${CYAN}└─────────┘             ${BG_RESET}${GRAY_BG}│ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}       │${GREEN_BG}${BLACK}offset_x${CYAN}                    ${BG_RESET}${GRAY_BG}│ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}       └────────────────────────────┘ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                    #Background color ${BG_RESET}│"
            echo -e "   └──────────────────────────────────────┘"
            echo -e "${NC}"
            ;;
        2)
            echo -e "${CYAN}"
            echo "   Animation on full screen background image"
            echo "   ┌─────────────────────────────────────┐"
            echo -e "   │${GRAY_BG}      Background image (full)        ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}             (center)                ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                ${BLACK}↓offset_y${CYAN}            ${BG_RESET}│"
            echo -e "   │${GRAY_BG}        ┌─────────┐                  ${BG_RESET}│"
            echo -e "   │${GRAY_BG}        │${CYAN_BG}${WHITE}Animation${BG_RESET}${CYAN}${GRAY_BG}│                  ${BG_RESET}│"
            echo -e "   │${GRAY_BG}       ${BLACK}→${CYAN}└─────────┘                  ${BG_RESET}│"
            echo -e "   │${GRAY_BG}${BLACK}offset_x${CYAN}                             ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   └─────────────────────────────────────┘"
            echo -e "${NC}"
            ;;
        3)
            echo -e "${CYAN}"
            echo "      Static image on solid background"
            echo "   ┌─────────────────────────────────────┐"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                                     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                  ${BLACK}image offset_y${CYAN}     ${BG_RESET}│"
            echo -e "   │${GRAY_BG}${BLACK}image offset_x${CYAN}    ${BLACK}↓${CYAN}                  ${BG_RESET}│"
            echo -e "   │${GRAY_BG}     ${BLACK}→${CYAN}┌────────────────────────────┐ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}      │${GREEN_BG}Static image                ${BG_RESET}${GRAY_BG}│ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}      │${GREEN_BG}                            ${BG_RESET}${GRAY_BG}│ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}      │${GREEN_BG}       (center)             ${BG_RESET}${GRAY_BG}│ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}      │${GREEN_BG}                            ${BG_RESET}${GRAY_BG}│ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}      │${GREEN_BG}                            ${BG_RESET}${GRAY_BG}│ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}      │${GREEN_BG}                            ${BG_RESET}${GRAY_BG}│ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}      └────────────────────────────┘ ${BG_RESET}│"
            echo -e "   │${GRAY_BG}                   #Background color ${BG_RESET}│"
            echo "   └─────────────────────────────────────┘"
            echo -e "${NC}"
            ;;
        4)
            echo -e "${CYAN}"
            echo "           Static image full screen"
            echo "   ┌─────────────────────────────────────┐"
            echo -e "   │${GREEN_BG}Full screen image (auto resized)     ${BG_RESET}│"
            echo -e "   │${GREEN_BG}                                     ${BG_RESET}│"
            echo -e "   │${GREEN_BG}                                     ${BG_RESET}│"
            echo -e "   │${GREEN_BG}                                     ${BG_RESET}│"
            echo -e "   │${GREEN_BG}                                     ${BG_RESET}│"
            echo -e "   │${GREEN_BG}                                     ${BG_RESET}│"
            echo -e "   │${GREEN_BG}             (center)                ${BG_RESET}│"
            echo -e "   │${GREEN_BG}                                     ${BG_RESET}│"
            echo -e "   │${GREEN_BG}                                     ${BG_RESET}│"
            echo -e "   │${GREEN_BG}                                     ${BG_RESET}│"
            echo -e "   │${GREEN_BG}                                     ${BG_RESET}│"
            echo -e "   │${GREEN_BG}                                     ${BG_RESET}│"
            echo -e "   │${GREEN_BG}                                     ${BG_RESET}│"
            echo -e "   └─────────────────────────────────────┘"
            echo -e "${NC}"
            ;;
    esac

    # Detect screen size first
    detect_screen_size
    
    # Get object dimensions for offset range calculation
    local max_x=0
    local max_y=0
    local anim_w=0
    local anim_h=0
    
    if [ $DISPLAY_MODE -le 2 ]; then
        # Animation modes - get first frame size
        if [ -d "$FRAME_DIR" ]; then
            local first_frame=$(ls "$FRAME_DIR"/*.{png,PNG,jpg,JPG,jpeg,JPEG} 2>/dev/null | head -1)
            if [ -n "$first_frame" ] && [ -f "$first_frame" ]; then
                get_object_size "$first_frame"
                anim_w=$OBJECT_W
                anim_h=$OBJECT_H
                max_x=$((SCREEN_W - anim_w))
                max_y=$((SCREEN_H - anim_h))
            fi
        fi
    elif [ $DISPLAY_MODE -eq 3 ]; then
        # Static image mode
        if [ -n "$FRAME_DIR" ] && [ -f "$FRAME_DIR" ]; then
            get_object_size "$FRAME_DIR"
            anim_w=$OBJECT_W
            anim_h=$OBJECT_H
            max_x=$((SCREEN_W - anim_w))
            max_y=$((SCREEN_H - anim_h))
        fi
    fi
    
    # Animation offset parameters (modes 0, 1, 2)
    if [ $DISPLAY_MODE -le 2 ]; then
        echo -e "\n${BOLD}Animation position:${NC}"
        echo -e "  ${YELLOW}Screen: ${SCREEN_W}x${SCREEN_H}, Animation: ${anim_w}x${anim_h}${NC}"
        echo -e "  ${CYAN}Full visibility: X=0 to $max_x, Y=0 to $max_y${NC}"
        echo -e "  ${CYAN}Extended (partial off-screen): X=-${anim_w} to ${SCREEN_W}, Y=-${anim_h} to ${SCREEN_H}${NC}"
        echo -en "  ➤ Horizontal offset [${CYAN}$FRAME_OFFSET_X${NC}]: "
        read -r input
        if [ -n "$input" ] && [[ "$input" =~ ^-?[0-9]+$ ]]; then
            FRAME_OFFSET_X="$input"
        fi
        
        echo -en "  ➤ Vertical offset [${CYAN}$FRAME_OFFSET_Y${NC}]: "
        read -r input
        if [ -n "$input" ] && [[ "$input" =~ ^-?[0-9]+$ ]]; then
            FRAME_OFFSET_Y="$input"
        fi
    fi
    
    # Background image offset (mode 1 only - centered background image)
    if [ $DISPLAY_MODE -eq 1 ]; then
        # Get background image size
        if [ -n "$BG_IMAGE" ] && [ -f "$BG_IMAGE" ]; then
            get_object_size "$BG_IMAGE"
            local bg_w=$OBJECT_W
            local bg_h=$OBJECT_H
            local bg_max_x=$((SCREEN_W - bg_w))
            local bg_max_y=$((SCREEN_H - bg_h))
        fi
        
        echo -e "\n${BOLD}Background image position:${NC}"
        echo -e "  ${YELLOW}Screen: ${SCREEN_W}x${SCREEN_H}, Background: ${bg_w}x${bg_h}${NC}"
        echo -e "  ${CYAN}Full visibility: X=0 to $bg_max_x, Y=0 to $bg_max_y${NC}"
        echo -e "  ${CYAN}Extended (partial off-screen): X=-${bg_w} to ${SCREEN_W}, Y=-${bg_h} to ${SCREEN_H}${NC}"
        echo -en "  ➤ Horizontal offset [${CYAN}$BG_OFFSET_X${NC}]: "
        read -r input
        if [ -n "$input" ] && [[ "$input" =~ ^-?[0-9]+$ ]]; then
            BG_OFFSET_X="$input"
        fi
        
        echo -en "  ➤ Vertical offset [${CYAN}$BG_OFFSET_Y${NC}]: "
        read -r input
        if [ -n "$input" ] && [[ "$input" =~ ^-?[0-9]+$ ]]; then
            BG_OFFSET_Y="$input"
        fi
    fi
    
    # Static image offset (mode 3 only - centered static image)
    if [ $DISPLAY_MODE -eq 3 ]; then
        echo -e "\n${BOLD}Image position:${NC}"
        echo -e "  ${YELLOW}Screen: ${SCREEN_W}x${SCREEN_H}, Image: ${anim_w}x${anim_h}${NC}"
        echo -e "  ${CYAN}Full visibility: X=0 to $max_x, Y=0 to $max_y${NC}"
        echo -e "  ${CYAN}Extended (partial off-screen): X=-${anim_w} to ${SCREEN_W}, Y=-${anim_h} to ${SCREEN_H}${NC}"
        echo -en "  ➤ Horizontal offset [${CYAN}$FRAME_OFFSET_X${NC}]: "
        read -r input
        if [ -n "$input" ] && [[ "$input" =~ ^-?[0-9]+$ ]]; then
            FRAME_OFFSET_X="$input"
        fi
        
        echo -en "  ➤ Vertical offset [${CYAN}$FRAME_OFFSET_Y${NC}]: "
        read -r input
        if [ -n "$input" ] && [[ "$input" =~ ^-?[0-9]+$ ]]; then
            FRAME_OFFSET_Y="$input"
        fi
    fi
    
    # Background color (modes 0, 1, 3)
    if [ $DISPLAY_MODE -eq 0 ] || [ $DISPLAY_MODE -eq 1 ] || [ $DISPLAY_MODE -eq 3 ]; then
        echo -e "\n${BOLD}Background color:${NC}"
        echo -e "  ${CYAN}Format: RRGGBB (hex)${NC}"
        echo -en "  Color [${CYAN}$BG_COLOR${NC}]: "
        read -r input
        if [ -n "$input" ] && [[ "$input" =~ ^[0-9A-Fa-f]{6}$ ]]; then
            BG_COLOR="$input"
        fi
    fi
    
    # Frame delay for animation modes
    if [ $DISPLAY_MODE -le 2 ]; then
        # Count frames for loop start validation (same filter as analysis)
        if [ -d "$FRAME_DIR" ]; then
            local frame_count=$(find "$FRAME_DIR" -maxdepth 1 -type f \( -name "*.png" -o -name "*.PNG" -o -name "*.jpg" -o -name "*.JPG" -o -name "*.jpeg" -o -name "*.JPEG" \) 2>/dev/null | wc -l)
        else
            local frame_count=0
        fi
        
        echo -e "\n${BOLD}Animation timing:${NC}"
        echo -e "  ${CYAN}Valid range: 1-1000 ms (1=1000 FPS max, 1000=1 FPS min)${NC}"
        echo -en "  Frame delay (ms) [${CYAN}$FRAME_DELAY${NC}]: "
        read -r input
        if [ -n "$input" ] && [[ "$input" =~ ^[0-9]+$ ]] && [ "$input" -ge 1 ] && [ "$input" -le 1000 ]; then
            FRAME_DELAY="$input"
        fi
        
        # Loop option for animation modes
        echo -e "\n${BOLD}Animation loop mode:${NC}"
        echo -e "╭───────────────────────────────────────────────────────────────╮"
        echo -e "│  ${YELLOW}1)${NC} Full loop     - Play 0→N, then restart from 0 (default)   │"
        echo -e "│  ${YELLOW}2)${NC} No loop       - Play once, stay on last frame             │"
        echo -e "│  ${YELLOW}3)${NC} Partial loop  - Play 0→N, then loop from frame X to N     │"
        echo -e "╰───────────────────────────────────────────────────────────────╯"
        echo -en "${YELLOW} ➤ Loop mode [${CYAN}1${YELLOW}]: "
        read -r input
        case "$input" in
            2)
                LOOP_MODE=0
                ;;
            3)
                LOOP_MODE=2
                echo -n "  Loop start frame (0-$((frame_count-1))): "
                read -r start_input
                if [ -n "$start_input" ] && [[ "$start_input" =~ ^[0-9]+$ ]] && [ "$start_input" -ge 0 ] && [ "$start_input" -lt "$frame_count" ]; then
                    LOOP_START="$start_input"
                else
                    LOOP_START=0
                fi
                ;;
            *)
                LOOP_MODE=1
                ;;
        esac
    fi
    
    # Summary
    echo -e "\n${GREEN}Parameters:${NC}"
    if [ $DISPLAY_MODE -le 2 ]; then
        echo -e "  ${CYAN}Animation offset:${NC}    X=$FRAME_OFFSET_X, Y=$FRAME_OFFSET_Y"
        local loop_desc=""
        case $LOOP_MODE in
            0) loop_desc="No loop (stay on last frame)" ;;
            1) loop_desc="Full loop (0→N, 0→N...)" ;;
            2) loop_desc="Partial loop (0→N, then $LOOP_START→N...)" ;;
        esac
        echo -e "  ${CYAN}Loop:${NC}             $loop_desc"
    fi
    if [ $DISPLAY_MODE -eq 1 ]; then
        echo -e "  ${CYAN}Background image offset:${NC} X=$BG_OFFSET_X, Y=$BG_OFFSET_Y"
    fi
    if [ $DISPLAY_MODE -eq 3 ]; then
        echo -e "  ${CYAN}Image offset:${NC}       X=$FRAME_OFFSET_X, Y=$FRAME_OFFSET_Y"
    fi
    if [ $DISPLAY_MODE -eq 0 ] || [ $DISPLAY_MODE -eq 1 ] || [ $DISPLAY_MODE -eq 3 ]; then
        echo -e "  ${CYAN}Background color:${NC}  #$BG_COLOR"
    fi
    if [ $DISPLAY_MODE -le 2 ]; then
        local fps=$((1000/FRAME_DELAY))
        echo -e "  ${CYAN}Frame delay:${NC}      $FRAME_DELAY ms (~${fps} FPS)"
    fi
    
    # Binary name prompt
    echo -e "\n${BOLD}Binary name:${NC}"
    
    # Determine default name based on source
    local default_name
    if [ $DISPLAY_MODE -le 2 ]; then
        # Animation mode: use frame directory name
        local dir_name
        if [ -d "$FRAME_DIR" ]; then
            dir_name=$(basename "$FRAME_DIR")
        else
            dir_name="anim"
        fi
        default_name="xbs_${dir_name}"
    else
        # Static image mode: use image name
        local img_name
        if [ -n "$FRAME_DIR" ] && [ -f "$FRAME_DIR" ]; then
            img_name=$(basename "$FRAME_DIR" | sed 's/\.[^.]*$//')
        else
            img_name="static"
        fi
        default_name="xbs_${img_name}"
    fi
    
    # Truncate default name to 15 chars if needed
    if [ ${#default_name} -gt 15 ]; then
        default_name="${default_name:0:15}"
    fi
    
    echo -e "  ${CYAN}Max 15 characters (kernel /proc/$PID/comm limit)${NC}"
    echo -e "  ${CYAN}Default: $default_name${NC}"
    echo -n "  ➤ Binary name [$default_name]: "
    read -r input
    if [ -n "$input" ]; then
        # Sanitize: only allow alphanumeric and underscore
        BINARY=$(echo "$input" | sed 's/[^a-zA-Z0-9_]//g')
        if [ -z "$BINARY" ]; then
            BINARY="$default_name"
        fi
    else
        BINARY="$default_name"
    fi
    
    # Enforce 15 char limit (kernel truncates /proc/$PID/comm to 15 chars)
    if [ ${#BINARY} -gt 15 ]; then
        local original_name="$BINARY"
        BINARY="${BINARY:0:15}"
        echo -e "  ${YELLOW}⚠ Name truncated to 15 chars: $original_name → $BINARY${NC}"
    fi
    
    echo -e "  ${GREEN}Binary will be: $BINARY${NC}"
    
    # Show parameter summary and ask for confirmation
    echo ""
    echo -e "${BOLD}${CYAN}  ════════════════════════════════════════════════════════════════${NC}"
    echo -e "${BOLD}${CYAN}                      BUILD PARAMETERS SUMMARY                      ${NC}"
    echo -e "${BOLD}${CYAN}  ════════════════════════════════════════════════════════════════${NC}"
    echo -e "   ${CYAN}Target resolution:${NC} ${SCREEN_W:-1920}x${SCREEN_H:-1080}"
    echo -e "   ${CYAN}Mode:${NC}              $(case $DISPLAY_MODE in 0|1|2) echo "Animation on solid background" ;; 3) echo "Static image on solid background" ;; 4) echo "Full screen image" ;; esac)"
    if [ $DISPLAY_MODE -le 2 ]; then
        echo -e "   ${CYAN}Frames:${NC}            $FRAME_DIR"
        echo -e "   ${CYAN}Frame size:${NC}        ${FRAME_W:-?}x${FRAME_H:-?}"
        echo -e "   ${CYAN}Animation offset:${NC}   X=$FRAME_OFFSET_X, Y=$FRAME_OFFSET_Y"
        echo -e "   ${CYAN}Frame delay:${NC}        $FRAME_DELAY ms"
        case $LOOP_MODE in
            0) echo -e "   ${CYAN}Loop mode:${NC}          No loop (stay on last frame)" ;;
            1) echo -e "   ${CYAN}Loop mode:${NC}          Full loop (0→N, 0→N...)" ;;
            2) echo -e "   ${CYAN}Loop mode:${NC}          Partial loop (0→N, then $LOOP_START→N...)" ;;
        esac
    elif [ $DISPLAY_MODE -eq 3 ]; then
        echo -e "   ${CYAN}Image:${NC}             $FRAME_DIR"
        echo -e "   ${CYAN}Position offset:${NC}    X=$FRAME_OFFSET_X, Y=$FRAME_OFFSET_Y"
    else
        echo -e "   ${CYAN}Image:${NC}             $FRAME_DIR (full screen)"
    fi
    echo -e "   ${CYAN}Background:${NC}         #$BG_COLOR"
    echo -e "   ${CYAN}Binary name:${NC}        $BINARY"
    echo -e "${BOLD}${CYAN}  ════════════════════════════════════════════════════════════════${NC}"
    
    if ! ask_yes_no "➤ Build with these parameters?"; then
        echo -e "\n${YELLOW}Returning to mode selection...${NC}"
        return 1  # Signal to restart from STEP 2
    fi
}

# Build animation
build_animation() {
    print_step "5" "Building splash"
    
    # Check if generator needs recompilation (source newer than binary)
    if [ ! -f "generate_splash" ] || [ "generate_splash.c" -nt "generate_splash" ]; then
        print_info "⚙ Compiling splash generator..."
        rm -f generate_splash
        gcc -O2 -o generate_splash generate_splash.c -lpng -lm
        print_success "Generator compiled"
    fi
    
    # Build generator arguments as array (safer than string with eval)
    local GEN_ARGS=()
    GEN_ARGS+=(-m "$DISPLAY_MODE")
    GEN_ARGS+=(-x "$FRAME_OFFSET_X")
    GEN_ARGS+=(-y "$FRAME_OFFSET_Y")
    GEN_ARGS+=(-c "$BG_COLOR")
    
    # Add mode-specific options
    # Modes 1 and 2: background image
    if [ $DISPLAY_MODE -eq 1 ] || [ $DISPLAY_MODE -eq 2 ]; then
        GEN_ARGS+=(-b "$BG_IMAGE")
    fi

    # Mode 1 (centered background): pass background offsets
    if [ $DISPLAY_MODE -eq 1 ]; then
        GEN_ARGS+=(-X "$BG_OFFSET_X")
        GEN_ARGS+=(-Y "$BG_OFFSET_Y")
    fi
    
    # Mode 2 (fullscreen anim): target resolution
    if [ $DISPLAY_MODE -eq 2 ] && [ -n "$TARGET_RES" ]; then
        GEN_ARGS+=(-r "$TARGET_RES")
    fi
    
    # Mode 4 (static fullscreen): target resolution
    if [ $DISPLAY_MODE -eq 4 ] && [ -n "$TARGET_RES" ]; then
        GEN_ARGS+=(-r "$TARGET_RES")
    fi
    
    # Add frame delay for animation modes (0, 1, 2)
    if [ $DISPLAY_MODE -le 2 ]; then
        GEN_ARGS+=(-d "$FRAME_DELAY")
        GEN_ARGS+=(-l "$LOOP_MODE")
        if [ $LOOP_MODE -eq 2 ]; then
            GEN_ARGS+=(-L "$LOOP_START")
        fi
        
        # Always use auto compression (tests all methods and picks best)
        GEN_ARGS+=(-z auto)
    fi
    
    # Add input path
    GEN_ARGS+=("$FRAME_DIR")
    
    # Generate frames header (direct call, no eval)
    print_info "⚙ Generating splash data..."
    if ! ./generate_splash "${GEN_ARGS[@]}" > frames_delta.h 2>build.log; then
        print_error "Splash generation failed"
        cat build.log
        exit 1
    fi
    
    # Show generator output
    grep -E "^(Display|Found|Frame|Offsets|Background|Image|Total|Resizing)" build.log | while read -r line; do
        print_info "$line"
    done
    
    print_success "Splash data generated"
    
    # Compile binary (DRM or fbdev based on mode)
    print_info "Compiling $BINARY binary..."
    make clean >/dev/null 2>&1 || true
    
    if [[ $USE_DRM -eq 1 ]]; then
        # DRM mode: compile splash_anim_drm.c
        print_info "Using DRM/KMS mode (libdrm)"
        if ! make drm TARGET="$BINARY" >/dev/null 2>build.log; then
            print_error "Compilation failed"
            cat build.log
            exit 1
        fi
        # Rename output
        if [ -f "${BINARY}_drm" ]; then
            mv "${BINARY}_drm" "$BINARY"
        fi
    else
        # fbdev mode: compile splash_anim_delta.c (nolibc)
        print_info "Using fbdev mode (/dev/fb0)"
        if ! make fbdev TARGET="$BINARY" >/dev/null 2>build.log; then
            print_error "Compilation failed"
            cat build.log
            exit 1
        fi
    fi
    
    # Verify binary exists
    if [ ! -f "$BINARY" ]; then
        print_error "Binary not found after compilation"
        exit 1
    fi
    
    # Get binary size
    local size=$(wc -c < "$BINARY" 2>/dev/null)
    local size_kb=$((size / 1024))
    local size_mb=$((size / 1048576))
    
    # Hard limit: 15 MB maximum to prevent /boot partition saturation
    # Large binaries can fill /boot during initramfs rebuild, requiring LiveUSB repair
    local MAX_BINARY_SIZE=$((15 * 1048576))  # 15 MB in bytes
    if [ $size -gt $MAX_BINARY_SIZE ]; then
        print_error "Binary size (${size_mb} MB) exceeds maximum allowed (15 MB)"
        echo ""
        echo -e "${RED} ═══════════════════════════════════════════════════════════════${NC}"
        echo -e "${RED}   CRITICAL: Binary too large for safe initramfs installation${NC}"
        echo -e "${RED} ═══════════════════════════════════════════════════════════════${NC}"
        echo ""
        echo -e " ${YELLOW}A binary this large could saturate /boot partition during${NC}"
        echo -e " ${YELLOW}initramfs rebuild, leaving the system unbootable.${NC}"
        echo ""
        echo -e " ${CYAN}To reduce binary size:${NC}"
        echo -e "   ${GREEN}•${NC} Reduce number of frames (current: ${size_mb} MB for ~${FRAME_COUNT:-unknown} frames)"
        echo -e "   ${GREEN}•${NC} Use smaller frame dimensions"
        echo -e "   ${GREEN}•${NC} Enable stronger compression (auto/rle_xor)"
        echo -e "   ${GREEN}•${NC} Simplify graphics (fewer colors, less detail)"
        echo ""
        echo -e " ${RED}Build aborted to protect system integrity.${NC}"
        rm -f "$BINARY"
        exit 1
    fi
    
    print_success "Binary compiled"
    
    # Display results
    echo -e "\n${BOLD}${GREEN}   ════════════════════════════════════════════════════════════════${NC}"
    echo -e "${BOLD}${GREEN}                    BUILD SUCCESSFUL${NC}"
    echo -e "${BOLD}${GREEN}   ════════════════════════════════════════════════════════════════${NC}"
    echo -e "  ${CYAN}Binary:${NC}          $BINARY"
    echo -e "  ${CYAN}Size:${NC}            ${size} bytes (${size_kb} KB)"
    if [[ $USE_DRM -eq 1 ]]; then
        echo -e "  ${CYAN}Mode:${NC}            ${GREEN}DRM/KMS${NC} (libdrm, dynamic)"
    else
        echo -e "  ${CYAN}Mode:${NC}            ${CYAN}fbdev${NC} (nolibc, static)"
    fi
    echo -e "  ${CYAN}Location:${NC}        $(pwd)/$BINARY"
    echo -e "${BOLD}${GREEN}   ════════════════════════════════════════════════════════════════${NC}"

    echo ""
    echo -e "${CYAN}Rebuild (non-interactive) with the exact same options:${NC}"
    local script_abs="$(cd "$(dirname "$0")" 2>/dev/null && pwd)/$(basename "$0")"
    local LIGHT_GRAY_BG='\033[48;5;253m'
    
    # Line 1: USE_DRM + script + binary name
    local cmd1="USE_DRM=$(printf '%q' "$USE_DRM") $script_abs -n $BINARY"
    
    # Line 2: Mode and position/delay/color params
    local cmd2="-m $DISPLAY_MODE -x $FRAME_OFFSET_X -y $FRAME_OFFSET_Y -d $FRAME_DELAY -c $BG_COLOR"
    
    # Line 3: Animation options OR static image (modes 3/4/5)
    local cmd3=""
    if [ $DISPLAY_MODE -le 2 ]; then
        # Animation modes: loop options
        cmd3="-l $LOOP_MODE"
        if [ $LOOP_MODE -eq 2 ]; then
            cmd3="$cmd3 -L $LOOP_START"
        fi
    fi
    
    # Line 4: Background image (modes 1/2) on separate line
    local cmd4=""
    if [ $DISPLAY_MODE -eq 1 ] || [ $DISPLAY_MODE -eq 2 ]; then
        cmd4="-b $BG_IMAGE"
        if [ $DISPLAY_MODE -eq 1 ]; then
            cmd4="$cmd4 -X $BG_OFFSET_X -Y $BG_OFFSET_Y"
        fi
        if [ $DISPLAY_MODE -eq 2 ] && [ -n "$TARGET_RES" ]; then
            cmd4="$cmd4 -r $TARGET_RES"
        fi
    fi
    
    # Line 5: Target res for mode 4 or frame directory
    local cmd5=""
    if [ $DISPLAY_MODE -eq 4 ] && [ -n "$TARGET_RES" ]; then
        cmd5="-r $TARGET_RES"
    fi
    
    # Frame directory (last line)
    local cmd_last="$FRAME_DIR"
    
    # Display with light gray background
    printf '%b%b%s \\%b\n' "$LIGHT_GRAY_BG" "$BLACK" "$cmd1" "$NC"
    printf '%b%b%s \\%b\n' "$LIGHT_GRAY_BG" "$BLACK" "$cmd2" "$NC"
    [ -n "$cmd3" ] && printf '%b%b%s \\%b\n' "$LIGHT_GRAY_BG" "$BLACK" "$cmd3" "$NC"
    [ -n "$cmd4" ] && printf '%b%b%s \\%b\n' "$LIGHT_GRAY_BG" "$BLACK" "$cmd4" "$NC"
    [ -n "$cmd5" ] && printf '%b%b%s \\%b\n' "$LIGHT_GRAY_BG" "$BLACK" "$cmd5" "$NC"
    printf '%b%b%s%b\n' "$LIGHT_GRAY_BG" "$BLACK" "$cmd_last" "$NC"
    
    # Warning for large binary (> 1MB)
    if [ $size -gt 1048576 ]; then
        echo ""
        echo -e "  ${BOLD}${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
        echo -e "  ${BOLD}${YELLOW}                       ⚠ SIZE WARNING ⚠${NC}"
        echo -e "  ${BOLD}${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
        echo -e "  ${RED}Binary size exceeds 1 MB${NC}"
        echo -e ""
        echo -e "   This is getting large for a boot animation."
        echo -e "   Consider reducing:"
        echo -e "   ${CYAN}•${NC} Number of frames (fewer frames)"
        echo -e "   ${CYAN}•${NC} Frame dimensions (smaller images)"
        echo -e "   ${CYAN}•${NC} Color complexity (simpler graphics)"
        echo -e ""
        echo -e "   ${GREEN}The animation will still work correctly.${NC}"
        echo -e "   However, a smaller binary loads faster from"
        echo -e "   initramfs and uses less memory."
        echo -e "════════════════════════════════════════════════════════════════${NC}"
    fi
    
    # Cleanup
    rm -f build.log
}

# Test animation
test_animation() {
    print_step "6" "Test animation"
    
    if [[ $USE_DRM -eq 1 ]]; then
        echo -e "\n${YELLOW}  ═══════════════════════════════════════════════════════════════════${NC}"
        echo -e "${YELLOW}    {ℹ}  IMPORTANT: DRM/KMS requires exclusive display access${NC}"
        echo -e "${YELLOW}  ═══════════════════════════════════════════════════════════════════${NC}"
        echo ""
        echo -e "  ${CYAN}DRM mode requires 'DRM master' privileges, which are blocked${NC}"
        echo -e "  ${CYAN}when X11/Wayland display server is running.${NC}"
        echo ""
        echo -e "  ${BOLD}To test properly:${NC}"
        echo ""
        echo -e "    ${GREEN}Option 1: Switch to TTY console${NC}"
        echo -e "      Press: ${YELLOW}Ctrl + Alt + F2${NC} (or F3, F4, F5, F6)"
        echo "      Login with your username/password"
        echo -e "      Run:   ${YELLOW}cd $(pwd) && sudo ./$BINARY${NC}"
        echo -e "      Return to GUI: ${YELLOW}Ctrl + Alt + F7${NC} (or F1)"
        echo ""
        echo -e "    ${GREEN}Option 2: Stop display manager${NC}"
        echo -e "      ${YELLOW}sudo systemctl stop display-manager${NC}"
        echo -e "      Then run the binary from console"
        echo -e "      Restart GUI: ${YELLOW}sudo systemctl start display-manager${NC}"
        echo ""
    else
        echo -e "\n${YELLOW}  ═══════════════════════════════════════════════════════════════════${NC}"
        echo -e "${YELLOW}    {ℹ}  IMPORTANT: Framebuffer requires TTY console mode${NC}"
        echo -e "${YELLOW}  ═══════════════════════════════════════════════════════════════════${NC}"
        echo ""
        echo -e "  ${CYAN}The animation requires direct framebuffer access, which only${NC}"
        echo -e "  ${CYAN}works in a real TTY console (not in X11/Wayland terminal).${NC}"
        echo ""
        echo -e "  ${BOLD}To test properly:${NC}"
        echo ""
        echo -e "    ${GREEN}Option 1: Switch to TTY${NC}"
        echo -e "      Press: ${YELLOW}Ctrl + Alt + F2${NC} (or F3, F4, F5, F6)"
        echo "      Login with your username/password"
        echo -e "      Run:   ${YELLOW}cd $(pwd) && sudo ./$BINARY${NC}"
        echo -e "      Return to GUI: ${YELLOW}Ctrl + Alt + F7${NC} (or F1)"
        echo ""
        echo -e "    ${GREEN}Option 2: From current terminal (may not work in GUI)${NC}"
        echo "      If you're already in a TTY console, proceed below"
        echo ""
    fi
    
    if ask_yes_no "Test animation now ?"; then
        # Check if running under GUI
        if is_gui_session; then
            echo ""
            echo -e "${YELLOW}⚠ GUI session detected (X11/Wayland)${NC}"
            echo -e "${CYAN}The animation may not display correctly from a GUI terminal.${NC}"
            echo ""
            echo -e "${BOLD}For best results, test from a TTY console:${NC}"
            echo -e "  1. Press ${YELLOW}Ctrl + Alt + F2${NC} (or F3-F6)"
            echo "  2. Login with your credentials"
            echo -e "  3. Run: ${YELLOW}cd $(pwd) && sudo ./$BINARY${NC}"
            echo "  4. Press Ctrl+C to stop"
            echo -e "  5. Return here: ${YELLOW}Ctrl + Alt + F7${NC} (or F1)"
            echo ""
            
            if ask_yes_no "Try anyway from current terminal ?"; then
                print_info "Running animation (Ctrl+C to stop)..."
                echo -e "\n${YELLOW}─────────────────────────────────────────${NC}"
                
                # Run in subshell so Ctrl+C doesn't exit the script
                ( if [ -w "/dev/fb0" ]; then
                    ./$BINARY
                  else
                    sudo ./$BINARY
                  fi ) || true
                
                echo -e "${YELLOW}─────────────────────────────────────────${NC}"
                print_success "Test completed"
            else
                print_info "Skipping test. You can test manually later with:"
                echo -e "  ${YELLOW}cd $(pwd) && sudo ./$BINARY${NC}"
            fi
        else
            # Pure TTY session - run directly
            print_info "Running animation (Ctrl+C to stop)..."
            echo -e "\n${YELLOW}─────────────────────────────────────────${NC}"
            
            # Run in subshell so Ctrl+C doesn't exit the script
            ( if [ -w "/dev/fb0" ]; then
                ./$BINARY
              else
                sudo ./$BINARY
              fi ) || true
            
            echo -e "${YELLOW}─────────────────────────────────────────${NC}"
            print_success "Test completed"
        fi
    else
        print_info "Skipping test. You can test manually with:"
        echo -e "  ${YELLOW}cd $(pwd) && sudo ./xbootsplash${NC}"
    fi
}

# ========================================
# INSTALLATION FUNCTIONS
# ========================================

# Show detailed method information
show_install_info() {
    echo ""
    echo -e "${BLUE} ========================================${NC}"
    echo -e "${BLUE}    Installation Methods Explained      ${NC}"
    echo -e "${BLUE} ========================================${NC}"
    echo ""
    echo -e "${GREEN} ═══════════════════════════════════════${NC}"
    echo -e "${GREEN}   METHOD 1: STANDARD (Recommended)     ${NC}"
    echo -e "${GREEN} ═══════════════════════════════════════${NC}"
    echo ""
    echo -e "${CYAN}What it does:${NC}"
    echo "  Integrates bootsplash binary with Debian/Ubuntu initramfs-tools"
    echo "  system. Uses standard hooks and scripts that are automatically"
    echo "  included in kernel updates."
    echo ""
    echo -e "${CYAN}How it works:${NC}"
    echo "  1. Copies binary to /sbin/<binary_name>"
    echo "  2. Creates hook in /etc/initramfs-tools/hooks/"
    echo "  3. Creates init-top script (starts splash early)"
    echo "  4. Creates init-bottom script (stops splash before handoff)"
    echo "  5. Rebuilds initramfs with update-initramfs"
    echo ""
    echo -e "${CYAN}Advantages:${NC}"
    echo -e "  ${GREEN}✓${NC} Safe with LUKS, LVM, mdadm, resume from disk"
    echo -e "  ${GREEN}✓${NC} Persists across kernel updates"
    echo -e "  ${GREEN}✓${NC} No modification of critical init scripts"
    echo -e "  ${GREEN}✓${NC} Clean integration with Debian boot process"
    echo ""
    echo -e "${CYAN}Requirements:${NC}"
    echo "  - Debian/Ubuntu with initramfs-tools"
    echo "  - Plymouth must be uninstalled (conflicts)"
    echo ""
    echo -e "${CYAN}Best for:${NC}"
    echo "  Most users on Debian/Ubuntu systems"
    echo ""
    echo ""
    echo -e "${YELLOW} ═══════════════════════════════════════${NC}"
    echo -e "${YELLOW}   METHOD 2: CUSTOM (Advanced)         ${NC}"
    echo -e "${YELLOW} ═══════════════════════════════════════${NC}"
    echo ""
    echo -e "${CYAN}What it does:${NC}"
    echo "  Creates a separate custom initramfs image that you can"
    echo "  configure manually for special boot requirements."
    echo ""
    echo -e "${CYAN}How it works:${NC}"
    echo "  1. Creates directory structure for custom initramfs"
    echo "  2. Copies binary to sbin/<binary_name>"
    echo "  3. Creates device nodes (fb0)"
    echo "  4. Creates minimal init script template"
    echo "  5. Builds initramfs image (.img file)"
    echo ""
    echo -e "${CYAN}Advantages:${NC}"
    echo -e "  ${GREEN}✓${NC} Full control over boot process"
    echo -e "  ${GREEN}✓${NC} Can customize for special setups"
    echo -e "  ${GREEN}✓${NC} Separate from system initramfs"
    echo ""
    echo -e "${CYAN}Disadvantages:${NC}"
    echo -e "  ${RED}✗${NC} Requires manual init script configuration"
    echo -e "  ${RED}✗${NC} Must handle LUKS/LVM/mdadm manually"
    echo -e "  ${RED}✗${NC} Not updated automatically with kernels"
    echo -e "  ${RED}✗${NC} Requires bootloader manual configuration"
    echo ""
    echo -e "${CYAN}Best for:${NC}"
    echo "  Advanced users with custom boot requirements"
    echo "  Embedded systems, special configurations"
    echo ""
    echo ""
    echo -e "${GREEN}Press Enter to return to menu...${NC}"
    read -r
}

# Check for Plymouth conflict
check_plymouth() {
    if command -v plymouth &>/dev/null || dpkg -l plymouth 2>/dev/null | grep -q '^ii' || rpm -q plymouth &>/dev/null 2>&1; then
        echo -e "  ${RED}==============================================${NC}"
        echo -e "  ${RED}⚠ WARNING: Plymouth is installed on this system${NC}"
        echo -e "  ${RED}==============================================${NC}"
        echo ""
        echo "  Plymouth and this bootsplash system are incompatible."
        echo "  Running both will cause boot issues (black screen, freezes, or other horrible things...)."
        echo ""
        echo -e "  ${YELLOW}➤➤ You MUST uninstall Plymouth before proceeding.${NC}"
        echo ""
        echo "  To uninstall Plymouth:"
        echo "    Debian/Ubuntu: sudo apt remove --purge plymouth plymouth-themes"
        echo "    RHEL/Fedora:   sudo dnf remove plymouth"
        echo "    Arch Linux:    sudo pacman -Rns plymouth"
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
            echo -e "${GREEN}✔ Plymouth uninstalled successfully.${NC}"
        else
            echo -e "${RED}✖✖ Plymouth must be uninstalled to continue. Aborting.${NC}"
            exit 1
        fi
    fi
}

# Verify installation
verify_installation() {
    echo -e "${BLUE}  ══════════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}             POST-INSTALLATION VERIFICATION                       ${NC}"
    echo -e "${BLUE}  ══════════════════════════════════════════════════════════════${NC}"
    echo ""
    
    local issues=()
    local warnings=()
    
    # Check 1: Binary exists and is executable
    echo -e "${CYAN}[1/6] Checking binary...${NC}"
    if [[ -x /sbin/$BINARY ]]; then
        local size=$(wc -c < /sbin/$BINARY 2>/dev/null || echo "unknown")
        echo -e "  ${GREEN}✓${NC} /sbin/$BINARY exists and is executable (${size} bytes)"
    else
        echo -e "  ${RED}✗${NC} /sbin/$BINARY missing or not executable"
        issues+=("Binary not installed correctly")
    fi
    
    # Check 2: Hook script
    echo -e "${CYAN}[2/6] Checking hook script...${NC}"
    if [[ -x /etc/initramfs-tools/hooks/$BINARY ]]; then
        echo -e "  ${GREEN}✓${NC} Hook script exists and is executable"
    else
        echo -e "  ${RED}✗${NC} Hook script missing or not executable"
        issues+=("Hook script not installed")
    fi
    
    # Check 3: init-top script
    echo -e "${CYAN}[3/6] Checking init-top script...${NC}"
    if [[ -x /etc/initramfs-tools/scripts/init-top/$BINARY ]]; then
        echo -e "  ${GREEN}✓${NC} init-top script exists and is executable"
    else
        echo -e "  ${RED}✗${NC} init-top script missing or not executable"
        issues+=("init-top script not installed")
    fi
    
    # Check 4: init-bottom script
    echo -e "${CYAN}[4/6] Checking init-bottom script...${NC}"
    if [[ -x /etc/initramfs-tools/scripts/init-bottom/$BINARY ]]; then
        echo -e "  ${GREEN}✓${NC} init-bottom script exists and is executable"
    else
        echo -e "  ${RED}✗${NC} init-bottom script missing or not executable"
        issues+=("init-bottom script not installed")
    fi
    
    # Check 5: initramfs was rebuilt
    echo -e "${CYAN}[5/6] Checking initramfs...${NC}"
    local current_kernel=$(uname -r)
    local initrd="/boot/initrd.img-${current_kernel}"
    if [[ -f "$initrd" ]]; then
        if lsinitramfs "$initrd" 2>/dev/null | grep -q "$BINARY"; then
            echo -e "  ${GREEN}✓${NC} $BINARY found in current initramfs"
        else
            echo -e "  ${YELLOW}!${NC} $BINARY not found in initramfs (may need rebuild)"
            warnings+=("Run: sudo update-initramfs -u")
        fi
    else
        echo -e "  ${YELLOW}!${NC} Cannot verify initramfs (file not found)"
        warnings+=("Initramfs file not found at $initrd")
    fi
    
    # Check 6: Framebuffer device
    echo -e "${CYAN}[6/6] Checking framebuffer device...${NC}"
    if [[ -c /dev/fb0 ]]; then
        echo -e "  ${GREEN}✓${NC} /dev/fb0 exists"
    else
        echo -e "  ${YELLOW}!${NC} /dev/fb0 not found (may not be available in current environment)"
        warnings+=("Framebuffer may need kernel parameter: video=efifb")
    fi
    
    # Summary
    echo ""
    echo -e "${BLUE}  ══════════════════════════════════════════════════════════════${NC}"
    
    if [ ${#issues[@]} -eq 0 ] && [ ${#warnings[@]} -eq 0 ]; then
        echo -e "${GREEN}✓✓ All checks passed! Installation appears successful.${NC}"
        echo ""
        echo -e "${GREEN}Reboot to see the splash animation.${NC}"
    elif [ ${#issues[@]} -eq 0 ]; then
        echo -e "${YELLOW}⚠ Installation completed with warnings:${NC}"
        for w in "${warnings[@]}"; do
            echo -e "  ${YELLOW}•${NC} $w"
        done
        echo ""
        echo -e "${GREEN}Reboot to see the splash animation.${NC}"
    else
        echo -e "${RED}✗ Installation has issues:${NC}"
        for i in "${issues[@]}"; do
            echo -e "  ${RED}•${NC} $i"
        done
        for w in "${warnings[@]}"; do
            echo -e "  ${YELLOW}•${NC} $w"
        done
        echo ""
        
        read -p "Would you like to revert the installation? [y/N] " -n 1 -r
        echo ""
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            do_uninstall
            echo -e "${YELLOW}Installation reverted. Please try again.${NC}"
            exit 1
        fi
    fi
    
    echo -e "${BLUE}  ══════════════════════════════════════════════════════════════${NC}"
}

# Standard installation method
install_standard() {
    # Disable set -e for this function - we handle errors explicitly
    # and don't want unexpected exits triggering rollback trap
    set +e
    
    echo -e "${GREEN}=== Installing via initramfs-tools (Standard Method) ===${NC}"
    echo ""

    # BINARY can be either a filename (built locally) or an absolute path (install-existing).
    # The initramfs scripts require a safe command name.
    local binary_src="$BINARY"
    local binary_name
    binary_name="$(basename -- "$BINARY")"
    if [[ "$binary_name" != "$BINARY" ]]; then
        BINARY="$binary_name"
    fi
    if [[ ! "$BINARY" =~ ^[A-Za-z0-9_]+$ ]]; then
        echo -e "${RED}ERROR: Invalid binary name: $BINARY${NC}"
        echo "Allowed characters: A-Z a-z 0-9 _"
        return 1
    fi
    if [[ ${#BINARY} -gt 15 ]]; then
        echo -e "${RED}ERROR: Binary name too long: $BINARY${NC}"
        echo "Max 15 characters (kernel /proc/PID/comm limit)."
        return 1
    fi
    
    # Auto-detect DRM mode from binary if not explicitly set by user
    if [[ "$USE_DRM_EXPLICIT" -eq 0 ]] && [[ -n "$BINARY" ]]; then
        if ldd "$binary_src" 2>/dev/null | grep -q libdrm; then
            USE_DRM=1
        else
            USE_DRM=0
        fi
    fi
    
    # Check initramfs-tools
    if [[ ! -d /etc/initramfs-tools ]]; then
        echo -e "${RED}ERROR: initramfs-tools not found${NC}"
        echo "Install with: sudo apt install initramfs-tools"
        exit 1
    fi
    
    # Track installed files for rollback
    local installed_files=(
        "/sbin/$BINARY"
        "/etc/initramfs-tools/hooks/$BINARY"
        "/etc/initramfs-tools/scripts/init-top/$BINARY"
        "/etc/initramfs-tools/scripts/init-bottom/$BINARY"
    )
    local backup_files=(
        "/sbin/${BINARY}.bak"
        "/etc/initramfs-tools/hooks/${BINARY}.bak"
        "/etc/initramfs-tools/scripts/init-top/${BINARY}.bak"
        "/etc/initramfs-tools/scripts/init-bottom/${BINARY}.bak"
    )
    local grub_backup=""
    
    # Store current initramfs size for integrity check
    local current_kernel=$(uname -r)
    local initramfs_path="/boot/initrd.img-${current_kernel}"
    local old_initramfs_size=0
    if [[ -f "$initramfs_path" ]]; then
        old_initramfs_size=$(stat -c%s "$initramfs_path" 2>/dev/null || echo 0)
    fi
    
    # Initramfs backup path (declared early for rollback function access)
    local initramfs_backup=""
    
    # Track installation progress for partial rollback
    local install_step=0
    local install_completed=0
    
    # Rollback function for trap handler
    install_rollback() {
        local exit_code=$?
        # Disable set -e to prevent cascading failures during rollback
        set +e
        # Disarm traps immediately to prevent double-trigger on exit
        trap - INT TERM EXIT
        
        # Only rollback if installation wasn't completed
        if [[ $exit_code -ne 0 && $install_completed -eq 0 ]]; then
            echo ""
            echo -e "${YELLOW}⚠ Installation interrupted at step $install_step. Rolling back...${NC}"
            
            # Remove installed files based on how far we got
            for f in "${installed_files[@]}"; do
                if [[ -f "$f" ]]; then
                    rm -f "$f"
                    echo "  -> Removed $f"
                fi
            done
            
            # Restore backups if they existed
            for i in "${!backup_files[@]}"; do
                if [[ -f "${backup_files[$i]}" ]]; then
                    mv "${backup_files[$i]}" "${installed_files[$i]}"
                    echo "  -> Restored ${installed_files[$i]} from backup"
                fi
            done
            
            # Restore grub.cfg if backed up
            if [[ -n "$grub_backup" && -f "$grub_backup" ]]; then
                if [[ -f /boot/grub/grub.cfg ]]; then
                    mv "$grub_backup" /boot/grub/grub.cfg
                    echo "  -> Restored /boot/grub/grub.cfg from backup"
                fi
            fi
            
            # Restore initramfs if backed up
            if [[ -n "$initramfs_backup" && -f "$initramfs_backup" ]]; then
                # Use stored initramfs_path (may be for target kernel, not running)
                if [[ -n "$initramfs_path" && -f "$initramfs_path" ]]; then
                    mv "$initramfs_backup" "$initramfs_path"
                    echo "  -> Restored initramfs from backup"
                fi
            fi
            
            echo -e "${RED}=== Installation Aborted ===${NC}"
            echo "System restored to previous state."
        fi
        exit $exit_code
    }
    
    # Any failed command will trigger rollback via EXIT trap
    # Note: We rely on explicit error checks rather than set -e for fine-grained control
    trap 'install_rollback' INT TERM EXIT
    
    echo -e "${BLUE}This method:${NC}"
    echo "  ✓ Safe with LUKS, LVM, mdadm, resume"
    echo "  ✓ Integrates with Debian/Ubuntu boot process"
    echo "  ✓ No modification of critical init scripts"
    echo "  ✓ Automatically included in kernel updates"
    echo ""
    
    # Create hook script
    echo -e "${GREEN}[1/4]⚙ Creating initramfs-tools hook...${NC}"
    install_step=1
    mkdir -p /etc/initramfs-tools/hooks
    
    # Backup existing hook BEFORE writing new one
    if [[ -f /etc/initramfs-tools/hooks/${BINARY}.bak ]]; then
        rm /etc/initramfs-tools/hooks/${BINARY}.bak
    fi
    if [[ -f /etc/initramfs-tools/hooks/$BINARY ]]; then
        cp /etc/initramfs-tools/hooks/$BINARY /etc/initramfs-tools/hooks/${BINARY}.bak
        echo "  -> Backed up existing hook"
    fi
    
    # Determine mode string for comment
    local mode_str="fbdev"
    if [[ $USE_DRM -eq 1 ]]; then
        mode_str="DRM/KMS"
    fi
    
    # Atomic write: write to temp file then rename
    local hook_tmp="/etc/initramfs-tools/hooks/${BINARY}.tmp"
    cat > "$hook_tmp" << HOOK_EOF
#!/bin/sh
# initramfs-tools hook for $BINARY bootsplash animation
# Mode: $mode_str

PREREQ=""
prereqs() { echo "\$PREREQ"; }
case "\$1" in prereqs) prereqs; exit 0;; esac

. /usr/share/initramfs-tools/hook-functions

# Copy binary with dependencies (copy_exec handles ldd for dynamic libs)
copy_exec /sbin/$BINARY /sbin
HOOK_EOF

    # Add mode-specific device setup (must be outside heredoc for variable expansion)
    if [[ $USE_DRM -eq 0 ]]; then
        cat >> "$hook_tmp" << 'HOOK_FBDEV'

# For fbdev mode: ensure framebuffer device node
if [ ! -e "${DESTDIR}/dev/fb0" ]; then
    mknod "${DESTDIR}/dev/fb0" c 29 0 2>/dev/null || true
fi
HOOK_FBDEV
    else
        cat >> "$hook_tmp" << 'HOOK_DRM'

# For DRM mode: ensure DRI directory exists
mkdir -p "${DESTDIR}/dev/dri" 2>/dev/null || true
HOOK_DRM
    fi
    
    cat >> "$hook_tmp" << 'HOOK_END'

# End of hook
HOOK_END

    # Atomic rename
    chmod +x "$hook_tmp"
    mv "$hook_tmp" /etc/initramfs-tools/hooks/$BINARY
    echo "  -> /etc/initramfs-tools/hooks/$BINARY"
    
    # Copy binary to /sbin
    echo -e "${GREEN}[2/4]⚙ Installing binary to /sbin...${NC}"
    install_step=2
    
    # Validate binary before installation
    if [[ ! -f "$binary_src" ]]; then
        echo -e "  ${RED}✗ Binary not found: $binary_src${NC}"
        return 1
    fi
    
    # Check ELF format and architecture
    local file_info
    file_info=$(file "$binary_src" 2>/dev/null)
    
    if ! echo "$file_info" | grep -q "ELF"; then
        echo -e "  ${RED}✗ Not a valid ELF binary${NC}"
        return 1
    fi
    
    if ! echo "$file_info" | grep -q "x86-64"; then
        echo -e "  ${RED}✗ Binary is not x86-64 architecture${NC}"
        return 1
    fi
    
    # Check mode consistency (DRM vs fbdev)
    local is_drm_binary=0
    if ldd "$binary_src" 2>/dev/null | grep -q libdrm; then
        is_drm_binary=1
    fi
    
    if [[ $is_drm_binary -eq 1 && $USE_DRM -eq 0 ]]; then
        echo -e "  ${YELLOW}⚠ Warning: Binary links to libdrm but installing as fbdev mode${NC}"
    elif [[ $is_drm_binary -eq 0 && $USE_DRM -eq 1 ]]; then
        echo -e "  ${YELLOW}⚠ Warning: Binary is static (fbdev) but installing as DRM mode${NC}"
    fi
    
    # For fbdev mode, verify static linking
    if [[ $USE_DRM -eq 0 ]]; then
        if ldd "$binary_src" 2>&1 | grep -q "Not a valid dynamic executable"; then
            echo "  -> Binary is statically linked (OK for fbdev)"
        elif ldd "$binary_src" 2>/dev/null | grep -q libdrm; then
            echo -e "  ${YELLOW}⚠ Warning: fbdev binary should be static, but links to libdrm${NC}"
        else
            echo "  -> Binary dependencies OK"
        fi
    fi
    
    echo "  -> Binary validated: $(echo "$file_info" | grep -oP 'ELF.*x86-64')"
    
    # Backup existing binary if present
    if [[ -f /sbin/$BINARY ]]; then
        cp /sbin/$BINARY /sbin/${BINARY}.bak
        echo "  -> Backed up existing binary to /sbin/${BINARY}.bak"
    fi
    # Atomic write: copy to temp then rename
    cp "$binary_src" /sbin/${BINARY}.tmp
    chmod +x /sbin/${BINARY}.tmp
    mv /sbin/${BINARY}.tmp /sbin/$BINARY
    echo "  -> /sbin/$BINARY"
    
    # Create init-top script (starts splash early)
    echo -e "${GREEN}[3/4]⚙ Creating init-top script...${NC}"
    install_step=3
    mkdir -p /etc/initramfs-tools/scripts/init-top
    # Backup existing init-top script if present
    if [[ -f /etc/initramfs-tools/scripts/init-top/$BINARY ]]; then
        cp /etc/initramfs-tools/scripts/init-top/$BINARY /etc/initramfs-tools/scripts/init-top/${BINARY}.bak
        echo "  -> Backed up existing init-top script"
    fi
    
    # Set PREREQ based on mode: fbdev needs no udev, DRM needs udev for /dev/dri/card*
    local prereq_str=""
    if [[ $USE_DRM -eq 1 ]]; then
        prereq_str="udev"
    fi
    
    # Atomic write: write to temp then rename
    local inittop_tmp="/etc/initramfs-tools/scripts/init-top/${BINARY}.tmp"
    cat > "$inittop_tmp" << INIT_TOP_EOF
#!/bin/sh
# Start $BINARY animation early in boot
# This runs after /dev is mounted, before root filesystem

PREREQ="$prereq_str"
prereqs() { echo "\$PREREQ"; }
case "\$1" in prereqs) prereqs; exit 0;; esac

. /scripts/functions

# Wait for framebuffer device (fbdev mode)
# efifb/vesafb may need a moment to initialize
INIT_TOP_EOF

    # Add device wait for fbdev mode
    if [[ $USE_DRM -eq 0 ]]; then
        cat >> "$inittop_tmp" << 'INIT_TOP_FBDEV_WAIT'
if [ ! -c /dev/fb0 ]; then
    # Wait up to 1 second for framebuffer to appear
    for i in 1 2 3 4 5; do
        sleep 0.2
        [ -c /dev/fb0 ] && break
    done
fi
INIT_TOP_FBDEV_WAIT
    fi

    cat >> "$inittop_tmp" << INIT_TOP_EOF

# Start splash in background
if [ -x /sbin/$BINARY ]; then
    /sbin/$BINARY &
    SPLASH_PID=\$!
    echo "\$SPLASH_PID" > /run/${BINARY}.pid
    # Store start_time for PID recycling protection
    # Field 22 in /proc/PID/stat is process start time in clock ticks
    START_TIME=\$(awk '{print \$22}' /proc/\$SPLASH_PID/stat 2>/dev/null)
    echo "\$START_TIME" > /run/${BINARY}.start_time
fi
INIT_TOP_EOF
    chmod +x "$inittop_tmp"
    mv "$inittop_tmp" /etc/initramfs-tools/scripts/init-top/$BINARY
    echo "  -> /etc/initramfs-tools/scripts/init-top/$BINARY"
    
    # Create init-bottom script (stops splash before switch_root)
    echo -e "${GREEN}[4/4]⚙ Creating init-bottom script...${NC}"
    install_step=4
    mkdir -p /etc/initramfs-tools/scripts/init-bottom
    # Backup existing init-bottom script if present
    if [[ -f /etc/initramfs-tools/scripts/init-bottom/$BINARY ]]; then
        cp /etc/initramfs-tools/scripts/init-bottom/$BINARY /etc/initramfs-tools/scripts/init-bottom/${BINARY}.bak
        echo "  -> Backed up existing init-bottom script"
    fi
    # Atomic write: write to temp then rename
    local initbottom_tmp="/etc/initramfs-tools/scripts/init-bottom/${BINARY}.tmp"
    cat > "$initbottom_tmp" << INIT_BOTTOM_EOF
#!/bin/sh
# Stop $BINARY before switching to real root

PREREQ=""
prereqs() { echo "\$PREREQ"; }
case "\$1" in prereqs) prereqs; exit 0;; esac

. /scripts/functions

INIT_BOTTOM_EOF

    # Add DRM-specific CRTC restore FIRST (before killing splash)
    # This prevents dangling CRTC if splash gets SIGKILL'd
    if [[ $USE_DRM -eq 1 ]]; then
        cat >> "$initbottom_tmp" << INIT_BOTTOM_DRM
# CRITICAL: Restore CRTC BEFORE killing splash process!
# If splash is killed by SIGKILL (frozen system), the dumb buffer is destroyed
# but CRTC still points to it, causing visual glitch (snow/noise).
# Restoring CRTC first ensures display is valid even if SIGKILL follows.
if [ -x /sbin/$BINARY ]; then
    /sbin/$BINARY --restore-crtc 2>/dev/null || true
fi

INIT_BOTTOM_DRM
    fi
    
    cat >> "$initbottom_tmp" << INIT_BOTTOM_KILL

# Stop splash animation safely
# Verify process name AND start_time before killing to avoid PID recycling race
# This makes the system absolutely bulletproof against PID recycling attacks
if [ -f /run/${BINARY}.pid ]; then
    PID=\$(cat /run/${BINARY}.pid 2>/dev/null)
    if [ -n "\$PID" ] && [ -d "/proc/\$PID" ]; then
        # Verify this is actually our binary (avoid PID recycling)
        COMM=\$(cat /proc/\$PID/comm 2>/dev/null)
        if [ "\$COMM" = "$BINARY" ]; then
            # Double-check with start_time (field 22 in /proc/PID/stat)
            # This makes PID recycling absolutely impossible
            SAVED_START=\$(cat /run/${BINARY}.start_time 2>/dev/null)
            CURRENT_START=\$(awk '{print \$22}' /proc/\$PID/stat 2>/dev/null)
            if [ -n "\$SAVED_START" ] && [ "\$SAVED_START" = "\$CURRENT_START" ]; then
                kill "\$PID" 2>/dev/null || true
            fi
        fi
    fi
    rm -f /run/${BINARY}.pid /run/${BINARY}.start_time
fi
INIT_BOTTOM_KILL

    cat >> "$initbottom_tmp" << 'INIT_BOTTOM_FBDEV'

# Clear framebuffer to black before handoff (fbdev mode)
if [ -c /dev/fb0 ]; then
    dd if=/dev/zero of=/dev/fb0 2>/dev/null || true
fi
INIT_BOTTOM_FBDEV
    chmod +x "$initbottom_tmp"
    mv "$initbottom_tmp" /etc/initramfs-tools/scripts/init-bottom/$BINARY
    echo "  -> /etc/initramfs-tools/scripts/init-bottom/$BINARY"
    
    # Check disk space on /boot before rebuilding initramfs
    echo ""
    echo -e "${GREEN}☉ Checking /boot disk space...${NC}"
    local boot_free=0
    if mountpoint -q /boot 2>/dev/null; then
        boot_free=$(df -k /boot 2>/dev/null | awk 'NR==2 {print $4}')
    else
        # /boot not mounted, check root
        boot_free=$(df -k / 2>/dev/null | awk 'NR==2 {print $4}')
    fi
    
    # Require at least 50MB free (initramfs can be 20-40MB + temp files)
    local min_free=51200  # 50MB in KB
    if [[ -n "$boot_free" && "$boot_free" -lt "$min_free" ]]; then
        local free_mb=$((boot_free / 1024))
        echo -e "  -> ${RED}Low disk space: ${free_mb}MB available${NC}"
        echo ""
        echo -e "${YELLOW}⚠ Insufficient disk space for initramfs rebuild${NC}"
        echo "  Required: at least 50MB free on /boot or /"
        echo "  Current:  ${free_mb}MB"
        echo ""
        echo "  Suggestions:"
        echo "    • Remove old kernels: 'sudo apt autoremove'"
        echo "    • Clean old initramfs: 'ls /boot/initrd.img-*'"
        echo "    • Check /boot usage: 'df -h /boot'"
        echo ""
        read -p "Continue anyway? [y/N] " -n 1 -r
        echo ""
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            echo -e "${YELLOW}Installation cancelled.${NC}"
            return 1
        fi
    else
        local free_mb=$((boot_free / 1024))
        echo -e "  -> ${free_mb}MB available (OK)"
    fi
    
    # Backup initramfs before rebuild (critical for recovery)
    initramfs_backup="${initramfs_path}.bak.$$"
    if [[ -f "$initramfs_path" ]]; then
        cp "$initramfs_path" "$initramfs_backup"
        echo "  -> Backed up initramfs to $initramfs_backup"
    fi
    
    # Rebuild initramfs
    echo ""
    echo -e "${GREEN}⚙ Rebuilding initramfs...${NC}"
    
    # Detect kernel version mismatch (user updated kernel but hasn't rebooted)
    local running_kernel=$(uname -r)
    local latest_kernel=""
    
    # Find latest installed kernel (Debian/Ubuntu)
    if [[ -d /boot ]]; then
        latest_kernel=$(ls /boot/vmlinuz-* 2>/dev/null | sort -V | tail -1 | sed 's|/boot/vmlinuz-||')
    fi
    
    # Warn if running kernel is older than latest installed
    if [[ -n "$latest_kernel" && "$running_kernel" != "$latest_kernel" ]]; then
        echo -e "  ${YELLOW}⚠ Warning: Running kernel ($running_kernel) differs from latest installed ($latest_kernel)${NC}"
        echo "     If you rebuild for running kernel only, the splash won't appear after reboot."
        echo ""
        echo -e "  ${CYAN}Options:${NC}"
        echo "     1) Rebuild for RUNNING kernel only ($running_kernel)"
        echo "     2) Rebuild for LATEST kernel only ($latest_kernel)"
        echo "     3) Rebuild for ALL installed kernels"
        echo ""
        echo -en "  ${BOLD}Choice [1/2/3] (default: 2): ${NC}"
        read choice 2>/dev/null || choice="2"
        
        case "$choice" in
            1)
                echo -e "  ${YELLOW}Building for running kernel only${NC}"
                KERNEL_TARGET="$running_kernel"
                ;;
            2)
                echo -e "  ${GREEN}Building for latest kernel ($latest_kernel)${NC}"
                KERNEL_TARGET="$latest_kernel"
                ;;
            3|*)
                echo -e "  ${GREEN}Building for ALL kernels${NC}"
                KERNEL_TARGET="all"
                ;;
        esac
        echo ""
    else
        KERNEL_TARGET="$running_kernel"
    fi
    
    # Update initramfs_path to match KERNEL_TARGET (may differ from running kernel)
    if [[ -n "$KERNEL_TARGET" && "$KERNEL_TARGET" != "$running_kernel" && "$KERNEL_TARGET" != "all" ]]; then
        initramfs_path="/boot/initrd.img-${KERNEL_TARGET}"
        [[ ! -f "$initramfs_path" ]] && initramfs_path="/boot/initramfs-${KERNEL_TARGET}.img"
        # Re-backup the TARGET kernel's initramfs (not running kernel's)
        initramfs_backup="${initramfs_path}.bak.$$"
        if [[ -f "$initramfs_path" ]]; then
            cp "$initramfs_path" "$initramfs_backup"
            echo "  -> Backed up target initramfs to $initramfs_backup"
        fi
    fi
    
    # Rebuild initramfs with appropriate kernel target
    local rebuild_cmd=""
    if [[ "$KERNEL_TARGET" == "all" ]]; then
        rebuild_cmd="update-initramfs -u -k all"
    elif [[ -n "$KERNEL_TARGET" && "$KERNEL_TARGET" != "$running_kernel" ]]; then
        rebuild_cmd="update-initramfs -u -k $KERNEL_TARGET"
    else
        rebuild_cmd="update-initramfs -u"
    fi
    
    if ! $rebuild_cmd; then
        echo -e "  -> ${RED}✖ FAILED!${NC}"
        # Restore initramfs backup immediately
        if [[ -f "$initramfs_backup" ]]; then
            mv "$initramfs_backup" "$initramfs_path"
            echo -e "  -> ${YELLOW}Restored initramfs from backup${NC}"
        fi
        echo ""
        echo -e "${YELLOW}Common causes and solutions:${NC}"
        echo "  • Disk full: Check 'df -h /boot' and free space"
        echo "  • Missing kernel modules: Ensure kernel headers match running kernel"
        echo "  • Corrupt initramfs config: Check /etc/initramfs-tools/initramfs.conf"
        echo ""
        echo -e "${YELLOW}Recovery: The initramfs has been restored. Fix the issue and retry.${NC}"
        install_rollback
        return 1
    fi
    echo "  -> Done"
    
    # Keep initramfs backup until the installation is fully committed.
    # This prevents ending up with an initramfs rebuilt with new hooks, while
    # disk files are rolled back after a later fatal error.
    
    # Verify initramfs integrity
    echo -e "${GREEN}☉ Verifying initramfs integrity...${NC}"
    local new_initramfs_size=$(stat -c%s "$initramfs_path" 2>/dev/null || echo 0)
    
    # Check that new initramfs exists and is reasonable size
    if [[ ! -f "$initramfs_path" ]]; then
        echo -e "  -> ${RED}✖ Initramfs file not found: $initramfs_path${NC}"
        install_rollback
        return 1
    fi
    
    # Check for significant size reduction (could indicate corruption)
    if [[ $old_initramfs_size -gt 0 && $new_initramfs_size -lt $((old_initramfs_size / 2)) ]]; then
        echo -e "  -> ${YELLOW}⚠ Warning: New initramfs is significantly smaller${NC}"
        echo "     Old: $old_initramfs_size bytes"
        echo "     New: $new_initramfs_size bytes"
        echo "     This could indicate corruption or missing modules."
    fi
    
    # Verify initramfs is readable (basic integrity check)
    # Support gzip, lz4, zstd (Fedora 40+), and xz compression
    if ! zcat "$initramfs_path" >/dev/null 2>&1 && \
       ! unlz4 "$initramfs_path" >/dev/null 2>&1 && \
       ! zstd -d "$initramfs_path" -o /dev/null 2>/dev/null && \
       ! xz -d < "$initramfs_path" >/dev/null 2>&1; then
        echo -e "  -> ${RED}✖ Initramfs appears corrupted (cannot decompress)${NC}"
        install_rollback
        return 1
    fi
    
    # Check that our binary is in the initramfs
    if command -v lsinitramfs &>/dev/null; then
        if lsinitramfs "$initramfs_path" 2>/dev/null | grep -q "sbin/$BINARY"; then
            echo -e "  -> ${GREEN}✓ Binary included in initramfs${NC}"
        else
            echo -e "  -> ${YELLOW}⚠ Warning: Binary not found in initramfs${NC}"
            echo "     Check hook script: /etc/initramfs-tools/hooks/$BINARY"
        fi
    else
        echo -e "  -> Initramfs size: $new_initramfs_size bytes (OK)"
    fi
    
    # Clear trap - installation successful
    install_completed=1
    trap - INT TERM EXIT
    
    # Check framebuffer availability
    echo ""
    echo -e "${GREEN}☉ Checking framebuffer...${NC}"
    if [[ -e /dev/fb0 ]]; then
        echo "  -> /dev/fb0 exists (OK)"
    else
        echo -e "  -> ${YELLOW}/dev/fb0 not found${NC}"
        echo "     Framebuffer may not be active. You might need to:"
        echo "     1. Add 'video=efifb' or 'video=vesafb' to kernel cmdline"
        echo "     2. Remove 'nomodeset' from kernel cmdline"
    fi
    
    # Offer to update GRUB
    if command -v update-grub &>/dev/null; then
        echo ""
        read -p "Run update-grub now? [Y/n] " -n 1 -r
        echo ""
        if [[ ! $REPLY =~ ^[Nn]$ ]]; then
            # Backup grub.cfg before modification
            if [[ -f /boot/grub/grub.cfg ]]; then
                grub_backup="/boot/grub/grub.cfg.bak.$$"
                cp /boot/grub/grub.cfg "$grub_backup"
                echo "  -> Backed up grub.cfg to $grub_backup"
            fi
            echo -e "${GREEN}⚙ Updating GRUB...${NC}"
            if update-grub; then
                echo "  -> Done"
                # Remove backup on success
                rm -f "$grub_backup"
                grub_backup=""
            else
                echo -e "  ${RED}✗ update-grub failed${NC}"
                if [[ -f "$grub_backup" ]]; then
                    echo -e "  ${YELLOW}⚠ grub.cfg backup available at: $grub_backup${NC}"
                fi
            fi
        fi
    fi
    
    echo ""
    echo -e "${GREEN}=== Installation Complete (Standard Method) ===${NC}"
    echo ""
    echo "Files installed:"
    echo "  /sbin/$BINARY"
    echo "  /etc/initramfs-tools/hooks/$BINARY"
    echo "  /etc/initramfs-tools/scripts/init-top/$BINARY"
    echo "  /etc/initramfs-tools/scripts/init-bottom/$BINARY"
    echo ""
    
    # Post-installation verification
    verify_installation

    # Installation fully finished: remove initramfs backup.
    if [[ -n "$initramfs_backup" ]]; then
        rm -f "$initramfs_backup" 2>/dev/null || true
    fi
}

# Custom installation method
install_custom() {
    echo -e "${YELLOW}=== Installing Custom Initramfs Method ===${NC}"
    echo ""
    echo -e "${RED}WARNING: This method requires manual configuration!${NC}"
    echo ""

    set -o pipefail

    local custom_completed=0
    local binary_src="$BINARY"
    local binary_name
    binary_name="$(basename -- "$BINARY")"
    if [[ "$binary_name" != "$BINARY" ]]; then
        BINARY="$binary_name"
    fi
    if [[ ! "$BINARY" =~ ^[A-Za-z0-9_]+$ ]]; then
        echo -e "${RED}ERROR: Invalid binary name: $BINARY${NC}"
        echo "Allowed characters: A-Z a-z 0-9 _"
        set +o pipefail
        return 1
    fi
    if [[ ${#BINARY} -gt 15 ]]; then
        echo -e "${RED}ERROR: Binary name too long: $BINARY${NC}"
        echo "Max 15 characters (kernel /proc/PID/comm limit)."
        set +o pipefail
        return 1
    fi
    echo -e "${BLUE}This method:${NC}"
    echo "  • Creates a separate custom initramfs"
    echo "  • Requires bootloader configuration"
    echo "  • Requires YOU to write the init script"
    echo "  • NOT compatible with LUKS/LVM/mdadm automatically"
    echo ""
    
    # Ask for initramfs directory
    local INITRAMFS_DIR=""
    echo -n "➤ Enter initramfs directory path: "
    read -r INITRAMFS_DIR
    
    if [[ -z "$INITRAMFS_DIR" ]]; then
        echo -e "${RED}ERROR: Directory path required${NC}"
        set +o pipefail
        return 1
    fi

    local OUTPUT_IMG="${INITRAMFS_DIR}.img"
    local INIT_SCRIPT="$INITRAMFS_DIR/init"
    local bin_dst="$INITRAMFS_DIR/sbin/$BINARY"
    local bin_bak=""
    local init_bak=""
    local img_bak=""
    local init_created=0

    custom_rollback() {
        local exit_code=$?
        trap - INT TERM EXIT
        if [[ $exit_code -ne 0 && $custom_completed -eq 0 ]]; then
            if [[ -n "$img_bak" && -f "$img_bak" ]]; then
                mv -f "$img_bak" "$OUTPUT_IMG" 2>/dev/null || true
            else
                rm -f "$OUTPUT_IMG" 2>/dev/null || true
            fi

            if [[ -n "$bin_bak" && -f "$bin_bak" ]]; then
                mv -f "$bin_bak" "$bin_dst" 2>/dev/null || true
            else
                rm -f "$bin_dst" 2>/dev/null || true
            fi

            if [[ $init_created -eq 1 ]]; then
                rm -f "$INIT_SCRIPT" 2>/dev/null || true
            elif [[ -n "$init_bak" && -f "$init_bak" ]]; then
                mv -f "$init_bak" "$INIT_SCRIPT" 2>/dev/null || true
            fi
        fi
        set +o pipefail
        exit $exit_code
    }

    trap 'custom_rollback' INT TERM EXIT
    
    # Create directory structure
    mkdir -p "$INITRAMFS_DIR"/{sbin,dev,proc,sys,etc,run}
    
    # Copy binary
    echo -e "${GREEN}[1/3]⚙ Copying binary...${NC}"
    if [[ ! -f "$binary_src" ]]; then
        echo -e "${RED}ERROR: Binary not found: $binary_src${NC}"
        return 1
    fi
    if [[ -f "$bin_dst" ]]; then
        bin_bak="${bin_dst}.bak.$$"
        cp -f "$bin_dst" "$bin_bak"
    fi
    cp -f "$binary_src" "$bin_dst"
    chmod +x "$bin_dst"
    echo "  -> $bin_dst"
    
    # Create framebuffer device
    echo -e "${GREEN}[2/3]⚙ Creating device nodes...${NC}"
    if [[ ! -e "$INITRAMFS_DIR/dev/fb0" ]]; then
        mknod "$INITRAMFS_DIR/dev/fb0" c 29 0 2>/dev/null || true
    fi
    echo "  -> $INITRAMFS_DIR/dev/fb0"
    
    # Check for existing init
    if [[ -f "$INIT_SCRIPT" ]]; then
        init_bak="${INIT_SCRIPT}.bak.$$"
        cp -f "$INIT_SCRIPT" "$init_bak"
    fi
    
    if [[ -f "$INIT_SCRIPT" ]]; then
        echo -e "${GREEN}[3/3]☉ Found existing init script${NC}"
        echo "  -> $INIT_SCRIPT"
        echo ""
        echo -e "${YELLOW}⚠ IMPORTANT: You must add the following to your init script:${NC}"
        echo ""
        echo "  # After mounting /dev:"
        echo "  /sbin/$BINARY &"
        echo "  SPLASH_PID=\$!"
        echo ""
        echo "  # Before switch_root:"
        echo "  kill \$SPLASH_PID"
        echo ""
    else
        echo -e "${YELLOW}[3/3]⚙ Creating minimal init script template${NC}"
        cat > "$INIT_SCRIPT" << INIT_EOF
#!/bin/sh
# Minimal init script with bootsplash
#=================================== CUSTOMIZE THIS FOR YOUR SYSTEM ====================================

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
/sbin/$BINARY &
SPLASH_PID=\$!

# --------------------------------- CUSTOMIZE HERE -----------------------------------------
# Add your root mount logic:
# - Load modules (modprobe)
# - Mount root filesystem
# - Handle LUKS/LVM/mdadm if needed
#
# Example for simple root:
#   mount /dev/sda1 /mnt
#   kill \$SPLASH_PID
#   exec switch_root /mnt /sbin/init
#
# Example for LUKS:
#   cryptsetup luksOpen /dev/sda2 cryptroot
#   mount /dev/mapper/cryptroot /mnt
#   kill \$SPLASH_PID
#   exec switch_root /mnt /sbin/init
# -----------------------------------------------------------------------------------------

# If no root mounted, drop to shell (for debugging)
echo "No root mounted. Dropping to shell."
exec /bin/sh
INIT_EOF
        chmod +x "$INIT_SCRIPT"
        init_created=1
        echo "  -> $INIT_SCRIPT (template created)"
        echo ""
        echo -e "${YELLOW}⚠ IMPORTANT: Edit $INIT_SCRIPT and add your root mount logic!${NC}"
    fi
    
    # Build initramfs image
    echo ""
    echo -e "${GREEN}⚙ Building initramfs image...${NC}"
    if [[ -f "$OUTPUT_IMG" ]]; then
        img_bak="${OUTPUT_IMG}.bak.$$"
        cp -f "$OUTPUT_IMG" "$img_bak"
    fi
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

    custom_completed=1
    trap - INT TERM EXIT
    rm -f "$bin_bak" "$init_bak" "$img_bak" 2>/dev/null || true
    set +o pipefail
}

# Shutdown splash installation (systemd-shutdown method)
install_shutdown() {
    echo -e "${GREEN}=== Installing Shutdown Splash (systemd-shutdown) ===${NC}"
    echo ""
    
    # Auto-detect DRM mode from binary if not set
    if [[ -z "$USE_DRM" ]] && [[ -n "$BINARY" ]]; then
        if ldd "$BINARY" 2>/dev/null | grep -q libdrm; then
            USE_DRM=1
        else
            USE_DRM=0
        fi
    fi
    
    echo -e "${BLUE}This method:${NC}"
    echo "  ✓ Ultra-minimal: no systemd unit, no dependencies"
    echo "  ✓ Executes at the very last moment before poweroff/reboot"
    echo ""
    echo -e "${YELLOW}Important: Shutdown splash requires a static binary${NC}"
    echo "  At shutdown time, systemd pivots to tmpfs and shared libraries"
    echo "  may be unavailable (separate /usr, network rootfs, etc.)."
    echo ""
    echo "  The kernel exposes /dev/fb0 via DRM fbdev emulation even on"
    echo "  'DRM-only' systems, so the fbdev static binary always works."
    echo ""
    
    # Check binary dependencies - MUST be static for shutdown
    echo -e "${CYAN}[1/3]☉ Checking binary compatibility...${NC}"
    local deps
    deps=$(ldd "$BINARY" 2>/dev/null)
    local is_static=false
    
    if echo "$deps" | grep -q "not a dynamic executable\|statically linked"; then
        is_static=true
        echo "  -> Binary is statically linked ✓"
    elif echo "$deps" | grep -qE "libdrm"; then
        echo -e "  ${RED}✖ Binary is DRM (dynamic) - not suitable for shutdown${NC}"
        echo ""
        echo -e "${YELLOW}The shutdown splash requires a static fbdev binary.${NC}"
        echo "  DRM binaries depend on libdrm.so which may be unavailable"
        echo "  at shutdown time (filesystem teardown, separate /usr, etc.)."
        echo ""
        echo -e "${CYAN}Options:${NC}"
        echo "  1) Rebuild fbdev static binary for shutdown (same animation data)"
        echo "  2) Cancel and build manually with 'make fbdev'"
        echo ""
        echo -en "${YELLOW}➤ Rebuild fbdev version for shutdown? [${CYAN}Y${YELLOW}/n]: ${NC}"
        read -r rebuild_choice
        
        if [[ ! "$rebuild_choice" =~ ^[Nn]$ ]]; then
            echo ""
            echo -e "${CYAN}Rebuilding fbdev static binary...${NC}"
            
            # Save current USE_DRM
            local saved_use_drm="$USE_DRM"
            
            # Force fbdev build
            USE_DRM=0
            make clean >/dev/null 2>&1 || true
            
            # Rebuild the binary with fbdev backend
            if ! make fbdev TARGET="$BINARY" >/dev/null 2>build.log; then
                echo -e "${RED}✖ Failed to rebuild fbdev binary${NC}"
                cat build.log
                USE_DRM="$saved_use_drm"
                return 1
            fi
            
            echo -e "${GREEN}✓ fbdev static binary ready${NC}"
        else
            echo -e "${RED}✖ Shutdown installation cancelled${NC}"
            return 1
        fi
    else
        # Has some dynamic libs but not libdrm (e.g., libc)
        echo -e "  ${YELLOW}⚠ Binary has dynamic dependencies:${NC}"
        echo "$deps" | grep "=>" | head -5 | while read line; do
            echo "    $line"
        done
        echo ""
        echo -e "  ${YELLOW}Warning: These may not be available at shutdown time.${NC}"
        echo -en "${YELLOW}➤ Continue anyway? [y/${CYAN}N${YELLOW}]: ${NC}"
        read -r force_continue
        if [[ ! "$force_continue" =~ ^[Yy]$ ]]; then
            echo -e "${RED}✖ Shutdown installation cancelled${NC}"
            return 1
        fi
    fi
    
    # Install binary
    echo -e "${GREEN}[2/3]⚙ Installing binary to /lib/systemd/system-shutdown...${NC}"
    mkdir -p /lib/systemd/system-shutdown
    
    # Install binary directly in system-shutdown directory
    # This guarantees availability even if /usr is unmounted at shutdown time
    local shutdown_binary="/lib/systemd/system-shutdown/$BINARY"
    
    if [[ -f "$shutdown_binary" ]]; then
        cp "$shutdown_binary" "${shutdown_binary}.bak"
        echo "  -> Backed up existing binary"
    fi
    
    cp "$BINARY" "$shutdown_binary"
    chmod +x "$shutdown_binary"
    echo "  -> $shutdown_binary"
    
    # Create systemd-shutdown script
    echo -e "${GREEN}[3/3]⚙ Creating systemd-shutdown script...${NC}"
    mkdir -p /lib/systemd/system-shutdown
    
    cat > /lib/systemd/system-shutdown/bootsplash.shutdown << 'SHUTDOWN_EOF'
#!/bin/sh
# Bootsplash shutdown animation
# Executed by systemd at the very end of shutdown/reboot
# Binary is in same directory for guaranteed availability
#
# NOTE: Watchdog is handled internally by the C binary via alarm(5)
# This is more reliable than shell-based sleep/kill under heavy I/O load

# Get directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Find and run the shutdown binary
for bin in "$SCRIPT_DIR"/xbs_*; do
    if [ -x "$bin" ]; then
        # Run splash with SHUTDOWN_MODE to enable internal watchdog
        # Binary will auto-terminate after 5 seconds via alarm(5)
        # This avoids shell sleep/kill issues under heavy I/O during shutdown
        SHUTDOWN_MODE=1 "$bin"
        break
    fi
done
SHUTDOWN_EOF
    
    chmod +x /lib/systemd/system-shutdown/bootsplash.shutdown
    echo "  -> /lib/systemd/system-shutdown/bootsplash.shutdown"
    
    echo ""
    echo -e "${GREEN}=== Shutdown Splash Installation Complete ===${NC}"
    echo ""
    echo "Files installed:"
    echo "  $shutdown_binary"
    echo "  /lib/systemd/system-shutdown/bootsplash.shutdown"
    echo ""
    echo -e "${YELLOW}Notes:${NC}"
    echo "  • Animation runs during final shutdown phase"
    echo "  • Duration limited by systemd timeout (typically 2-3 sec visible)"
    echo "  • Works for both poweroff and reboot"
    echo "  • Test with: sudo systemctl poweroff"
    echo ""
}

# Uninstall
do_uninstall() {
    local revert_mode="${1:-false}"
    
    echo -e "${YELLOW}=== Uninstalling xbootsplash ===${NC}"
    echo ""
    
    local removed=0
    local restored_backup=false
    
    # Find all xbs_* binaries installed (boot)
    echo -e "${CYAN}☉ Searching for installed bootsplash binaries (xbs_*)...${NC}"
    local installed_binaries=()
    for bin in /sbin/xbs_*; do
        if [[ -f "$bin" ]]; then
            installed_binaries+=("$(basename "$bin")")
        fi
    done
    
    # Also check for shutdown binaries in systemd-shutdown directory
    local shutdown_binaries=()
    for bin in /lib/systemd/system-shutdown/xbs_*; do
        if [[ -f "$bin" ]]; then
            shutdown_binaries+=("$(basename "$bin")")
        fi
    done
    
    if [ ${#installed_binaries[@]} -eq 0 ] && [ ${#shutdown_binaries[@]} -eq 0 ]; then
        echo -e "${YELLOW}No xbs_* binaries found in /sbin or /lib/systemd/system-shutdown${NC}"
    else
        if [ ${#installed_binaries[@]} -gt 0 ]; then
            echo -e "${GREEN}Found ${#installed_binaries[@]} boot binary(ies) in /sbin:${NC}"
            for bin in "${installed_binaries[@]}"; do
                echo "  - $bin (boot)"
            done
        fi
        if [ ${#shutdown_binaries[@]} -gt 0 ]; then
            echo -e "${GREEN}Found ${#shutdown_binaries[@]} shutdown binary(ies) in /lib/systemd/system-shutdown:${NC}"
            for bin in "${shutdown_binaries[@]}"; do
                echo "  - $bin (shutdown)"
            done
        fi
        echo ""
        
        # Ask which to remove or remove all
        echo -en "${YELLOW}➤ Remove all? [${CYAN}Y${YELLOW}/n/s for selective]: "
        read -r remove_choice
        case "$remove_choice" in
            [nN])
                echo "Cancelling uninstall."
                return 0
                ;;
            [sS])
                # Selective removal - choose which binary to remove
                echo ""
                echo "Select which binary to remove:"
                local idx=1
                local all_bins=()
                for bin in "${installed_binaries[@]}"; do
                    echo "  $idx) $bin (boot)"
                    all_bins+=("$bin")
                    idx=$((idx + 1))
                done
                for bin in "${shutdown_binaries[@]}"; do
                    echo "  $idx) $bin (shutdown)"
                    all_bins+=("$bin")
                    idx=$((idx + 1))
                done
                echo "  0) Cancel"
                echo ""
                echo -en "${YELLOW}➤ Enter number: "
                read -r selection
                if [[ "$selection" =~ ^[0-9]+$ ]] && [[ "$selection" -ge 1 ]] && [[ "$selection" -le ${#all_bins[@]} ]]; then
                    local selected_bin="${all_bins[$((selection-1))]}"
                    echo ""
                    echo -e "${GREEN}Removing: $selected_bin...${NC}"
                    
                    # Check if it's a boot or shutdown binary
                    if [[ " ${installed_binaries[*]} " =~ " ${selected_bin} " ]]; then
                        # Boot binary
                        rm -f /sbin/$selected_bin
                        rm -f /sbin/${selected_bin}.bak 2>/dev/null || true
                        rm -f /etc/initramfs-tools/hooks/$selected_bin 2>/dev/null || true
                        rm -f /etc/initramfs-tools/hooks/${selected_bin}.bak 2>/dev/null || true
                        rm -f /etc/initramfs-tools/scripts/init-top/$selected_bin 2>/dev/null || true
                        rm -f /etc/initramfs-tools/scripts/init-top/${selected_bin}.bak 2>/dev/null || true
                        rm -f /etc/initramfs-tools/scripts/init-bottom/$selected_bin 2>/dev/null || true
                        rm -f /etc/initramfs-tools/scripts/init-bottom/${selected_bin}.bak 2>/dev/null || true
                        rm -f /run/${selected_bin}.pid 2>/dev/null || true
                        rm -f /run/${selected_bin}.start_time 2>/dev/null || true
                        rm -f /run/xbs_drm_crtc.info 2>/dev/null || true
                        echo "  -> Removed boot binary and associated files"
                        removed=$((removed + 1))
                    else
                        # Shutdown binary
                        rm -f /lib/systemd/system-shutdown/$selected_bin
                        rm -f /lib/systemd/system-shutdown/${selected_bin}.bak 2>/dev/null || true
                        echo "  -> Removed shutdown binary"
                        removed=$((removed + 1))
                    fi
                else
                    echo "Cancelled."
                    return 0
                fi
                ;;
            *)
                # Remove boot binaries
                for bin in "${installed_binaries[@]}"; do
                    echo -e "${GREEN}Removing boot splash: $bin...${NC}"
                    
                    # Restore backup if exists and in revert mode
                    if [[ "$revert_mode" == "true" ]] && [[ -f /sbin/${bin}.bak ]]; then
                        mv /sbin/${bin}.bak /sbin/$bin
                        echo "  -> Restored /sbin/$bin from backup"
                        restored_backup=true
                    elif [[ "$restored_backup" == "false" ]]; then
                        rm -f /sbin/$bin
                        echo "  -> Removed /sbin/$bin"
                    fi
                    
                    # Remove backup file
                    rm -f /sbin/${bin}.bak 2>/dev/null || true
                    
                    # Remove hook and its backup
                    rm -f /etc/initramfs-tools/hooks/$bin 2>/dev/null && echo "  -> Removed hook" || true
                    rm -f /etc/initramfs-tools/hooks/${bin}.bak 2>/dev/null || true
                    
                    # Remove init-top script and its backup
                    rm -f /etc/initramfs-tools/scripts/init-top/$bin 2>/dev/null && echo "  -> Removed init-top" || true
                    rm -f /etc/initramfs-tools/scripts/init-top/${bin}.bak 2>/dev/null || true
                    
                    # Remove init-bottom script and its backup
                    rm -f /etc/initramfs-tools/scripts/init-bottom/$bin 2>/dev/null && echo "  -> Removed init-bottom" || true
                    rm -f /etc/initramfs-tools/scripts/init-bottom/${bin}.bak 2>/dev/null || true
                    
                    # Remove PID file
                    rm -f /run/${bin}.pid 2>/dev/null || true
                    rm -f /run/${bin}.start_time 2>/dev/null || true
                    
                    # Remove CRTC state file (DRM mode) - fixed path in splash_anim_drm.c
                    rm -f /run/xbs_drm_crtc.info 2>/dev/null || true
                    
                    removed=$((removed + 1))
                done
                
                # Remove shutdown binaries
                for bin in "${shutdown_binaries[@]}"; do
                    echo -e "${GREEN}Removing shutdown splash: $bin...${NC}"
                    rm -f /lib/systemd/system-shutdown/$bin
                    echo "  -> Removed /lib/systemd/system-shutdown/$bin"
                    rm -f /lib/systemd/system-shutdown/${bin}.bak 2>/dev/null || true
                    removed=$((removed + 1))
                done
                
                # Remove systemd-shutdown script
                if [[ -f /lib/systemd/system-shutdown/bootsplash.shutdown ]]; then
                    rm -f /lib/systemd/system-shutdown/bootsplash.shutdown
                    echo "  -> Removed /lib/systemd/system-shutdown/bootsplash.shutdown"
                fi
                ;;
        esac
    fi
    
    # Clean up dracut modules (Fedora/RHEL/Arch)
    if [[ -d /usr/lib/dracut/modules.d/90bootsplash ]]; then
        echo -e "${GREEN}Removing dracut bootsplash module...${NC}"
        rm -rf /usr/lib/dracut/modules.d/90bootsplash
        echo "  -> Removed /usr/lib/dracut/modules.d/90bootsplash"
        removed=$((removed + 1))
    fi
    
    # Clean up mkinitcpio hooks (Arch Linux)
    if [[ -f /etc/initcpio/hooks/bootsplash ]] || [[ -f /etc/initcpio/install/bootsplash ]]; then
        echo -e "${GREEN}Removing mkinitcpio bootsplash hooks...${NC}"
        rm -f /etc/initcpio/hooks/bootsplash 2>/dev/null && echo "  -> Removed /etc/initcpio/hooks/bootsplash" || true
        rm -f /etc/initcpio/install/bootsplash 2>/dev/null && echo "  -> Removed /etc/initcpio/install/bootsplash" || true
        removed=$((removed + 1))
    fi
    
    # Check for custom initramfs installations
    echo ""
    echo -e "${CYAN}☉ Checking for custom initramfs installations...${NC}"
    local custom_found=()
    for dir in /boot/*.img /boot/initrd*.img /boot/initramfs*.img; do
        if [[ -f "$dir" ]] && lsinitramfs "$dir" 2>/dev/null | grep -q "xbs_"; then
            custom_found+=("$dir")
        fi
    done
    
    if [ ${#custom_found[@]} -gt 0 ]; then
        echo -e "${YELLOW}☉ Found bootsplash in custom initramfs images:${NC}"
        for img in "${custom_found[@]}"; do
            echo "  - $img"
        done
        echo ""
        echo -e "${YELLOW}➤ These need to be rebuilt manually or removed.${NC}"
        echo "  Example: sudo rm -f /boot/custom-initrd.img"
    fi
    
    # Rebuild initramfs
    if [[ $removed -gt 0 ]]; then
        echo ""
        echo -e "${GREEN}⚙ Rebuilding initramfs...${NC}"
        
        # Backup initramfs before rebuild (critical for recovery)
        local running_kernel=$(uname -r)
        local initramfs_path="/boot/initrd.img-${running_kernel}"
        [[ ! -f "$initramfs_path" ]] && initramfs_path="/boot/initramfs-${running_kernel}.img"
        local initramfs_backup="${initramfs_path}.bak.$$"
        if [[ -f "$initramfs_path" ]]; then
            cp "$initramfs_path" "$initramfs_backup"
            echo "  -> Backed up initramfs to $initramfs_backup"
        fi
        
        # Detect kernel version mismatch (user updated kernel but hasn't rebooted)
        local latest_kernel=""
        if [[ -d /boot ]]; then
            latest_kernel=$(ls /boot/vmlinuz-* 2>/dev/null | sort -V | tail -1 | sed 's|/boot/vmlinuz-||')
        fi
        
        # Determine kernel target for rebuild
        local KERNEL_TARGET="$running_kernel"
        if [[ -n "$latest_kernel" && "$running_kernel" != "$latest_kernel" ]]; then
            echo -e "  ${YELLOW}⚠ Warning: Running kernel ($running_kernel) differs from latest installed ($latest_kernel)${NC}"
            echo ""
            echo -e "  ${CYAN}Options:${NC}"
            echo "     1) Rebuild for RUNNING kernel only ($running_kernel)"
            echo "     2) Rebuild for LATEST kernel only ($latest_kernel)"
            echo "     3) Rebuild for ALL installed kernels"
            echo ""
            echo -en "  ${BOLD}Choice [1/2/3] (default: 3): ${NC}"
            read choice 2>/dev/null || choice="3"
            
            case "$choice" in
                1)
                    echo -e "  ${YELLOW}Rebuilding for running kernel only${NC}"
                    KERNEL_TARGET="$running_kernel"
                    ;;
                2)
                    echo -e "  ${GREEN}Rebuilding for latest kernel ($latest_kernel)${NC}"
                    KERNEL_TARGET="$latest_kernel"
                    ;;
                3|*)
                    echo -e "  ${GREEN}Rebuilding for ALL kernels${NC}"
                    KERNEL_TARGET="all"
                    ;;
            esac
        fi
        
        local rebuild_ok=false
        
        # Try update-initramfs (Debian/Ubuntu)
        if command -v update-initramfs &>/dev/null; then
            local rebuild_cmd=""
            if [[ "$KERNEL_TARGET" == "all" ]]; then
                rebuild_cmd="update-initramfs -u -k all"
            elif [[ -n "$KERNEL_TARGET" && "$KERNEL_TARGET" != "$(uname -r)" ]]; then
                rebuild_cmd="update-initramfs -u -k $KERNEL_TARGET"
            else
                rebuild_cmd="update-initramfs -u"
            fi
            if $rebuild_cmd; then
                echo "  -> Done (update-initramfs)"
                rebuild_ok=true
            else
                echo -e "  -> ${RED}✖ update-initramfs FAILED!${NC}"
                # Restore from backup
                if [[ -f "$initramfs_backup" ]]; then
                    mv "$initramfs_backup" "$initramfs_path"
                    echo -e "  -> ${YELLOW}Restored initramfs from backup${NC}"
                fi
            fi
        # Try mkinitcpio (Arch Linux)
        elif command -v mkinitcpio &>/dev/null; then
            if mkinitcpio -P; then
                echo "  -> Done (mkinitcpio)"
                rebuild_ok=true
            else
                echo -e "  -> ${RED}✖ mkinitcpio FAILED!${NC}"
                # Restore from backup
                if [[ -f "$initramfs_backup" ]]; then
                    mv "$initramfs_backup" "$initramfs_path"
                    echo -e "  -> ${YELLOW}Restored initramfs from backup${NC}"
                fi
            fi
        # Try dracut (Fedora/RHEL)
        elif command -v dracut &>/dev/null; then
            local kernel_ver="$KERNEL_TARGET"
            [[ "$kernel_ver" == "all" ]] && kernel_ver=$(uname -r)
            if dracut --force /boot/initramfs-${kernel_ver}.img ${kernel_ver}; then
                echo "  -> Done (dracut)"
                rebuild_ok=true
            else
                echo -e "  -> ${RED}✖ dracut FAILED!${NC}"
                # Restore from backup
                if [[ -f "$initramfs_backup" ]]; then
                    mv "$initramfs_backup" "$initramfs_path"
                    echo -e "  -> ${YELLOW}Restored initramfs from backup${NC}"
                fi
            fi
        else
            echo -e "  -> ${YELLOW}⚠ No supported initramfs builder found${NC}"
            echo "     Supported: update-initramfs (Debian), mkinitcpio (Arch), dracut (Fedora)"
            echo "     Please rebuild initramfs manually."
        fi
        
        if [[ "$rebuild_ok" == "true" ]]; then
            # Verify binary was removed from initramfs
            local current_kernel=$(uname -r)
            local initramfs_path="/boot/initrd.img-${current_kernel}"
            [[ ! -f "$initramfs_path" ]] && initramfs_path="/boot/initramfs-${current_kernel}.img"
            
            if [[ -f "$initramfs_path" ]] && command -v lsinitramfs &>/dev/null; then
                if lsinitramfs "$initramfs_path" 2>/dev/null | grep -q "sbin/xbs_"; then
                    echo -e "  -> ${YELLOW}⚠ Warning: xbs_* binary still found in initramfs${NC}"
                    echo "     This may be a cached image. Try: update-initramfs -u -k all"
                else
                    echo -e "  -> ${GREEN}✓ Verified: binary removed from initramfs${NC}"
                fi
            elif [[ -f "$initramfs_path" ]] && command -v lsinitcpio &>/dev/null; then
                # Arch Linux uses lsinitcpio
                if lsinitcpio "$initramfs_path" 2>/dev/null | grep -q "sbin/xbs_"; then
                    echo -e "  -> ${YELLOW}⚠ Warning: xbs_* binary still found in initramfs${NC}"
                else
                    echo -e "  -> ${GREEN}✓ Verified: binary removed from initramfs${NC}"
                fi
            fi
        else
            echo -e "${RED}initramfs rebuild failed. Check disk space and kernel version.${NC}"
            if [[ "$restored_backup" == "true" ]]; then
                echo -e "${YELLOW}Backup was restored, but initramfs may be outdated.${NC}"
            fi
            return 1
        fi
    fi
    
    echo ""
    if [[ $removed -gt 0 ]]; then
        echo -e "${GREEN}=== Uninstall Complete ===${NC}"
        echo "Removed $removed file(s)"
    else
        echo -e "${YELLOW}No xbootsplash files found to remove${NC}"
    fi
}

# Generate GIF preview
generate_preview() {
    print_step "7" "Generate preview"
    
    # Skip for static modes - offer PNG instead
    if [ $DISPLAY_MODE -eq 3 ] || [ $DISPLAY_MODE -eq 4 ]; then
        if ask_yes_no "Generate PNG preview (800px width)?"; then
            print_info "Generating preview PNG..."
            
            # Default screen size if not detected
            local sw=${SCREEN_W:-1920}
            local sh=${SCREEN_H:-1080}
            [ "$sw" -eq 0 ] && sw=1920
            [ "$sh" -eq 0 ] && sh=1080
            
            # Target preview width
            local pw=800
            local ph=$(( sh * pw / sw ))
            
            local out_file="${BINARY}_preview.png"
            
            # Mode 4: full screen image - just resize it
            if [ $DISPLAY_MODE -eq 4 ] && [ -f "$FRAME_DIR" ]; then
                if convert "$FRAME_DIR" -resize "${pw}x${ph}" "$out_file" 2>/dev/null; then
                    print_success "Preview generated: $out_file ($(du -h "$out_file" | cut -f1))"
                else
                    print_error "Failed to generate preview"
                fi
            # Mode 3: static image on solid background
            elif [ -f "$FRAME_DIR" ]; then
                local scale=$(( pw * 100 / sw ))
                local off_x=$(( FRAME_OFFSET_X * scale / 100 ))
                local off_y=$(( FRAME_OFFSET_Y * scale / 100 ))
                
                if convert -size "${pw}x${ph}" "xc:#${BG_COLOR}" \
                    "$FRAME_DIR" -resize "${scale}%" \
                    -gravity Center -geometry "+${off_x}+${off_y}" \
                    -composite "$out_file" 2>/dev/null; then
                    print_success "Preview generated: $out_file ($(du -h "$out_file" | cut -f1))"
                else
                    print_error "Failed to generate preview"
                fi
            fi
        fi
        return 0
    fi
    
    # Animation modes (0, 1, 2)
    if ask_yes_no "Generate animated GIF preview (800px width)?"; then
        print_info "⚙ Generating preview GIF..."
        
        # Default screen size if not detected
        local sw=${SCREEN_W:-1920}
        local sh=${SCREEN_H:-1080}
        [ "$sw" -eq 0 ] && sw=1920
        [ "$sh" -eq 0 ] && sh=1080
        
        # Target preview width
        local pw=800
        local ph=$(( sh * pw / sw ))
        
        # Scale percentage for resizing elements
        local scale=$(( pw * 100 / sw ))
        
        # GIF delay (centiseconds = ms / 10)
        local gif_delay=$(( FRAME_DELAY / 10 ))
        [ "$gif_delay" -lt 1 ] && gif_delay=1
        
        # Loop setting for GIF: 0 = infinite, 1 = once
        # For partial loop, we can't easily represent it in GIF, so use infinite loop
        local loop_setting=0
        [ "$LOOP_MODE" -eq 0 ] && loop_setting=1
        
        local out_file="${BINARY}_preview.gif"
        
        # Build frame list sorted to ensure correct order
        local frames_list=($(find "$FRAME_DIR" -maxdepth 1 -name "*.png" -type f | sort))
        local frame_count=${#frames_list[@]}
        
        print_info "Processing $frame_count frames with ${FRAME_DELAY}ms delay..."
        
        # Get frame dimensions (use first frame as reference)
        local first_frame="${frames_list[0]}"
        local frame_info=$(identify -format "%w %h" "$first_frame" 2>/dev/null)
        local frame_w=$(echo "$frame_info" | cut -d' ' -f1)
        local frame_h=$(echo "$frame_info" | cut -d' ' -f2)
        
        # Calculate scaled frame size
        local scaled_w=$(( frame_w * scale / 100 ))
        local scaled_h=$(( frame_h * scale / 100 ))
        
        # Calculate center position in preview, then add offset (like bootsplash does)
        # bootsplash: y = (screen_h - frame_h) / 2 + VERTICAL_OFFSET
        local center_x=$(( (pw - scaled_w) / 2 ))
        local center_y=$(( (ph - scaled_h) / 2 ))
        
        # Apply offsets (relative to center, like bootsplash)
        local off_x=$(( center_x + FRAME_OFFSET_X * scale / 100 ))
        local off_y=$(( center_y + FRAME_OFFSET_Y * scale / 100 ))
        
        # Create temporary background (WYSIWYG composition like runtime)
        local tmp_bg="tmp_bg.png"
        convert -size "${pw}x${ph}" "xc:#${BG_COLOR}" "$tmp_bg" || {
            print_error "Failed to create background"
            return 1
        }

        if [ $DISPLAY_MODE -eq 1 ] || [ $DISPLAY_MODE -eq 2 ]; then
            # Scale background with the same factor as frames (preserve aspect)
            local tmp_bg_img="tmp_bg_img.png"
            if ! convert "$BG_IMAGE" -resize "${scale}%" "$tmp_bg_img" 2>/dev/null; then
                print_error "Failed to resize background image"
                rm -f "$tmp_bg" "$tmp_bg_img"
                return 1
            fi

            local bg_px=0
            local bg_py=0
            if [ $DISPLAY_MODE -eq 1 ]; then
                # Centered background + scaled offsets
                local bg_w=0
                local bg_h=0
                bg_w=$(identify -format "%w" "$tmp_bg_img" 2>/dev/null | head -1)
                bg_h=$(identify -format "%h" "$tmp_bg_img" 2>/dev/null | head -1)
                [ -z "$bg_w" ] && bg_w=0
                [ -z "$bg_h" ] && bg_h=0
                local bg_center_x=$(( (pw - bg_w) / 2 ))
                local bg_center_y=$(( (ph - bg_h) / 2 ))
                local bg_off_x=$(( BG_OFFSET_X * scale / 100 ))
                local bg_off_y=$(( BG_OFFSET_Y * scale / 100 ))
                bg_px=$(( bg_center_x + bg_off_x ))
                bg_py=$(( bg_center_y + bg_off_y ))
            fi

            # Composite background image onto canvas
            if ! convert "$tmp_bg" "$tmp_bg_img" -gravity NorthWest -geometry "+${bg_px}+${bg_py}" -composite "$tmp_bg" 2>/dev/null; then
                print_error "Failed to composite background image"
                rm -f "$tmp_bg" "$tmp_bg_img"
                return 1
            fi
            rm -f "$tmp_bg_img"
        fi

        # Create temporary directory for composited frames
        local tmp_dir="tmp_frames_$$"
        mkdir -p "$tmp_dir"
        
        # Composite each frame onto background (WYSIWYG - simulates framebuffer composition)
        local i=0
        local composite_failed=false
        for f in "${frames_list[@]}"; do
            # Build geometry string (handle negative offsets correctly for ImageMagick)
            local geom=""
            if [[ $off_x -ge 0 && $off_y -ge 0 ]]; then
                geom="+${off_x}+${off_y}"
            elif [[ $off_x -ge 0 && $off_y -lt 0 ]]; then
                geom="+${off_x}${off_y}"
            elif [[ $off_x -lt 0 && $off_y -ge 0 ]]; then
                geom="${off_x}+${off_y}"
            else
                geom="${off_x}${off_y}"
            fi
            
            # Composite frame on background at scaled offset
            if ! convert "$tmp_bg" \
                \( "$f" -resize "${scale}%" \) \
                -gravity NorthWest -geometry "$geom" \
                -composite \
                "${tmp_dir}/frame_$(printf '%04d' $i).png" 2>/dev/null; then
                print_warning "Failed to composite frame: $(basename "$f")"
                composite_failed=true
            fi
            i=$((i + 1))
        done
        
        # Check if any frames were composited
        if [[ ! -f "${tmp_dir}/frame_0000.png" ]]; then
            rm -rf "$tmp_dir" "$tmp_bg"
            print_error "No frames could be composited"
            return 1
        fi
        
        # For partial loop mode, create one-shot animation showing loop effect
        # GIF doesn't support partial loops, so we append loop section 3 times
        if [ "$LOOP_MODE" -eq 2 ] && [ -n "$LOOP_START" ] && [ "$LOOP_START" -gt 0 ]; then
            print_info "Partial loop: appending frames $LOOP_START→end 3 times for preview..."
            
            local loop_frames=()
            # Collect loop section frames (LOOP_START to end)
            for f in "${tmp_dir}"/frame_*.png; do
                local idx=$(basename "$f" | sed 's/frame_0*//' | sed 's/.png//')
                # Skip if idx is not a valid number
                [ -z "$idx" ] || ! [[ "$idx" =~ ^[0-9]+$ ]] && continue
                if [ "$idx" -ge "$LOOP_START" ]; then
                    loop_frames+=("$f")
                fi
            done
            
            # Append loop section 3 times
            local append_idx=$frame_count
            for rep in 1 2 3; do
                for f in "${loop_frames[@]}"; do
                    local src_idx=$(basename "$f" | sed 's/frame_0*//' | sed 's/.png//')
                    cp "$f" "${tmp_dir}/frame_$(printf '%04d' $append_idx).png"
                    append_idx=$((append_idx + 1))
                done
            done
            
            local total_frames=$append_idx
            print_info "Preview will have $total_frames frames (original + 3× loop section)"
        fi
        
        # Assemble composited frames into GIF (no loop for one-shot preview)
        local gif_loop=0
        [ "$LOOP_MODE" -eq 0 ] && gif_loop=1  # No loop mode: play once
        local cmd=(convert -delay "$gif_delay" -loop "$gif_loop" "${tmp_dir}"/frame_*.png -layers Optimize "$out_file")

        if "${cmd[@]}" 2>build.log; then
            rm -rf "$tmp_dir" "$tmp_bg"
            local fsize=$(du -h "$out_file" | cut -f1)
            local display_count=${total_frames:-$frame_count}
            print_success "Preview generated: $out_file ($fsize, $display_count frames)"
            [[ "$composite_failed" == "true" ]] && print_warning "Some frames failed to composite"
            case $LOOP_MODE in
                0) print_info "Loop: No (stops at last frame)" ;;
                1) print_info "Loop: Full (0→N, 0→N...)" ;;
                2) print_info "Loop: Partial (0→N, then $LOOP_START→N...)" ;;
            esac
        else
            rm -rf "$tmp_dir" "$tmp_bg"
            print_error "Failed to generate preview GIF"
            [[ -s build.log ]] && cat build.log
            return 1
        fi
    else
        print_info "Skipping preview generation."
    fi
}

# Install animation
install_animation() {
    print_step "8" "Installation & packaging"
    
    if [ ! -f "$BINARY" ]; then
        print_error "Binary not found: $BINARY"
        print_info "Build it first with the build step"
        return 1
    fi
    
    echo ""
    echo -e "╭───────────────────────────────────────────────────────────────────╮"
    echo -e "│  ${CYAN}1)${NC} Boot splash      - Install for boot (initramfs)               │"
    echo -e "│  ${CYAN}2)${NC} Shutdown splash  - Install for shutdown (systemd-shutdown)    │"
    echo -e "│  ${CYAN}3)${NC} Both             - Install for boot AND shutdown              │"
    echo -e "│  ${CYAN}4)${NC} Packaging        - Create a package with the generated splash │"
    echo -e "│  ${CYAN}Q)${NC} Quit             - Exit script                                │"
    echo -e "╰───────────────────────────────────────────────────────────────────╯"
    echo -n " ➤ Select option [1]: "
    read -r install_type
    
    case "$install_type" in
        1|"")
            install_boot_menu
            ;;
        2)
            # Check root for shutdown installation
            if [[ $EUID -ne 0 ]]; then
                print_info "✜ This option requires root privileges"
                print_info "✜ Re-running with sudo..."
                # Convert BINARY to absolute path to avoid CWD issues under sudo
                local abs_binary="$(cd "$(dirname "$BINARY")" 2>/dev/null && pwd)/$(basename "$BINARY")"
                exec sudo "$0" --install-existing "$abs_binary" --install-type shutdown
            fi
            install_shutdown
            ;;
        3)
            # Check root for both installations
            if [[ $EUID -ne 0 ]]; then
                print_info "✜ This option requires root privileges"
                print_info "✜ Re-running with sudo..."
                # Convert BINARY to absolute path to avoid CWD issues under sudo
                local abs_binary="$(cd "$(dirname "$BINARY")" 2>/dev/null && pwd)/$(basename "$BINARY")"
                exec sudo "$0" --install-existing "$abs_binary" --install-type both
            fi
            echo ""
            echo -e "${CYAN}═══════════════════════════════════════${NC}"
            echo -e "${CYAN}   Part 1/2: Boot Splash Installation   ${NC}"
            echo -e "${CYAN}═══════════════════════════════════════${NC}"
            install_boot_menu
            echo ""
            echo -e "${CYAN}═══════════════════════════════════════${NC}"
            echo -e "${CYAN}   Part 2/2: Shutdown Splash Installation${NC}"
            echo -e "${CYAN}═══════════════════════════════════════${NC}"
            install_shutdown
            ;;
        4)
            export_xbs_package
            ;;
        [Qq])
            echo -e "\n${GREEN}${BOLD}✓ Done!${NC}\n"
            exit 0
            ;;
        *)
            install_boot_menu
            ;;
    esac
}

# Export splash as distributable .xbs package
export_xbs_package() {
    print_step "9" "Export as .xbs package"
    
    if [ ! -f "$BINARY" ]; then
        print_error "Binary not found: $BINARY"
        return 1
    fi
    
    # Derive splash name from binary filename (strip xbs_ prefix if present)
    local binary_basename=$(basename "$BINARY")
    local splash_name="${binary_basename#xbs_}"
    [[ -z "$splash_name" ]] && splash_name="splash"
    
    # Sanitize splash name: only alphanumeric and underscore allowed
    local sanitized_name=$(echo "$splash_name" | tr -cd 'A-Za-z0-9_')
    if [[ "$sanitized_name" != "$splash_name" ]]; then
        echo -e "${YELLOW}⚠ Sanitized splash name: '$splash_name' -> '$sanitized_name'${NC}"
        splash_name="$sanitized_name"
    fi
    [[ -z "$splash_name" ]] && splash_name="splash"
    
    local backend_str="fbdev"
    [[ $USE_DRM -eq 1 ]] && backend_str="drm"
    local resolution="${SCREEN_W:-1920}x${SCREEN_H:-1080}"
    local pkg_name="${splash_name}_${backend_str}_${resolution}.xbs"
    
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}   XBootsplash Package Exporter        ${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo -e "${CYAN}Package name:${NC}    $pkg_name"
    echo -e "${CYAN}Splash name:${NC}     $splash_name"
    echo -e "${CYAN}Backend:${NC}         $backend_str"
    echo -e "${CYAN}Resolution:${NC}      $resolution"
    echo ""
    
    if ! ask_yes_no "Create .xbs package?"; then
        print_info "Skipping package export"
        return 0
    fi
    
    # Create temporary build directory
    local tmp_dir=$(mktemp -d /tmp/xbs_pkg_build.XXXXXX)
    local pkg_dir="packages"
    
    print_info "Creating package structure..."
    
    # Copy binary with its actual name
    local binary_name=$(basename "$BINARY")
    cp "$BINARY" "$tmp_dir/$binary_name" || {
        print_error "Failed to copy binary"
        rm -rf "$tmp_dir"
        return 1
    }
    
    # Ensure preview exists
    local preview_file="${BINARY}_preview.gif"
    local preview_png="${BINARY}_preview.png"
    
    # For static modes, check PNG preview
    if [ $DISPLAY_MODE -eq 3 ] || [ $DISPLAY_MODE -eq 4 ]; then
        preview_file="$preview_png"
    fi
    
    if [ ! -f "$preview_file" ]; then
        print_info "Generating preview (required for package)..."
        
        # Generate preview based on mode
        if [ $DISPLAY_MODE -eq 3 ] || [ $DISPLAY_MODE -eq 4 ]; then
            # Static mode - generate PNG preview
            local sw=${SCREEN_W:-1920}
            local sh=${SCREEN_H:-1080}
            [ "$sw" -eq 0 ] && sw=1920
            [ "$sh" -eq 0 ] && sh=1080
            local pw=800
            local ph=$(( sh * pw / sw ))
            
            if [ $DISPLAY_MODE -eq 4 ] && [ -f "$FRAME_DIR" ]; then
                convert "$FRAME_DIR" -resize "${pw}x${ph}" "$preview_png" 2>/dev/null
            elif [ -f "$FRAME_DIR" ]; then
                local scale=$(( pw * 100 / sw ))
                local off_x=$(( FRAME_OFFSET_X * scale / 100 ))
                local off_y=$(( FRAME_OFFSET_Y * scale / 100 ))
                convert -size "${pw}x${ph}" "xc:#${BG_COLOR}" \
                    "$FRAME_DIR" -resize "${scale}%" \
                    -gravity Center -geometry "+${off_x}+${off_y}" \
                    -composite "$preview_png" 2>/dev/null
            fi
            preview_file="$preview_png"
        else
            # Animation mode - generate GIF preview
            local preview_dir="${FRAME_DIR:-frames}"
            if [ -d "$preview_dir" ]; then
                local delay=$(( 100 / ${FPS:-30} ))
                convert -delay $delay -loop 0 "$preview_dir"/*.png -resize 800x480\> "$preview_file" 2>/dev/null
            fi
        fi
    fi
    
    # Copy preview (mandatory)
    if [ -f "$preview_file" ]; then
        cp "$preview_file" "$tmp_dir/preview.gif" 2>/dev/null || cp "$preview_file" "$tmp_dir/preview.png" 2>/dev/null
        # Rename to preview.gif for consistency
        if [ -f "$tmp_dir/preview.png" ]; then
            mv "$tmp_dir/preview.png" "$tmp_dir/preview.gif"
        fi
    else
        print_error "Preview generation failed - cannot create package without preview"
        rm -rf "$tmp_dir"
        return 1
    fi
    
    # Calculate SHA256 of binary
    local sha256_hash=$(sha256sum "$BINARY" | cut -d' ' -f1)
    
    # Calculate actual frame count for animations
    local actual_nframes=1
    if [ $DISPLAY_MODE -le 2 ] && [ -d "$FRAME_DIR" ]; then
        actual_nframes=$(find "$FRAME_DIR" -maxdepth 1 -type f \( -name "*.png" -o -name "*.PNG" -o -name "*.jpg" -o -name "*.JPG" \) 2>/dev/null | wc -l)
    fi
    
    # Calculate FPS from delay
    local fps_value=$((1000/FRAME_DELAY))
    
    # Get relative frame directory name
    local frame_dir_name=""
    if [ -n "$FRAME_DIR" ] && [ -d "$FRAME_DIR" ]; then
        frame_dir_name="/$(basename "$FRAME_DIR")"
    fi
    
    # Generate metadata.conf
    cat > "$tmp_dir/metadata.conf" << METADATA_EOF
# XBootsplash Package Metadata
XBS_PKG_VERSION="1.0"
SPLASH_NAME="$splash_name"
BACKEND="$backend_str"
RESOLUTION="$resolution"
DISPLAY_MODE="$DISPLAY_MODE"
BG_COLOR="$BG_COLOR"
FRAME_W="$FRAME_W"
FRAME_H="$FRAME_H"
NFRAMES="$actual_nframes"
FRAME_DELAY="$FRAME_DELAY"
FPS="$fps_value"
FRAME_OFFSET_X="$FRAME_OFFSET_X"
FRAME_OFFSET_Y="$FRAME_OFFSET_Y"
BG_OFFSET_X="$BG_OFFSET_X"
BG_OFFSET_Y="$BG_OFFSET_Y"
BINARY_SHA256="$sha256_hash"
METADATA_EOF
    
    # Add optional source frame directory (relative name only)
    if [ -n "$frame_dir_name" ]; then
        echo "# Source frame directory: $frame_dir_name" >> "$tmp_dir/metadata.conf"
    fi
    
    # Create packages directory if needed
    mkdir -p "$pkg_dir"
    
    # Create the .xbs archive
    print_info "Creating archive..."
    if tar -czf "$pkg_dir/$pkg_name" -C "$tmp_dir" .; then
        local pkg_size=$(du -h "$pkg_dir/$pkg_name" | cut -f1)
        print_success "Package created: $pkg_dir/$pkg_name ($pkg_size)"
        echo ""
        echo -e "${CYAN}Package contents:${NC}"
        tar -tzf "$pkg_dir/$pkg_name"
        echo ""
        echo -e "${CYAN}Package metadata:${NC}"
        tar -xzf "$pkg_dir/$pkg_name" -O ./metadata.conf 2>/dev/null | sed 's/^/  /'
    else
        print_error "Failed to create package archive"
        rm -rf "$tmp_dir"
        return 1
    fi
    
    # Cleanup
    rm -rf "$tmp_dir"
    
    echo ""
    echo -e "${GREEN}Package ready for distribution!${NC}"
    echo -e "${CYAN}Install with:${NC} sudo $0 --install-package $pkg_dir/$pkg_name"
}

# Parse metadata.conf securely (avoid code injection)
parse_metadata_secure() {
    local meta_file="$1"
    local key value
    
    while IFS='=' read -r key value; do
        # Skip comments and empty lines
        [[ "$key" =~ ^[[:space:]]*# ]] && continue
        [[ -z "$key" ]] && continue
        
        # Only allow alphanumeric keys with underscores
        if [[ "$key" =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]]; then
            # Remove surrounding quotes from value
            value="${value#\"}"
            value="${value%\"}"
            value="${value#\'}"
            value="${value%\'}"
            # Export as environment variable
            printf -v "$key" '%s' "$value"
            export "$key"
        fi
    done < "$meta_file"
}

# Install from .xbs package file
install_from_package() {
    local pkg_file="$1"
    
    if [ ! -f "$pkg_file" ]; then
        print_error "Package file not found: $pkg_file"
        return 1
    fi
    
    # Check for .xbs extension
    if [[ "$pkg_file" != *.xbs ]]; then
        print_error "Invalid package format. Expected .xbs file"
        return 1
    fi
    
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}   XBootsplash Package Installer       ${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    
    # Create temporary extraction directory
    local tmp_dir=$(mktemp -d /tmp/xbs_extract.XXXXXX)
    
    print_info "Extracting package..."
    if ! tar -xzf "$pkg_file" -C "$tmp_dir"; then
        print_error "Failed to extract package"
        rm -rf "$tmp_dir"
        return 1
    fi
    
    # Verify required files exist
    if [ ! -f "$tmp_dir/splash_bin" ]; then
        print_error "Invalid package: missing splash_bin"
        rm -rf "$tmp_dir"
        return 1
    fi
    
    if [ ! -f "$tmp_dir/metadata.conf" ]; then
        print_error "Invalid package: missing metadata.conf"
        rm -rf "$tmp_dir"
        return 1
    fi
    
    # Parse metadata securely
    print_info "Reading package metadata..."
    
    # Initialize variables with defaults
    XBS_PKG_VERSION=""
    SPLASH_NAME=""
    BACKEND=""
    RESOLUTION=""
    DISPLAY_MODE=""
    BG_COLOR=""
    FRAME_W=""
    FRAME_H=""
    NFRAMES=""
    FRAME_DELAY=""
    FPS=""
    BINARY_SHA256=""
    
    parse_metadata_secure "$tmp_dir/metadata.conf"
    
    # Verify SHA256 if present
    if [ -n "$BINARY_SHA256" ]; then
        print_info "Verifying binary integrity..."
        local actual_hash=$(sha256sum "$tmp_dir/splash_bin" | cut -d' ' -f1)
        if [ "$actual_hash" != "$BINARY_SHA256" ]; then
            print_error "SHA256 checksum mismatch!"
            echo -e "  ${YELLOW}Expected:${NC} $BINARY_SHA256"
            echo -e "  ${YELLOW}Actual:${NC}   $actual_hash"
            rm -rf "$tmp_dir"
            return 1
        fi
        print_success "Checksum verified"
    fi
    
    # Backend compatibility check
    echo ""
    print_info "Checking system compatibility..."
    
    local system_backend="fbdev"
    if [ -d /sys/module/drm ] || command -v drm-info &>/dev/null || [ -n "$LIBDRM_AVAILABLE" ]; then
        system_backend="drm"
    fi
    
    local backend_warning=""
    if [ "${BACKEND}" = "fbdev" ] && [ "$system_backend" = "drm" ]; then
        backend_warning="Package uses fbdev backend but system supports DRM"
    elif [ "${BACKEND}" = "drm" ] && [ "$system_backend" = "fbdev" ]; then
        backend_warning="Package uses DRM backend but system only has fbdev"
    fi
    
    # Resolution warning
    local current_res="${SCREEN_W:-unknown}x${SCREEN_H:-unknown}"
    local res_warning=""
    if [ -n "$RESOLUTION" ] && [ "$RESOLUTION" != "$current_res" ] && [ "$current_res" != "unknownxunknown" ]; then
        res_warning="Package optimized for $RESOLUTION, your screen is $current_res"
    fi
    
    # Translate DISPLAY_MODE to human readable
    local mode_desc="Unknown"
    case "$DISPLAY_MODE" in
        0) mode_desc="Animation on solid background" ;;
        1) mode_desc="Animation on background image (centered)" ;;
        2) mode_desc="Animation on background image (fullscreen)" ;;
        3) mode_desc="Static image on solid background" ;;
        4) mode_desc="Static image fullscreen" ;;
    esac
    
    # Display package info
    echo ""
    echo -e "${BLUE}══════════════════════════════════════${NC}"
    echo -e "${BLUE}       Package Information            ${NC}"
    echo -e "${BLUE}══════════════════════════════════════${NC}"
    echo -e "  ${YELLOW}Name:${NC}        ${SPLASH_NAME:-unknown}"
    echo -e "  ${YELLOW}Backend:${NC}     ${BACKEND:-unknown}"
    echo -e "  ${YELLOW}Resolution:${NC}  ${RESOLUTION:-unknown}"
    echo -e "  ${YELLOW}Mode:${NC}        $mode_desc"
    echo -e "  ${YELLOW}Frames:${NC}      ${NFRAMES:-1}"
    echo -e "  ${YELLOW}Timing:${NC}      ${FRAME_DELAY:-33} ms (${FPS:-30} FPS)"
    
    # Show warnings if any
    if [ -n "$backend_warning" ]; then
        echo ""
        echo -e "  ${YELLOW}⚠ Warning: $backend_warning${NC}"
    fi
    if [ -n "$res_warning" ]; then
        echo ""
        echo -e "  ${YELLOW}⚠ Note: $res_warning${NC}"
    fi
    if [ -z "$backend_warning" ] && [ -z "$res_warning" ]; then
        echo ""
        echo -e "  ${GREEN}✓ All checks passed${NC}"
    fi
    
    # Interactive menu for install/preview/quit
    while true; do
        echo ""
        echo -e "    ╭────────────────────────────────────╮"
        echo -e "    │  ${CYAN}1)${NC} Install this splash theme      │"
        echo -e "    │  ${CYAN}2)${NC} Display splash preview         │"
        echo -e "    │  ${CYAN}Q)${NC} Quit                           │"
        echo -e "    ╰────────────────────────────────────╯"
        echo -en "   ${YELLOW}➤ Select option [${CYAN}1${YELLOW}]: ${NC}"
        read -r menu_choice
        
        case "$menu_choice" in
            1|"")
                # Install
                break
                ;;
            2)
                # Preview with xdg-open in background
                if [ -f "$tmp_dir/preview.gif" ]; then
                    ( xdg-open "$tmp_dir/preview.gif" 2>/dev/null & )
                    print_info "Opening preview..."
                else
                    print_error "No preview available in package"
                fi
                ;;
            [Qq])
                print_info "Installation cancelled"
                rm -rf "$tmp_dir"
                return 0
                ;;
            *)
                print_error "Invalid option"
                ;;
        esac
    done
    
    # Set BINARY to extracted splash_bin
    BINARY="$tmp_dir/splash_bin"
    
    # Set USE_DRM based on package backend
    if [ "${BACKEND}" = "drm" ]; then
        USE_DRM=1
    else
        USE_DRM=0
    fi
    
    # Proceed with installation
    echo ""
    install_animation
    
    # Cleanup
    rm -rf "$tmp_dir"
}

# Boot splash installation menu (existing logic)
install_boot_menu() {
    echo ""
    echo -e "${GREEN}Select boot installation method:${NC}"
    echo -e " ╭───────────────────────────────────────────────────────────────╮"
    echo -e " │  ${CYAN}1)${NC} Standard   - Debian/Ubuntu initramfs-tools (${GREEN}RECOMMENDED${NC})  │"
    echo -e " │  ${CYAN}2)${NC} Custom     - Full custom initramfs (advanced)             │"
    echo -e " │  ${CYAN}3)${NC} Uninstall  - Remove bootsplash from system                │"
    echo -e " │  ${CYAN}I)${NC} Info       - Learn about each installation method         │"
    echo -e " │  ${CYAN}Q)${NC} Quit       - Skip boot installation                       │"
    echo -e " ╰───────────────────────────────────────────────────────────────╯"
    echo -en "${YELLOW}➤ Select option [${CYAN}1${YELLOW}]: "
    read -r choice
    
    case "$choice" in
        1|"")
            # Check root
            if [[ $EUID -ne 0 ]]; then
                print_info "✜ This option requires root privileges"
                print_info "✜ Re-running with sudo..."
                local abs_binary="$(cd "$(dirname "$BINARY")" 2>/dev/null && pwd)/$(basename "$BINARY")"
                exec sudo "$0" --install-existing "$abs_binary" --install-type standard
            fi
            check_plymouth
            install_standard
            ;;
        2)
            if [[ $EUID -ne 0 ]]; then
                print_error "Custom installation requires root privileges."
                print_info "Please run: sudo $0"
                print_info "Then select option 2 again."
                return 1
            fi
            check_plymouth
            install_custom
            ;;
        3)
            if [[ $EUID -ne 0 ]]; then
                print_info "✜ This option requires root privileges"
                print_info "✜ Re-running with sudo..."
                exec sudo "$0" --uninstall-only
            fi
            do_uninstall
            ;;
        [Ii])
            show_install_info
            install_boot_menu
            ;;
        [Qq])
            print_info "Skipping boot installation"
            ;;
        *)
            if [[ $EUID -ne 0 ]]; then
                local abs_binary="$(cd "$(dirname "$BINARY")" 2>/dev/null && pwd)/$(basename "$BINARY")"
                exec sudo "$0" --install-existing "$abs_binary" --install-type standard
            fi
            check_plymouth
            install_standard
            ;;
    esac
}

# Install existing xbs_* binary
install_existing_binary() {
    echo -e "\n${BLUE}}═══════════════════════════════════════${NC}"
    echo -e "${BLUE}   Install Existing Bootsplash Binary  ${NC}"
    echo -e "${BLUE}}═══════════════════════════════════════${NC}"
    echo ""
    
    # Ask for directory to search
    echo -e "${CYAN}➤ Enter directory to search for xbs_* binaries:${NC}"
    echo -e "  ${YELLOW}(or press Enter for current directory: $(pwd))${NC}"
    echo -n "> "
    read -r search_dir
    
    # Default to current directory
    if [ -z "$search_dir" ]; then
        search_dir="$(pwd)"
    fi
    
    # Expand path
    search_dir="${search_dir/#\~/$HOME}"
    
    # Check directory exists
    if [ ! -d "$search_dir" ]; then
        print_error "Directory not found: $search_dir"
        return 1
    fi
    
    # Find xbs_* binaries
    echo ""
    echo -e "${CYAN}☉ Searching for xbs_* binaries in $search_dir...${NC}"
    
    local binaries=()
    local paths=()
    
    while IFS= read -r -d '' file; do
        if [ -f "$file" ] && [ -x "$file" ]; then
            binaries+=("$(basename "$file")")
            paths+=("$file")
        fi
    done < <(find "$search_dir" -maxdepth 1 -name "xbs_*" -type f -print0 2>/dev/null | sort -z)
    
    if [ ${#binaries[@]} -eq 0 ]; then
        print_error "No xbs_* binaries found in $search_dir"
        echo ""
        echo -e "${YELLOW}Tip: xbs_* binaries are created when you build a splash animation.${NC}"
        echo -e "${YELLOW}Run option 1 to build a new splash first.${NC}"
        return 1
    fi
    
    # Display found binaries
    echo ""
    echo -e " ${GREEN}☉ Found ${#binaries[@]} binary(ies):${NC}"
    echo    " ╭──────────────────────────────────────────────────────╮"
    echo    " │                                                      │"
    
    local i=1
    for bin in "${binaries[@]}"; do
        local size=$(wc -c < "${paths[$((i-1))]}" 2>/dev/null || echo "unknown")
        local size_kb=$((size / 1024))
        echo -e " │  ${CYAN}$i)${NC} $bin ${YELLOW}(${size_kb} KB)${NC}"
        i=$((i + 1))
    done
    
    echo " │                                                      │"
    echo -e " │  ${CYAN}Q)${NC} Quit (return to main menu)                       │"
    echo    " ╰──────────────────────────────────────────────────────╯"
    
    # Select binary
    echo -en " ${YELLOW}➤ Select binary to install [${CYAN}1${YELLOW}]: "
    read -r selection
    
    case "$selection" in
        [Qq])
            print_info "Returning to main menu..."
            return 0
            ;;
        "")
            selection=1
            ;;
    esac
    
    # Validate selection
    if ! [[ "$selection" =~ ^[0-9]+$ ]] || [ "$selection" -lt 1 ] || [ "$selection" -gt ${#binaries[@]} ]; then
        print_error "Invalid selection"
        return 1
    fi
    
    # Set BINARY to selected path
    local selected_idx=$((selection - 1))
    BINARY="${paths[$selected_idx]}"
    local binary_name="${binaries[$selected_idx]}"
    
    echo ""
    echo -e "${GREEN}Selected: $binary_name${NC}"
    echo -e "${CYAN}Path: $BINARY${NC}"
    echo ""
    
    # Check root
    if [[ $EUID -ne 0 ]]; then
        print_info "✜ Installation requires root privileges"
        print_info "✜ Re-running with sudo..."
        # BINARY is already an absolute path from find, but ensure it
        local abs_binary="$(cd "$(dirname "$BINARY")" 2>/dev/null && pwd)/$(basename "$BINARY")"
        exec sudo "$0" --install-existing "$abs_binary"
    fi
    
    # Proceed with installation
    check_plymouth
    
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}   Bootsplash Binary Installer         ${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo -e "${GREEN}Select installation method:${NC}"
    echo ""
    echo -e "  ${CYAN}1)${NC} Standard   - Debian/Ubuntu initramfs-tools (${GREEN}RECOMMENDED${NC})"
    echo -e "  ${CYAN}2)${NC} Custom     - Full custom initramfs (advanced)"
    echo -e "  ${CYAN}I)${NC} Info       - Learn about each installation method"
    echo -e "  ${CYAN}Q)${NC} Quit       - Skip installation"
    echo ""
    echo -en "${YELLOW}➤ Select option [${CYAN}1${YELLOW}]: "
    read -r choice
    
    case "$choice" in
        1|"")
            install_standard
            ;;
        2)
            install_custom
            ;;
        [Ii])
            show_install_info
            ;;
        [Qq])
            print_info "Skipping installation"
            print_info "You can manually copy $BINARY to your initramfs"
            ;;
        *)
            install_standard
            ;;
    esac
}

# Main
main() {
    print_header

    # Check dependencies first
    check_dependencies
    
    # Check for libdrm and set default mode
    print_info "  ${WHITE}☉${CYAN} Checking libdrm..."
    check_libdrm
    if [[ $LIBDRM_AVAILABLE -eq 1 ]]; then
        if [[ $USE_DRM_EXPLICIT -eq 0 ]]; then
            USE_DRM=1  # Default to DRM if available
        fi
        echo -e "    ${GREEN}✜ libdrm detected, using DRM/KMS by default${NC}"
        echo -e "    ${CYAN}  (you can switch to fbdev via option T)${NC}"
    else
        USE_DRM=0
        echo -e "    ${YELLOW}☉ libdrm not detected, using fbdev (/dev/fb0)${NC}"
    fi
    
    # Show main menu (with optional toggle)
    if [ "$NONINTERACTIVE_BUILD" -eq 1 ] && [ -n "$FRAME_DIR" ]; then
        run_noninteractive_build
        return
    fi

    show_main_menu
}

# Show current bootsplash status (boot and shutdown)
show_bootsplash_status() {
    echo ""
    echo -e "${BLUE}════════════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}               BOOTSPLASH SYSTEM STATUS                         ${NC}"
    echo -e "${BLUE}════════════════════════════════════════════════════════════════${NC}"
    
    # === BOOT PROCESS STATUS ===
    echo -e "${BLUE}║${NC} ${BOLD}${CYAN}BOOT PROCESS:${NC}"
    
    local boot_status="No splash"
    local boot_details=""
    
    # Check for xbs_* binary in /sbin (initramfs boot splash)
    local xbs_boot=""
    if [[ -d /etc/initramfs-tools/scripts/init-top ]]; then
        xbs_boot=$(find /etc/initramfs-tools/scripts/init-top -name "xbs_*" -type f 2>/dev/null | head -1)
    fi
    if [[ -n "$xbs_boot" ]]; then
        local bin_name=$(basename "$xbs_boot")
        boot_status="xbsbootsplash"
        boot_details="binary: $bin_name"
    fi
    
    # Check for Plymouth
    if command -v plymouthd &>/dev/null || [[ -f /usr/share/plymouth/themes/default.plymouth ]]; then
        if [[ "$boot_status" != "No splash" ]]; then
            boot_details="$boot_details + plymouth detected"
        else
            boot_status="Plymouth"
            boot_details="theme: $(basename $(readlink /usr/share/plymouth/themes/default.plymouth 2>/dev/null) 2>/dev/null || echo 'default')"
        fi
    fi
    
    # Check for custom fbcondecor/fbsplash (Gentoo/Sabayon)
    if [[ -f /etc/conf.d/splash ]]; then
        if [[ "$boot_status" == "No splash" ]]; then
            boot_status="fbsplash"
            boot_details="config: /etc/conf.d/splash"
        fi
    fi
    
    # Check kernel cmdline for splash parameters
    local cmdline_splash=""
    if [[ -f /proc/cmdline ]]; then
        cmdline_splash=$(cat /proc/cmdline 2>/dev/null | grep -oE 'splash|plymouth\.[^ ]*|bootsplash\.[^ ]*' | head -3)
    fi
    
    echo -e "${NC}   Status: ${GREEN}$boot_status${NC}"
    [[ -n "$boot_details" ]] && echo -e "${BLUE}║${NC}   Details: $boot_details"
    [[ -n "$cmdline_splash" ]] && echo -e "${BLUE}║${NC}   Kernel params: $cmdline_splash"
    
    # === SHUTDOWN PROCESS STATUS ===
    echo -e "${NC}"
    echo -e "${NC} ${BOLD}${CYAN}SHUTDOWN PROCESS:${NC}"
    
    local shutdown_status="No splash"
    local shutdown_details=""
    
    # Check for xbs_* in /lib/systemd/system-shutdown (shutdown splash)
    local xbs_shutdown=""
    if [[ -d /lib/systemd/system-shutdown ]]; then
        xbs_shutdown=$(find /lib/systemd/system-shutdown -name "xbs_*" -type f -executable 2>/dev/null | head -1)
    fi
    if [[ -n "$xbs_shutdown" ]]; then
        local shutdown_bin=$(basename "$xbs_shutdown")
        shutdown_status="xbsbootsplash"
        shutdown_details="binary: $shutdown_bin"
    fi
    
    # Check for systemd-shutdown script
    if [[ -f /lib/systemd/system-shutdown/bootsplash.shutdown ]]; then
        if [[ "$shutdown_status" != "No splash" ]]; then
            shutdown_details="$shutdown_details + systemd hook"
        else
            shutdown_status="Custom script"
            shutdown_details="systemd-shutdown/bootsplash.shutdown"
        fi
    fi
    
    # Check for Plymouth shutdown (Plymouth handles shutdown via systemd integration)
    # If Plymouth is installed and active, it handles both boot and shutdown
    if [[ "$boot_status" == "Plymouth" ]] || command -v plymouthd &>/dev/null || [[ -d /usr/share/plymouth ]]; then
        if [[ "$shutdown_status" == "No splash" ]]; then
            shutdown_status="Plymouth"
            shutdown_details="systemd integration"
        fi
    fi
    
    echo -e "${NC}   Status: ${GREEN}$shutdown_status${NC}"
    [[ -n "$shutdown_details" ]] && echo -e "${BLUE}║${NC}   Details: $shutdown_details"
    
    # === INSTALLED FILES SUMMARY ===
    echo -e ""
    echo -e "${NC} ${BOLD}${CYAN}INSTALLED FILES:${NC}"
    
    local has_files=false
    
    # Initramfs hooks
    if [[ -d /etc/initramfs-tools/hooks ]]; then
        local hooks=$(find /etc/initramfs-tools/hooks -name "xbs_*" -type f 2>/dev/null)
        if [[ -n "$hooks" ]]; then
            echo -e "${BLUE}║${NC}   Boot hooks:"
            for h in $hooks; do
                echo -e "${BLUE}║${NC}     - $h"
            done
            has_files=true
        fi
    fi
    
    # Initramfs scripts
    if [[ -d /etc/initramfs-tools/scripts/init-top ]]; then
        local inittop=$(find /etc/initramfs-tools/scripts/init-top -name "xbs_*" -type f 2>/dev/null)
        if [[ -n "$inittop" ]]; then
            echo -e "${NC}   Init-top scripts:"
            for s in $inittop; do
                echo -e "${NC}     - $s"
            done
            has_files=true
        fi
    fi
    
    if [[ -d /etc/initramfs-tools/scripts/init-bottom ]]; then
        local initbottom=$(find /etc/initramfs-tools/scripts/init-bottom -name "xbs_*" -type f 2>/dev/null)
        if [[ -n "$initbottom" ]]; then
            echo -e "${NC}   Init-bottom scripts:"
            for s in $initbottom; do
                echo -e "${NC}     - $s"
            done
            has_files=true
        fi
    fi
    
    # /sbin binaries
    local sbin_bins=$(find /sbin -name "xbs_*" -type f -executable 2>/dev/null)
    if [[ -n "$sbin_bins" ]]; then
        echo -e "${NC}   Boot binaries (/sbin):"
        for b in $sbin_bins; do
            local bsize=$(du -h "$b" 2>/dev/null | cut -f1)
            echo -e "${NC}     - $b ($bsize)"
        done
        has_files=true
    fi
    
    # Shutdown binaries
    local shutdown_bins=$(find /lib/systemd/system-shutdown -name "xbs_*" -type f -executable 2>/dev/null)
    if [[ -n "$shutdown_bins" ]]; then
        echo -e "${NC}   Shutdown binaries (/lib/systemd/system-shutdown):"
        for b in $shutdown_bins; do
            local bsize=$(du -h "$b" 2>/dev/null | cut -f1)
            echo -e "${NC}     - $b ($bsize)"
        done
        has_files=true
    fi
    
    # Systemd shutdown script
    if [[ -f /lib/systemd/system-shutdown/bootsplash.shutdown ]]; then
        echo -e "${NC}   Systemd shutdown: /lib/systemd/system-shutdown/bootsplash.shutdown"
        has_files=true
    fi
    
    if [[ "$has_files" == "false" ]]; then
        echo -e "${NC}   ${YELLOW}No xbsbootsplash files installed${NC}"
    fi
    
    echo -e "${BLUE}════════════════════════════════════════════════════════════════${NC}"
    echo ""
}

# Uninstall ALL detected splash systems
uninstall_all_splash() {
    echo ""
    echo -e "${RED}${BOLD}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}${BOLD}║         UNINSTALL ALL SPLASH SYSTEMS                         ║${NC}"
    echo -e "${RED}${BOLD}╚══════════════════════════════════════════════════════════════╝${NC}"
    
    # Show current status first
    show_bootsplash_status
    
    # Detect what needs to be removed
    local to_remove=()
    local has_plymouth=false
    local has_xbs=false
    local has_fbsplash=false
    
    # Check for xbsbootsplash
    local xbs_files=""
    xbs_files=$(find /etc/initramfs-tools/hooks -name "xbs_*" -type f 2>/dev/null)
    xbs_files="$xbs_files $(find /etc/initramfs-tools/scripts/init-top -name "xbs_*" -type f 2>/dev/null)"
    xbs_files="$xbs_files $(find /etc/initramfs-tools/scripts/init-bottom -name "xbs_*" -type f 2>/dev/null)"
    xbs_files="$xbs_files $(find /sbin -name "xbs_*" -type f 2>/dev/null)"
    xbs_files="$xbs_files $(find /lib/systemd/system-shutdown -name "xbs_*" -type f 2>/dev/null)"
    
    if [[ -n "$xbs_files" ]] || [[ -f /lib/systemd/system-shutdown/bootsplash.shutdown ]]; then
        has_xbs=true
        to_remove+=("xbsbootsplash")
    fi
    
    # Check for Plymouth
    if command -v plymouthd &>/dev/null || [[ -f /usr/share/plymouth/themes/default.plymouth ]]; then
        has_plymouth=true
        to_remove+=("Plymouth")
    fi
    
    # Check for fbsplash
    if [[ -f /etc/conf.d/splash ]]; then
        has_fbsplash=true
        to_remove+=("fbsplash")
    fi
    
    if [[ ${#to_remove[@]} -eq 0 ]]; then
        print_info "No splash systems detected."
        return 0
    fi
    
    # Show what will be removed
    echo -e "${YELLOW}The following splash systems will be removed:${NC}"
    for sys in "${to_remove[@]}"; do
        echo -e "  ${RED}✗${NC} $sys"
    done
    echo ""
    
    # Confirm
    echo -en "${YELLOW}➤ Proceed with removal? [y/${CYAN}N${YELLOW}]: ${NC}"
    read -r confirm
    if [[ ! $confirm =~ ^[Yy]$ ]]; then
        print_info "Uninstall cancelled."
        return 0
    fi
    
    # Remove xbsbootsplash
    if [[ "$has_xbs" == "true" ]]; then
        echo ""
        echo -e "${CYAN}[1/3] Removing xbsbootsplash...${NC}"
        
        # Remove initramfs files
        find /etc/initramfs-tools/hooks -name "xbs_*" -type f -exec rm -f {} \; 2>/dev/null
        find /etc/initramfs-tools/scripts/init-top -name "xbs_*" -type f -exec rm -f {} \; 2>/dev/null
        find /etc/initramfs-tools/scripts/init-bottom -name "xbs_*" -type f -exec rm -f {} \; 2>/dev/null
        find /sbin -name "xbs_*" -type f -exec rm -f {} \; 2>/dev/null
        find /lib/systemd/system-shutdown -name "xbs_*" -type f -exec rm -f {} \; 2>/dev/null
        rm -f /lib/systemd/system-shutdown/bootsplash.shutdown
        
        echo "  -> Removed xbsbootsplash files"
    fi
    
    # Disable Plymouth (don't uninstall package, just disable)
    if [[ "$has_plymouth" == "true" ]]; then
        echo ""
        echo -e "${CYAN}[2/3] Disabling Plymouth...${NC}"
        
        # Remove splash from kernel cmdline
        if [[ -f /etc/default/grub ]]; then
            if grep -q 'GRUB_CMDLINE_LINUX.*splash' /etc/default/grub || grep -q 'GRUB_CMDLINE_LINUX_DEFAULT.*splash' /etc/default/grub; then
                # Backup before modifying
                cp /etc/default/grub /etc/default/grub.bak.$$
                sed -i \
                    -e '/^GRUB_CMDLINE_LINUX/ s/[[:space:]]*\<splash\>[[:space:]]*/ /g' \
                    -e '/^GRUB_CMDLINE_LINUX/ s/[[:space:]]\{2,\}/ /g' \
                    -e '/^GRUB_CMDLINE_LINUX/ s/="[[:space:]]*/="/g' \
                    -e '/^GRUB_CMDLINE_LINUX/ s/[[:space:]]*"$/"/g' \
                    /etc/default/grub
                sed -i 's/plymouth\.[^ "]*//g' /etc/default/grub
                echo "  -> Removed splash from GRUB config (backup: /etc/default/grub.bak.$$)"
                
                # Update grub
                if command -v update-grub &>/dev/null; then
                    update-grub
                    echo "  -> Updated GRUB"
                fi
            fi
        fi
        
        # Disable plymouth service
        if command -v systemctl &>/dev/null; then
            systemctl disable plymouth 2>/dev/null || true
            echo "  -> Disabled Plymouth systemd service"
        fi
        
        echo -e "  ${YELLOW}Note: Plymouth package not uninstalled (use package manager to remove)${NC}"
    fi
    
    # Disable fbsplash
    if [[ "$has_fbsplash" == "true" ]]; then
        echo ""
        echo -e "${CYAN}[3/3] Disabling fbsplash...${NC}"
        # Just warn - fbsplash config is distro-specific
        echo -e "  ${YELLOW}Note: fbsplash detected. Edit /etc/conf.d/splash to disable.${NC}"
    fi
    
    # Rebuild initramfs if needed
    if [[ "$has_xbs" == "true" ]] || [[ "$has_plymouth" == "true" ]]; then
        echo ""
        echo -e "${GREEN}⚙ Rebuilding initramfs...${NC}"
        
        # Backup initramfs before rebuild (critical for recovery)
        local running_kernel=$(uname -r)
        local initramfs_path="/boot/initrd.img-${running_kernel}"
        [[ ! -f "$initramfs_path" ]] && initramfs_path="/boot/initramfs-${running_kernel}.img"
        local initramfs_backup="${initramfs_path}.bak.$$"
        if [[ -f "$initramfs_path" ]]; then
            cp "$initramfs_path" "$initramfs_backup"
            echo "  -> Backed up initramfs to $initramfs_backup"
        fi
        
        local rebuild_ok=false
        if command -v update-initramfs &>/dev/null; then
            if update-initramfs -u; then
                echo "  -> Done (update-initramfs)"
                rebuild_ok=true
            fi
        elif command -v mkinitcpio &>/dev/null; then
            if mkinitcpio -P; then
                echo "  -> Done (mkinitcpio)"
                rebuild_ok=true
            fi
        elif command -v dracut &>/dev/null; then
            if dracut --force; then
                echo "  -> Done (dracut)"
                rebuild_ok=true
            fi
        fi
        
        if [[ "$rebuild_ok" == "false" ]]; then
            print_warning "Could not rebuild initramfs automatically. Please rebuild manually."
            # Restore from backup
            if [[ -f "$initramfs_backup" ]]; then
                mv "$initramfs_backup" "$initramfs_path"
                echo "  -> Restored initramfs from backup"
            fi
        else
            # Clean up backup on success
            rm -f "$initramfs_backup" 2>/dev/null
        fi
    fi
    
    echo ""
    echo -e "${GREEN}=== All splash systems removed ===${NC}"
}

# Show main menu with optional DRM/fbdev toggle
show_main_menu() {
    print_step "1" " Select action"
    echo -e "    ╭────────────────────────────────────────────╮ "
    echo -e "    │  ${CYAN}1)${NC} Build new splash                       │"
    echo -e "    │  ${CYAN}2)${NC} Install existing xbs_* binary          │"
    echo -e "    │  ${CYAN}3)${NC} Uninstall xbootsplash                  │"
    echo -e "    │  ${CYAN}4)${NC} Splash current status                  │"
    echo -e "    │  ${CYAN}5)${NC} Uninstall ALL splash systems           │"
    if [[ $LIBDRM_AVAILABLE -eq 1 ]]; then
        if [[ $USE_DRM -eq 1 ]]; then
            echo -e "    │  ${CYAN}T)${NC} Toggle mode (current: ${GREEN}DRM${NC})             │"
        else
            echo -e "    │  ${CYAN}T)${NC} Toggle mode (current: ${GREEN}fbdev${NC})           │"
        fi
    fi
    echo -e "    │  ${CYAN}Q)${NC} Quit                                   │"
    echo -e "    ╰────────────────────────────────────────────╯ "
    echo -en "   ${YELLOW}  ➤ Select option [${CYAN}1${YELLOW}]: ${NC}"
    read -r main_choice
    
    case "$main_choice" in
        2)
            install_existing_binary
            exit 0
            ;;
        3)
            if [[ $EUID -ne 0 ]]; then
                print_info "This option requires root privileges"
                exec sudo "$0" --uninstall-only
            fi
            do_uninstall
            exit 0
            ;;
        4)
            show_bootsplash_status
            show_main_menu
            return
            ;;
        5)
            if [[ $EUID -ne 0 ]]; then
                print_info "This option requires root privileges"
                exec sudo "$0" --uninstall-all
            fi
            uninstall_all_splash
            exit 0
            ;;
        [Tt])
            if [[ $LIBDRM_AVAILABLE -eq 1 ]]; then
                USE_DRM=$((1 - USE_DRM))
                show_main_menu  # Redraw menu with new state
                return
            fi
            ;;
        [Qq])
            print_info " "
            print_info "                     >X<        "
            print_info "                    (o o)       "
            print_info "             ---ooO--(_)--Ooo---"
            print_info " "
            print_info "                  Goodbye !!"
            print_info " "
            exit 0
            ;;
        1|"")
            # Continue with normal build flow
            ;;
        *)
            print_error "Invalid option"
            exit 1
            ;;
    esac
    
    # Select mode FIRST so we know what type of input to expect
    select_mode
    
    while true; do
        # Get input if not already provided via argument or if previous attempt failed
        if [ -z "$FRAME_DIR" ] || [ ! -e "$FRAME_DIR" ]; then
            if [ $DISPLAY_MODE -eq 3 ] || [ $DISPLAY_MODE -eq 4 ]; then
                echo -en "\n   ${YELLOW}➤ Enter the path to the static image (PNG/JPG) [or 'Q' to quit]: ${NC}"
            else
                echo -en "\n   ${YELLOW}➤ Enter the directory containing frame images [or 'Q' to quit]: ${NC}"
            fi
            read -r FRAME_DIR
            [ -z "$FRAME_DIR" ] && continue
        fi
        
        # Check for quit
        if [[ "$FRAME_DIR" =~ ^[Qq]$ ]]; then
            print_info "Operation cancelled by user."
                        print_info " "
            print_info "                    ()_()       "
            print_info "                    (o o)       "
            print_info "             ---ooO-- o --Ooo---"
            print_info " "
            print_info "                  Goodbye !!"
            print_info " "
            exit 0
        fi
        
        # Expand path
        FRAME_DIR="${FRAME_DIR/#\~/$HOME}"
        
        # Validate input based on mode
        if [ $DISPLAY_MODE -eq 3 ] || [ $DISPLAY_MODE -eq 4 ]; then
            # Static image mode - expect a file
            if [ ! -f "$FRAME_DIR" ]; then
                print_error "Image file not found: $FRAME_DIR"
                FRAME_DIR="" # Reset to prompt again
                continue
            fi
            print_success "Image found: $FRAME_DIR"
            break
        else
            # Animation mode - expect a directory
            if [ ! -d "$FRAME_DIR" ]; then
                print_error "Directory not found: $FRAME_DIR"
                FRAME_DIR="" # Reset to prompt again
                continue
            fi
            
            if analyze_frames "$FRAME_DIR"; then
                if ask_continue "➤ Proceed with these frames ?"; then
                    break
                else
            print_info "                    .:::.           "
            print_info "                   :(o o):      "
            print_info "             ---ooO--(_)--Ooo---"
            print_info " "
            print_info "                  Goodbye !!"
            print_info " "
                    exit 0
                fi
            else
                FRAME_DIR="" # Reset to prompt again
                continue
            fi
        fi
    done
    
    # Get parameters - if user declines, restart from mode selection
    while ! get_parameters; do
        # User declined parameters - restart from STEP 2
        select_mode
        FRAME_DIR=""
        while true; do
            if [ -z "$FRAME_DIR" ] || [ ! -e "$FRAME_DIR" ]; then
                if [ $DISPLAY_MODE -eq 3 ] || [ $DISPLAY_MODE -eq 4 ]; then
                    echo -en "\n   ${YELLOW}➤ Enter the path to the static image (PNG/JPG) [or 'Q' to quit]: ${NC}"
                else
                    echo -en "\n   ${YELLOW}➤ Enter the directory containing frame images [or 'Q' to quit]: ${NC}"
                fi
                read -r FRAME_DIR
                [ -z "$FRAME_DIR" ] && continue
            fi
            
            if [[ "$FRAME_DIR" =~ ^[Qq]$ ]]; then
                print_info "Operation cancelled by user."
            print_info "                      ___          "      
            print_info "                     /_\ *          "
            print_info "                    (o o)       "
            print_info "             ---ooO--(_)--Ooo---"
            print_info " "
            print_info "                  Goodbye !!"
            print_info " "
                exit 0
            fi
            
            FRAME_DIR="${FRAME_DIR/#\~/$HOME}"
            
            if [ $DISPLAY_MODE -eq 3 ] || [ $DISPLAY_MODE -eq 4 ]; then
                if [ ! -f "$FRAME_DIR" ]; then
                    print_error "Image file not found: $FRAME_DIR"
                    FRAME_DIR=""
                    continue
                fi
                print_success "Image found: $FRAME_DIR"
                break
            else
                if [ ! -d "$FRAME_DIR" ]; then
                    print_error "Directory not found: $FRAME_DIR"
                    FRAME_DIR=""
                    continue
                fi
                
                if analyze_frames "$FRAME_DIR"; then
                    if ask_continue "➤ Proceed with these frames ?"; then
                        break
                    else
            print_info "                   __MMM__         "
            print_info "                    (o o)       "
            print_info "             ---ooO--(_)--Ooo---"
            print_info " "
            print_info "                  Goodbye !!"
            print_info " "
                        exit 0
                    fi
                else
                    FRAME_DIR=""
                    continue
                fi
            fi
        done
    done
    
    build_animation
    test_animation
    generate_preview
    install_animation
    
    echo -e "\n${GREEN}${BOLD}✓ All done!${NC}\n"
}

# Handle deferred flags (functions must be defined before calling)
if [ -n "$INSTALL_EXISTING" ]; then
    BINARY="$INSTALL_EXISTING"
    
    # Auto-detect DRM mode from binary linkage (same as install_standard)
    if ldd "$BINARY" 2>/dev/null | grep -q libdrm; then
        USE_DRM=1
    else
        USE_DRM=0
    fi
    
    # Handle installation type (preserve context from sudo re-exec)
    case "$INSTALL_TYPE" in
        shutdown)
            install_shutdown
            ;;
        both)
            echo ""
            echo -e "${CYAN}═══════════════════════════════════════${NC}"
            echo -e "${CYAN}   Part 1/2: Boot Splash Installation   ${NC}"
            echo -e "${CYAN}═══════════════════════════════════════${NC}"
            check_plymouth
            install_standard
            echo ""
            echo -e "${CYAN}═══════════════════════════════════════${NC}"
            echo -e "${CYAN}   Part 2/2: Shutdown Splash Installation${NC}"
            echo -e "${CYAN}═══════════════════════════════════════${NC}"
            install_shutdown
            ;;
        boot|standard|"")
            check_plymouth
            install_standard
            ;;
    esac
    exit 0
fi

if [ -n "$UNINSTALL_ONLY" ]; then
    do_uninstall
    exit 0
fi

if [ -n "$INSTALL_PACKAGE" ]; then
    install_from_package "$INSTALL_PACKAGE"
    exit 0
fi

if [ -n "$UNINSTALL_ALL" ]; then
    uninstall_all_splash
    exit 0
fi

# Run
main "$@"
