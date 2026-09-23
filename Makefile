# xbootsplash - Boot splash animation Makefile
# Target: Linux x86_64, minimal binary
#
# Build modes:
#   make              -> fbdev version (nolibc, static, minimal)
#   make drm          -> DRM/KMS version (libdrm, dynamic linking)
#   make USE_DRM=1    -> same as 'make drm'

CC = gcc

# Engine source directory
ENGINE_DIR = engine_src

# Nolibc flags (freestanding, no libc) - for fbdev version
NOLIBC_FLAGS = -ffreestanding -fno-builtin -nostdlib -nostartfiles \
               -O2 -march=x86-64 -msse2 -fomit-frame-pointer -fstrict-aliasing \
               -fno-asynchronous-unwind-tables -fno-stack-protector \
               -fno-pic -fno-pie -fvisibility=hidden \
               -ffunction-sections -fdata-sections \
               -flto=2 -fno-ident \
               -I$(ENGINE_DIR) -I.

NOLIBC_LDFLAGS = -static -nostdlib -nostartfiles \
                 -Wl,--build-id=none,--strip-all,-O1,--gc-sections \
                 -flto=2 -fuse-ld=gold \
                 -T $(ENGINE_DIR)/linker.ld

# DRM flags (uses libdrm, dynamic linking)
DRM_FLAGS = -O2 -march=x86-64 -msse2 -fomit-frame-pointer \
            -fno-asynchronous-unwind-tables -fno-stack-protector \
            -fvisibility=hidden -ffunction-sections -fdata-sections \
            -flto=2 -fno-ident \
            -I$(ENGINE_DIR) -I. \
            $(shell pkg-config --cflags libdrm 2>/dev/null || echo -I/usr/include/libdrm)

DRM_LDFLAGS = -flto=2 -fuse-ld=gold -Wl,--gc-sections,--as-needed \
              $(shell pkg-config --libs libdrm 2>/dev/null || echo -ldrm)

# Target binary name (can be overridden via make TARGET=name or environment)
TARGET ?= xbootsplash
GENERATOR = generate_splash

# Default animation parameters
FRAME_DIR ?= win10_png
FRAME_OFFSET ?= 80
FRAME_DELAY ?= 33

# Build mode detection
USE_DRM ?= 0

.PHONY: all clean debug debug_x test test_ioctl test_mmap test_simple test_frame test_pattern test_debug test_rgb test_frame0 test_square generate drm fbdev generator gui appimage deb cli

# Default: build fbdev version (backward compatible)
all: fbdev
	@ls -l $(TARGET)
	@echo "Binary size: $$(stat -c%s $(TARGET)) bytes"

# fbdev version (nolibc, static)
fbdev: $(TARGET)
	@ls -l $(TARGET)
	@echo "Binary size: $$(stat -c%s $(TARGET)) bytes (fbdev mode)"

# DRM version (libdrm, dynamic)
drm: $(TARGET)_drm
	@ls -l $(TARGET)_drm
	@echo "Binary size: $$(stat -c%s $(TARGET)_drm) bytes (DRM mode)"

generator: $(GENERATOR)
	@ls -l $(GENERATOR)

extract_frames: CFLAGS = -O2 -Wall -Wextra -Wshadow
extract_frames: LDLIBS = -lpng -lm
extract_frames: CLI_src/extract_frames.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

generate: $(GENERATOR)
	./$(GENERATOR) -o $(FRAME_OFFSET) -d $(FRAME_DELAY) $(FRAME_DIR) > frames_delta.h

$(GENERATOR): $(ENGINE_DIR)/generate_splash.c
	$(CC) -O2 -I$(ENGINE_DIR) -I. -o $@ $< -lpng -lm

test_square: test_square.c $(ENGINE_DIR)/nolibc.h $(ENGINE_DIR)/start.S $(ENGINE_DIR)/linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_square.o test_square.c
	$(CC) -c -o start.o $(ENGINE_DIR)/start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_square start.o test_square.o
	strip --strip-all test_square

test_frame0: test_frame0.c $(ENGINE_DIR)/nolibc.h frames_delta.h $(ENGINE_DIR)/start.S $(ENGINE_DIR)/linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_frame0.o test_frame0.c
	$(CC) -c -o start.o $(ENGINE_DIR)/start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_frame0 start.o test_frame0.o
	strip --strip-all test_frame0

test_rgb: test_rgb.c $(ENGINE_DIR)/nolibc.h $(ENGINE_DIR)/start.S $(ENGINE_DIR)/linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_rgb.o test_rgb.c
	$(CC) -c -o start.o $(ENGINE_DIR)/start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_rgb start.o test_rgb.o
	strip --strip-all test_rgb

test_debug: test_debug.c $(ENGINE_DIR)/nolibc.h $(ENGINE_DIR)/start.S $(ENGINE_DIR)/linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_debug.o test_debug.c
	$(CC) -c -o start.o $(ENGINE_DIR)/start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_debug start.o test_debug.o
	strip --strip-all test_debug

test_pattern: test_pattern.c $(ENGINE_DIR)/nolibc.h $(ENGINE_DIR)/start.S $(ENGINE_DIR)/linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_pattern.o test_pattern.c
	$(CC) -c -o start.o $(ENGINE_DIR)/start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_pattern start.o test_pattern.o
	strip --strip-all test_pattern

test_frame: test_frame.c $(ENGINE_DIR)/nolibc.h frames_delta.h $(ENGINE_DIR)/start.S $(ENGINE_DIR)/linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_frame.o test_frame.c
	$(CC) -c -o start.o $(ENGINE_DIR)/start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_frame start.o test_frame.o
	strip --strip-all test_frame

test_simple: test_simple.c $(ENGINE_DIR)/start.S $(ENGINE_DIR)/linker.ld
	$(CC) -c -o test_simple.o test_simple.c
	$(CC) -c -o start.o $(ENGINE_DIR)/start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_simple start.o test_simple.o
	strip --strip-all test_simple

test_mmap: test_mmap.c $(ENGINE_DIR)/nolibc.h $(ENGINE_DIR)/start.S $(ENGINE_DIR)/linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_mmap.o test_mmap.c
	$(CC) -c -o start.o $(ENGINE_DIR)/start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_mmap start.o test_mmap.o
	strip --strip-all test_mmap

test_ioctl: test_ioctl.c $(ENGINE_DIR)/nolibc.h $(ENGINE_DIR)/start.S $(ENGINE_DIR)/linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_ioctl.o test_ioctl.c
	$(CC) -c -o start.o $(ENGINE_DIR)/start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_ioctl start.o test_ioctl.o
	strip --strip-all test_ioctl

test: test_fb
	@ls -l test_fb

test_fb: test_fb.c $(ENGINE_DIR)/nolibc.h
	$(CC) $(NOLIBC_FLAGS) -c -o test_fb.o test_fb.c
	$(CC) $(NOLIBC_LDFLAGS) -o test_fb test_fb.o
	strip --strip-all test_fb

debug: debug_nolibc
	@ls -l debug_nolibc

debug_x: debug_xbootsplash
	@ls -l debug_xbootsplash

debug_nolibc: debug_nolibc.c $(ENGINE_DIR)/nolibc.h
	$(CC) $(NOLIBC_FLAGS) -c -o debug_nolibc.o debug_nolibc.c
	$(CC) $(NOLIBC_LDFLAGS) -o debug_nolibc debug_nolibc.o
	strip --strip-all debug_nolibc

debug_xbootsplash: debug_xbootsplash.c $(ENGINE_DIR)/nolibc.h frames_delta.h
	$(CC) $(NOLIBC_FLAGS) -c -o debug_xbootsplash.o debug_xbootsplash.c
	$(CC) $(NOLIBC_LDFLAGS) -o debug_xbootsplash debug_xbootsplash.o
	strip --strip-all debug_xbootsplash

$(TARGET): splash_anim_delta.o frames_delta.h $(ENGINE_DIR)/nolibc.h $(ENGINE_DIR)/start.S $(ENGINE_DIR)/linker.ld
	$(CC) -c -o start.o $(ENGINE_DIR)/start.S
	$(CC) $(NOLIBC_LDFLAGS) -o $@ start.o splash_anim_delta.o
	strip --strip-all $@

splash_anim_delta.o: $(ENGINE_DIR)/splash_anim_delta.c $(ENGINE_DIR)/nolibc.h frames_delta.h
	$(CC) $(NOLIBC_FLAGS) -c -o $@ $<

# DRM version build rules
$(TARGET)_drm: splash_anim_drm.o frames_delta.h
	$(CC) -o $@ splash_anim_drm.o $(DRM_LDFLAGS)
	strip --strip-all $@

splash_anim_drm.o: $(ENGINE_DIR)/splash_anim_drm.c frames_delta.h
	$(CC) $(DRM_FLAGS) -c -o $@ $<

# Regenerate frames_delta.h from PNG images
frames: $(GENERATOR)
	./$(GENERATOR) -o $(FRAME_OFFSET) -d $(FRAME_DELAY) $(FRAME_DIR) > frames_delta.h

clean:
	rm -f $(TARGET) $(TARGET)_drm extract_frames *.o build.log

distclean: clean
	rm -f $(GENERATOR) frames_delta.h

# GUI Studio build & packaging targets
gui:
	./GUI_src/build.sh

appimage:
	./build_appimage.sh

deb:
	./build_deb.sh

cli:
	./build_cli.sh
