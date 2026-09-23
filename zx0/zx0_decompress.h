#ifndef ZX0_DECOMPRESS_H
#define ZX0_DECOMPRESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Decompress ZX0 compressed data into destination buffer.
 * Freestanding: requires no heap/malloc, no libc.
 * Returns number of bytes decompressed on success, or -1 on failure.
 */
int zx0_decompress_to(const unsigned char *in, int in_size,
                      unsigned char *out, int out_max);

#ifdef __cplusplus
}
#endif

#endif /* ZX0_DECOMPRESS_H */
