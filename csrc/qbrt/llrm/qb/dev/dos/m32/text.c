/* The text screen: cells in video memory, the BIOS for the cursor; both on the active page. */
#include "device.h"

enum {
    BIOS_COLUMNS = 0x4A,
    BIOS_MODE = 0x49,
    BIOS_ROWS = 0x84,
    BIOS_CURSOR_SHAPE = 0x60,
    BIOS_PAGE_START = 0x4E,
    BIOS_PAGE = 0x62,
    MONO_MODE = 7,
    DEFAULT_ROWS = 25,
    SET_CURSOR_SHAPE = 0x0100,
    SET_CURSOR_POSITION = 0x0200,
    GET_CURSOR_POSITION = 0x0300,
    CURSOR_OFF = 0x2000
};

/* The BIOS data area, 0040h:0000h. */
static const volatile u8 QB_FAR *const bios = (const volatile u8 QB_FAR *)0x400UL;

static unsigned columns_of(void)
{
    return bios[BIOS_COLUMNS];
}

static u16 QB_FAR *cell(unsigned row, unsigned column)
{
    u16 *base = QB_VIDEO_MEMORY(bios[BIOS_MODE] == MONO_MODE ? 0xB000 : 0xB800);
    unsigned start = *(const volatile u16 QB_FAR *)(bios + BIOS_PAGE_START);

    return base + start / 2 + (row * columns_of() + column);
}

static unsigned page_of(void)
{
    return (unsigned)bios[BIOS_PAGE] << 8;
}

unsigned dev_text_size(void)
{
    unsigned rows = bios[BIOS_ROWS];

    return (rows ? rows + 1 : DEFAULT_ROWS) << 8 | columns_of();
}

unsigned dev_text_cursor(void)
{
    Regs r;

    r.rax = GET_CURSOR_POSITION;
    r.rbx = page_of();
    dev_int10(&r);
    return r.rdx;
}

void dev_text_move(unsigned row, unsigned column)
{
    Regs r;

    r.rax = SET_CURSOR_POSITION;
    r.rbx = page_of();
    r.rdx = row << 8 | column;
    dev_int10(&r);
}

/* The cursor as the program found it, kept the first time it is hidden. */
void dev_text_cursor_show(int visible)
{
    static unsigned shape;
    Regs r;

    if (!shape) {
        shape = *(const volatile u16 QB_FAR *)(bios + BIOS_CURSOR_SHAPE);
        if (shape & CURSOR_OFF)
            shape = 0;
    }
    r.rax = SET_CURSOR_SHAPE;
    r.rcx = visible ? shape : CURSOR_OFF;
    dev_int10(&r);
}

unsigned dev_text_get(unsigned row, unsigned column)
{
    return *cell(row, column);
}

void dev_text_put(unsigned row, unsigned column, unsigned character, unsigned attribute)
{
    *cell(row, column) = (u16)(attribute << 8 | character);
}

void dev_text_write(unsigned row, unsigned column, const char *text, unsigned count,
                    unsigned attribute)
{
    u16 QB_FAR *at = cell(row, column);

    while (count--)
        *at++ = (u16)(attribute << 8 | (u8)*text++);
}

void dev_text_scroll(unsigned top, unsigned bottom, unsigned lines, unsigned attribute)
{
    unsigned rows = bottom - top + 1, width = columns_of();
    BlockOp op;

    if (lines == 0 || lines >= rows)
        lines = rows;
    op.dst = cell(top, 0);
    op.src = cell(top + lines, 0);
    op.count = (rows - lines) * width * 2;
    op.value = 0;
    dev_move(&op);
    op.dst = cell(bottom + 1 - lines, 0);
    op.count = lines * width;
    op.value = (unsigned)(attribute << 8 | ' ');
    dev_fill(&op);
}
