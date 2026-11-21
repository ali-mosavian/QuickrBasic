/* A DOS slice's image: its interface. How it works: image.c. */
#ifndef UTIL_IMAGE_H
#define UTIL_IMAGE_H
#include "base.h"

/* An image read: its bytes stored, its size in memory (the rest zero), its entry's offset, and the
   offsets of its 32-bit words that its address is added to. Each points into the slice. */
typedef struct {
    const uint8_t *stored, *relocs;
    uint32_t nstored, size, entry, nrelocs;
} Image;

/* The n bytes at s as an image: false, not one, or a part or relocation outside it. */
bool imag_read(const uint8_t *s, uint32_t n, Image *out);
/* Image im into block (im->size bytes), as if it lay at address base. */
void imag_place(const Image *im, uint8_t *block, uint32_t base);
#endif
