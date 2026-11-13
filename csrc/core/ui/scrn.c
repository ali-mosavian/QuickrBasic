/* The editor's screen (src/ui/scr.asm). */
#include "ui/scrn.h"
#include "ui/them.h"
#include "plat/pscr.h"

static PscrCell cells[PSCR_ROWS * PSCR_COLS]; /* the screen as drawn, then shown */

static PscrCell *cell(uint8_t row, uint8_t col) { return &cells[row * PSCR_COLS + col]; }
static PscrCell make(uint8_t ch, uint8_t attr) { return (PscrCell)(attr << 8 | ch); }

uint8_t scrn_fill(uint8_t row, uint8_t col, uint8_t slot, uint8_t ch, uint16_t n)
{
    PscrCell c = make(ch, them_attr(slot)), *p = cell(row, col);
    for (uint16_t i = 0; i < n; i++)
        p[i] = c;
    return col + n;
}

uint8_t scrn_str(uint8_t row, uint8_t col, const char *s, uint8_t slot, uint8_t hot)
{
    uint8_t attr = them_attr(slot), hattr = them_attr(hot);
    PscrCell *p = cell(row, col);
    for (; *s; s++, col++) {
        if (*s == '&')
            *p++ = make(*++s, hattr);
        else
            *p++ = make(*s, attr);
    }
    return col;
}

uint8_t scrn_strn(uint8_t row, uint8_t col, const uint8_t *s, uint16_t n, uint8_t slot)
{
    uint8_t attr = them_attr(slot);
    PscrCell *p = cell(row, col);
    for (uint16_t i = 0; i < n; i++)
        p[i] = make(s[i], attr);
    return col + n;
}

uint16_t scrn_slen(const char *s)
{
    uint16_t n = 0;
    for (; *s; s++)
        n += *s != '&';
    return n;
}

/* A box's row: left, n - 2 of mid, right. */
static void box_row(uint8_t row, uint8_t col, uint8_t n, uint8_t slot, uint8_t left, uint8_t mid,
                    uint8_t right)
{
    col = scrn_fill(row, col, slot, left, 1);
    col = scrn_fill(row, col, slot, mid, (uint16_t)(n - 2));
    scrn_fill(row, col, slot, right, 1);
}

void scrn_box(uint8_t row, uint8_t col, uint8_t rows, uint8_t cols, uint8_t slot,
              const char *title)
{
    uint8_t r = row;
    box_row(r, col, cols, slot, 0xDA, 0xC4, 0xBF);
    for (int i = rows - 2; i > 0; i--)
        box_row(++r, col, cols, slot, 0xB3, ' ', 0xB3);
    box_row(++r, col, cols, slot, 0xC0, 0xC4, 0xD9);
    if (!title)
        return;
    uint8_t c = col + (uint8_t)((uint32_t)(cols - (scrn_slen(title) + 2)) >> 1);
    c = scrn_fill(row, c, slot, ' ', 1);
    c = scrn_str(row, c, title, slot, slot);
    scrn_fill(row, c, slot, ' ', 1);
}

void scrn_sep(uint8_t row, uint8_t col, uint8_t cols, uint8_t slot)
{
    box_row(row, col, cols, slot, 0xC3, 0xC4, 0xB4);
}

void scrn_attr(uint8_t row, uint8_t col, uint16_t n, uint8_t slot)
{
    uint8_t attr = them_attr(slot);
    PscrCell *p = cell(row, col);
    for (uint16_t i = 0; i < n; i++)
        p[i] = (PscrCell)((p[i] & 0xFF) | attr << 8);
}

void scrn_shadow(uint8_t row, uint8_t col, uint8_t rows, uint8_t cols)
{
    uint8_t c = col + cols;
    for (uint8_t r = row + 1, i = rows; r < PSCR_ROWS; r++) {
        if (c < PSCR_COLS)
            scrn_attr(r, c, c == 79 ? 1 : 2, SL_SHD);
        if (!--i)
            break;
    }
    uint8_t r = row + rows;
    c = col + 2;
    if (r >= PSCR_ROWS || c >= PSCR_COLS)
        return;
    scrn_attr(r, c, cols < PSCR_COLS - c ? cols : PSCR_COLS - c, SL_SHD);
}

void scrn_cursor(uint8_t row, uint8_t col) { pscr_cursor(0, row, col); }

void scrn_shape(uint16_t lines) { pscr_shape(lines); }

void scrn_flip(void)
{
    pscr_retrace();
    pscr_put(0, 0, 0, cells, PSCR_ROWS * PSCR_COLS);
}
