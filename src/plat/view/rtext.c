/* The runtime's text screen under a display is pscr's pages, as on DOS32, where both are the
 * BIOS's: the program's page 0 is the one shown when first drawn on or reset, so a program's PRINT under the editor
 * lands on its output page. Over the host video's own (dev.h's dev_text_*) by ld's --wrap, or
 * wrap.h where the linker has none; a text mode set clears the screen to 25 rows, as INT 10h's.
 */
#include "plat/host/host.h"
#include "plat/pscr.h"

enum { MODE_TEXT = 3, MODE_KEEP = 0x80, SHAPE = 0x0607, PAGES = 8, PAGES_TALL = 4 };

static struct {
    bool known;                    /* base read */
    uint8_t base, active, visible; /* the pscr page of the program's page 0; the program's pages */
} pages;

void __wrap_dev_text_pages_reset(void);

/* The program's pages: 8 of 25 rows, 4 of 43 or 50 (QB 4.5's). */
static uint8_t count(void) { return pscr_rows() == 25 ? PAGES : PAGES_TALL; }

static uint8_t pscr_of(const unsigned p)
{
    if (!pages.known)
        __wrap_dev_text_pages_reset();

    return (uint8_t)((pages.base + p) % count());
}

static uint8_t page(void) { return pscr_of(pages.active); }

void __wrap_dev_text_pages_reset(void)
{
    pages.known = true;
    pages.base = host_screen().page;
    pages.active = pages.visible = 0;
}

int __wrap_dev_text_pages(const unsigned active, const unsigned visible)
{
    if (active >= count() || visible >= count())
        return 0;

    pages.active = (uint8_t)active;
    pages.visible = (uint8_t)visible;
    pscr_page(pscr_of(visible));
    return 1;
}

int __wrap_dev_text_copy(const unsigned from, const unsigned to)
{
    if (from >= count() || to >= count())
        return 0;

    PscrCell row[PSCR_COLS];

    for (uint8_t r = 0; r < pscr_rows(); r++) {
        pscr_get(pscr_of(from), r, 0, row, PSCR_COLS);
        pscr_put(pscr_of(to), r, 0, row, PSCR_COLS);
    }

    return 1;
}

int __real_gd_set_mode(unsigned mode);

int __wrap_gd_set_mode(const unsigned mode)
{
    const int ok = __real_gd_set_mode(mode);

    if (ok && mode == MODE_TEXT) {
        pscr_text(25);
        __wrap_dev_text_pages_reset();
    }

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
    const unsigned rows = bottom - top + 1;
    const unsigned by = lines && lines < rows ? lines : rows;
    PscrCell row[PSCR_COLS];

    for (unsigned r = top; r + by <= bottom; r++) {
        pscr_get(page(), (uint8_t)(r + by), 0, row, PSCR_COLS);
        pscr_put(page(), (uint8_t)r, 0, row, PSCR_COLS);
    }

    pscr_fill(
        page(),
        (uint8_t)(bottom + 1 - by),
        0,
        (PscrCell)(attribute << 8 | ' '),
        by * PSCR_COLS
    );
}
