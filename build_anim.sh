#!/bin/bash
#
# build_anim.sh - Interactive splash builder for xbootsplash
# by seb3773 - https://github.com/seb3773
#
# Usage: ./build_anim.sh [options] [input]
#
# Display modes:
#   0 = Animation on solid background (default)
#   1 = Animation on background image (full screen)
#   2 = Static image on solid background
#   3 = Static image full screen
#
# Options:
#   -h, --help       Show this help message
#   -m, --mode       Display mode (0-3)
#   -x, --offset-x   Horizontal offset from center (default: 0)
#   -y, --offset-y   Vertical offset from center (default: 80)
#   -d, --delay      Frame delay in ms (default: 33)
#   -c, --bg-color   Background color as RRGGBB hex (default: 000000)
#   -b, --bg-image   Background image for mode 1
#   -r, --resolution Target resolution WxH for full screen modes
#
# Input:
#   - Animation modes: directory containing PNG frames
#   - Static modes: single PNG image file
#
# If no input specified, will prompt interactively.
#

set -e

RED='\033[31m'
GREEN='\033[32m'
YELLOW='\033[1;33m'
BLUE='\033[34m'
CYAN='\033[36m'
BOLD='\033[1m'
WHITE='\033[37m'
BLACK='\033[30m'
NC='\033[0m' # No Color

BLACK_BG='\033[40m'
RED_BG='\033[41m'
GREEN_BG='\033[42m'
YELLOW_BG='\033[43m'
BLUE_BG='\033[44m'
MAGENTA_BG='\033[45m'
CYAN_BG='\033[46m'
GRAY_BG='\033[100m'
BG_RESET='\033[49m'

# Default parameters
FRAME_DIR=""
FRAME_OFFSET_X=0
FRAME_OFFSET_Y=0
FRAME_DELAY=33
BINARY="xbootsplash"
COMPRESS_METHOD=""
DISPLAY_MODE=0
BG_COLOR="000000"
BG_IMAGE=""
BG_OFFSET_X=0
BG_OFFSET_Y=0
TARGET_RES=""
LOOP=1  # 1=loop, 0=no-loop (stay on last frame)

# Screen dimensions (detected or specified)
SCREEN_W=0
SCREEN_H=0
OBJECT_W=0
OBJECT_H=0

# Show help
show_help() {
    sed -n '2,/^$/p' "$0" | sed 's/^# //'
    exit 0
}

# Parse options
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            ;;
        --install-existing)
            # Internal: install existing binary (called with sudo)
            BINARY="$2"
            shift 2
            # Source the script again to get functions, then install
            check_plymouth
            install_standard
            exit 0
            ;;
        --uninstall-only)
            # Internal: uninstall only (called with sudo)
            do_uninstall
            exit 0
            ;;
        -m|--mode)
            DISPLAY_MODE="$2"
            shift 2
            ;;
        -x|--offset-x)
            FRAME_OFFSET_X="$2"
            shift 2
            ;;
        -y|--offset-y)
            FRAME_OFFSET_Y="$2"
            shift 2
            ;;
        -o|--offset)
            FRAME_OFFSET_Y="$2"
            shift 2
            ;;
        -d|--delay)
            FRAME_DELAY="$2"
            shift 2
            ;;
        -c|--bg-color)
            BG_COLOR="$2"
            shift 2
            ;;
        -b|--bg-image)
            BG_IMAGE="$2"
            shift 2
            ;;
        -r|--resolution)
            TARGET_RES="$2"
            shift 2
            ;;
        -*)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Run: $0 --help"
            exit 1
            ;;
        *)
            FRAME_DIR="$1"
            shift
            ;;
    esac
done

# Print functions
print_header() {
  echo -e "\n${CYAN}${BOLD}╔═══════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}${BOLD}║${NC}           ___             _    ___       _           _            ${CYAN}${BOLD}║${NC}"
    echo -e "${CYAN}${BOLD}║${NC}      __  | . > ___  ___ _| |_ / __> ___ | | ___  ___| |_          ${CYAN}${BOLD}║${NC}"
    echo -e "${CYAN}${BOLD}║${NC}      \\ \\/| . \\/ . \\/ . \\ | |  \\__ \\| . \\| |<_> |<_-<| . |         ${CYAN}${BOLD}║${NC}"
    echo -e "${CYAN}${BOLD}║${NC}      /\\_\\|___/\\___/\\___/ |_|  <___/|  _/|_|<___|/__/|_|_|         ${CYAN}${BOLD}║${NC}"
    echo -e "${CYAN}${BOLD}║${NC}                                    |_|                            ${CYAN}${BOLD}║${NC}"
    echo -e "${CYAN}${BOLD}║${NC}                                                                   ${CYAN}${BOLD}║${NC}"
    echo -e "${CYAN}${BOLD}║                XBOOTSPLASH ANIMATION BUILDER v1.0                 ${CYAN}${BOLD}║${NC}"
    echo -e "${CYAN}${BOLD}║                by seb3773-[ github.com/seb3773 ]-                 ${CYAN}${BOLD}║${NC}"
    echo -e "${CYAN}${BOLD}╚═══════════════════════════════════════════════════════════════════╝${NC}\n"
}

print_step() {
    echo -e "\n${BLUE}${BOLD}[STEP $1]${NC} ${YELLOW}$2${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${CYAN}  $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

ask_continue() {
    echo -e "\n${BOLD}$1${NC} ${YELLOW}[Y/n]${NC}"
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
        local size=$(identify -format "%wx%h" "$file" 2>/dev/null | head -1)
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
    echo -e "\n${BOLD}$1${NC} ${YELLOW}[Y/n]${NC}"
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
    echo ""
    print_info "☉ Detecting initramfs system..."
    
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
    echo ""
    print_info "☉ Checking framebuffer..."
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
    echo ""
    print_info "☉ Checking kernel command line..."
    
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
    print_step "0" "☉ Checking dependencies..."
    
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
    
    # libpng for generator
    if ! dpkg -l libpng-dev 2>/dev/null | grep -q '^ii' 2>/dev/null; then
        if ! ld -lpng 2>/dev/null; then
            missing+=("libpng-dev")
            missing_pkgs+=("libpng-dev")
        fi
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
                    libpng-dev) ld -lpng 2>/dev/null || still_missing+=("$dep") ;;
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
    echo ""
    print_info "☉ Checking for sstrip (optional, reduces binary size)..."
    
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
        git clone https://github.com/aunali1/super-strip.git "$tmpdir/super-strip" 2>/dev/null
        sstrip_bin="$tmpdir/super-strip/sstrip"
    elif command -v wget &> /dev/null; then
        # Download prebuilt if available, or source
        wget -q "https://github.com/aunali1/super-strip/archive/refs/heads/master.tar.gz" -O "$tmpdir/sstrip.tar.gz" 2>/dev/null
        tar -xzf "$tmpdir/sstrip.tar.gz" -C "$tmpdir" 2>/dev/null
        sstrip_bin="$tmpdir/super-strip-master/sstrip"
    elif command -v curl &> /dev/null; then
        curl -sL "https://github.com/aunali1/super-strip/archive/refs/heads/master.tar.gz" -o "$tmpdir/sstrip.tar.gz" 2>/dev/null
        tar -xzf "$tmpdir/sstrip.tar.gz" -C "$tmpdir" 2>/dev/null
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
        
        # Build sstrip
        cd "$srcdir"
        if make 2>/dev/null; then
            # Install to /usr/local/bin
            sudo mkdir -p /usr/local/bin
            sudo cp sstrip /usr/local/bin/sstrip
            sudo chmod +x /usr/local/bin/sstrip
            
            # Verify
            if command -v sstrip &> /dev/null || [ -x "/usr/local/bin/sstrip" ]; then
                print_success "sstrip installed to /usr/local/bin/sstrip"
            else
                print_warning "sstrip built but installation failed"
            fi
        else
            print_error "Failed to build sstrip"
            print_info "You may need to install build-essential or base-devel"
        fi
        cd - > /dev/null
    else
        print_error "Failed to download sstrip source"
    fi
    
    rm -rf "$tmpdir"
}

# Analyze frames directory
analyze_frames() {
    local dir="$1"
    
    print_step "2" "Analyzing frames directory..."
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
            frame_size=$(identify -format "%wx%h" "$file")
        fi
        
    done < <(find "$dir" -maxdepth 1 -type f \( -name "*.png" -o -name "*.PNG" -o -name "*.jpg" -o -name "*.JPG" -o -name "*.jpeg" -o -name "*.JPEG" \) 2>/dev/null | sort)
    
    if [ $last_index -lt 0 ]; then
        print_error "Could not detect frame numbering pattern"
        print_warning "Filenames should contain numbers (e.g., frame_00.png, spin001.jpg)"
        exit 1
    fi
    
    # Display analysis results
    echo -e "\n     ${BOLD}${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "     ${BOLD}${GREEN}║                    ANALYSIS RESULTS                          ║${NC}"
    echo -e "     ${BOLD}${GREEN}╠══════════════════════════════════════════════════════════════╣${NC}"
    echo -e "     ${BOLD}${GREEN}║${NC} ${CYAN}Total frames:${NC}    $img_count"
    echo -e "     ${BOLD}${GREEN}║${NC} ${CYAN}Frame size:${NC}      $frame_size pixels"
    echo -e "     ${BOLD}${GREEN}║${NC} ${CYAN}First frame:${NC}     $first_frame (index $first_index)"
    echo -e "     ${BOLD}${GREEN}║${NC} ${CYAN}Last frame:${NC}      $last_frame (index $last_index)"
    echo -e "     ${BOLD}${GREEN}║${NC} ${CYAN}Index range:${NC}     $first_index → $last_index"
    echo -e "     ${BOLD}${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
    
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
    print_step "1" "Select display mode..."
    echo -e "\n   ╭──────────────────────────────────────────────────────────────────────────────╮"
    echo -e "   │${BOLD}Available display modes:${NC}                                                      │"
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
    print_info "Selected: ${mode_names[$DISPLAY_MODE]}"
    
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
    print_step "3" "Configure display parameters..."
    
    # Show mode-specific diagram
    case $DISPLAY_MODE in
        0)
            echo -e "${CYAN}"
            echo "     Mode 0: Animation on solid background"
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
            echo "     Mode 1: Animation on background image"
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
            echo "     Mode 2: Animation on full screen background image"
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
            echo "    Mode 3: Static image on solid background"
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
            echo "     Mode 4: Static image full screen"
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
        echo -n "  ➤ Horizontal offset [$FRAME_OFFSET_X]: "
        read -r input
        if [ -n "$input" ] && [[ "$input" =~ ^-?[0-9]+$ ]]; then
            FRAME_OFFSET_X="$input"
        fi
        
        echo -n "  ➤ Vertical offset [$FRAME_OFFSET_Y]: "
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
        echo -n "  ➤ Horizontal offset [$BG_OFFSET_X]: "
        read -r input
        if [ -n "$input" ] && [[ "$input" =~ ^-?[0-9]+$ ]]; then
            BG_OFFSET_X="$input"
        fi
        
        echo -n "  ➤ Vertical offset [$BG_OFFSET_Y]: "
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
        echo -n "  ➤ Horizontal offset [$FRAME_OFFSET_X]: "
        read -r input
        if [ -n "$input" ] && [[ "$input" =~ ^-?[0-9]+$ ]]; then
            FRAME_OFFSET_X="$input"
        fi
        
        echo -n "  ➤ Vertical offset [$FRAME_OFFSET_Y]: "
        read -r input
        if [ -n "$input" ] && [[ "$input" =~ ^-?[0-9]+$ ]]; then
            FRAME_OFFSET_Y="$input"
        fi
    fi
    
    # Background color (modes 0, 1, 3)
    if [ $DISPLAY_MODE -eq 0 ] || [ $DISPLAY_MODE -eq 1 ] || [ $DISPLAY_MODE -eq 3 ]; then
        echo -e "\n${BOLD}Background color:${NC}"
        echo -e "  ${CYAN}Format: RRGGBB (hex)${NC}"
        echo -n "  Color [$BG_COLOR]: "
        read -r input
        if [ -n "$input" ] && [[ "$input" =~ ^[0-9A-Fa-f]{6}$ ]]; then
            BG_COLOR="$input"
        fi
    fi
    
    # Frame delay for animation modes
    if [ $DISPLAY_MODE -le 2 ]; then
        echo -e "\n${BOLD}Animation timing:${NC}"
        echo -e "  ${CYAN}Valid range: 1-1000 ms (1=1000 FPS max, 1000=1 FPS min)${NC}"
        echo -n "  Frame delay (ms) [$FRAME_DELAY]: "
        read -r input
        if [ -n "$input" ] && [[ "$input" =~ ^[0-9]+$ ]] && [ "$input" -ge 1 ] && [ "$input" -le 1000 ]; then
            FRAME_DELAY="$input"
        fi
        
        # Loop option for animation modes
        echo -e "\n${BOLD}Animation loop:${NC}"
        echo -e "  ${YELLOW}Loop: restart from frame 0 after last frame${NC}"
        echo -e "  ${YELLOW}No-loop: stay on last frame after animation ends${NC}"
        echo -n "  Loop animation? [Y/n]: "
        read -r input
        case "$input" in
            [nN][oO]|[nN])
                LOOP=0
                ;;
            *)
                LOOP=1
                ;;
        esac
    fi
    
    # Summary
    echo -e "\n${GREEN}Parameters:${NC}"
    if [ $DISPLAY_MODE -le 2 ]; then
        echo -e "  ${CYAN}Animation offset:${NC}    X=$FRAME_OFFSET_X, Y=$FRAME_OFFSET_Y"
        echo -e "  ${CYAN}Loop:${NC}             $([ $LOOP -eq 1 ] && echo "Yes" || echo "No (stay on last frame)")"
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
    
    echo -e "  ${GREEN}Binary will be: $BINARY${NC}"
}

# Build animation
build_animation() {
    print_step "4" "Building splash..."
    
    # Check generator exists
    if [ ! -f "generate_splash" ]; then
        print_info "⚙ Compiling splash generator..."
        gcc -O2 -o generate_splash generate_splash.c -lpng -lm
        print_success "Generator compiled"
    fi
    
    # Build generator command
    local GEN_CMD="./generate_splash -m $DISPLAY_MODE -x $FRAME_OFFSET_X -y $FRAME_OFFSET_Y -c $BG_COLOR"
    
    # Add mode-specific options
    # Modes 1 and 2: background image
    if [ $DISPLAY_MODE -eq 1 ] || [ $DISPLAY_MODE -eq 2 ]; then
        GEN_CMD="$GEN_CMD -b $BG_IMAGE"
    fi
    
    # Mode 2 (fullscreen anim): target resolution
    if [ $DISPLAY_MODE -eq 2 ] && [ -n "$TARGET_RES" ]; then
        GEN_CMD="$GEN_CMD -r $TARGET_RES"
    fi
    
    # Mode 4 (static fullscreen): target resolution
    if [ $DISPLAY_MODE -eq 4 ] && [ -n "$TARGET_RES" ]; then
        GEN_CMD="$GEN_CMD -r $TARGET_RES"
    fi
    
    # Add frame delay for animation modes (0, 1, 2)
    if [ $DISPLAY_MODE -le 2 ]; then
        GEN_CMD="$GEN_CMD -d $FRAME_DELAY"
        GEN_CMD="$GEN_CMD -l $LOOP"
        
        # Always use auto compression (tests all methods and picks best)
        COMPRESS_METHOD="auto"
        GEN_CMD="$GEN_CMD -z $COMPRESS_METHOD"
    fi
    
    # Add input path
    GEN_CMD="$GEN_CMD \"$FRAME_DIR\""
    
    # Generate frames header
    print_info "⚙ Generating splash data..."
    if ! eval "$GEN_CMD" > frames_delta.h 2>build.log; then
        print_error "Splash generation failed"
        cat build.log
        exit 1
    fi
    
    # Show generator output
    grep -E "^(Display|Found|Frame|Offsets|Background|Image|Total|Resizing)" build.log | while read -r line; do
        print_info "$line"
    done
    
    print_success "Splash data generated"
    
    # Compile binary
    print_info "Compiling $BINARY binary..."
    make clean >/dev/null 2>&1 || true
    if ! make TARGET="$BINARY" >/dev/null 2>build.log; then
        print_error "Compilation failed"
        cat build.log
        exit 1
    fi
    
    # Verify binary exists
    if [ ! -f "$BINARY" ]; then
        print_error "Binary not found after compilation"
        exit 1
    fi
    
    print_success "Binary compiled"
    
    # Get binary size
    local size=$(wc -c < "$BINARY" 2>/dev/null)
    local size_kb=$((size / 1024))
    
    # Display results
    echo -e "\n${BOLD}${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BOLD}${GREEN}║                    BUILD SUCCESSFUL                          ║${NC}"
    echo -e "${BOLD}${GREEN}╠══════════════════════════════════════════════════════════════╣${NC}"
    echo -e "${BOLD}${GREEN}║${NC} ${CYAN}Binary:${NC}          $BINARY"
    echo -e "${BOLD}${GREEN}║${NC} ${CYAN}Size:${NC}            ${size} bytes (${size_kb} KB)"
    echo -e "${BOLD}${GREEN}║${NC} ${CYAN}Location:${NC}        $(pwd)/$BINARY"
    echo -e "${BOLD}${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
    
    # Warning for large binary (> 1MB)
    if [ $size -gt 1048576 ]; then
        echo ""
        echo -e "${BOLD}${YELLOW}╔══════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${BOLD}${YELLOW}║                      ⚠ SIZE WARNING ⚠                       ║${NC}"
        echo -e "${BOLD}${YELLOW}╠══════════════════════════════════════════════════════════════╣${NC}"
        echo -e "${BOLD}${YELLOW}║${NC} ${RED}Binary size exceeds 1 MB${NC}"
        echo -e "${BOLD}${YELLOW}║${NC}"
        echo -e "${BOLD}${YELLOW}║${NC} This is getting large for a boot animation."
        echo -e "${BOLD}${YELLOW}║${NC} Consider reducing:"
        echo -e "${BOLD}${YELLOW}║${NC}   ${CYAN}•${NC} Number of frames (fewer frames)"
        echo -e "${BOLD}${YELLOW}║${NC}   ${CYAN}•${NC} Frame dimensions (smaller images)"
        echo -e "${BOLD}${YELLOW}║${NC}   ${CYAN}•${NC} Color complexity (simpler graphics)"
        echo -e "${BOLD}${YELLOW}║${NC}"
        echo -e "${BOLD}${YELLOW}║${NC} ${GREEN}The animation will still work correctly.${NC}"
        echo -e "${BOLD}${YELLOW}║${NC} However, a smaller binary loads faster from"
        echo -e "${BOLD}${YELLOW}║${NC} initramfs and uses less memory."
        echo -e "${BOLD}${YELLOW}╚══════════════════════════════════════════════════════════════╝${NC}"
    fi
    
    # Cleanup
    rm -f build.log
}

# Test animation
test_animation() {
    print_step "5" "Test animation..."
    
    echo -e "\n${YELLOW}══════════════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}  ℹ  IMPORTANT: Framebuffer requires TTY console mode  !       ${NC}"
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "${CYAN}The animation requires direct framebuffer access, which only${NC}"
    echo -e "${CYAN}works in a real TTY console (not in X11/Wayland terminal).${NC}"
    echo ""
    echo -e "${BOLD}To test properly:${NC}"
    echo ""
    echo -e "  ${GREEN}Option 1: Switch to TTY${NC}"
    echo -e "    Press: ${YELLOW}Ctrl + Alt + F2${NC} (or F3, F4, F5, F6)"
    echo "    Login with your username/password"
    echo -e "    Run:   ${YELLOW}cd $(pwd) && sudo ./$BINARY${NC}"
    echo -e "    Return to GUI: ${YELLOW}Ctrl + Alt + F7${NC} (or F1)"
    echo ""
    echo -e "  ${GREEN}Option 2: From current terminal (may not work in GUI)${NC}"
    echo "    If you're already in a TTY console, proceed below"
    echo ""
    
    if ask_yes_no "Test animation now?"; then
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
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}   Installation Methods Explained      ${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo -e "${GREEN}═══════════════════════════════════════${NC}"
    echo -e "${GREEN}  METHOD 1: STANDARD (Recommended)     ${NC}"
    echo -e "${GREEN}═══════════════════════════════════════${NC}"
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
    echo -e "${YELLOW}═══════════════════════════════════════${NC}"
    echo -e "${YELLOW}  METHOD 2: CUSTOM (Advanced)         ${NC}"
    echo -e "${YELLOW}═══════════════════════════════════════${NC}"
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
        echo -e "${RED}==============================================${NC}"
        echo -e "${RED}⚠ WARNING: Plymouth is installed on this system${NC}"
        echo -e "${RED}==============================================${NC}"
        echo ""
        echo "Plymouth and this bootsplash system are incompatible."
        echo "Running both will cause boot issues (black screen, freezes, or other horrible things...)."
        echo ""
        echo -e "${YELLOW}➤➤ You MUST uninstall Plymouth before proceeding.${NC}"
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
            echo -e "${GREEN}✔ Plymouth uninstalled successfully.${NC}"
        else
            echo -e "${RED}✖✖ Plymouth must be uninstalled to continue. Aborting.${NC}"
            exit 1
        fi
    fi
}

# Verify installation
verify_installation() {
    echo -e "${BLUE}══════════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}           POST-INSTALLATION VERIFICATION                       ${NC}"
    echo -e "${BLUE}══════════════════════════════════════════════════════════════${NC}"
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
    echo -e "${BLUE}══════════════════════════════════════════════════════════════${NC}"
    
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
    
    echo -e "${BLUE}══════════════════════════════════════════════════════════════${NC}"
}

# Standard installation method
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
    echo -e "${GREEN}[1/4]⚙ Creating initramfs-tools hook...${NC}"
    mkdir -p /etc/initramfs-tools/hooks
    cat > /etc/initramfs-tools/hooks/$BINARY << HOOK_EOF
#!/bin/sh
# initramfs-tools hook for $BINARY bootsplash animation

PREREQ=""
prereqs() { echo "\$PREREQ"; }
case "\$1" in prereqs) prereqs; exit 0;; esac

. /usr/share/initramfs-tools/hook-functions

# Copy binary
copy_file binary /sbin/$BINARY /sbin/$BINARY

# Ensure framebuffer device
if [ ! -e "\${DESTDIR}/dev/fb0" ]; then
    mknod "\${DESTDIR}/dev/fb0" c 29 0 2>/dev/null || true
fi
HOOK_EOF
    chmod +x /etc/initramfs-tools/hooks/$BINARY
    echo "  -> /etc/initramfs-tools/hooks/$BINARY"
    
    # Copy binary to /sbin
    echo -e "${GREEN}[2/4]⚙ Installing binary to /sbin...${NC}"
    # Backup existing binary if present
    if [[ -f /sbin/$BINARY ]]; then
        cp /sbin/$BINARY /sbin/${BINARY}.bak
        echo "  -> Backed up existing binary to /sbin/${BINARY}.bak"
    fi
    cp "$BINARY" /sbin/$BINARY
    chmod +x /sbin/$BINARY
    echo "  -> /sbin/$BINARY"
    
    # Create init-top script (starts splash early)
    echo -e "${GREEN}[3/4]⚙ Creating init-top script...${NC}"
    mkdir -p /etc/initramfs-tools/scripts/init-top
    cat > /etc/initramfs-tools/scripts/init-top/$BINARY << INIT_TOP_EOF
#!/bin/sh
# Start $BINARY animation early in boot
# This runs after /dev is mounted, before root filesystem

PREREQ="udev"
prereqs() { echo "\$PREREQ"; }
case "\$1" in prereqs) prereqs; exit 0;; esac

. /scripts/functions

# Start splash in background
if [ -x /sbin/$BINARY ]; then
    /sbin/$BINARY &
    SPLASH_PID=\$!
    echo "\$SPLASH_PID" > /run/${BINARY}.pid
fi
INIT_TOP_EOF
    chmod +x /etc/initramfs-tools/scripts/init-top/$BINARY
    echo "  -> /etc/initramfs-tools/scripts/init-top/$BINARY"
    
    # Create init-bottom script (stops splash before switch_root)
    echo -e "${GREEN}[4/4]⚙ Creating init-bottom script...${NC}"
    mkdir -p /etc/initramfs-tools/scripts/init-bottom
    cat > /etc/initramfs-tools/scripts/init-bottom/$BINARY << INIT_BOTTOM_EOF
#!/bin/sh
# Stop $BINARY before switching to real root

PREREQ=""
prereqs() { echo "\$PREREQ"; }
case "\$1" in prereqs) prereqs; exit 0;; esac

. /scripts/functions

# Stop splash animation
if [ -f /run/${BINARY}.pid ]; then
    kill \$(cat /run/${BINARY}.pid) 2>/dev/null || true
    rm -f /run/${BINARY}.pid
fi

# Clear framebuffer to black before handoff
if [ -c /dev/fb0 ]; then
    dd if=/dev/zero of=/dev/fb0 2>/dev/null || true
fi
INIT_BOTTOM_EOF
    chmod +x /etc/initramfs-tools/scripts/init-bottom/$BINARY
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
    
    # Track installed files for potential rollback
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
    
    # Rebuild initramfs
    echo ""
    echo -e "${GREEN}⚙ Rebuilding initramfs...${NC}"
    if update-initramfs -u; then
        echo "  -> Done"
    else
        echo -e "  -> ${RED}✖ FAILED!${NC}"
        echo ""
        echo -e "${YELLOW}⚠ update-initramfs failed. Rolling back installation...${NC}"
        
        # Remove installed files
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
        
        echo ""
        echo -e "${RED}=== Installation Aborted ===${NC}"
        echo "System restored to previous state."
        echo "Check disk space and kernel version, then try again."
        return 1
    fi
    
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
            echo -e "${GREEN}⚙ Updating GRUB...${NC}"
            update-grub
            echo "  -> Done"
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
}

# Custom installation method
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
    
    # Ask for initramfs directory
    local INITRAMFS_DIR=""
    echo -n "➤ Enter initramfs directory path: "
    read -r INITRAMFS_DIR
    
    if [[ -z "$INITRAMFS_DIR" ]]; then
        echo -e "${RED}ERROR: Directory path required${NC}"
        return 1
    fi
    
    # Create directory structure
    mkdir -p "$INITRAMFS_DIR"/{sbin,dev,proc,sys,etc,run}
    
    # Copy binary
    echo -e "${GREEN}[1/3]⚙ Copying binary...${NC}"
    cp "$BINARY" "$INITRAMFS_DIR/sbin/$BINARY"
    chmod +x "$INITRAMFS_DIR/sbin/$BINARY"
    echo "  -> $INITRAMFS_DIR/sbin/$BINARY"
    
    # Create framebuffer device
    echo -e "${GREEN}[2/3]⚙ Creating device nodes...${NC}"
    if [[ ! -e "$INITRAMFS_DIR/dev/fb0" ]]; then
        mknod "$INITRAMFS_DIR/dev/fb0" c 29 0 2>/dev/null || true
    fi
    echo "  -> $INITRAMFS_DIR/dev/fb0"
    
    # Check for existing init
    INIT_SCRIPT="$INITRAMFS_DIR/init"
    
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
        echo "  -> $INIT_SCRIPT (template created)"
        echo ""
        echo -e "${YELLOW}⚠ IMPORTANT: Edit $INIT_SCRIPT and add your root mount logic!${NC}"
    fi
    
    # Build initramfs image
    echo ""
    echo -e "${GREEN}⚙ Building initramfs image...${NC}"
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
}

# Shutdown splash installation (systemd-shutdown method)
install_shutdown() {
    echo -e "${GREEN}=== Installing Shutdown Splash (systemd-shutdown) ===${NC}"
    echo ""
    
    echo -e "${BLUE}This method:${NC}"
    echo "  ✓ Ultra-minimal: no systemd unit, no dependencies"
    echo "  ✓ Executes at the very last moment before poweroff/reboot"
    echo "  ✓ Same binary as boot splash (no modification needed)"
    echo ""
    echo -e "${YELLOW}Requirements:${NC}"
    echo "  • Binary must be statically linked or use only basic libs"
    echo "  • No external file access (all data embedded)"
    echo "  • Framebuffer must still be active at shutdown"
    echo ""
    
    # Check binary dependencies
    echo -e "${CYAN}[1/3]☉ Checking binary dependencies...${NC}"
    local deps
    deps=$(ldd "$BINARY" 2>/dev/null)
    local has_problematic=false
    
    if echo "$deps" | grep -qE "libpng|libdrm|libz|libjpeg"; then
        echo -e "  ${YELLOW}⚠ Warning: Binary links to external libraries${NC}"
        echo "  These may not be available at shutdown time."
        echo "$deps" | grep -E "libpng|libdrm|libz|libjpeg" | while read line; do
            echo "    $line"
        done
        echo ""
        echo -e "  ${YELLOW}Recommendation: Rebuild with static linking if issues occur${NC}"
        has_problematic=true
    else
        echo "  -> Dependencies OK (only basic libs)"
    fi
    
    # Install binary
    echo -e "${GREEN}[2/3]⚙ Installing binary to /usr/local/sbin...${NC}"
    mkdir -p /usr/local/sbin
    
    # Use same name but in /usr/local/sbin for shutdown
    local shutdown_binary="/usr/local/sbin/$BINARY"
    
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

# Start splash in background
if [ -x /usr/local/sbin/xbs_* ]; then
    # Find the installed shutdown binary
    for bin in /usr/local/sbin/xbs_*; do
        if [ -x "$bin" ]; then
            "$bin" &
            SPLASH_PID=$!
            # Give animation time to run (systemd may kill abruptly)
            sleep 2
            wait $SPLASH_PID 2>/dev/null
            break
        fi
    done
fi
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
    
    echo -e "${YELLOW}=== Uninstalling bootsplash ===${NC}"
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
    
    # Also check for shutdown binaries
    local shutdown_binaries=()
    for bin in /usr/local/sbin/xbs_*; do
        if [[ -f "$bin" ]]; then
            shutdown_binaries+=("$(basename "$bin")")
        fi
    done
    
    if [ ${#installed_binaries[@]} -eq 0 ] && [ ${#shutdown_binaries[@]} -eq 0 ]; then
        echo -e "${YELLOW}No xbs_* binaries found in /sbin or /usr/local/sbin${NC}"
    else
        if [ ${#installed_binaries[@]} -gt 0 ]; then
            echo -e "${GREEN}Found ${#installed_binaries[@]} boot binary(ies) in /sbin:${NC}"
            for bin in "${installed_binaries[@]}"; do
                echo "  - $bin (boot)"
            done
        fi
        if [ ${#shutdown_binaries[@]} -gt 0 ]; then
            echo -e "${GREEN}Found ${#shutdown_binaries[@]} shutdown binary(ies) in /usr/local/sbin:${NC}"
            for bin in "${shutdown_binaries[@]}"; do
                echo "  - $bin (shutdown)"
            done
        fi
        echo ""
        
        # Ask which to remove or remove all
        echo -n "➤ Remove all? [Y/n]: "
        read -r remove_all
        case "$remove_all" in
            [nN])
                echo "Cancelling uninstall."
                return 0
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
                    rm -f /sbin/${bin}.bak 2>/dev/null
                    
                    # Remove hook
                    rm -f /etc/initramfs-tools/hooks/$bin 2>/dev/null && echo "  -> Removed hook" || true
                    
                    # Remove init-top script
                    rm -f /etc/initramfs-tools/scripts/init-top/$bin 2>/dev/null && echo "  -> Removed init-top" || true
                    
                    # Remove init-bottom script
                    rm -f /etc/initramfs-tools/scripts/init-bottom/$bin 2>/dev/null && echo "  -> Removed init-bottom" || true
                    
                    # Remove PID file
                    rm -f /run/${bin}.pid 2>/dev/null
                    
                    ((removed++))
                done
                
                # Remove shutdown binaries
                for bin in "${shutdown_binaries[@]}"; do
                    echo -e "${GREEN}Removing shutdown splash: $bin...${NC}"
                    rm -f /usr/local/sbin/$bin
                    echo "  -> Removed /usr/local/sbin/$bin"
                    rm -f /usr/local/sbin/${bin}.bak 2>/dev/null
                    ((removed++))
                done
                
                # Remove systemd-shutdown script
                if [[ -f /lib/systemd/system-shutdown/bootsplash.shutdown ]]; then
                    rm -f /lib/systemd/system-shutdown/bootsplash.shutdown
                    echo "  -> Removed /lib/systemd/system-shutdown/bootsplash.shutdown"
                fi
                ;;
        esac
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
        if update-initramfs -u; then
            echo "  -> Done"
        else
            echo -e "  -> ${RED}✖ FAILED!${NC}"
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
        echo -e "${YELLOW}No bootsplash files found to remove${NC}"
    fi
}

# Generate GIF preview
generate_preview() {
    print_step "6" "Generate preview..."
    
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
        
        # Scale offsets
        local off_x=$(( FRAME_OFFSET_X * scale / 100 ))
        local off_y=$(( FRAME_OFFSET_Y * scale / 100 ))
        
        # GIF delay (centiseconds = ms / 10)
        local gif_delay=$(( FRAME_DELAY / 10 ))
        [ "$gif_delay" -lt 1 ] && gif_delay=1
        
        # Loop setting: 0 = infinite, 1 = once
        local loop_setting=0
        [ "$LOOP" -eq 0 ] && loop_setting=1
        
        local out_file="${BINARY}_preview.gif"
        
        # Build frame list sorted to ensure correct order
        local frames_list=($(find "$FRAME_DIR" -maxdepth 1 -name "*.png" -type f | sort))
        local frame_count=${#frames_list[@]}
        
        print_info "Processing $frame_count frames with ${FRAME_DELAY}ms delay..."
        
        # Create temporary background
        local tmp_bg="tmp_bg.png"
        if [ $DISPLAY_MODE -eq 0 ]; then
            convert -size "${pw}x${ph}" "xc:#${BG_COLOR}" "$tmp_bg"
        else
            convert "$BG_IMAGE" -resize "${pw}x${ph}!" "$tmp_bg"
        fi

        # Generate GIF using -dispose background to avoid ghosting
        # We use a subshell to generate the frame arguments to ensure proper sorting and options
        local cmd=(convert -delay "$gif_delay" -loop "$loop_setting" -dispose background -page "${pw}x${ph}" "$tmp_bg" -dispose previous)
        
        for f in "${frames_list[@]}"; do
            cmd+=("-page" "+${off_x}+${off_y}" "(" "$f" "-resize" "${scale}%" ")")
        done
        
        cmd+=("-layers" "Optimize" "$out_file")

        if "${cmd[@]}" 2>build.log; then
            rm -f "$tmp_bg"
            local fsize=$(du -h "$out_file" | cut -f1)
            print_success "Preview generated: $out_file ($fsize, $frame_count frames)"
            [ "$LOOP" -eq 1 ] && print_info "Loop: Enabled" || print_info "Loop: Disabled (stops at last frame)"
        else
            rm -f "$tmp_bg"
            print_error "Failed to generate preview GIF"
            cat build.log
        fi
    else
        print_info "Skipping preview generation."
    fi
}

# Install animation
install_animation() {
    print_step "7" "Install animation..."
    
    if [ ! -f "$BINARY" ]; then
        print_error "Binary not found: $BINARY"
        print_info "Build it first with the build step"
        return 1
    fi
    
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}   Bootsplash Animation Installer      ${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo -e "${GREEN}Select installation type:${NC}"
    echo ""
    echo -e "  ${CYAN}1)${NC} Boot splash      - Install for boot (initramfs)"
    echo -e "  ${CYAN}2)${NC} Shutdown splash  - Install for shutdown (systemd-shutdown)"
    echo -e "  ${CYAN}3)${NC} Both             - Install for boot AND shutdown"
    echo -e "  ${CYAN}Q)${NC} Quit             - Skip installation"
    echo ""
    echo -n "➤ Select option [1]: "
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
                local sudo_args=("-m" "$DISPLAY_MODE" "-x" "$FRAME_OFFSET_X" "-y" "$FRAME_OFFSET_Y" "-c" "$BG_COLOR")
                [[ -n "$BG_IMAGE" ]] && sudo_args+=("-b" "$BG_IMAGE")
                [[ -n "$TARGET_RES" ]] && sudo_args+=("-r" "$TARGET_RES")
                exec sudo "$0" "${sudo_args[@]}" "$FRAME_DIR"
            fi
            install_shutdown
            ;;
        3)
            # Check root for both installations
            if [[ $EUID -ne 0 ]]; then
                print_info "✜ This option requires root privileges"
                print_info "✜ Re-running with sudo..."
                local sudo_args=("-m" "$DISPLAY_MODE" "-x" "$FRAME_OFFSET_X" "-y" "$FRAME_OFFSET_Y" "-c" "$BG_COLOR")
                [[ -n "$BG_IMAGE" ]] && sudo_args+=("-b" "$BG_IMAGE")
                [[ -n "$TARGET_RES" ]] && sudo_args+=("-r" "$TARGET_RES")
                exec sudo "$0" "${sudo_args[@]}" "$FRAME_DIR"
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
        [Qq])
            print_info "Skipping installation"
            print_info "You can manually copy $BINARY to your initramfs"
            ;;
        *)
            install_boot_menu
            ;;
    esac
}

# Boot splash installation menu (existing logic)
install_boot_menu() {
    echo ""
    echo -e "${GREEN}Select boot installation method:${NC}"
    echo ""
    echo -e "  ${CYAN}1)${NC} Standard   - Debian/Ubuntu initramfs-tools (${GREEN}RECOMMENDED${NC})"
    echo -e "  ${CYAN}2)${NC} Custom     - Full custom initramfs (advanced)"
    echo -e "  ${CYAN}3)${NC} Uninstall  - Remove bootsplash from system"
    echo -e "  ${CYAN}I)${NC} Info       - Learn about each installation method"
    echo -e "  ${CYAN}Q)${NC} Quit       - Skip boot installation"
    echo ""
    echo -n "➤ Select option [1]: "
    read -r choice
    
    case "$choice" in
        1|"")
            # Check root
            if [[ $EUID -ne 0 ]]; then
                print_info "✜ This option requires root privileges"
                print_info "✜ Re-running with sudo..."
                local sudo_args=("-m" "$DISPLAY_MODE" "-x" "$FRAME_OFFSET_X" "-y" "$FRAME_OFFSET_Y" "-c" "$BG_COLOR")
                [[ -n "$BG_IMAGE" ]] && sudo_args+=("-b" "$BG_IMAGE")
                [[ -n "$TARGET_RES" ]] && sudo_args+=("-r" "$TARGET_RES")
                exec sudo "$0" "${sudo_args[@]}" "$FRAME_DIR"
            fi
            check_plymouth
            install_standard
            ;;
        2)
            if [[ $EUID -ne 0 ]]; then
                print_info "✜ This option requires root privileges"
                print_info "✜ Re-running with sudo..."
                local sudo_args=("-m" "$DISPLAY_MODE" "-x" "$FRAME_OFFSET_X" "-y" "$FRAME_OFFSET_Y" "-c" "$BG_COLOR")
                [[ -n "$BG_IMAGE" ]] && sudo_args+=("-b" "$BG_IMAGE")
                [[ -n "$TARGET_RES" ]] && sudo_args+=("-r" "$TARGET_RES")
                exec sudo "$0" "${sudo_args[@]}" "$FRAME_DIR"
            fi
            check_plymouth
            install_custom
            ;;
        3)
            if [[ $EUID -ne 0 ]]; then
                print_info "✜ This option requires root privileges"
                print_info "✜ Re-running with sudo..."
                exec sudo "$0" -m "$DISPLAY_MODE" -x "$FRAME_OFFSET_X" -y "$FRAME_OFFSET_Y" -c "$BG_COLOR" "$FRAME_DIR"
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
                local sudo_args=("-m" "$DISPLAY_MODE" "-x" "$FRAME_OFFSET_X" "-y" "$FRAME_OFFSET_Y" "-c" "$BG_COLOR")
                [[ -n "$BG_IMAGE" ]] && sudo_args+=("-b" "$BG_IMAGE")
                [[ -n "$TARGET_RES" ]] && sudo_args+=("-r" "$TARGET_RES")
                exec sudo "$0" "${sudo_args[@]}" "$FRAME_DIR"
            fi
            check_plymouth
            install_standard
            ;;
    esac
}

# Install existing xbs_* binary
install_existing_binary() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}   Install Existing Bootsplash Binary  ${NC}"
    echo -e "${BLUE}========================================${NC}"
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
        ((i++))
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
        exec sudo "$0" --install-existing "$BINARY"
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
    echo -n "➤ Select option [1]: "
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
    
    # Show main menu
    echo -e "\n   ╭────────────────────────────────────────────╮ "
    echo -e "   │${BOLD}Select action:${NC}                              │"
    echo "   │                                            │"
    echo -e "   │  ${CYAN}1)${NC} Build new splash animation/splash      │"
    echo -e "   │  ${CYAN}2)${NC} Install existing xbs_* binary          │"
    echo -e "   │  ${CYAN}3)${NC} Uninstall bootsplash                   │"
    echo -e "   │  ${CYAN}Q)${NC} Quit                                   │"
    echo -e "   ╰────────────────────────────────────────────╯ "
    echo -en "   ${YELLOW}➤ Select option [${CYAN}1${YELLOW}]: ${NC}"
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
        [Qq])
            print_info " "
            print_info "        >X<        "
            print_info "       (o o)       "
            print_info "---ooO--(_)--Ooo---"
            print_info " "
            print_info "     Goodbye !!"
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
                if ask_continue "Proceed with these frames?"; then
                    break
                else
                    exit 0
                fi
            else
                FRAME_DIR="" # Reset to prompt again
                continue
            fi
        fi
    done
    
    get_parameters
    build_animation
    test_animation
    generate_preview
    install_animation
    
    echo -e "\n${GREEN}${BOLD}✓ All done!${NC}\n"
}

# Run
main "$@"
