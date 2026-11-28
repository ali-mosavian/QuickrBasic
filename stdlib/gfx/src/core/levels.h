/* Levels: a texture's mipmap, each level half the last, made on the CPU so that every backend
 * draws from the same texels.
 *
 * # How it works
 * A level's texel is the mean of the four under it in the level before; an odd last row or column
 * is left out. Keyed (a draw that skips keys, px_key), a texel is drawn when two or more of its
 * four are, as their mean, else it is a key: SMOOTH's rule for coverage. At 8 bits the mean is of
 * row 0's colours, matched back to the nearest index, never the key when keyed. Levels stop before
 * the shorter side falls under LEVELS_LEAST, a 3dfx card's least.
 */
#ifndef GFX_LEVELS_H
#define GFX_LEVELS_H
#include "base/pixels.h"

enum { LEVELS_LEAST = 4, LEVELS_MOST = 16 };

/* A texture's levels: at[0] its own pixels, the rest made, in block. */
typedef struct {
    Pixels at[LEVELS_MOST];
    uint32_t n;
    uint8_t *block;
} Levels;

/* The levels of a w by h texture, its own counted: 1 when its shorter side is under twice
   LEVELS_LEAST. */
uint32_t levels_of(int32_t w, int32_t h);
/* into, the level after from: half its sides, rounded down. row: row 0's colours, at 8 bits. */
void levels_halve(const Pixels *from, const Pixels *into, bool keyed, const uint32_t *row);
/* l, source's levels made: false, no memory, and l only source. */
bool levels_make(Levels *l, const Pixels *source, bool keyed, const uint32_t *row);
void levels_done(Levels *l);
#endif
