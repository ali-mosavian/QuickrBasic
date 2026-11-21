/* The text screen on the host.
 *
 * # How it works
 * Pages in memory, for tests and a display to read (host.h). The card is an SVGA's: VGA's sizes,
 * then VESA's text modes (108h, 109h .. 10Ch).
 */
#include "plat/host/host.h"
#include "plat/pscr.h"

enum { NPAGE = 8, CH_BEL = 7 };
enum : PscrCell { BLANK = 0x0720 };

static const PscrSize sizes[] = {
    {80, 25},
    {80, 43},
    {80, 50},
    {80, 60},
    {132, 25},
    {132, 43},
    {132, 50},
    {132, 60},
};
static_assert(countof(sizes) <= PSCR_SIZES_MAX);

static PscrCell pages[NPAGE][PSCR_ROWS_MAX * PSCR_COLS_MAX];
static PscrSize size = PSCR_80X25;
static struct {
    uint8_t row, col;
} cursors[NPAGE];
static uint8_t shown;
/* The BIOS's 16 colours, 6 bits a channel. */
#define BIOS_PALETTE                                                                               \
    {                                                                                              \
        0,  0,  0,  0,  0,  42, 0,  42, 0,  0,  42, 42, 42, 0,  0,  42,                            \
        0,  42, 42, 21, 0,  42, 42, 42, 21, 21, 21, 21, 21, 63, 21, 63,                            \
        21, 21, 63, 63, 63, 21, 21, 63, 21, 63, 63, 63, 21, 63, 63, 63                             \
    }
static const uint8_t bios_palette[48] = BIOS_PALETTE;
static uint8_t palette[48] = BIOS_PALETTE;
static uint16_t shape = 0x0607;

static PscrCell *cell(const uint8_t page, const uint8_t row, const uint8_t col)
{
    const uint32_t cols = size.cols;
    return &pages[page % NPAGE][(row * cols + col) % (size.rows * cols)];
}

PscrCell host_cell(const uint8_t page, const uint8_t row, const uint8_t col)
{
    return *cell(page, row, col);
}

HostScreen host_screen(void)
{
    HostScreen s = {
        .cells = pages[shown],
        .page = shown,
        .cols = size.cols,
        .rows = size.rows,
        .row = cursors[shown].row,
        .col = cursors[shown].col,
        .shape = shape,
    };

    __builtin_memcpy(s.palette, palette, sizeof s.palette);
    return s;
}

uint8_t pscr_sizes(PscrSize out[PSCR_SIZES_MAX])
{
    __builtin_memcpy(out, sizes, sizeof sizes);
    return countof(sizes);
}

PscrSize pscr_size(void) { return size; }

PscrSize pscr_text(const PscrSize want)
{
    size = PSCR_80X25;

    for (uint32_t i = 0; i < countof(sizes); i++)
        if (sizes[i].cols == want.cols && sizes[i].rows == want.rows)
            size = want;

    for (uint32_t p = 0; p < NPAGE; p++)
        for (uint32_t i = 0; i < PSCR_ROWS_MAX * PSCR_COLS_MAX; i++)
            pages[p][i] = BLANK;

    shown = 0;
    shape = 0x0607;
    __builtin_memcpy(palette, bios_palette, sizeof palette);
    return size;
}

void pscr_page(const uint8_t page) { shown = page % NPAGE; }

void pscr_put(
    const uint8_t page,
    const uint8_t row,
    const uint8_t col,
    const PscrCell *const cells,
    const uint32_t n
)
{
    PscrCell *const p = cell(page, row, col);

    for (uint32_t i = 0; i < n; i++)
        p[i] = cells[i];
}

void pscr_get(
    const uint8_t page,
    const uint8_t row,
    const uint8_t col,
    PscrCell *const cells,
    const uint32_t n
)
{
    const PscrCell *const p = cell(page, row, col);

    for (uint32_t i = 0; i < n; i++)
        cells[i] = p[i];
}

void pscr_fill(
    const uint8_t page,
    const uint8_t row,
    const uint8_t col,
    const PscrCell c,
    const uint32_t n
)
{
    PscrCell *const p = cell(page, row, col);

    for (uint32_t i = 0; i < n; i++)
        p[i] = c;
}

void pscr_cursor(const uint8_t page, const uint8_t row, const uint8_t col)
{
    cursors[page % NPAGE].row = row;
    cursors[page % NPAGE].col = col;
}

void pscr_where(const uint8_t page, uint8_t *const row, uint8_t *const col)
{
    *row = cursors[page % NPAGE].row;
    *col = cursors[page % NPAGE].col;
}

void pscr_shape(const uint16_t lines) { shape = lines; }

void pscr_scroll(
    const uint8_t lines,
    const uint8_t top,
    const uint8_t left,
    const uint8_t bottom,
    const uint8_t right,
    const uint8_t attr
)
{
    const uint32_t height = bottom - top + 1u;
    const uint32_t by = lines && lines < height ? lines : height;
    const PscrCell blank = (PscrCell)(attr << 8 | ' ');

    for (uint32_t r = top; r <= bottom; r++)
        for (uint32_t c = left; c <= right; c++)
            *cell(shown, (uint8_t)r, (uint8_t)c) =
                r + by <= bottom ? *cell(shown, (uint8_t)(r + by), (uint8_t)c) : blank;
}

void pscr_palette(const uint8_t rgb[48]) { __builtin_memcpy(palette, rgb, sizeof palette); }
void pscr_retrace(void) {}
void pscr_beep(void) {}
