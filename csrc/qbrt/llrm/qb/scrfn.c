/* B$FSCN: SCREEN(row, column[, f]) (QB rt/gwscr.asm B$FSCN, llscnfcn.asm B$SCREEN). The BIOS's
   read of a cell: a graphics mode's character is the first of the font its pixels match, the
   attribute 0; a NUL a space. */
#include "cndriver.h"
#include "dev.h"
#include "error.h"
#include "gfx.h"
#include "gfxdev.h"
#include "qb.h"

#pragma aux B_FSCN "B$FSCN"

enum { CELL_WIDTH = 8, GLYPHS = 256 };

/* The cell's pixels, a byte a row, set where not the background; the font's character so drawn,
   0 none. */
static unsigned recognised(unsigned row, unsigned column)
{
    unsigned height = gfx_current->cell_height, y, x, c;
    const u8 QB_FAR *font = gd_font(height);
    u8 bits[16];

    for (y = 0; y < height; y++) {
        bits[y] = 0;
        for (x = 0; x < CELL_WIDTH; x++)
            if (gd_read(column * CELL_WIDTH + x, row * height + y))
                bits[y] |= (u8)(0x80 >> x);
    }
    for (c = 0; c < GLYPHS; c++) {
        for (y = 0; y < height && font[c * height + y] == bits[y]; y++)
            ;
        if (y == height)
            return c;
    }
    return 0;
}

short B_FSCN(short row, short column, short attribute)
{
    unsigned rows = cn_graphics ? gfx_current->height / gfx_current->cell_height
                                : dev_text_size() >> 8;
    unsigned cell, c;

    if (row < 1 || column < 1 || (unsigned)row > rows || (unsigned)column > cn_driver()->width())
        qb_error(BE_ILLFUN);
    cell = cn_graphics ? recognised((unsigned)row - 1, (unsigned)column - 1)
                       : dev_text_get((unsigned)row - 1, (unsigned)column - 1);
    if (attribute)
        return (short)(cell >> 8);
    c = cell & 0xFF;
    return (short)(c ? c : ' ');
}
