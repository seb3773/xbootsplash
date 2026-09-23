#ifndef ZX0_COMPRESS_H
#define ZX0_COMPRESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*zx0_progress_fn)(size_t processed, size_t total);

void zx0_set_progress_cb(zx0_progress_fn cb);

/*
 * Compress data with ZX0.
 * offset_limit: 0 for auto (32640 if <= 64KB, 4096 if > 64KB), or explicit max offset.
 * *out is allocated via malloc() and must be freed by caller.
 * Returns 0 on success, -1 on failure.
 */
int zx0_compress_custom(const unsigned char *in, size_t in_sz,
                        unsigned char **out, size_t *out_sz,
                        int offset_limit);

#ifdef __cplusplus
}
#endif

#endif /* ZX0_COMPRESS_H */
