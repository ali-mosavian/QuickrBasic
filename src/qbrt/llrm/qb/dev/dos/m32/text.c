/* The text screen: cells in video memory, the BIOS for the cursor; both on the active page. The program's page 0
   is the BIOS's page shown when it is first drawn on or reset (the editor's output page). */
#include "device.h"

enum {
    BIOS_COLUMNS = 0x4A,
    BIOS_MODE = 0x49,
    BIOS_ROWS = 0x84,
    BIOS_CURSOR_SHAPE = 0x60,
    BIOS_PAGE_SIZE = 0x4C,
    BIOS_PAGE = 0x62,
    MONO_MODE = 7,
    DEFAULT_ROWS = 25,
    SET_CURSOR_SHAPE = 0x0100,
    SET_CURSOR_POSITION = 0x0200,
    SET_PAGE = 0x0500,
    PAGE_MEMORY = 0x8000,
    MOST_PAGES = 8,
    GET_CURSOR_POSITION = 0x0300,
    CURSOR_OFF = 0x2000
};

/* The BIOS data area, 0040h:0000h. */
static const volatile u8 QB_FAR *const bios = (const volatile u8 QB_FAR *)0x400UL;

static unsigned columns_of(void)
{
    return bios[BIOS_COLUMNS];
}

static struct {
    int known;               /* base read from the BIOS */
    unsigned base;           /* the BIOS's page of the program's page 0 */
    unsigned active, visible; /* the program's */
} pages;

static unsigned page_size(void)
{
    return *(const volatile u16 QB_FAR *)(bios + BIOS_PAGE_SIZE);
}

/* 8 pages of 25 rows, 4 of 43 or 50 (QB 4.5's). */
static unsigned page_count(void)
{
    unsigned size = page_size(), n = size ? PAGE_MEMORY / size : 1;

    return n > MOST_PAGES ? MOST_PAGES : n ? n : 1;
}

static unsigned bios_page(unsigned page)
{
    if (!pages.known)
        dev_text_pages_reset();
    return (pages.base + page) % page_count();
}

static u16 QB_FAR *page_memory(unsigned page)
{
    u8 *base = QB_VIDEO_MEMORY(bios[BIOS_MODE] == MONO_MODE ? 0xB000 : 0xB800);

    return (u16 *)(base + bios_page(page) * page_size());
}

static u16 QB_FAR *cell(unsigned row, unsigned column)
{
    return page_memory(pages.active) + (row * columns_of() + column);
}

static unsigned page_of(void)
{
    return bios_page(pages.active) << 8;
}

void dev_text_pages_reset(void)
{
    pages.known = 1;
    pages.base = bios[BIOS_PAGE];
    pages.active = pages.visible = 0;
}

int dev_text_pages(unsigned active, unsigned visible)
{
    Regs r;

    if (active >= page_count() || visible >= page_count())
        return 0;
    pages.active = active;
    pages.visible = visible;
    r.rax = SET_PAGE | bios_page(visible);
    dev_int10(&r);
    return 1;
}

int dev_text_copy(unsigned from, unsigned to)
{
    BlockOp op;

    if (from >= page_count() || to >= page_count())
        return 0;
    op.dst = page_memory(to);
    op.src = page_memory(from);
    op.count = page_size();
    op.value = 0;
    dev_move(&op);
    return 1;
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
