#ifndef UPKR_DECOMPRESS_H
#define UPKR_DECOMPRESS_H

#ifdef __cplusplus
extern "C" {
#endif

void* upkr_unpack(void* destination, const void* compressed_data);

#ifdef __cplusplus
}
#endif

#endif /* UPKR_DECOMPRESS_H */
