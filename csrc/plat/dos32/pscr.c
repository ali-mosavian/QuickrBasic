/* The text screen on DOS32.
 *
 * # How it works
 * Colour text memory at B8000h, INT 10h, the DAC's ports.
 */
#include "dos.h"
#include "plat/pscr.h"
#include "plat/psys.h"

enum : uint32_t { VIDEO = 0xB8000, PAGESZ = 0x1000 };
enum : uint16_t { DAC_INDEX = 0x3C8, DAC_DATA = 0x3C9, INPUT_STATUS = 0x3DA };
enum { RETRACE = 8, CH_BEL = 7 };

/* The attribute controller's palette as the BIOS sets it: colour n's DAC register. */
static const uint8_t acpal[16] = {0, 1, 2, 3, 4, 5, 20, 7, 56, 57, 58, 59, 60, 61, 62, 63};

static volatile uint16_t *cell(const uint8_t page, const uint8_t row, const uint8_t col)
{
    return (volatile uint16_t *)pdos_lin(VIDEO + page * PAGESZ + (row * PSCR_COLS + col) * 2);
}

/* INT 10h function fn, al, with BX, CX, DX. */
static void int10(
    const uint8_t fn,
    const uint8_t al,
    const uint32_t bx,
    const uint32_t cx,
    const uint32_t dx
)
{
    PdosRegs r = {
        .eax = pdos_fn(fn, al),
        .ebx = bx,
        .ecx = cx,
        .edx = dx,
    };

    pdos_int(INT_VIDEO, &r);
}

void pscr_mode(const uint8_t mode) { int10(VID_MODE, mode, 0, 0, 0); }

void pscr_page(const uint8_t page) { int10(VID_PAGE, page, 0, 0, 0); }

void pscr_put(
    const uint8_t page,
    const uint8_t row,
    const uint8_t col,
    const PscrCell *const cells,
    const uint32_t n
)
{
    volatile uint16_t *const p = cell(page, row, col);

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
    volatile const uint16_t *const p = cell(page, row, col);

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
    pscr_cursor(page, row, col);
    int10(VID_WRITE, (uint8_t)c, (uint32_t)page << 8 | c >> 8, n, 0);
}

void pscr_cursor(const uint8_t page, const uint8_t row, const uint8_t col)
{
    int10(VID_CURSOR, 0, (uint32_t)page << 8, 0, (uint32_t)row << 8 | col);
}

void pscr_where(const uint8_t page, uint8_t *const row, uint8_t *const col)
{
    PdosRegs r = {.eax = pdos_fn(VID_WHERE, 0), .ebx = (uint32_t)page << 8};

    pdos_int(INT_VIDEO, &r);
    *row = (uint8_t)(r.edx >> 8);
    *col = (uint8_t)r.edx;
}

void pscr_shape(const uint16_t lines) { int10(VID_SHAPE, 0, 0, lines, 0); }

void pscr_scroll(
    const uint8_t lines,
    const uint8_t top,
    const uint8_t left,
    const uint8_t bottom,
    const uint8_t right,
    const uint8_t attr
)
{
    int10(
        VID_SCROLL,
        lines,
        (uint32_t)attr << 8,
        (uint32_t)top << 8 | left,
        (uint32_t)bottom << 8 | right
    );
}

void pscr_palette(const uint8_t rgb[48])
{
    for (uint32_t i = 0; i < countof(acpal); i++) {
        psys_outp(DAC_INDEX, acpal[i]);

        for (uint32_t j = 0; j < 3; j++)
            psys_outp(DAC_DATA, rgb[i * 3 + j]);
    }
}

void pscr_retrace(void)
{
    while (psys_inp(INPUT_STATUS) & RETRACE) /* past a retrace already going */
        ;
    while (!(psys_inp(INPUT_STATUS) & RETRACE))
        ;
}

void pscr_beep(void) { int10(VID_TELETYPE, CH_BEL, 0, 0, 0); }
