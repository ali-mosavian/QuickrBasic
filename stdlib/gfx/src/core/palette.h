/* Colour: the palette's rows, the nearest index to a colour, and the tables an 8-bit surface draws
 * light, tint, palette rows and blending through.
 *
 * # How it works
 * Row 0 is the screen's, shown at the flip; the rest are draw.set_palette_row's. Each is 256 colours,
 * &HRRGGBB. Row 0 starts as the VGA's mode 13h palette.
 *
 * The tables come from row 0 and are made when first asked for, then kept until it changes:
 * - the map: 5 bits a channel to row 0's nearest index (32 KB), so any colour finds its index at
 *   once;
 * - the light table: 64 rows of 256 indices, Quake's colormap; row 63 is each index itself;
 * - a blend table for each mode and alpha asked for, the last few kept: 256 rows by the colour
 *   drawn, 256 by what is there (64 KB).
 * A palette_row, light and tint mix asked for is a row of 256 indices, made through the map.
 */
#ifndef GFX_PALETTE_H
#define GFX_PALETTE_H
#include "base/pixels.h"

/* PAL_CELLS: the map's, 5 bits a channel. */
enum {
    PAL_ROWS = 16,
    PAL_COLOURS = 256,
    PAL_FULL = PX_FULL,
    PAL_WHITE = PX_WHITE,
    PAL_BLENDS = 4,
    PAL_CELLS = 1 << 15,
};

/* What a mix of a row was made from. */
typedef struct {
    int32_t row, light;
    uint32_t tint, changed[2]; /* row 0's changes and row's */
} PalKey;

typedef struct {
    uint8_t *table;
    BlendMode mode;
    uint8_t alpha;
    uint32_t used; /* the call it was last asked for: the oldest goes first */
} PalBlend;

typedef struct {
    uint32_t rows[PAL_ROWS][PAL_COLOURS];
    uint32_t changed[PAL_ROWS]; /* each row's changes, counted */
    uint8_t *map, *light;       /* null, not made since row 0 changed */
    PalBlend blends[PAL_BLENDS];
    uint8_t *lut;   /* the last mix asked for: its row, light, tint and their rows' changes */
    uint32_t *argb; /* the last colours asked for, likewise */
    PalKey lut_key, argb_key;
    uint32_t *pixels; /* the last pixel table asked for, likewise, and its formats */
    PalKey pixels_key;
    PxFormat pixels_from, pixels_into;
    uint32_t asked;
} Palette;

void pal_default(Palette *p);
void pal_done(Palette *p);
/* Row's first n colours set. */
void pal_set(Palette *p, int32_t row, const uint32_t *colours, uint32_t n);
/* The index in row nearest rgb, &HRRGGBB, searched. */
uint8_t pal_nearest(const uint32_t *row, uint32_t rgb);
/* Likewise, but never 0, the key. */
uint8_t pal_nearest_drawn(const uint32_t *row, uint32_t rgb);
/* Row 0's index for rgb through the map, to *index: false, no memory. */
bool pal_index(Palette *p, uint32_t rgb, uint8_t *index);
/* The indices index i draws as: row's colour i (row 0 itself), lit by light (0 to 63) and
   multiplied by tint, to *lut; null each as it is. false: no memory. */
bool pal_lut(Palette *p, int32_t row, int32_t light, uint32_t tint, const uint8_t **lut);
/* The blend table of mode at alpha (0 to 63): null, no memory. */
const uint8_t *pal_blend(Palette *p, BlendMode mode, int32_t alpha);
/* rgb lit by light and multiplied by tint. */
uint32_t pal_shade(uint32_t rgb, int32_t light, uint32_t tint);
/* The map of row 0: null, no memory. */
const uint8_t *pal_map(Palette *p);
/* What row lit by light and multiplied by tint is made from. */
PalKey pal_key(const Palette *p, int32_t row, int32_t light, uint32_t tint);
bool pal_same(PalKey a, PalKey b);
/* The colours each index of row draws as, &HFFRRGGBB, lit by light and multiplied by tint: null,
   no memory. */
const uint32_t *pal_argb(Palette *p, int32_t row, int32_t light, uint32_t tint);
/* px_table's of texels of format from into into, lit by light and multiplied by tint, row's
   colours from 8 bits: null, no memory. */
const uint32_t *pal_pixels(
    Palette *p,
    int32_t row,
    int32_t light,
    uint32_t tint,
    PxFormat from,
    PxFormat into
);
#endif
