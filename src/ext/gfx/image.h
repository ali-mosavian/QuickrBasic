/* Images: a file's bytes to pixels, BMP or PNG, told apart by their first bytes.
 *
 * # How it works
 * A BMP of 8 bits a pixel, uncompressed (Windows 3's and later headers): its rows bottom first,
 * unless its height is negative, each padded to 4 bytes; its colours B, G, R and a 0, as many as
 * it says, or 256.
 *
 * A PNG of any type and depth, not interlaced. An indexed one with at most one transparent colour
 * stays 8 bits: that colour swaps places with index 0, the key's. The rest are 32 bits, alpha as
 * the file has it, or tRNS's.
 */
#ifndef GFX_IMAGE_H
#define GFX_IMAGE_H
#include "pixels.h"

/* w by h pixels, top row first, w a row: 8 bits with 256 colours &HRRGGBB, or 32 bits and no
   palette. The memory is the caller's. */
typedef struct {
    int32_t w, h;
    PxFormat format;
    uint8_t *pixels;
    uint32_t *palette;
} Image;

typedef enum : uint8_t { IMG_OK, IMG_NOT_ONE, IMG_NO_MEMORY } ImgStatus;

/* A BMP or a PNG. */
ImgStatus img_load(const uint8_t *data, uint32_t n, Image *out);
ImgStatus img_bmp(const uint8_t *data, uint32_t n, Image *out);
ImgStatus img_png(const uint8_t *data, uint32_t n, Image *out);
#endif
