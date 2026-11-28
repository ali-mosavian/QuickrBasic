/* Texels as a card takes them: ARGB1555, A a key's bit, or an index in the low byte, in tiles of
 * the card's sides.
 *
 * # How it works
 * A texture's tile is its sides each rounded up to a power of two, within the card's least and
 * most, an indexed one's at least a word of texels across, then the shorter doubled until the
 * card's aspect holds. Its tiles go across and down from its top left; past the texture's right or
 * bottom edge, a tile repeats the edge's texels, so the card holding a tile's edge holds the
 * texture's.
 */
#ifndef PLAT_DOS32_ACCEL_TEXELS_H
#define PLAT_DOS32_ACCEL_TEXELS_H
#include "plat/dos32/accel/cut.h"

/* A word's indexed texels, as a driver takes them. */
enum { TEXELS_INDEXED = 4 };

/* The tiles of a w by h texture, w and h at least 1, indexed or not, on card c: none wrapped. */
CutGrid texels_grid(uint16_t w, uint16_t h, bool indexed, const AccelCard *c);
/* h rows of w texels from bytes R, G, B and A, from_pitch bytes a row, into to, to_pitch texels
   a row: A at least a half keyed. */
void texels_from_rgba(
    uint16_t *to,
    uint32_t to_pitch,
    const uint8_t *from,
    uint32_t from_pitch,
    uint32_t w,
    uint32_t h
);
/* h rows of w texels from from, from_pitch texels a row, into bytes R, G, B and A, w a row. */
void texels_to_rgba(uint8_t *to, const uint16_t *from, uint32_t from_pitch, uint32_t w, uint32_t h);
/* h rows of w indices from bytes, from_pitch a row, into to, to_pitch texels a row. */
void texels_from_indices(
    uint16_t *to,
    uint32_t to_pitch,
    const uint8_t *from,
    uint32_t from_pitch,
    uint32_t w,
    uint32_t h
);
/* h rows of w indexed texels, from_pitch a row, into out, its index in each of 4 bytes, w a row. */
void texels_to_indices(
    uint8_t *to,
    const uint16_t *from,
    uint32_t from_pitch,
    uint32_t w,
    uint32_t h
);
/* Of size texels from at, those a draw reaches of n: its own, and the one past its last a filter
   reads. */
uint32_t texels_reached(uint32_t n, uint32_t at, uint32_t size);
/* Rows y0 up to y1 of tile column, row of a w by h texture's texels into out, wide across from the
   tile's left: past the texture, its last column and row repeated. */
void texels_tile(
    const uint16_t *texels,
    uint16_t w,
    uint16_t h,
    const CutGrid *g,
    uint32_t column,
    uint32_t row,
    uint32_t y0,
    uint32_t y1,
    uint32_t wide,
    uint16_t *out
);
#endif
