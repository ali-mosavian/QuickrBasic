/* A DOS slice's image, a 32-bit DLL: its interface. How it works: image.c. */
#ifndef UTIL_IMAGE_H
#define UTIL_IMAGE_H
#include "base.h"

/* A DLL read: its file; its base and size in memory; its section table; where its export
   directory lies in memory, and its relocations and their bytes. Pointers point into the file. */
typedef struct {
    const uint8_t *file, *sections;
    uint32_t nfile, nsections, base, size, exports, relocs, nrelocs;
} Image;

/* The n bytes at s as an i386 DLL that imports nothing, its sections inside the file and the image
   and each relocation a 32-bit word inside the image: false, not one. */
bool imag_read(const uint8_t *s, uint32_t n, Image *out);
/* The n bytes at offset at of im in memory, as its file stores them: null, not all stored. */
const uint8_t *imag_bytes(const Image *im, uint32_t at, uint32_t n);
/* *at, the offset in memory of im's export name: false, none. */
bool imag_export(const Image *im, const char *name, uint32_t *at);
/* Image im into block (im->size bytes), as if it lay at address base. */
void imag_place(const Image *im, uint8_t *block, uint32_t base);
#endif
