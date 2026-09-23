#ifndef UPKR_COMPRESS_H
#define UPKR_COMPRESS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compress `in_size` bytes from `in` into `out` buffer of size `out_max`.
 * `level`: 0 = fast greedy, 1-9 = optimal/lazy parsing.
 * Returns the compressed size in bytes, or 0 if output buffer is too small or on error.
 */
size_t upkr_compress(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_max, int level);

#ifdef __cplusplus
}
#endif

#endif /* UPKR_COMPRESS_H */
