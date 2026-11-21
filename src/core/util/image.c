/* A DOS slice's image: code and data linked at address 0, placed anywhere (plat/dos32/pext.c).
 *
 * # How it works
 * tools/qbx.py pack makes it from a DLL, little-endian: "QXI" and its version, 1; the bytes stored,
 * the image's size in memory, its entry's offset and its relocations' count (32 bits each); the
 * bytes, padded to 4; then each relocation, the offset of a 32-bit word. Placed, the block is
 * zeroed past the bytes stored (the DLL's bss) and its address added to each word.
 */
#include "util/image.h"

enum { HEAD = 20, VERSION = 1 };

bool imag_read(const uint8_t *const s, const uint32_t n, Image *const out)
{
    if (n < HEAD || memcmp(s, "QXI", 3) || s[3] != VERSION)
        return false;

    const uint32_t nstored = load32(s + 4), size = load32(s + 8), entry = load32(s + 12);
    const uint32_t nrelocs = load32(s + 16), padded = (nstored + 3) & ~3u;

    if (nstored > size || entry >= size || nstored > n - HEAD || padded > n - HEAD ||
        nrelocs > (n - HEAD - padded) / 4)
        return false;

    *out = (Image){
        .stored = s + HEAD,
        .relocs = s + HEAD + padded,
        .nstored = nstored,
        .size = size,
        .entry = entry,
        .nrelocs = nrelocs,
    };

    for (uint32_t i = 0; i < nrelocs; i++)
        if (size < 4 || load32(out->relocs + 4 * i) > size - 4)
            return false;

    return true;
}

void imag_place(const Image *const im, uint8_t *const block, const uint32_t base)
{
    memcpy(block, im->stored, im->nstored);
    memset(block + im->nstored, 0, im->size - im->nstored);

    for (uint32_t i = 0; i < im->nrelocs; i++) {
        uint8_t *const w = block + load32(im->relocs + 4 * i);
        store32(w, load32(w) + base);
    }
}
