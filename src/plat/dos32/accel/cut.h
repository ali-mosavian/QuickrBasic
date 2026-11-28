/* A triangle cut where it leaves the card's reach, then where it crosses a texture's tiles or a
 * wrap's periods, each piece then in one tile, its texels the tile's own: a card's corners wrap
 * round past its reach, its textures are smaller than gfx's, and wrap only whole.
 *
 * # How it works
 * The cut is in screen space, where x, y, z, q, s times q, t times q and the colour all go
 * linearly: the line s = c is where s q - c q is 0, so a piece's texels are where the whole's
 * were, in perspective too. A wrap cuts at each period of its rectangle and moves each piece's
 * texels into the first; the tiles then cut at their seams. Texels before the first tile or past
 * the last are the edge tile's, held at its edge by the card. A triangle with a corner past the
 * band is cut to it first, by x and y, and its part within cut on as triangles.
 */
#ifndef PLAT_DOS32_ACCEL_CUT_H
#define PLAT_DOS32_ACCEL_CUT_H
#include "plat/dos32/accel/driver.h"

/* The most corners a piece has: a triangle cut by a period's two sides and a tile's, each axis. */
enum { CUT_CORNERS = 12 };

/* A texture's tiles, tile_w by tile_h texels each, across by down of them; wrap_s and wrap_t, the
   axes cut at wrap's periods (those the card cannot wrap itself); band, the x and y a corner
   may take, x up to x + w and y up to y + h: w 0, any. */
typedef struct {
    uint16_t tile_w, tile_h, across, down;
    bool wrap_s, wrap_t;
    AccelRect wrap, band;
} CutGrid;

/* Whether v's n corners lie in band: w 0, any. */
bool cut_inside(const AccelRect *band, const AccelVertex *v, uint32_t n);

/* A piece, in the tile at column and row: n corners round, a convex polygon, s and t the tile's. */
typedef void CutPiece(void *ctx, uint32_t column, uint32_t row, const AccelVertex *v, uint32_t n);

/* Triangle v's pieces to piece: the triangle itself when it lies in the band, in one tile, and its
   texels in one period of each wrap. Each corner's q is above 0. */
void cut_triangle(const CutGrid *g, const AccelVertex v[3], CutPiece *piece, void *ctx);
#endif
