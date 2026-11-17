/* The runtime's text screen under a display is pscr's page shown, as on DOS32, where both are the
 * BIOS's: a program's PRINT under the editor lands on its output page. Over the host video's own
 * (dev.h's dev_text_*) by ld's --wrap, or wrap.h where the linker has none; a text mode set clears
 * the screen to 25 rows, as INT 10h's.
 */
#include "plat/host/host.h"
#include "plat/pscr.h"

enum { MODE_TEXT = 3, MODE_KEEP = 0x80, SHAPE = 0x0607 };

static uint8_t page(void) { return host_screen().page; }

int __real_gd_set_mode(unsigned mode);

int __wrap_gd_set_mode(const unsigned mode)
{
    const int ok = __real_gd_set_mode(mode);

    if (ok && mode == MODE_TEXT)
        pscr_text(25);

    return ok;
}

unsigned __wrap_dev_text_size(void) { return (unsigned)pscr_rows() << 8 | PSCR_COLS; }

unsigned __wrap_dev_text_cursor(void)
{
    uint8_t row, col;
    pscr_where(page(), &row, &col);
    return (unsigned)row << 8 | col;
}

void __wrap_dev_text_move(const unsigned row, const unsigned column)
{
    const unsigned rows = pscr_rows();
    pscr_cursor(
        page(),
        (uint8_t)(row < rows ? row : rows - 1),
        (uint8_t)(column < PSCR_COLS ? column : PSCR_COLS - 1)
    );
}

void __wrap_dev_text_cursor_show(const int visible) { pscr_shape(visible ? SHAPE : PSCR_HIDDEN); }

unsigned __wrap_dev_text_get(const unsigned row, const unsigned column)
{
    if (row >= pscr_rows() || column >= PSCR_COLS)
        return 0;

    PscrCell c;
    pscr_get(page(), (uint8_t)row, (uint8_t)column, &c, 1);
    return c;
}

void __wrap_dev_text_put(
    const unsigned row,
    const unsigned column,
    const unsigned character,
    const unsigned attribute
)
{
    if (row < pscr_rows() && column < PSCR_COLS)
        pscr_fill(
            page(),
            (uint8_t)row,
            (uint8_t)column,
            (PscrCell)(attribute << 8 | (character & 0xFF)),
            1
        );
}

void __wrap_dev_text_write(
    unsigned row,
    unsigned column,
    const char *text,
    unsigned count,
    const unsigned attribute
)
{
    while (count--)
        __wrap_dev_text_put(row, column++, (unsigned char)*text++, attribute);
}

void __wrap_dev_text_scroll(
    const unsigned top,
    const unsigned bottom,
    const unsigned lines,
    const unsigned attribute
)
{
    pscr_scroll(
        (uint8_t)lines,
        (uint8_t)top,
        0,
        (uint8_t)bottom,
        PSCR_COLS - 1,
        (uint8_t)attribute
    );
}
