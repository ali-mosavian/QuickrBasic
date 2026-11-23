/* Images: a file's bytes to pixels and a palette. BMP so far.
 *
 * # How it works
 * A BMP of 8 bits a pixel, uncompressed (Windows 3's and later headers): its rows bottom first,
 * unless its height is negative, each padded to 4 bytes; its colours B, G, R and a 0, as many as
 * it says, or 256.
 */
#ifndef GFX_IMAGE_H
#define GFX_IMAGE_H
#include <stdint.h>

/* w by h pixels, top row first, w a row; 256 colours, &HRRGGBB. The memory is the caller's. */
typedef struct {
    int32_t w, h;
    uint8_t *pixels;
    uint32_t *palette;
} Image;

typedef enum : uint8_t { IMG_OK, IMG_NOT_ONE, IMG_NO_MEMORY } ImgStatus;

ImgStatus img_bmp(const uint8_t *data, uint32_t n, Image *out);
#endif
