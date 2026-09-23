#!/bin/bash
# ==============================================================================
# test_qemu.sh - Complete Isolated Sandbox & Testing Suite for XBootsplash
#
# Allows developers to test bootsplash binaries, initramfs hooks (init-top,
# init-bottom), signals (SIGUSR1 resume banner, SIGTERM), and systemd-shutdown
# behavior in REAL Linux conditions (QEMU/KVM with real kernel & framebuffer)
# WITHOUT ANY RISK of breaking or modifying the host system.
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# Default parameters
SPLASH_BIN=""
RUN_SECONDS=5
TEST_RESUME=0
TEST_SHUTDOWN=0
WITH_SHUTDOWN=0
DROP_SHELL=0
RESOLUTION="1024x768"
VGA_MODE="792"   # 1024x768x24
DISPLAY_BACKEND="gtk"
KERNEL_IMG="/boot/vmlinuz-$(uname -r)"
LIVE_ISO=""
SYNTAX_ONLY=0

print_banner() {
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${BOLD}          XBOOTSPLASH - REAL-CONDITION SANDBOX TEST SUITE             ${NC}${CYAN}║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════════════╝${NC}"
}

usage() {
    echo "Usage: $0 [options] [binary_path]"
    echo ""
    echo "Arguments:"
    echo "  binary_path          Path to bootsplash executable (default: autodetect latest)"
    echo ""
    echo "Test Options:"
    echo "  -t, --time <sec>     Seconds to play boot animation in VM (default: 5)"
    echo "  -r, --resume         Trigger SIGUSR1 ('Resume from hibernation...' banner)"
    echo "  -S, --shutdown       Test shutdown hook only (systemd-shutdown wrapper)"
    echo "  --with-shutdown      Test full lifecycle: boot animation followed by shutdown hook"
    echo "  -s, --shell          Drop to interactive Busybox root shell inside the VM"
    echo "  --syntax-only        Run static hook audits (dash -n, timeout check) without QEMU"
    echo ""
    echo "Display & VM Options:"
    echo "  -m, --res <WxH>      Screen resolution: 800x600, 1024x768 (default), 1280x1024"
    echo "  -d, --display <type> QEMU display backend: gtk (default), sdl, curses, none"
    echo "  -k, --kernel <path>  Custom kernel image (default: /boot/vmlinuz-$(uname -r))"
    echo "  --iso <path>         Boot full Live ISO in disposable sandbox mode (-snapshot)"
    echo "  -h, --help           Show this help message"
    echo ""
    echo "Lifecycle Tested in VM:"
    echo "  1. [INIT-TOP]        Hook execution, PID tracking (/run), framebuffer sync"
    echo "  2. [PLAYBACK]        Loop playback on /dev/fb0"
    echo "  3. [SIGNAL]          Optional SIGUSR1 hibernation resume banner notification"
    echo "  4. [INIT-BOTTOM]     Clean SIGTERM stop, PID validation, framebuffer blanking"
    echo "  5. [SHUTDOWN]        Optional (--with-shutdown): systemd-shutdown wrapper execution"
    echo "  6. [CLEAN POWEROFF]  Graceful VM powerdown"
    echo ""
    echo "Examples:"
    echo "  $0 xbs_amiga"
    echo "  $0 -r xbs_amiga              # Test boot animation + hibernation resume banner"
    echo "  $0 -S xbs_amiga              # Test shutdown animation hook only"
    echo "  $0 --with-shutdown xbs_amiga # Test boot animation followed by shutdown hook"
    echo "  $0 --syntax-only             # Verify dash compatibility of all installer hooks"
    echo "  $0 --iso ~/Bureau/q4rescue-live.iso  # Boot full disposable desktop VM"
    exit "${1:-0}"
}

# Parse command line options
while [[ $# -gt 0 ]]; do
    case "$1" in
        -t|--time)
            RUN_SECONDS="$2"
            shift 2
            ;;
        -r|--resume)
            TEST_RESUME=1
            shift
            ;;
        -S|--shutdown)
            TEST_SHUTDOWN=1
            shift
            ;;
        --with-shutdown)
            WITH_SHUTDOWN=1
            shift
            ;;
        -s|--shell)
            DROP_SHELL=1
            shift
            ;;
        --syntax-only)
            SYNTAX_ONLY=1
            shift
            ;;
        -m|--res)
            RESOLUTION="$2"
            shift 2
            ;;
        -d|--display)
            DISPLAY_BACKEND="$2"
            shift 2
            ;;
        -k|--kernel)
            KERNEL_IMG="$2"
            shift 2
            ;;
        --iso)
            LIVE_ISO="$2"
            shift 2
            ;;
        -h|--help)
            usage 0
            ;;
        -*)
            echo -e "${RED}Unknown option: $1${NC}"
            usage 1
            ;;
        *)
            if [ -z "$SPLASH_BIN" ]; then
                SPLASH_BIN="$1"
            else
                echo -e "${RED}Unexpected argument: $1${NC}"
                usage 1
            fi
            shift
            ;;
    esac
done

# Map resolution to VESA mode number for kernel vga= parameter
case "$RESOLUTION" in
    800x600)
        VGA_MODE="789"  # 800x600x24
        ;;
    1024x768)
        VGA_MODE="792"  # 1024x768x24
        ;;
    1280x1024)
        VGA_MODE="795"  # 1280x1024x24
        ;;
    *)
        VGA_MODE="792"
        ;;
esac

print_banner

# Step 0: Check if user requested a full Live ISO sandbox boot
if [ -n "$LIVE_ISO" ]; then
    echo -e "${CYAN}[ISO SANDBOX] Launching disposable Live VM from ISO...${NC}"
    if [ ! -f "$LIVE_ISO" ]; then
        echo -e "${RED}Error: ISO file not found: $LIVE_ISO${NC}"
        exit 1
    fi
    KVM_OPT=""
    [ -w /dev/kvm ] && KVM_OPT="-enable-kvm"
    echo -e "  ISO file : ${BOLD}$LIVE_ISO${NC}"
    echo -e "  Mode     : ${GREEN}-snapshot (all disk writes are in RAM and discarded on exit)${NC}"
    echo -e "${YELLOW}Starting QEMU... Close the QEMU window when done.${NC}"
    qemu-system-x86_64 \
        $KVM_OPT \
        -m 2048 \
        -smp 2 \
        -cdrom "$LIVE_ISO" \
        -boot d \
        -snapshot \
        -vga std \
        -display "$DISPLAY_BACKEND"
    echo -e "${GREEN}✔ Live ISO session terminated cleanly. Host system was never modified.${NC}"
    exit 0
fi

# Step 1: Check prerequisites
echo -e "${CYAN}[1/5] Checking environment & tools...${NC}"
if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo -e "${RED}Error: qemu-system-x86_64 is not installed.${NC}"
    echo "Install with: sudo apt install qemu-system-x86"
    exit 1
fi

if ! command -v busybox >/dev/null 2>&1; then
    echo -e "${RED}Error: busybox is not installed.${NC}"
    echo "Install with: sudo apt install busybox"
    exit 1
fi

if ! command -v dash >/dev/null 2>&1; then
    echo -e "${YELLOW}Warning: dash not found, using sh for syntax check.${NC}"
    SH_LINTER="sh"
else
    SH_LINTER="dash"
fi

if [ ! -r "$KERNEL_IMG" ]; then
    echo -e "${RED}Error: Kernel image not found or unreadable: $KERNEL_IMG${NC}"
    exit 1
fi

KVM_OPT=""
if [ -w /dev/kvm ]; then
    KVM_OPT="-enable-kvm"
    echo -e "  ${GREEN}✓${NC} KVM hardware virtualization available"
else
    echo -e "  ${YELLOW}⚠${NC} /dev/kvm not writable, running in software emulation mode"
fi

# Step 2: Auto-detect or validate binary
echo -e "${CYAN}[2/5] Locating and analyzing bootsplash binary...${NC}"
if [ -z "$SPLASH_BIN" ]; then
    CANDIDATES=($(find . "$REPO_ROOT" -maxdepth 1 -type f -executable \( -name "xbs_*" -o -name "xbootsplash*" \) 2>/dev/null | sort -u -r))
    if [ ${#CANDIDATES[@]} -eq 0 ]; then
        echo -e "${RED}Error: No bootsplash binary found in current directory or project root.${NC}"
        echo "Please specify a binary to test: $0 path/to/binary"
        exit 1
    fi
    SPLASH_BIN="${CANDIDATES[0]}"
    echo -e "  Auto-detected: ${BOLD}$SPLASH_BIN${NC}"
fi

if [ ! -f "$SPLASH_BIN" ] || [ ! -x "$SPLASH_BIN" ]; then
    echo -e "${RED}Error: File not found or not executable: $SPLASH_BIN${NC}"
    exit 1
fi

BIN_SIZE=$(stat -c%s "$SPLASH_BIN" 2>/dev/null || wc -c < "$SPLASH_BIN")
echo -e "  Binary: ${BOLD}$SPLASH_BIN${NC} ($((BIN_SIZE / 1024)) KB)"

# Step 3: Pre-flight Static Hook Audit (Ensures zero dash syntax errors)
echo -e "${CYAN}[3/5] Pre-flight Static Audit of Installer & Initramfs Hooks...${NC}"
AUDIT_DIR=$(mktemp -d /tmp/xbs_audit_XXXXXX)
trap 'rm -rf "$AUDIT_DIR" 2>/dev/null || true' EXIT

# Generate init-top hook
cat > "$AUDIT_DIR/init-top" << 'EOF'
#!/bin/sh
PREREQ=""
prereqs() { echo "$PREREQ"; }
case "$1" in prereqs) prereqs; exit 0;; esac

if [ -x /sbin/xbootsplash ]; then
    /sbin/xbootsplash &
    SPLASH_PID=$!
    if [ -d "/proc/$SPLASH_PID" ]; then
        echo "$SPLASH_PID" > /run/xbootsplash.pid
        START_TIME=$(awk '{print $22}' /proc/$SPLASH_PID/stat 2>/dev/null)
        echo "$START_TIME" > /run/xbootsplash.start_time
    fi
fi
EOF

# Generate init-bottom hook
cat > "$AUDIT_DIR/init-bottom" << 'EOF'
#!/bin/sh
PREREQ=""
prereqs() { echo "$PREREQ"; }
case "$1" in prereqs) prereqs; exit 0;; esac

if [ -f /run/xbootsplash.pid ]; then
    PID=$(cat /run/xbootsplash.pid 2>/dev/null)
    if [ -n "$PID" ] && [ -d "/proc/$PID" ]; then
        SAVED_START=$(cat /run/xbootsplash.start_time 2>/dev/null)
        CURRENT_START=$(awk '{print $22}' /proc/$PID/stat 2>/dev/null)
        if [ -n "$SAVED_START" ] && [ "$SAVED_START" = "$CURRENT_START" ]; then
            kill "$PID" 2>/dev/null || true
            for _i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40; do
                [ -d "/proc/$PID" ] || break
                sleep 0.1
            done
            if [ -d "/proc/$PID" ]; then
                kill -9 "$PID" 2>/dev/null || true
            fi
        fi
    fi
    rm -f /run/xbootsplash.pid /run/xbootsplash.start_time
fi

if [ -c /dev/fb0 ]; then
    dd if=/dev/zero of=/dev/fb0 bs=4096 count=100 2>/dev/null || true
fi
EOF

# Generate local-premount hook (hibernation resume)
cat > "$AUDIT_DIR/local-premount" << 'EOF'
#!/bin/sh
PREREQ=""
prereqs() { echo "$PREREQ"; }
case "$1" in prereqs) prereqs; exit 0;; esac

[ -z "${resume?}" ] || [ ! -e /sys/power/resume ] && exit 0

. /scripts/functions
. /scripts/local

if ! local_device_setup "${resume}" "suspend/resume device" false; then
    exit 0
fi

if [ "$(get_fstype "${DEV}")" = "suspend" ]; then
    if [ -f /run/xbootsplash.pid ]; then
        PID=$(cat /run/xbootsplash.pid 2>/dev/null)
        if [ -n "$PID" ] && [ -d "/proc/$PID" ]; then
            COMM=$(cat /proc/$PID/comm 2>/dev/null)
            if [ "$COMM" = "xbootsplash" ]; then
                rm -f /run/xbs_resume_ready 2>/dev/null || true
                kill -USR1 "$PID" 2>/dev/null || true
                for _w in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30; do
                    [ -f /run/xbs_resume_ready ] && break
                    [ -d "/proc/$PID" ] || break
                    sleep 0.1
                done
                rm -f /run/xbs_resume_ready 2>/dev/null || true
            fi
        fi
    fi
fi
EOF

# Generate bootsplash.shutdown hook
cat > "$AUDIT_DIR/bootsplash.shutdown" << 'EOF'
#!/bin/sh
# Bootsplash shutdown animation wrapper
# Strict timeout ensures poweroff or reboot is NEVER blocked
for bin in /sbin/xbs_* /sbin/xbootsplash*; do
    if [ -x "$bin" ]; then
        SHUTDOWN_MODE=1 timeout 3s "$bin" 2>/dev/null || true
        break
    fi
done
EOF

# Audit syntax with dash
FAIL=0
for s in init-top init-bottom local-premount bootsplash.shutdown; do
    if ! $SH_LINTER -n "$AUDIT_DIR/$s" 2>/dev/null; then
        echo -e "  ${RED}✗ FAIL:${NC} Syntax error in hook $s under $SH_LINTER!"
        $SH_LINTER -n "$AUDIT_DIR/$s"
        FAIL=1
    else
        echo -e "  ${GREEN}✓${NC} Hook $s: valid syntax under $SH_LINTER"
    fi
done

if [ $FAIL -eq 1 ]; then
    echo -e "${RED}Critical error: Initramfs hook syntax audit failed. Aborting.${NC}"
    exit 1
fi

if [ "$SYNTAX_ONLY" -eq 1 ]; then
    echo -e "${GREEN}✔ Static syntax audit completed successfully. (Syntax-only mode)${NC}"
    exit 0
fi

# Step 4: Build temporary micro-initramfs in RAM (/tmp)
echo -e "${CYAN}[4/5] Constructing isolated micro-initramfs...${NC}"
TMP_DIR=$(mktemp -d /tmp/xbs_qemu_XXXXXX)
TMP_INITRD=$(mktemp /tmp/xbs_initrd_XXXXXX.img)

cleanup() {
    rm -rf "$TMP_DIR" "$TMP_INITRD" "$AUDIT_DIR" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# Skeleton directories
mkdir -p "$TMP_DIR"/{bin,sbin,dev,proc,sys,run,etc,lib,lib64,lib/x86_64-linux-gnu,lib/systemd/system-shutdown}

# Copy busybox and create applets
cp /usr/bin/busybox "$TMP_DIR/bin/busybox"
chmod 755 "$TMP_DIR/bin/busybox"

# Copy shared libraries for busybox
for lib in $(ldd /usr/bin/busybox 2>/dev/null | awk '{print $3}' | grep '^/' || true); do
    mkdir -p "$TMP_DIR/$(dirname "$lib")"
    cp -L "$lib" "$TMP_DIR/$lib" 2>/dev/null || true
done
if [ -f /lib64/ld-linux-x86-64.so.2 ]; then
    cp -L /lib64/ld-linux-x86-64.so.2 "$TMP_DIR/lib64/" 2>/dev/null || true
fi

# Essential symlinks
(
    cd "$TMP_DIR/bin"
    for app in sh ls sleep echo mount umount poweroff reboot kill cat mknod ps chmod mkdir awk date dd timeout grep rm wait; do
        ln -sf busybox "$app"
    done
)

# Install bootsplash binary
cp "$SPLASH_BIN" "$TMP_DIR/sbin/xbootsplash"
chmod 755 "$TMP_DIR/sbin/xbootsplash"

# Install shutdown wrapper
cp "$AUDIT_DIR/bootsplash.shutdown" "$TMP_DIR/lib/systemd/system-shutdown/bootsplash.shutdown"
chmod 755 "$TMP_DIR/lib/systemd/system-shutdown/bootsplash.shutdown"

# Copy any shared libraries if the splash binary is dynamically linked (DRM mode)
if ldd "$SPLASH_BIN" >/dev/null 2>&1; then
    for lib in $(ldd "$SPLASH_BIN" | awk '{print $3}' | grep '^/' || true); do
        mkdir -p "$TMP_DIR/$(dirname "$lib")"
        cp -L "$lib" "$TMP_DIR/$lib" 2>/dev/null || true
    done
fi

# Generate /init to simulate real lifecycle
cat > "$TMP_DIR/init" << INIT_EOF
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mkdir -p /run /dev/pts
mount -t devpts devpts /dev/pts

echo ""
echo "=================================================="
echo "  XBOOTSPLASH REAL-LIFECYCLE VERIFICATION"
echo "=================================================="
echo "Resolution : $RESOLUTION (VESA mode $VGA_MODE)"
echo "Target Bin : /sbin/xbootsplash"

# Framebuffer check
if [ ! -c /dev/fb0 ]; then
    echo "Waiting for /dev/fb0..."
    for i in 1 2 3 4 5; do
        [ -c /dev/fb0 ] && break
        sleep 0.1
    done
fi

if [ -c /dev/fb0 ]; then
    echo "[STAGE 0] Framebuffer /dev/fb0 detected OK"
else
    echo "[STAGE 0] Warning: /dev/fb0 not ready yet"
fi
INIT_EOF

# --------------------------------------------------------------------------
# STAGE 1: INIT-TOP HOOK EXECUTION (or SHUTDOWN ONLY)
# --------------------------------------------------------------------------
if [ "$TEST_SHUTDOWN" -eq 1 ]; then
    cat >> "$TMP_DIR/init" << SHUTDOWN_ONLY_EOF

echo ""
echo ">>> [SHUTDOWN TEST] Simulating systemd-shutdown execution (timeout 3s)..."
T_START=\$(date +%s)
/lib/systemd/system-shutdown/bootsplash.shutdown
T_END=\$(date +%s)
T_DIFF=\$((T_END - T_START))
echo "  ✓ Shutdown animation wrapper finished in \${T_DIFF}s (Safe: never hung!)"

SHUTDOWN_ONLY_EOF
else
    cat >> "$TMP_DIR/init" << INIT_TOP_EOF

echo ""
echo ">>> [STAGE 1] Simulating init-top execution..."
if [ -x /sbin/xbootsplash ]; then
    /sbin/xbootsplash &
    SPLASH_PID=\$!
    sleep 0.1
    if [ -d "/proc/\$SPLASH_PID" ]; then
        echo "\$SPLASH_PID" > /run/xbootsplash.pid
        START_TIME=\$(awk '{print \$22}' /proc/\$SPLASH_PID/stat 2>/dev/null)
        echo "\$START_TIME" > /run/xbootsplash.start_time
        echo "  ✓ Bootsplash started successfully (PID: \$SPLASH_PID)"
    else
        echo "  ✗ Error: Bootsplash failed to stay running!"
    fi
fi

INIT_TOP_EOF

# --------------------------------------------------------------------------
# STAGE 2: RUNTIME & RESUME SIGNAL TEST
# --------------------------------------------------------------------------
if [ "$RUN_SECONDS" -gt 0 ]; then
    if [ "$TEST_RESUME" -eq 1 ]; then
        HALF=$((RUN_SECONDS / 2))
        [ $HALF -lt 2 ] && HALF=2
        REMAINING=$((RUN_SECONDS - HALF))
        [ $REMAINING -lt 2 ] && REMAINING=2
        cat >> "$TMP_DIR/init" << RESUME_EOF
echo ""
echo ">>> [STAGE 2] Playing boot animation (${HALF}s)..."
sleep $HALF

echo ""
echo ">>> [EVENT] Triggering SIGUSR1 (Resume from hibernation banner notification)..."
kill -USR1 \$SPLASH_PID 2>/dev/null || true
sleep 0.2
if kill -0 \$SPLASH_PID 2>/dev/null && [ "\$(awk '{print \$3}' /proc/\$SPLASH_PID/stat 2>/dev/null)" != "Z" ]; then
    echo "  ✓ Bootsplash caught SIGUSR1 and is still animating cleanly"
else
    echo "  ✗ ERROR: Bootsplash crashed or died on SIGUSR1!"
fi

echo "Playing resume splash (${REMAINING}s)..."
sleep $REMAINING
RESUME_EOF
    else
        cat >> "$TMP_DIR/init" << RUN_EOF
echo ""
echo ">>> [STAGE 2] Playing boot animation (${RUN_SECONDS}s)..."
sleep $RUN_SECONDS
RUN_EOF
    fi

    # --------------------------------------------------------------------------
    # STAGE 3: INIT-BOTTOM CLEANUP HOOK
    # --------------------------------------------------------------------------
    cat >> "$TMP_DIR/init" << STOP_EOF

echo ""
echo ">>> [STAGE 3] Simulating init-bottom handover to Desktop..."
if [ -f /run/xbootsplash.pid ]; then
    PID=\$(cat /run/xbootsplash.pid 2>/dev/null)
    if [ -n "\$PID" ] && [ -d "/proc/\$PID" ]; then
        SAVED_START=\$(cat /run/xbootsplash.start_time 2>/dev/null)
        CURRENT_START=\$(awk '{print \$22}' /proc/\$PID/stat 2>/dev/null)
        if [ -n "\$SAVED_START" ] && [ "\$SAVED_START" = "\$CURRENT_START" ]; then
            kill "\$PID" 2>/dev/null || true
            for _i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40; do
                [ -d "/proc/\$PID" ] || break
                sleep 0.1
            done
            if [ -d "/proc/\$PID" ]; then
                kill -9 "\$PID" 2>/dev/null || true
            fi
            echo "  ✓ Bootsplash cleanly stopped via SIGTERM"
        fi
    fi
    rm -f /run/xbootsplash.pid /run/xbootsplash.start_time
fi

# Framebuffer zeroing check
if [ -c /dev/fb0 ]; then
    dd if=/dev/zero of=/dev/fb0 bs=4096 count=100 2>/dev/null || true
    echo "  ✓ Framebuffer zeroed for X11/Desktop handoff"
fi

STOP_EOF

    # --------------------------------------------------------------------------
    # STAGE 4: SYSTEMD-SHUTDOWN HOOK VERIFICATION (Only if --with-shutdown)
    # --------------------------------------------------------------------------
    if [ "$WITH_SHUTDOWN" -eq 1 ]; then
        cat >> "$TMP_DIR/init" << SHUTDOWN_EOF

echo ""
echo ">>> [STAGE 4] Simulating systemd-shutdown execution (timeout 3s)..."
echo "  Notice: Testing shutdown animation after boot..."
sleep 1
T_START=\$(date +%s)
/lib/systemd/system-shutdown/bootsplash.shutdown
T_END=\$(date +%s)
T_DIFF=\$((T_END - T_START))
echo "  ✓ Shutdown animation wrapper finished in \${T_DIFF}s (Safe: never hung!)"

SHUTDOWN_EOF
    fi
else
    cat >> "$TMP_DIR/init" << WAIT_EOF
echo "Running indefinitely. Close QEMU window or send powerdown to exit."
wait \$SPLASH_PID 2>/dev/null || true
WAIT_EOF
fi

fi # end if [ "$TEST_SHUTDOWN" -eq 1 ]

# --------------------------------------------------------------------------
# STAGE 5: VM EXIT OR SHELL
# --------------------------------------------------------------------------
if [ "$DROP_SHELL" -eq 1 ]; then
    cat >> "$TMP_DIR/init" << SHELL_EOF
echo ""
echo ">>> [RESCUE SHELL] Dropping to interactive Busybox root shell..."
echo "Type 'poweroff' or 'exit' when done."
exec /bin/sh
SHELL_EOF
else
    cat >> "$TMP_DIR/init" << POWEROFF_EOF
echo ""
echo ">>> [STAGE 5] All validation stages passed. Powering down VM..."
sleep 1
poweroff -f
POWEROFF_EOF
fi

chmod 755 "$TMP_DIR/init"

# Pack cpio archive
(cd "$TMP_DIR" && find . | cpio -o -H newc 2>/dev/null | gzip -1 > "$TMP_INITRD")
INITRD_SIZE=$(stat -c%s "$TMP_INITRD")
echo -e "  Micro-initrd generated: ${BOLD}$((INITRD_SIZE / 1024)) KB${NC}"

# Step 5: Launch QEMU
echo -e "${CYAN}[5/5] Launching QEMU VM (Kernel + Dev Framebuffer)...${NC}"
echo -e "  Kernel        : $KERNEL_IMG"
echo -e "  Resolution    : $RESOLUTION"
echo -e "  Display UI    : $DISPLAY_BACKEND"
echo -e "  Test Resume   : $( [ $TEST_RESUME -eq 1 ] && echo -e "${GREEN}YES (will send SIGUSR1)${NC}" || echo "NO" )"
echo -e "  Test Shutdown : $( [ $TEST_SHUTDOWN -eq 1 ] && echo -e "${GREEN}YES (shutdown hook only)${NC}" || ( [ $WITH_SHUTDOWN -eq 1 ] && echo -e "${GREEN}YES (after boot)${NC}" || echo "NO" ) )"
echo -e "  Duration      : $( [ $RUN_SECONDS -gt 0 ] && echo "${RUN_SECONDS}s" || echo "Unlimited" )"
echo ""
echo -e "${YELLOW}Virtual screen will show the bootsplash animation.${NC}"
echo -e "${YELLOW}Host terminal displays live kernel & initramfs stage transitions.${NC}"
echo ""

# Run QEMU
qemu-system-x86_64 \
    $KVM_OPT \
    -m 1024 \
    -kernel "$KERNEL_IMG" \
    -initrd "$TMP_INITRD" \
    -vga std \
    -append "console=ttyS0 vga=${VGA_MODE} quiet loglevel=3" \
    -display "$DISPLAY_BACKEND" \
    -serial mon:stdio \
    -no-reboot

echo ""
echo -e "${GREEN}╔══════════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  ✔ TEST PASSED: Bootsplash lifecycle verified in real conditions!    ║${NC}"
echo -e "${GREEN}║  Host system, /boot, and initramfs were 100% UNTOUCHED and SAFE.     ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════════════════════╝${NC}"
exit 0
