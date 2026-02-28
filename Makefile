# xbootsplash - Boot splash animation Makefile
# Target: Linux x86_64, minimal binary

CC = gcc

# Nolibc flags (freestanding, no libc)
NOLIBC_FLAGS = -ffreestanding -fno-builtin -nostdlib -nostartfiles \
               -Os -march=x86-64 -msse2 -fomit-frame-pointer -fstrict-aliasing \
               -fno-asynchronous-unwind-tables -fno-stack-protector \
               -fno-pic -fno-pie -fvisibility=hidden \
               -ffunction-sections -fdata-sections \
               -DNOLIBC_NO_ARENA

NOLIBC_LDFLAGS = -static -nostdlib -nostartfiles \
                 -Wl,--build-id=none,--strip-all,-O1,--gc-sections \
                 -T linker.ld

# Target binary name (can be overridden via make TARGET=name or environment)
TARGET ?= xbootsplash
GENERATOR = generate_splash

# Default animation parameters
FRAME_DIR ?= win10_png
FRAME_OFFSET ?= 80
FRAME_DELAY ?= 33

.PHONY: all clean debug debug_x test test_ioctl test_mmap test_simple test_frame test_pattern test_debug test_rgb test_frame0 test_square generate

all: $(TARGET)
	@ls -l $(TARGET)
	@echo "Binary size: $$(stat -c%s $(TARGET)) bytes"

generate: $(GENERATOR)
	./$(GENERATOR) -o $(FRAME_OFFSET) -d $(FRAME_DELAY) $(FRAME_DIR) > frames_delta.h

$(GENERATOR): generate_splash.c
	$(CC) -O2 -o $@ $< -lpng -lm

test_square: test_square.c nolibc.h start.S linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_square.o test_square.c
	$(CC) -c -o start.o start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_square start.o test_square.o
	strip --strip-all test_square

test_frame0: test_frame0.c nolibc.h frames_delta.h start.S linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_frame0.o test_frame0.c
	$(CC) -c -o start.o start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_frame0 start.o test_frame0.o
	strip --strip-all test_frame0

test_rgb: test_rgb.c nolibc.h start.S linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_rgb.o test_rgb.c
	$(CC) -c -o start.o start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_rgb start.o test_rgb.o
	strip --strip-all test_rgb

test_debug: test_debug.c nolibc.h start.S linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_debug.o test_debug.c
	$(CC) -c -o start.o start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_debug start.o test_debug.o
	strip --strip-all test_debug

test_pattern: test_pattern.c nolibc.h start.S linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_pattern.o test_pattern.c
	$(CC) -c -o start.o start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_pattern start.o test_pattern.o
	strip --strip-all test_pattern

test_frame: test_frame.c nolibc.h frames_delta.h start.S linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_frame.o test_frame.c
	$(CC) -c -o start.o start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_frame start.o test_frame.o
	strip --strip-all test_frame

test_simple: test_simple.c start.S linker.ld
	$(CC) -c -o test_simple.o test_simple.c
	$(CC) -c -o start.o start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_simple start.o test_simple.o
	strip --strip-all test_simple

test_mmap: test_mmap.c nolibc.h start.S linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_mmap.o test_mmap.c
	$(CC) -c -o start.o start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_mmap start.o test_mmap.o
	strip --strip-all test_mmap

test_ioctl: test_ioctl.c nolibc.h start.S linker.ld
	$(CC) $(NOLIBC_FLAGS) -c -o test_ioctl.o test_ioctl.c
	$(CC) -c -o start.o start.S
	$(CC) $(NOLIBC_LDFLAGS) -o test_ioctl start.o test_ioctl.o
	strip --strip-all test_ioctl

test: test_fb
	@ls -l test_fb

test_fb: test_fb.c nolibc.h
	$(CC) $(NOLIBC_FLAGS) -c -o test_fb.o test_fb.c
	$(CC) $(NOLIBC_LDFLAGS) -o test_fb test_fb.o
	strip --strip-all test_fb

debug: debug_nolibc
	@ls -l debug_nolibc

debug_x: debug_xbootsplash
	@ls -l debug_xbootsplash

debug_nolibc: debug_nolibc.c nolibc.h
	$(CC) $(NOLIBC_FLAGS) -c -o debug_nolibc.o debug_nolibc.c
	$(CC) $(NOLIBC_LDFLAGS) -o debug_nolibc debug_nolibc.o
	strip --strip-all debug_nolibc

debug_xbootsplash: debug_xbootsplash.c nolibc.h frames_delta.h
	$(CC) $(NOLIBC_FLAGS) -c -o debug_xbootsplash.o debug_xbootsplash.c
	$(CC) $(NOLIBC_LDFLAGS) -o debug_xbootsplash debug_xbootsplash.o
	strip --strip-all debug_xbootsplash

$(TARGET): splash_anim_delta.o frames_delta.h nolibc.h start.S linker.ld
	$(CC) -c -o start.o start.S
	$(CC) $(NOLIBC_LDFLAGS) -o $@ start.o splash_anim_delta.o
	strip --strip-all $@
	-sstrip $@ 2>/dev/null || true

splash_anim_delta.o: splash_anim_delta.c nolibc.h frames_delta.h
	$(CC) $(NOLIBC_FLAGS) -c -o $@ splash_anim_delta.c

# Regenerate frames_delta.h from PNG images
frames: $(GENERATOR)
	./$(GENERATOR) -o $(FRAME_OFFSET) -d $(FRAME_DELAY) $(FRAME_DIR) > frames_delta.h

clean:
	rm -f $(TARGET) $(GENERATOR) *.o
