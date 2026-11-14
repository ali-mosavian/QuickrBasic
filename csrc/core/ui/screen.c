/* The editor's screen: cells, strings, boxes and shadows, drawn off-screen and shown in one flip.
 *
 * # How it works
 * Drawing goes to a copy of page 0; scrn_flip shows it. A place is row, col; a slot is an attribute
 * slot (SL_*), made the theme's attribute (them_attr) as it is drawn.
 *
 * A string's '&' marks its hotkey, drawn in slot hot; a plain one has hot == slot. A run goes on
 * into the next row past column 79.
 *
 * # asm structure (src/ui/scr.asm)
 * | asm | here | |
 * |---|---|---|
 * | sc_fill, sc_str, sc_strn, sc_slen | scrn_fill, scrn_str, scrn_strn, scrn_slen | MIRROR |
 * | sc_box, sc_sep, sc_attr, sc_shadow | scrn_box, scrn_sep, scrn_attr, scrn_shadow | MIRROR |
 * | sc_cur, sc_shape, sc_flip | scrn_cursor, scrn_shape, scrn_flip | MIRROR |
 * | sc_at | cell | MIRROR |
 * | g_scrb | cells (static) | MIRROR |
 */
#include "ui/screen.h"
#include "ui/theme.h"
#include "plat/pscr.h"

static PscrCell cells[PSCR_ROWS * PSCR_COLS]; /* the screen as drawn, then shown */

enum { SHADOW_COLS = 2 }; /* a shadow's columns right of a box */
enum { LIGHT_GRAY = 0x07, DARK_GRAY = 0x08 };

static PscrCell *cell(const uint8_t row, const uint8_t col)
{
    return &cells[row * PSCR_COLS + col];
}
static PscrCell make(const uint8_t ch, const uint8_t attr) { return (PscrCell)(attr << 8 | ch); }

uint8_t scrn_fill(
    const uint8_t row,
    const uint8_t col,
    const uint8_t slot,
    const uint8_t ch,
    const uint16_t n
)
{
    const PscrCell c = make(ch, them_attr(slot));
    PscrCell *const p = cell(row, col);

    for (uint16_t i = 0; i < n; i++)
        p[i] = c;

    return col + n;
}

uint8_t scrn_str(
    const uint8_t row,
    const uint8_t col,
    const char *const s,
    const uint8_t slot,
    const uint8_t hot
)
{
    const uint8_t attr = them_attr(slot), hattr = them_attr(hot);
    PscrCell *const p = cell(row, col);
    uint32_t n = 0;

    for (uint32_t i = 0; s[i]; i++, n++) {
        const bool hotkey = s[i] == '&';
        i += hotkey;
        p[n] = make(s[i], hotkey ? hattr : attr);
    }

    return (uint8_t)(col + n);
}

uint8_t scrn_strn(
    const uint8_t row,
    const uint8_t col,
    const uint8_t *const s,
    const uint16_t n,
    const uint8_t slot
)
{
    const uint8_t attr = them_attr(slot);
    PscrCell *const p = cell(row, col);

    for (uint16_t i = 0; i < n; i++)
        p[i] = make(s[i], attr);

    return col + n;
}

uint8_t scrn_dim(const uint8_t row, const uint8_t col, const uint8_t *const s, const uint16_t n)
{
    const uint8_t bg = them_attr(SL_ED) & 0xF0;
    const uint8_t attr = bg | (bg == DARK_GRAY << 4 ? LIGHT_GRAY : DARK_GRAY);
    PscrCell *const p = cell(row, col);

    for (uint16_t i = 0; i < n; i++)
        p[i] = make(s[i], attr);

    return col + n;
}

uint16_t scrn_slen(const char *const s)
{
    uint16_t n = 0;

    for (uint32_t i = 0; s[i]; i++)
        n += s[i] != '&';

    return n;
}

/* A box's row: left, n - 2 of mid, right. */
static void box_row(
    const uint8_t row,
    const uint8_t col,
    const uint8_t n,
    const uint8_t slot,
    const uint8_t left,
    const uint8_t mid,
    const uint8_t right
)
{
    const uint8_t mid_col = scrn_fill(row, col, slot, left, 1);
    const uint8_t right_col = scrn_fill(row, mid_col, slot, mid, (uint16_t)(n - 2));
    scrn_fill(row, right_col, slot, right, 1);
}

/* title, a space either side, centred on row from col, cols wide. */
static void box_title(
    const uint8_t row,
    const uint8_t col,
    const uint8_t cols,
    const uint8_t slot,
    const char *const title
)
{
    const uint8_t at = col + (uint8_t)((uint32_t)(cols - (scrn_slen(title) + 2)) >> 1);
    const uint8_t text = scrn_fill(row, at, slot, ' ', 1);
    const uint8_t end = scrn_str(row, text, title, slot, slot);
    scrn_fill(row, end, slot, ' ', 1);
}

void scrn_box(
    const uint8_t row,
    const uint8_t col,
    const uint8_t rows,
    const uint8_t cols,
    const uint8_t slot,
    const char *const title
)
{
    uint8_t r = row;
    box_row(r, col, cols, slot, GLYPH_TOP_LEFT, GLYPH_H, GLYPH_TOP_RIGHT);

    for (int i = 2; i < rows; i++)
        box_row(++r, col, cols, slot, GLYPH_V, ' ', GLYPH_V);

    box_row(++r, col, cols, slot, GLYPH_BOTTOM_LEFT, GLYPH_H, GLYPH_BOTTOM_RIGHT);

    if (title)
        box_title(row, col, cols, slot, title);
}

void scrn_sep(const uint8_t row, const uint8_t col, const uint8_t cols, const uint8_t slot)
{
    box_row(row, col, cols, slot, GLYPH_V_RIGHT, GLYPH_H, GLYPH_V_LEFT);
}

void scrn_attr(const uint8_t row, const uint8_t col, const uint16_t n, const uint8_t slot)
{
    const uint8_t attr = them_attr(slot);
    PscrCell *const p = cell(row, col);

    for (uint16_t i = 0; i < n; i++)
        p[i] = (PscrCell)((p[i] & 0xFF) | attr << 8);
}

void scrn_shadow(const uint8_t row, const uint8_t col, const uint8_t rows, const uint8_t cols)
{
    const uint8_t right = col + cols;
    uint8_t left = rows; /* rows 0: 256 */
    for (uint8_t r = row + 1; r < PSCR_ROWS; r++) {
        if (right < PSCR_COLS)
            scrn_attr(r, right, right == PSCR_COLS - 1 ? 1 : SHADOW_COLS, SL_SHD);
        if (!--left)
            break;
    }

    const uint8_t below = row + rows, from = col + SHADOW_COLS;

    if (below >= PSCR_ROWS || from >= PSCR_COLS)
        return;

    scrn_attr(below, from, cols < PSCR_COLS - from ? cols : PSCR_COLS - from, SL_SHD);
}

void scrn_cursor(const uint8_t row, const uint8_t col) { pscr_cursor(0, row, col); }

void scrn_shape(const uint16_t lines) { pscr_shape(lines); }

void scrn_flip(void)
{
    pscr_retrace();
    pscr_put(0, 0, 0, cells, PSCR_ROWS * PSCR_COLS);
}
