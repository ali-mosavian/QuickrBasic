/* Texels as a card takes them: ARGB1555, A a key's bit, in tiles of the card's sides.
 *
 * # How it works
 * A texture's tile is its sides each rounded up to a power of two, at most the card's side, then
 * the shorter doubled until the card's aspect holds. Its tiles go across and down from its top
 * left; past the texture's right or bottom edge, a tile repeats the edge's texels, so the card
 * holding a tile's edge holds the texture's.
 */
#ifndef PLAT_DOS32_ACCEL_TEXELS_H
#define PLAT_DOS32_ACCEL_TEXELS_H
#include "plat/dos32/accel/cut.h"

/* The tiles of a w by h texture, w and h at least 1, on card c: none wrapped. */
CutGrid texels_grid(uint16_t w, uint16_t h, const AccelCard *c);
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
/* Rows y0 up to y1 of tile column, row of a w by h texture's texels into out, g's tile_w a row. */
void texels_tile(
    const uint16_t *texels,
    uint16_t w,
    uint16_t h,
    const CutGrid *g,
    uint32_t column,
    uint32_t row,
    uint32_t y0,
    uint32_t y1,
    uint16_t *out
);
#endif
