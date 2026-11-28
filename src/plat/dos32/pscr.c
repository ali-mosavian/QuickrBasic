/* The text screen on DOS32.
 *
 * # How it works
 * Colour text memory at B8000h, INT 10h, the DAC's ports.
 *
 * Sizes past VGA's are the card's VESA text modes, listed once: VBE's mode list (INT 10h AX=4F00h),
 * each mode's attributes and size (4F01h), copied out of a DOS block before the next call reuses
 * it. A size is set by 4F02h. A mode without its size given is VBE 1.2's own (108h .. 10Ch).
 */
#include "dos.h"
#include "plat/pscr.h"
#include "plat/psys.h"
#include <string.h>

enum : uint32_t { VIDEO = 0xB8000, BIOS_COLS = 0x44A, BIOS_PAGESZ = 0x44C, BIOS_ROWS = 0x484 };
enum : uint8_t { FONT_8X8 = 0x12, ALT_LINES = 0x30, LINES_350 = 1, LINES_400 = 2, ROWS_STD = 25 };
enum : uint16_t { DAC_INDEX = 0x3C8, DAC_DATA = 0x3C9, INPUT_STATUS = 0x3DA };
enum { RETRACE = 8, CH_BEL = 7, PAGE_MEMORY = 0x8000 };
enum : uint16_t {
    VBE_SET = 0x4F02,
    VBE_OK = 0x004F,
    VBE_SUPPORTED = 1, /* a mode's attributes */
    VBE_SIZED = 2,     /* its size given */
    VBE_GRAPHICS = 0x10,
    VBE_MODES_MAX = 128,
};

/* A mode's information (4F01h): what is read of it; a text mode's size in characters. */
typedef struct __attribute__((packed)) {
    uint16_t attributes;
    uint8_t windows[14];
    uint16_t pitch, cols, rows;
} VbeTextInfo;
static_assert(offsetof(VbeTextInfo, cols) == 0x12);

/* VBE 1.2's text modes, 108h on: their sizes. */
static const PscrSize vbe12[] = {{80, 60}, {132, 25}, {132, 43}, {132, 50}, {132, 60}};
enum : uint16_t { VBE12_FIRST = 0x108 };

/* The sizes, VGA's first, and each one's VESA mode, 0 VGA's. */
static struct {
    bool listed;
    uint8_t n;
    PscrSize size[PSCR_SIZES_MAX];
    uint16_t mode[PSCR_SIZES_MAX];
} sizes = {.n = 3, .size = {{80, 25}, {80, 43}, {80, 50}}};

/* The attribute controller's palette as the BIOS sets it: colour n's DAC register. */
static const uint8_t acpal[16] = {0, 1, 2, 3, 4, 5, 20, 7, 56, 57, 58, 59, 60, 61, 62, 63};

static volatile uint16_t *cell(const uint8_t page, const uint8_t row, const uint8_t col)
{
    const uint32_t size = *(volatile uint16_t *)pdos_lin(BIOS_PAGESZ); /* the BIOS's page's bytes */
    const uint32_t cols = *(volatile uint16_t *)pdos_lin(BIOS_COLS);
    return (volatile uint16_t *)pdos_lin(VIDEO + page * size + (row * cols + col) * 2u);
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

/* VESA mode's size, if a text one the editor can use: its page and the output's fit text
   memory. */
static bool vbe_size(const uint16_t mode, const PdosBlock *const b, PscrSize *const s)
{
    if (!pdos_vbe_mode(b, mode))
        return false;

    const VbeTextInfo m = *(const VbeTextInfo *)b->at;

    if (!(m.attributes & VBE_SUPPORTED) || m.attributes & VBE_GRAPHICS)
        return false;

    if (m.attributes & VBE_SIZED) {
        if (m.cols > PSCR_COLS_MAX || m.rows > PSCR_ROWS_MAX)
            return false;

        *s = (PscrSize){(uint8_t)m.cols, (uint8_t)m.rows};
    } else if (mode >= VBE12_FIRST && mode < VBE12_FIRST + countof(vbe12)) {
        *s = vbe12[mode - VBE12_FIRST];
    } else {
        return false;
    }

    return s->cols * s->rows * sizeof(PscrCell) * 2 <= PAGE_MEMORY;
}

/* Size s to the list, by columns then rows, unless it is there. */
static void add(const PscrSize s, const uint16_t mode)
{
    uint8_t at = 0;

    for (; at < sizes.n; at++) {
        const PscrSize t = sizes.size[at];

        if (t.cols == s.cols && t.rows == s.rows)
            return;
        if (t.cols > s.cols || (t.cols == s.cols && t.rows > s.rows))
            break;
    }

    if (sizes.n == PSCR_SIZES_MAX)
        return;

    memmove(&sizes.size[at + 1], &sizes.size[at], (sizes.n - at) * sizeof sizes.size[0]);
    memmove(&sizes.mode[at + 1], &sizes.mode[at], (sizes.n - at) * sizeof sizes.mode[0]);
    sizes.size[at] = s;
    sizes.mode[at] = mode;
    sizes.n++;
}

/* The card's VESA text modes added to VGA's sizes. */
static void list(void)
{
    sizes.listed = true;
    PdosBlock b;

    if (!pdos_alloc(PDOS_VBE_BLOCK, &b))
        return;

    uint16_t modes[VBE_MODES_MAX];
    PdosVbe card;
    const uint32_t n = pdos_vbe_modes(&b, &card, modes, VBE_MODES_MAX);

    for (uint32_t i = 0; i < n; i++) {
        PscrSize s;

        if (vbe_size(modes[i], &b, &s))
            add(s, modes[i]);
    }

    pdos_free(&b);
}

uint8_t pscr_sizes(PscrSize out[PSCR_SIZES_MAX])
{
    if (!sizes.listed)
        list();

    memcpy(out, sizes.size, sizes.n * sizeof sizes.size[0]);
    return sizes.n;
}

/* VESA mode's text: whether the card set it. */
static bool vbe_set(const uint16_t mode)
{
    PdosRmRegs rm = {.eax = VBE_SET, .ebx = mode};

    return pdos_rm_int(INT_VIDEO, &rm) && (uint16_t)rm.eax == VBE_OK;
}

PscrSize pscr_text(const PscrSize want)
{
    if (!sizes.listed)
        list();

    uint8_t i = 0;

    while (i < sizes.n && (sizes.size[i].cols != want.cols || sizes.size[i].rows != want.rows))
        i++;

    if (i < sizes.n && sizes.mode[i] && vbe_set(sizes.mode[i]))
        return pscr_size();

    const uint8_t rows = i < sizes.n && !sizes.mode[i] ? want.rows : ROWS_STD;
    int10(VID_ALT, rows == 43 ? LINES_350 : LINES_400, ALT_LINES, 0, 0); /* an EGA: no change */
    int10(VID_MODE, 3, 0, 0, 0);

    if (rows > ROWS_STD)
        int10(VID_FONT, FONT_8X8, 0, 0, 0);

    return pscr_size();
}

PscrSize pscr_size(void)
{
    const uint8_t last = *pdos_lin(BIOS_ROWS);
    return (PscrSize){*pdos_lin(BIOS_COLS), last ? last + 1 : ROWS_STD};
}

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
