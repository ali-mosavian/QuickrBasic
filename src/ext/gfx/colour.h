/* Colour: the palette's rows, and the nearest index to a colour.
 *
 * # How it works
 * Row 0 is the canvas's, shown at the flip; the rest are pen.recolor's. Each is 256 colours,
 * &HRRGGBB. Row 0 starts as the VGA's mode 13h palette.
 */
#ifndef GFX_COLOUR_H
#define GFX_COLOUR_H
#include <stdint.h>

enum { PAL_ROWS = 16, PAL_COLOURS = 256 };

typedef struct {
    uint32_t rows[PAL_ROWS][PAL_COLOURS];
} Palette;

void pal_default(Palette *p);
/* The index in row nearest rgb, &HRRGGBB. */
uint8_t pal_nearest(const uint32_t *row, uint32_t rgb);
#endif
