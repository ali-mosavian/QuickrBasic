/* A DOS slice's image: a 32-bit Windows DLL as lld-link /dll writes it, placed anywhere
 * (plat/dos32/pext.c).
 *
 * # How it works
 * Its headers give its base and size in memory, its sections and its directories: the exports and
 * the base relocations. Placed, the block is zeroed, each section's stored bytes copied to its
 * offset, and the block's address less the DLL's base added to the 32-bit word at each HIGHLOW
 * relocation. It imports nothing: a slice has no C library to link against.
 */
#include "util/image.h"
#include <string.h>

enum {
    PE_AT = 0x3C,
    COFF = 4,
    OPTIONAL = 24,
    SECTION = 40,
    DIRECTORY = 96,
    I386 = 0x14C,
    PE32 = 0x10B,
    EXPORTS = 0,
    IMPORTS = 1,
    RELOCATIONS = 5,
    EXPORT_HEAD = 40,
    BLOCK_HEAD = 8,
    ABSOLUTE = 0,
    HIGHLOW = 3,
};

/* The n bytes at offset at lie inside size's. */
static bool inside(const uint32_t size, const uint32_t at, const uint32_t n)
{
    return at <= size && n <= size - at;
}

/* Section s's bytes stored in the file: its raw bytes, no more than it holds in memory. */
static uint32_t stored(const uint8_t *const s)
{
    const uint32_t memory = load32(s + 8), raw = load32(s + 16);
    return memory && memory < raw ? memory : raw;
}

/* Each relocation of im checked inside it and, with a block, delta added to its word there: false,
   a block cut short or a relocation not a 32-bit word's inside the image. */
static bool relocate(const Image *const im, uint8_t *const block, const uint32_t delta)
{
    const uint8_t *const r = imag_bytes(im, im->relocs, im->nrelocs);

    if (im->nrelocs && !r)
        return false;

    for (uint32_t at = 0; at < im->nrelocs;) {
        if (!inside(im->nrelocs, at, BLOCK_HEAD))
            return false;

        const uint32_t page = load32(r + at), bytes = load32(r + at + 4);

        if (bytes < BLOCK_HEAD || !inside(im->nrelocs, at, bytes))
            return false;

        for (uint32_t i = BLOCK_HEAD; i + 2 <= bytes; i += 2) {
            const uint16_t e = load16(r + at + i);
            const uint32_t word = page + (e & 0xFFF);

            if (e >> 12 == ABSOLUTE)
                continue;

            if (e >> 12 != HIGHLOW || !inside(im->size, page, (e & 0xFFFu) + 4))
                return false;

            if (block)
                store32(block + word, load32(block + word) + delta);
        }

        at += bytes;
    }

    return true;
}

bool imag_read(const uint8_t *const s, const uint32_t n, Image *const out)
{
    if (n < PE_AT + 4 || memcmp(s, "MZ", 2))
        return false;

    const uint32_t pe = load32(s + PE_AT);

    if (!inside(n, pe, OPTIONAL) || memcmp(s + pe, "PE\0\0", 4) || load16(s + pe + COFF) != I386)
        return false;

    const uint32_t opt = pe + OPTIONAL, optsize = load16(s + pe + 20);
    const uint32_t nsections = load16(s + pe + 6);
    const uint8_t *const dirs = s + opt + DIRECTORY;

    if (optsize < DIRECTORY + 8 * (RELOCATIONS + 1) || !inside(n, opt, optsize) ||
        load16(s + opt) != PE32 || load32(s + opt + 92) <= RELOCATIONS ||
        !inside(n, opt + optsize, nsections * SECTION) || load32(dirs + 8 * IMPORTS + 4))
        return false;

    *out = (Image){
        .file = s,
        .sections = s + opt + optsize,
        .nfile = n,
        .nsections = nsections,
        .base = load32(s + opt + 28),
        .size = load32(s + opt + 56),
        .exports = load32(dirs + 8 * EXPORTS),
        .relocs = load32(dirs + 8 * RELOCATIONS),
        .nrelocs = load32(dirs + 8 * RELOCATIONS + 4),
    };

    for (uint32_t i = 0; i < nsections; i++) {
        const uint8_t *const sec = out->sections + SECTION * i;
        const uint32_t held = stored(sec), memory = load32(sec + 8);

        if (!inside(out->size, load32(sec + 12), memory > held ? memory : held) ||
            !inside(n, load32(sec + 20), held))
            return false;
    }

    return relocate(out, nullptr, 0);
}

const uint8_t *imag_bytes(const Image *const im, const uint32_t at, const uint32_t n)
{
    for (uint32_t i = 0; i < im->nsections; i++) {
        const uint8_t *const sec = im->sections + SECTION * i;
        const uint32_t va = load32(sec + 12);

        if (at >= va && inside(stored(sec), at - va, n))
            return im->file + load32(sec + 20) + (at - va);
    }

    return nullptr;
}

bool imag_export(const Image *const im, const char *const name, uint32_t *const at)
{
    const uint8_t *const d = imag_bytes(im, im->exports, EXPORT_HEAD);

    if (!d)
        return false;

    const uint32_t nfuncs = load32(d + 20), nnames = load32(d + 24);

    if (nfuncs > im->size / 4 || nnames > im->size / 4)
        return false;

    const uint8_t *const funcs = imag_bytes(im, load32(d + 28), 4 * nfuncs);
    const uint8_t *const names = imag_bytes(im, load32(d + 32), 4 * nnames);
    const uint8_t *const ords = imag_bytes(im, load32(d + 36), 2 * nnames);
    const uint32_t len = (uint32_t)strlen(name) + 1;

    for (uint32_t i = 0; funcs && names && ords && i < nnames; i++) {
        const uint8_t *const s = imag_bytes(im, load32(names + 4 * i), len);
        const uint16_t ord = load16(ords + 2 * i);

        if (s && !memcmp(s, name, len) && ord < nfuncs) {
            *at = load32(funcs + 4 * ord);
            return *at < im->size;
        }
    }

    return false;
}

void imag_place(const Image *const im, uint8_t *const block, const uint32_t base)
{
    memset(block, 0, im->size);

    for (uint32_t i = 0; i < im->nsections; i++) {
        const uint8_t *const sec = im->sections + SECTION * i;
        memcpy(block + load32(sec + 12), im->file + load32(sec + 20), stored(sec));
    }

    relocate(im, block, base - im->base);
}
