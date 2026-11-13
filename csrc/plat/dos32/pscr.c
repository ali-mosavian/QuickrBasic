/* The text screen on DOS32: colour text memory at B8000h, INT 10h, the DAC's ports. */
#include "dos.h"
#include "plat/pscr.h"
#include "plat/psys.h"

enum : uint32_t { VIDEO = 0xB8000, PAGESZ = 0x1000 };

/* The attribute controller's palette as the BIOS sets it: colour n's DAC register. */
static const uint8_t acpal[16] = {0, 1, 2, 3, 4, 5, 20, 7, 56, 57, 58, 59, 60, 61, 62, 63};

static volatile uint16_t *cell(uint8_t page, uint8_t row, uint8_t col)
{
    return (volatile uint16_t *)pdos_lin(VIDEO + page * PAGESZ + (row * PSCR_COLS + col) * 2);
}

static void int10(uint32_t ax, uint32_t bx, uint32_t cx, uint32_t dx)
{
    PdosRegs r = {.eax = ax, .ebx = bx, .ecx = cx, .edx = dx};
    pdos_int(0x10, &r);
}

void pscr_mode(uint8_t mode) { int10(mode, 0, 0, 0); }

void pscr_page(uint8_t page) { int10(0x0500 | page, 0, 0, 0); }

void pscr_put(uint8_t page, uint8_t row, uint8_t col, const PscrCell *cells, uint32_t n)
{
    volatile uint16_t *p = cell(page, row, col);
    for (uint32_t i = 0; i < n; i++)
        p[i] = cells[i];
}

void pscr_get(uint8_t page, uint8_t row, uint8_t col, PscrCell *cells, uint32_t n)
{
    volatile uint16_t *p = cell(page, row, col);
    for (uint32_t i = 0; i < n; i++)
        cells[i] = p[i];
}

void pscr_fill(uint8_t page, uint8_t row, uint8_t col, PscrCell c, uint32_t n)
{
    pscr_cursor(page, row, col);
    int10(0x0900 | (c & 0xFF), (uint32_t)page << 8 | c >> 8, n, 0);
}

void pscr_cursor(uint8_t page, uint8_t row, uint8_t col)
{
    int10(0x0200, (uint32_t)page << 8, 0, (uint32_t)row << 8 | col);
}

void pscr_where(uint8_t page, uint8_t *row, uint8_t *col)
{
    PdosRegs r = {.eax = 0x0300, .ebx = (uint32_t)page << 8};
    pdos_int(0x10, &r);
    *row = (uint8_t)(r.edx >> 8);
    *col = (uint8_t)r.edx;
}

void pscr_shape(uint16_t lines) { int10(0x0100, 0, lines, 0); }

void pscr_scroll(uint8_t lines, uint8_t top, uint8_t left, uint8_t bottom, uint8_t right,
                 uint8_t attr)
{
    int10(0x0600 | lines, (uint32_t)attr << 8, (uint32_t)top << 8 | left,
          (uint32_t)bottom << 8 | right);
}

void pscr_palette(const uint8_t rgb[48])
{
    for (int i = 0; i < 16; i++) {
        psys_outp(0x3C8, acpal[i]);
        for (int j = 0; j < 3; j++)
            psys_outp(0x3C9, rgb[i * 3 + j]);
    }
}

void pscr_retrace(void)
{
    while (psys_inp(0x3DA) & 8) /* past a retrace already going */
        ;
    while (!(psys_inp(0x3DA) & 8))
        ;
}

void pscr_beep(void) { int10(0x0E07, 0, 0, 0); }
