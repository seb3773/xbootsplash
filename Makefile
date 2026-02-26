# xbootsplash - Boot splash animation Makefile
# Target: Linux x86_64, minimal binary

CC = gcc

# Nolibc flags (freestanding, no libc)
NOLIBC_FLAGS = -ffreestanding -fno-builtin -nostdlib -nostartfiles \
               -Os -march=x86-64 -fomit-frame-pointer -fstrict-aliasing \
               -fno-asynchronous-unwind-tables -fno-stack-protector \
               -fno-pic -fno-pie -fvisibility=hidden

NOLIBC_LDFLAGS = -static -nostdlib -nostartfiles \
                 -Wl,--build-id=none,--strip-all,-O1

TARGET = xbootsplash
GENERATOR = generate_delta_v2

.PHONY: all clean

all: $(TARGET)
	@ls -l $(TARGET)
	@echo "Binary size: $$(stat -c%s $(TARGET)) bytes"

$(TARGET): splash_anim_delta.o frames_delta.h nolibc.h
	$(CC) $(NOLIBC_LDFLAGS) -o $@ splash_anim_delta.o
	strip --strip-all $@
	-sstrip $@ 2>/dev/null || true

splash_anim_delta.o: splash_anim_delta.c nolibc.h frames_delta.h
	$(CC) $(NOLIBC_FLAGS) -c -o $@ splash_anim_delta.c

# Frame generator tool (requires libpng)
$(GENERATOR): generate_delta_v2.c
	$(CC) -O2 -o $@ generate_delta_v2.c -lpng

# Regenerate frames_delta.h from PNG images
frames: $(GENERATOR)
	./$(GENERATOR) win10_png > frames_delta.h

clean:
	rm -f $(TARGET) $(GENERATOR) *.o
