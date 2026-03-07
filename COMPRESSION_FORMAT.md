# xbootsplash Compression Format Specification

This document describes the compression formats used in xbootsplash for frame data.

---

## Overview

xbootsplash uses multiple compression methods optimized for boot splash animations:
- **Frame 0**: Always stored as raw RGB565 (no compression)
- **Delta frames**: Compressed using one of four methods

The compression method is selected at build time (`generate_splash.c`) and embedded in the binary via `COMPRESS_METHOD` define.

---

## Compression Methods

| ID | Method | Description |
|----|--------|-------------|
| 0 | RLE XOR | Run-length encoded XOR deltas |
| 1 | RLE Direct | Run-length encoded direct pixel values |
| 2 | Sparse XOR | Position + XOR for changed pixels only |
| 3 | Raw | Uncompressed RGB565 (rarely used) |
| 4 | Palette + LZSS | For static images and backgrounds (mode 3,4 and backgrounds in mode 1,2) |

---

## Method 0: RLE XOR

**Purpose**: Delta compression for frames with moderate changes.

**Format**:
```
Stream of commands, terminated by 0x00:

  cmd = 0x00              -> End of stream
  cmd = 0x01-0x7F         -> Literal run: next (cmd) uint16_t XOR values
  cmd = 0x80-0xFF         -> Skip run: skip (cmd & 0x7F) + 1 pixels
```

**Decoding**:
1. Start at pixel index 0
2. Read command byte
3. If literal run (0x01-0x7F): read N uint16_t values, XOR into frame_buffer[pixel_idx++]
4. If skip run (0x80-0xFF): advance pixel_idx by (cmd & 0x7F) + 1
5. Repeat until 0x00 or end of data

**Example**:
```
0x03 0xAB 0xCD 0x12 0x34 0x56 0x78 0x80 0x00
  |    |------|  |------|  |------|  |    |
  |    3 XOR    2 more    skip 1    end
  |    values
  3 literal values
```

---

## Method 1: RLE Direct

**Purpose**: Direct pixel encoding for frames with many repeated colors.

**Format**:
```
Stream of commands, terminated by 0x00:

  cmd = 0x00              -> End of stream
  cmd = 0x01-0x7F         -> Literal run: next (cmd) uint16_t pixel values
  cmd = 0x80-0xFF         -> Repeat run: next uint16_t repeated (cmd & 0x7F) times
```

**Decoding**:
1. Start at pixel index 0
2. Read command byte
3. If literal run (0x01-0x7F): read N uint16_t values, store directly
4. If repeat run (0x80-0xFF): read 1 uint16_t, repeat N times
5. Repeat until 0x00 or end of data

**Example**:
```
0x82 0xFF 0x00 0x05 0xAA 0xBB ... 0x00
  |    |------|  |    |------|
  |    repeat   5    5 literal
  |    0x00FF   pixels values
  |    2 times
  repeat 0x00FF twice
```

---

## Method 2: Sparse XOR

**Purpose**: Optimal for frames with very few changed pixels.

**Format**:
```
Header: 4 bytes (uint32_t little-endian) - number of changed pixels

For each changed pixel:
  Position: 4 bytes (uint32_t little-endian) - pixel index
  XOR value: 2 bytes (uint16_t little-endian) - XOR to apply
```

**Decoding**:
1. Read 4-byte count N
2. For each of N entries:
   - Read 4-byte pixel index
   - Read 2-byte XOR value
   - Apply: frame_buffer[index] ^= xor_value

**Size calculation**:
```
Total size = 4 + (changed_count * 6)
```

**Best for**: Animations with sparse changes (e.g., small moving object on static background).

---

## Method 3: Raw RGB565

**Purpose**: Uncompressed fallback, rarely used.

**Format**:
```
Direct sequence of uint16_t RGB565 pixels in little-endian byte order.
Size = width * height * 2 bytes.
```

**Decoding**: Direct memcpy to frame buffer.

---

## Method 4: Palette + LZSS

**Purpose**: Static images and backgrounds (256-color palette + LZSS compression).

**Format**:
```
Palette: N * 2 bytes (uint16_t RGB565 values, N <= 256)
Compressed indices: LZSS-compressed byte stream
```

**LZSS Parameters**:
- Window size: 4096 bytes
- Minimum match: 3 bytes
- Flag byte: 8 items per flag (1=literal, 0=back-reference)

**LZSS Back-reference**:
```
2 bytes: offset (12 bits) + length (4 bits)
  b1: bits 0-7 of offset
  b2: bits 4-7 = bits 8-11 of offset, bits 0-3 = length - 3
```

**Decoding**:
1. Read flag byte
2. For each bit (LSB first):
   - If bit=1: read literal byte, expand via palette[index]
   - If bit=0: read 2-byte back-reference, copy from window
3. Repeat until pixel_count reached

---

## Frame Storage Order

### Animation Modes (0, 1, 2)

```
[Frame 0 - Raw RGB565]
[Frame 1 - Delta (method-dependent)]
[Frame 2 - Delta (method-dependent)]
...
[Frame N-1 - Delta (method-dependent)]
```

### Modes with Background (1, 2)

```
[Background - Palette + LZSS]
[Frame 0 - Raw RGB565]
[Frame 1 - Delta]
...
```

### Static Modes (3, 4)

```
[Single image - Palette + LZSS]
```

---

## Byte Order

All multi-byte values are **little-endian** (x86_64 native).

---

## CRC32 Verification

Frame 0 includes a CRC32 checksum for integrity verification:
- Calculated over first 1024 bytes of compressed frame 0
- Stored in watermark struct as `FRAME_CRC`
- Used for package validation during installation

**CRC32 Polynomial**: 0xEDB88320 (standard)

---

## Selection Algorithm (Auto Mode)

`generate_splash.c` tests all applicable methods and selects the smallest:
1. Compress all frames with each method
2. Sum total compressed size
3. Select method with smallest total

**Current auto-test methods**: RLE_XOR, Sparse, RLE_DIRECT

---

## Implementation Files

| File | Purpose |
|------|---------|
| `generate_splash.c` | Compression implementation |
| `splash_anim_delta.c` | Decompression (fbdev) |
| `splash_anim_drm.c` | Decompression (DRM) |
| `extract_frames.c` | Frame extraction utility |

---

## Performance Notes

- **RLE XOR**: Best for animations with localized changes
- **Sparse XOR**: Best for very sparse changes (< 5% pixels)
- **RLE Direct**: Best for frames with large solid areas
- **Palette + LZSS**: Best for static images with limited colors (<= 256)

All decompression functions include bounds checking for corrupted data safety.
