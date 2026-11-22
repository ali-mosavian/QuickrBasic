/* The graphics screen: each kind of mode has its table of operations, the
   planar EGA and VGA modes (QB rt/llega.asm, rt/llegasup.asm) and the linear
   256-colour one.

   A write goes through the graphics controller's write mode 2, which spreads
   the low four bits of the byte written over the planes, under the bit mask and
   the function applied to what the latches hold of the byte read first. The
   controller rests in write mode 2, function 0 and bit mask FFh while a
   graphics mode is on; the operations that change the function or the mask put
   them back.

   Modes 04h, 06h, 12h and 13h have one page on the adapter, the visible one;
   their other is `hidden`, in memory. It has the screen's layout in 04h, 06h
   and 13h, drawn by the same routines with `screen` pointing at it; in 12h a
   byte a pixel, drawn by pixmem.c. A change of the visible page swaps the two;
   PCOPY copies one over the other. In 04h, 06h and 13h a copy of the visible
   page is kept while nothing draws on it: a swap copies the hidden page out
   and takes the copy as the hidden one. The two are taken when a program first
   uses its second page, and given back at the next mode set.

   With VBE 2.0, 13h and 12h move to VBE's mode of their size instead, a byte a
   pixel in the linear frame buffer, when a program first uses its second page:
   both pages are in video memory past the first 256 KB (a text mode's planes,
   so the IDE's text screen leaves them), the display start shows one, and the
   A000h window is the shown one's, as the adapter's page is. 12h's colours
   keep to 4 bits there, and GET and PUT to its planes' layout (pixmem.c). A
   program on one page keeps the BIOS's mode. Without a frame buffer (VBE
   1.2), 13h's pages are there too, each in one step of the A000h window: the
   window is the page drawn on. */
#include <string.h>

#include "device.h"
#include "gfxdev.h"
#include "pixmem.h"

enum {
    GC_PORT = 0x3CE,
    GC_DATA_ROTATE = 3,
    GC_SET_RESET_ENABLE = 1,
    GC_READ_MAP = 4,
    GC_COLOR_COMPARE = 2,
    GC_MODE = 5,
    GC_DONT_CARE = 7,
    GC_BIT_MASK = 8,
    WRITE_MODE_1 = 1,
    READ_MODE_1 = 8,
    WRITE_MODE_2 = 2,
    FUNCTION_SHIFT = 3,
    PLANES = 4,
    MODE_320 = 0x0D,
    MODE_640_200 = 0x0E,
    MODE_640_350 = 0x10,
    MODE_640_480 = 0x12,
    SEQ_PORT = 0x3C4,
    SEQ_MAP_MASK = 2,
    ALL_PLANES = 0x0F,
    WRITE_MODE_0 = 0,
    PLANAR_WIDTH = 640,
    PLANAR_HEIGHT = 480,
    PLANAR_ROW = PLANAR_WIDTH / 8,
    LINEAR_BYTES = 320 * 200,
    CGA_BYTES = 0x4000,
    SOFT_PAGES = 2,
    SET_PAGE = 0x0500,
    MODE_LINEAR = 0x13,
    SET_COLOR_SELECT = 0x0B00,
    MODE_CGA4 = 4,
    MODE_CGA2 = 6,
    LAST_TEXT_MODE = 7,
    GET_MODE = 0x0F00,
    SET_PALETTE = 0x1000,
    DAC_WRITE_INDEX = 0x3C8,
    DAC_DATA = 0x3C9,
    GET_FONT = 0x1130,
    FONT_14 = 0x0200,
    FONT_8 = 0x0300,
    FONT_16 = 0x0600,
    GET_ATTRIBUTE = 0x1007,
    ATTRIBUTES = 16,
    DAC_READ_INDEX = 0x3C7,
    DAC_BYTES = 256 * 3
};

/* Volatile: a read loads the latches, which the bits a mask leaves alone come
   from, so it must not be dropped. */
typedef volatile u8 QB_FAR Video;
static Video *screen = (Video *)QB_VIDEO_MEMORY(0xA000);
static unsigned pitch = 80; /* bytes of a row of one plane */
static unsigned page_count = 1, page_paragraphs; /* the pages the mode holds, and the paragraphs of one */

static Video *adapter;                         /* the adapter's screen */
static u8 *soft_block;                         /* hidden's and shown_copy's, taken */
static u8 *hidden;                             /* the page not on the adapter */
static u8 *shown_copy;                         /* the adapter's page, while shown_ok (not 12h) */
static int shown_ok;
static unsigned soft_mode;                     /* the mode's: 0, or 04h, 06h, 12h or 13h */
static unsigned shown;                         /* the page on the adapter, where soft_mode */
static int in_memory;                          /* drawing goes to `hidden` through pixmem.c (12h) */

static struct {
    u8 *lfb;              /* the frame buffer, mapped once */
    unsigned long base;   /* its physical address */
    unsigned mode;        /* VBE's, while the pages are in it; else 0 */
    unsigned long stride; /* from a page to the next */
    unsigned granule;     /* the A000h window's step, bytes; 0: no window */
    int nibbles;          /* 12h's: a pixel's colour 4 bits */
    int banked;           /* no frame buffer: drawn through the window */
    unsigned drawn;       /* the page drawn on */
} vbe;

static void controller(unsigned index, unsigned value)
{
    dev_outw(GC_PORT, index | value << 8);
}

static Video *byte_of(unsigned x, unsigned y)
{
    return screen + (unsigned long)y * pitch + (x >> 3);
}

typedef struct GdOps {
    void (*plot)(unsigned x, unsigned y, unsigned color, unsigned operation);
    unsigned (*read)(unsigned x, unsigned y);
    int (*search)(int x, int last, unsigned y, unsigned c1, unsigned c2, int match);
    void (*move_rows)(unsigned to, unsigned from, unsigned count);
    void (*glyph)(unsigned x, unsigned y, const u8 QB_FAR *bits, unsigned height,
                  unsigned foreground);
} GdOps;

enum { KIND_PLANAR, KIND_LINEAR, KIND_PACKED4, KIND_PACKED2, KINDS };

static const GdOps planar_ops, linear_ops, packed4_ops, packed2_ops;
static const GdOps *ops = &planar_ops;
static unsigned kind = KIND_PLANAR;

int gd_set_mode(unsigned mode)
{
    Regs r;

    r.rax = mode;
    dev_int10(&r);
    r.rax = GET_MODE;
    dev_int10(&r);
    kind = mode == MODE_LINEAR ? KIND_LINEAR
           : mode == MODE_CGA4 ? KIND_PACKED4
           : mode == MODE_CGA2 ? KIND_PACKED2
                               : KIND_PLANAR;
    ops = kind == KIND_LINEAR    ? &linear_ops
          : kind == KIND_PACKED4 ? &packed4_ops
          : kind == KIND_PACKED2 ? &packed2_ops
                                 : &planar_ops;
    pitch = mode == MODE_320 ? 40 : mode == MODE_LINEAR ? 320 : 80;
    /* the BIOS's pages of the EGA modes: 0Dh holds 8 of 2000h bytes, 0Eh 4 of 4000h, 10h 2 of 8000h */
    page_count = mode == MODE_320 ? 8 : mode == MODE_640_200 ? 4 : mode == MODE_640_350 ? 2 : 1;
    page_paragraphs = mode == MODE_320 ? 0x200 : mode == MODE_640_200 ? 0x400 : 0x800;
    if (mode == MODE_CGA4 || mode == MODE_CGA2)
        screen = (Video *)QB_VIDEO_MEMORY(0xB800);
    else
        screen = (Video *)QB_VIDEO_MEMORY(0xA000);
    adapter = screen;
    soft_mode = mode == MODE_CGA4 || mode == MODE_CGA2 || mode == MODE_640_480 || mode == MODE_LINEAR ? mode : 0;
    if (soft_mode)
        page_count = SOFT_PAGES;
    shown = 0;
    in_memory = 0;
    vbe.mode = 0;
    vbe.nibbles = 0;
    vbe.banked = 0;
    dev_give(soft_block);
    soft_block = hidden = shown_copy = 0;
    shown_ok = 0;
    if ((r.rax & 0x7F) != (mode & 0x7F))
        return 0;

    if (mode > LAST_TEXT_MODE && mode != MODE_LINEAR && mode != MODE_CGA4 && mode != MODE_CGA2)
        controller(GC_MODE, WRITE_MODE_2);
    return 1;
}

static void no_touch(unsigned top, unsigned count)
{
}

/* The bytes of a page in the adapter's layout (04h, 06h, 13h). */
static unsigned soft_bytes(void)
{
    return soft_mode == MODE_LINEAR ? LINEAR_BYTES : CGA_BYTES;
}

static void seq_map(unsigned planes)
{
    dev_outw(SEQ_PORT, SEQ_MAP_MASK | planes << 8);
}

/* 12h's row y between the adapter and pixels, a byte a pixel. */
static void planar_row_in(unsigned y, u8 *pixels)
{
    unsigned plane, at, bit;

    memset(pixels, 0, PLANAR_WIDTH);
    for (plane = 0; plane < PLANES; plane++) {
        Video *from = adapter + y * PLANAR_ROW;

        controller(GC_READ_MAP, plane);
        for (at = 0; at < PLANAR_ROW; at++) {
            unsigned bits = from[at];

            for (bit = 0; bit < 8; bit++)
                pixels[at * 8 + bit] |= (u8)((bits >> (7 - bit) & 1) << plane);
        }
    }
    controller(GC_READ_MAP, 0);
}

/* Write mode 0, a plane at a time; the controller back as it rests after. */
static void planar_row_out(unsigned y, const u8 *pixels)
{
    unsigned plane, at, bit;

    controller(GC_MODE, WRITE_MODE_0);
    controller(GC_SET_RESET_ENABLE, 0);
    for (plane = 0; plane < PLANES; plane++) {
        Video *to = adapter + y * PLANAR_ROW;

        seq_map(1u << plane);
        for (at = 0; at < PLANAR_ROW; at++) {
            unsigned bits = 0;

            for (bit = 0; bit < 8; bit++)
                bits |= (pixels[at * 8 + bit] >> plane & 1u) << (7 - bit);
            to[at] = (u8)bits;
        }
    }
    seq_map(ALL_PLANES);
    controller(GC_MODE, WRITE_MODE_2);
}

static void move(void *to, const void *from, unsigned count)
{
    BlockOp op;

    op.dst = to;
    op.src = from;
    op.count = count;
    op.value = 0;
    dev_move(&op);
}

enum {
    VBE_INFO = 0x4F00,
    VBE_MODE_INFO = 0x4F01,
    VBE_SET = 0x4F02,
    VBE_WINDOW = 0x4F05,
    VBE_START = 0x4F07,
    VBE_OK = 0x004F,
    VBE_LINEAR = 0x4000,
    VBE_KEEP = 0x8000,
    VBE_VERSION_2 = 0x200,
    VBE_VERSION_12 = 0x102,
    VBE_MODES = 256,      /* of the list, at most */
    VBE_NEEDS = 0x1B,     /* a mode's attributes: supported, its size given, colour, graphics */
    VBE_FRAME = 0x80,     /* the attribute: a linear frame buffer */
    VBE_NO_WINDOW = 0x40, /* the attribute: no A000h window */
    VBE_WINDOW_RW = 7,    /* the window's attributes: there, readable, writable */
    VBE_PACKED = 4,       /* the memory model: a byte a pixel */
    VBE_MODE_AT = 0x200,  /* the buffer: the info block (512 bytes), then a mode's */
    VBE_BUFFER = 0x300,
    VBE_PAGE0 = 0x40000, /* past a text mode's planes */
    VBE_ALIGN = 0x10000, /* a page's start: a step of any window */
    DPMI_SEGMENT_BASE = 0x0006,
    DPMI_DOS_ALLOC = 0x0100,
    DPMI_DOS_FREE = 0x0101,
    DPMI_MAP = 0x0800,
    PARAGRAPH = 16,
    CARRY = 1
};

static unsigned word_at(const u8 *p) { return p[0] | p[1] << 8; }

static unsigned long ds_base(void)
{
    Regs r = {0};

    r.rax = DPMI_SEGMENT_BASE;
    r.rbx = dev_ds_selector();
    dev_int31(&r);
    return (unsigned long)r.rcx << 16 | (r.rdx & 0xFFFF);
}

/* VBE's mode of `width` by `height`, a byte a pixel and a row `width` bytes, with room for 2 pages
   past VBE_PAGE0, in a linear frame buffer (2.0) or else each page in a step of the A000h window: its
   number, vbe's base, stride, granule and banked set; 0 none. */
static unsigned vbe_find(unsigned width, unsigned height)
{
    Regs r = {0};

    unsigned short modes[VBE_MODES];
    unsigned n = 0, at, found = 0, segment, selector;
    unsigned long base = ds_base(),
                  stride = ((unsigned long)width * height + VBE_ALIGN - 1) & ~(VBE_ALIGN - 1UL);
    u8 *block, *info;

    r.rax = DPMI_DOS_ALLOC;
    r.rbx = VBE_BUFFER / PARAGRAPH;
    dev_int31(&r);

    if (r.rflags & CARRY)
        return 0;

    segment = r.rax & 0xFFFF;
    selector = r.rdx & 0xFFFF;
    block = (u8 *)((unsigned long)segment * PARAGRAPH - base);
    info = block + VBE_MODE_AT;
    memcpy(block, "VBE2", 4);
    r = (Regs){0};

    r.rax = VBE_INFO;
    r.res = segment;
    dev_int10r(&r);

    if ((r.rax & 0xFFFF) == VBE_OK && !memcmp(block, "VESA", 4) &&
        word_at(block + 4) >= VBE_VERSION_12 &&
        VBE_PAGE0 + 2 * stride <= word_at(block + 0x12) * 0x10000UL) {
        const u8 *list = (const u8 *)(word_at(block + 0x10) * 16UL + word_at(block + 0x0E) - base);

        while (n < VBE_MODES && word_at(list + 2 * n) != 0xFFFF) {
            modes[n] = (unsigned short)word_at(list + 2 * n);
            n++;
        }

        for (at = 0; at < n && !found; at++) {
            unsigned attributes, granule;
            int frame;

            r = (Regs){0};

            r.rax = VBE_MODE_INFO;
            r.rcx = modes[at];
            r.res = segment;
            r.rdi = VBE_MODE_AT;
            dev_int10r(&r);
            attributes = word_at(info);

            if ((r.rax & 0xFFFF) != VBE_OK || (attributes & VBE_NEEDS) != VBE_NEEDS ||
                word_at(info + 0x12) != width || word_at(info + 0x14) != height ||
                word_at(info + 0x10) != width || info[0x19] != 8 || info[0x1B] != VBE_PACKED)
                continue;

            frame = word_at(block + 4) >= VBE_VERSION_2 && attributes & VBE_FRAME;
            granule = !(attributes & VBE_NO_WINDOW) && (info[2] & VBE_WINDOW_RW) == VBE_WINDOW_RW &&
                              word_at(info + 8) == 0xA000
                          ? word_at(info + 4) * 1024u
                          : 0;

            if (!frame && (!granule || VBE_ALIGN % granule ||
                           word_at(info + 6) * 1024UL < (unsigned long)width * height))
                continue;

            found = modes[at];
            vbe.base = word_at(info + 0x28) | (unsigned long)word_at(info + 0x2A) << 16;
            vbe.stride = stride;
            vbe.granule = granule;
            vbe.banked = !frame;
        }
    }

    r = (Regs){0};

    r.rax = DPMI_DOS_FREE;
    r.rdx = selector;
    dev_int31(&r);
    return found;
}

/* The frame buffer mapped (DPMI's, kept: a host's mappings are few); 0 when the host will not. */
static int vbe_map(void)
{
    static unsigned long mapped;
    Regs r = {0};

    const unsigned long size = VBE_PAGE0 + 2 * vbe.stride;

    if (vbe.banked || (vbe.lfb && mapped == vbe.base))
        return 1;

    r.rax = DPMI_MAP;
    r.rbx = vbe.base >> 16;
    r.rcx = vbe.base & 0xFFFF;
    r.rsi = size >> 16;
    r.rdi = size & 0xFFFF;
    dev_int31(&r);

    if (r.rflags & CARRY)
        return 0;

    vbe.lfb = (u8 *)(((unsigned long)(r.rbx & 0xFFFF) << 16 | (r.rcx & 0xFFFF)) - ds_base());
    mapped = vbe.base;
    return 1;
}

static u8 *vbe_page(unsigned page) { return vbe.lfb + VBE_PAGE0 + page * vbe.stride; }

/* The A000h window on page `page`. */
static void vbe_window(unsigned page)
{
    Regs r = {0};

    r.rax = VBE_WINDOW;
    r.rdx = (VBE_PAGE0 + page * vbe.stride) / vbe.granule;
    dev_int10(&r);
}

/* Page `page` shown, and the A000h window its unless drawing needs it. */
static void vbe_show(unsigned page)
{
    const unsigned long at = VBE_PAGE0 + page * vbe.stride;
    Regs r = {0};

    r.rax = VBE_START;
    r.rcx = at % pitch;
    r.rdx = at / pitch;
    dev_int10(&r);

    if (vbe.granule && !vbe.banked)
        vbe_window(page);
}

static void vbe_draw_on(unsigned page)
{
    vbe.drawn = page;

    if (!vbe.banked) {
        screen = vbe_page(page);
        return;
    }

    vbe_window(page);
    screen = adapter;
}

/* `bytes` of page `from` copied over page `to`'s, through the window when banked. */
static void vbe_copy(unsigned from, unsigned to, unsigned bytes)
{
    u8 chunk[0x1000];
    unsigned at, n;

    if (!vbe.banked) {
        move(vbe_page(to), vbe_page(from), bytes);
        return;
    }

    for (at = 0; at < bytes; at += n) {
        n = bytes - at < sizeof chunk ? bytes - at : sizeof chunk;
        vbe_window(from);
        move(chunk, (const u8 *)adapter + at, n);
        vbe_window(to);
        move((u8 *)adapter + at, chunk, n);
    }

    vbe_window(vbe.drawn);
}

static void dac_read(u8 *dac)
{
    unsigned at;

    dev_outb(DAC_READ_INDEX, 0);

    for (at = 0; at < DAC_BYTES; at++)
        dac[at] = (u8)dev_inb(DAC_DATA);
}

/* 13h or 12h (soft_mode) into VBE's mode of its size, the screen and its colours kept: 0 when there
   is no such mode, the frame buffer cannot be mapped or the screen kept meanwhile. */
static int vbe_on(void)
{
    const int planar = soft_mode == MODE_640_480;
    const unsigned width = planar ? PLANAR_WIDTH : 320, height = planar ? PLANAR_HEIGHT : 200;
    const unsigned long bytes =
        planar ? (unsigned long)PLANES * PLANAR_ROW * PLANAR_HEIGHT : LINEAR_BYTES;
    const unsigned mode = vbe_find(width, height);
    u8 dac[DAC_BYTES], attributes[ATTRIBUTES], *kept;
    unsigned at, plane, x, y;
    Regs r;

    if (!mode || !vbe_map() || !(kept = dev_try(bytes)))
        return 0;

    dac_read(dac);

    for (at = 0; at < ATTRIBUTES; at++) {
        r = (Regs){0};

        r.rax = GET_ATTRIBUTE;
        r.rbx = at;
        dev_int10(&r);
        attributes[at] = (u8)(r.rbx >> 8);
    }

    if (planar) {
        for (plane = 0; plane < PLANES; plane++) {
            controller(GC_READ_MAP, plane);
            move(
                kept + plane * PLANAR_ROW * PLANAR_HEIGHT,
                (const void *)adapter,
                PLANAR_ROW * PLANAR_HEIGHT
            );
        }

        controller(GC_READ_MAP, 0);
    } else {
        move(kept, (const void *)adapter, LINEAR_BYTES);
    }

    r = (Regs){0};

    r.rax = VBE_SET;
    r.rbx = mode | (vbe.banked ? 0 : VBE_LINEAR);
    dev_int10(&r);

    if ((r.rax & 0xFFFF) != VBE_OK) {
        dev_give(kept);
        return 0;
    }

    vbe.mode = mode;
    vbe.nibbles = planar;
    kind = KIND_LINEAR;
    ops = &linear_ops;
    pitch = width;

    if (vbe.banked) { /* 13h's alone */
        vbe_window(1);
        memset((void *)adapter, 0, LINEAR_BYTES);
        vbe_window(0);
        move((void *)adapter, kept, LINEAR_BYTES);
    } else if (planar) {
        memset(vbe_page(1), 0, (size_t)width * height);

        for (y = 0; y < height; y++) {
            u8 *const row = vbe_page(0) + y * width;

            for (x = 0; x < width; x++) {
                unsigned pixel = 0;

                for (plane = 0; plane < PLANES; plane++)
                    pixel |= (kept[plane * PLANAR_ROW * PLANAR_HEIGHT + y * PLANAR_ROW + x / 8] >>
                                  (7 - x % 8) &
                              1u)
                             << plane;

                row[x] = (u8)pixel;
            }
        }
    } else {
        memset(vbe_page(1), 0, LINEAR_BYTES);
        move(vbe_page(0), kept, LINEAR_BYTES);
    }

    dev_give(kept);
    dev_outb(DAC_WRITE_INDEX, 0);

    for (at = 0; at < (planar ? ATTRIBUTES : 256u); at++) {
        const u8 *const color = dac + 3 * (planar ? attributes[at] : at);

        dev_outb(DAC_DATA, color[0]);
        dev_outb(DAC_DATA, color[1]);
        dev_outb(DAC_DATA, color[2]);
    }

    vbe_draw_on(0);
    vbe_show(0);
    return 1;
}

unsigned gd_vbe(void) { return vbe.mode; }

void gd_vbe_show(void)
{
    Regs r = {0};

    r.rax = VBE_SET;
    r.rbx = vbe.mode | (vbe.banked ? 0 : VBE_LINEAR) | VBE_KEEP;
    dev_int10(&r);
    vbe_show(shown);

    if (vbe.banked)
        vbe_window(vbe.drawn);
}

/* A colour as 12h keeps it in VBE's mode: 4 bits. */
static unsigned ink(unsigned color) { return vbe.nibbles ? color & 0x0F : color; }

/* GET and PUT of 12h in VBE's mode: pixmem.c's, over the page drawn on. */
static int vbe_planes(void)
{
    if (vbe.nibbles)
        pm_use((u8 *)screen, PLANAR_WIDTH, PLANAR_HEIGHT, PM_PLANAR, no_touch);

    return vbe.nibbles;
}

/* The hidden page and the shown one's copy, taken once a mode's second page is used: VBE's pages
   where there are. */
static void soft_take(void)
{
    if (soft_block || vbe.mode)
        return;
    if ((soft_mode == MODE_LINEAR || soft_mode == MODE_640_480) && vbe_on())
        return;
    if (soft_mode == MODE_640_480) {
        soft_block = hidden = dev_take(PLANAR_WIDTH * PLANAR_HEIGHT);
        return;
    }

    soft_block = hidden = dev_take(2UL * soft_bytes());
    shown_copy = hidden + soft_bytes();
}

/* The adapter's page and the hidden one change places. */
static void soft_swap(void)
{
    if (soft_mode == MODE_640_480) {
        u8 row[PLANAR_WIDTH];
        unsigned y;

        for (y = 0; y < PLANAR_HEIGHT; y++) {
            planar_row_in(y, row);
            planar_row_out(y, hidden + y * PLANAR_WIDTH);
            memcpy(hidden + y * PLANAR_WIDTH, row, PLANAR_WIDTH);
        }
    } else {
        u8 *const was = hidden;

        if (!shown_ok)
            move(shown_copy, (const void *)adapter, soft_bytes());
        move((void *)adapter, hidden, soft_bytes());
        hidden = shown_copy;
        shown_copy = was;
        shown_ok = 1;
    }
}

/* One of the two pages copied over the other: the adapter's to `hidden`, or back. */
static void soft_copy(int to_hidden)
{
    unsigned y;

    if (soft_mode != MODE_640_480) {
        BlockOp op;

        shown_ok = shown_ok && to_hidden;
        op.dst = to_hidden ? (void *)hidden : (void *)adapter;
        op.src = to_hidden ? (const void *)adapter : (const void *)hidden;
        op.count = soft_bytes();
        op.value = 0;
        dev_move(&op);
        return;
    }
    for (y = 0; y < PLANAR_HEIGHT; y++)
        if (to_hidden)
            planar_row_in(y, hidden + y * PLANAR_WIDTH);
        else
            planar_row_out(y, hidden + y * PLANAR_WIDTH);
}

static void soft_draw_on(unsigned active)
{
    int on_hidden = active != shown;

    if (!on_hidden)
        shown_ok = 0; /* the adapter's page drawn on */

    screen = on_hidden && soft_mode != MODE_640_480 ? (Video *)hidden : adapter;
    in_memory = on_hidden && soft_mode == MODE_640_480;
    if (in_memory)
        pm_use(hidden, PLANAR_WIDTH, PLANAR_HEIGHT, PM_PLANAR, no_touch);
}

int gd_set_pages(unsigned active, unsigned visible)
{
    Regs r;

    if (active >= page_count || visible >= page_count)
        return 0;
    if (soft_mode) {
        if (active | visible | shown)
            soft_take();
        if (vbe.mode) {
            vbe_draw_on(active);

            if (visible != shown)
                vbe_show(visible);

            shown = visible;
            return 1;
        }
        if (visible != shown) {
            soft_swap();
            shown = visible;
        }
        soft_draw_on(active);
        return 1;
    }
    screen = (Video *)QB_VIDEO_MEMORY(0xA000 + active * page_paragraphs);
    r.rax = SET_PAGE | visible;
    dev_int10(&r);
    return 1;
}

/* The EGA modes' pages are all on the adapter: write mode 1 writes the latches a read loaded, every plane at once. */
int gd_copy_page(unsigned from, unsigned to)
{
    unsigned at, n = page_paragraphs << 4;
    Video *src, *dst;

    if (from >= page_count || to >= page_count)
        return 0;
    if (from == to)
        return 1;
    if (soft_mode) {
        soft_take();

        if (vbe.mode)
            vbe_copy(from, to, pitch * (vbe.nibbles ? PLANAR_HEIGHT : 200));
        else
            soft_copy(to != shown);
        return 1;
    }
    src = (Video *)QB_VIDEO_MEMORY(0xA000 + from * page_paragraphs);
    dst = (Video *)QB_VIDEO_MEMORY(0xA000 + to * page_paragraphs);
    controller(GC_MODE, WRITE_MODE_1);
    for (at = 0; at < n; at++)
        dst[at] = src[at];
    controller(GC_MODE, WRITE_MODE_2);
    return 1;
}

/* One of red, green and blue of an EGA colour (rgbRGB): 2/3 for its bit, 1/3 for the other's. */
static unsigned ega_level(unsigned color, unsigned bit)
{
    return (color >> bit & 1) * 0x2A + (color >> (bit + 3) & 1) * 0x15;
}

void gd_palette(unsigned index, unsigned color)
{
    Regs r;

    if (vbe.nibbles) { /* no attribute palette: the colour register's */
        gd_palette_mix(index, ega_level(color, 2), ega_level(color, 1), ega_level(color, 0));
        return;
    }

    r.rax = SET_PALETTE;
    r.rbx = color << 8 | index;
    dev_int10(&r);
}

void gd_cga_color(unsigned background, unsigned palette)
{
    Regs r;

    r.rax = SET_COLOR_SELECT;
    r.rbx = background;
    dev_int10(&r);
    r.rax = SET_COLOR_SELECT;
    r.rbx = 0x0100 | palette;
    dev_int10(&r);
}

/* The DAC's ports, as the BIOS's 1010h writes them: no trip to real mode. */
void gd_palette_mix(unsigned index, unsigned red, unsigned green, unsigned blue)
{
    dev_outb(DAC_WRITE_INDEX, index & 0xFF);
    dev_outb(DAC_DATA, red & 0x3F);
    dev_outb(DAC_DATA, green & 0x3F);
    dev_outb(DAC_DATA, blue & 0x3F);
}

/* One byte of pixels under `mask`: `color` in the planes. */
static void put(Video *at, unsigned mask, unsigned color)
{
    controller(GC_BIT_MASK, mask);
    (void)*at; /* loads the latches */
    *at = (u8)color;
}

static void planar_plot(unsigned x, unsigned y, unsigned color, unsigned operation)
{
    if (operation)
        controller(GC_DATA_ROTATE, operation << FUNCTION_SHIFT);
    put(byte_of(x, y), 0x80 >> (x & 7), color);
    controller(GC_BIT_MASK, 0xFF);
    if (operation)
        controller(GC_DATA_ROTATE, 0);
}

static unsigned planar_read(unsigned x, unsigned y)
{
    Video *at = byte_of(x, y);
    unsigned bit = 0x80 >> (x & 7), plane, color = 0;

    for (plane = PLANES; plane--;) {
        controller(GC_READ_MAP, plane);
        color = color << 1 | ((*at & bit) != 0);
    }
    return color;
}

/* The pixels of the byte at `at` that are of colour `color`, a bit each, by the
   graphics controller's colour compare: one read for eight pixels. */
static unsigned equal_to(Video *at, unsigned color)
{
    controller(GC_COLOR_COMPARE, color);
    return *at;
}

static int planar_search(int x, int last, unsigned y, unsigned c1, unsigned c2, int match)
{
    int back = last < x;
    unsigned first_byte = (unsigned)x >> 3, last_byte = (unsigned)last >> 3;
    FillScan scan;

    scan.at = byte_of(x, y);
    scan.count = (back ? first_byte - last_byte : last_byte - first_byte) + 1;
    scan.c1 = c1;
    scan.c2 = c2;
    scan.flags = (match ? 1 : 0) | (back ? 2 : 0);
    scan.first = back ? 0xFF << (7 - (x & 7)) & 0xFF : 0xFF >> (x & 7);
    scan.last = back ? 0xFF >> (last & 7) : 0xFF << (7 - (last & 7)) & 0xFF;
    controller(GC_DONT_CARE, 0x0F);
    controller(GC_MODE, READ_MODE_1 | WRITE_MODE_2);
    dev_scan_planar(&scan);
    controller(GC_MODE, WRITE_MODE_2);
    if (scan.found < 0)
        return -1;
    {
        unsigned bit = 0;

        if (back) {
            while (!(scan.hits >> bit & 1))
                bit++;
            return (int)((first_byte - (unsigned)scan.found) * 8 + 7 - bit);
        }
        while (!(scan.hits << bit & 0x80))
            bit++;
        return (int)((first_byte + (unsigned)scan.found) * 8 + bit);
    }
}

static void planar_move_rows(unsigned to, unsigned from, unsigned count)
{
    BlockOp op;

    op.dst = byte_of(0, to);
    op.src = byte_of(0, from);
    op.count = count * pitch;
    op.value = 1; /* by bytes: a latch holds one */
    controller(GC_MODE, WRITE_MODE_1);
    dev_move(&op);
    controller(GC_MODE, WRITE_MODE_2);
}

static void planar_glyph(unsigned x, unsigned y, const u8 QB_FAR *bits, unsigned height,
                         unsigned foreground)
{
    Video *at = byte_of(x, y);

    while (height--) {
        put(at, 0xFF, 0);
        put(at, *bits++, foreground);
        at += pitch;
    }
    controller(GC_BIT_MASK, 0xFF);
}

const u8 QB_FAR *gd_font(unsigned height)
{
    Regs r = {0};

    r.rax = GET_FONT;
    r.rbx = height == 14 ? FONT_14 : height == 16 ? FONT_16 : FONT_8;
    dev_int10r(&r);
    return (const u8 QB_FAR *)QB_REAL_POINTER(r.res, r.rbp);
}

/* The 256-colour modes: a byte a pixel, rows of `pitch`. */
static Video *pixel_at(unsigned x, unsigned y)
{
    return screen + (unsigned long)y * pitch + x;
}

static unsigned combined(unsigned old, unsigned color, unsigned operation)
{
    switch (operation) {
    case 1:
        return old & color;
    case 2:
        return old | color;
    case 3:
        return old ^ color;
    default:
        return color;
    }
}

static void linear_plot(unsigned x, unsigned y, unsigned color, unsigned operation)
{
    Video *at = pixel_at(x, y);

    *at = (u8)combined(*at, color, operation);
}

static unsigned linear_read(unsigned x, unsigned y)
{
    return *pixel_at(x, y);
}

static int linear_search(int x, int last, unsigned y, unsigned c1, unsigned c2, int match)
{
    int step = last >= x ? 1 : -1;
    FillScan scan;

    scan.at = pixel_at(x, y);
    scan.count = (unsigned)(step > 0 ? last - x : x - last) + 1;
    scan.c1 = c1;
    scan.c2 = c2;
    scan.flags = (match ? 1 : 0) | (step < 0 ? 2 : 0);
    dev_scan_linear(&scan);
    return scan.found < 0 ? -1 : x + step * scan.found;
}

static void linear_move_rows(unsigned to, unsigned from, unsigned count)
{
    BlockOp op;

    op.dst = pixel_at(0, to);
    op.src = pixel_at(0, from);
    op.count = count * pitch;
    op.value = 0;
    dev_move(&op);
}

/* A glyph row of 8 pixels, a nibble at a time: the 4 pixels of a nibble as 4
 * bytes, each 0xFF where the pixel is set. */
static const u32 nibble_bytes[16] = {0x00000000UL, 0xFF000000UL, 0x00FF0000UL, 0xFFFF0000UL,
                                     0x0000FF00UL, 0xFF00FF00UL, 0x00FFFF00UL, 0xFFFFFF00UL,
                                     0x000000FFUL, 0xFF0000FFUL, 0x00FF00FFUL, 0xFFFF00FFUL,
                                     0x0000FFFFUL, 0xFF00FFFFUL, 0x00FFFFFFUL, 0xFFFFFFFFUL};

static void linear_glyph(unsigned x, unsigned y, const u8 QB_FAR *bits, unsigned height,
                         unsigned foreground)
{
    u32 ink = (u32)(foreground & 0xFF) * 0x01010101UL;
    volatile u32 QB_FAR *at = (volatile u32 QB_FAR *)pixel_at(x, y);

    while (height--) {
        unsigned row = *bits++;

        at[0] = nibble_bytes[row >> 4] & ink;
        at[1] = nibble_bytes[row & 15] & ink;
        at += pitch / 4;
    }
}

static const GdOps linear_ops = {linear_plot, linear_read, linear_search, linear_move_rows,
                                 linear_glyph};

static const GdOps planar_ops = {planar_plot, planar_read, planar_search, planar_move_rows,
                                 planar_glyph};

void gd_plot(unsigned x, unsigned y, unsigned color, unsigned operation)
{
    color = ink(color);
    if (in_memory)
        pm_plot(x, y, color, operation);
    else
        ops->plot(x, y, color, operation);
}

unsigned gd_read(unsigned x, unsigned y)
{
    return in_memory ? pm_read(x, y) : ops->read(x, y);
}

int gd_search(int x, int last, unsigned y, unsigned c1, unsigned c2, int match)
{
    c1 = ink(c1);
    c2 = ink(c2);
    return in_memory ? pm_search(x, last, y, c1, c2, match) : ops->search(x, last, y, c1, c2, match);
}

void gd_move_rows(unsigned to, unsigned from, unsigned count)
{
    if (in_memory)
        pm_move_rows(to, from, count);
    else
        ops->move_rows(to, from, count);
}

void gd_glyph(unsigned x, unsigned y, const u8 QB_FAR *bits, unsigned height, unsigned foreground)
{
    foreground = ink(foreground);
    if (in_memory)
        pm_glyph(x, y, bits, height, foreground);
    else
        ops->glyph(x, y, bits, height, foreground);
}

/* The CGA modes: pixels packed in bytes, `bits` each with the leftmost in the
   high bits, the even rows of the screen in the first 8 KB and the odd rows in
   the next, 80 bytes a row. */
enum { CGA_ODD_ROWS = 0x2000, CGA_ROW_BYTES = 80 };

static Video *packed_row(unsigned y)
{
    return screen + (y & 1 ? CGA_ODD_ROWS : 0) + (unsigned long)(y >> 1) * CGA_ROW_BYTES;
}

/* The byte of pixel `x` in a row, and the shift that brings its pixel to the
   low bits, for pixels of `bits` bits. */
static unsigned packed_shift(unsigned x, unsigned bits)
{
    return 8 - bits - (x * bits & 7);
}

static void packed_plot(unsigned x, unsigned y, unsigned color, unsigned operation, unsigned bits)
{
    Video *at = packed_row(y) + (x * bits >> 3);
    unsigned shift = packed_shift(x, bits), mask = ((1u << bits) - 1) << shift;
    unsigned old = *at,
             new_bits = combined(old >> shift & (mask >> shift), color, operation) << shift & mask;

    *at = (u8)(old & ~mask | new_bits);
}

static unsigned packed_read(unsigned x, unsigned y, unsigned bits)
{
    return *(packed_row(y) + (x * bits >> 3)) >> packed_shift(x, bits) & ((1u << bits) - 1);
}

/* The CGA modes' scan, a byte of pixels at a time (B$FSCC): the masks of the
   pixels of the first and the last byte that are in range go to the kernel,
   which gives the first byte with a wanted pixel in range and which pixels of
   it, and the first of those, from the left going right and from the right
   going left, is the pixel. */
/* The search of a row of 2-bit or 1-bit pixels: the kernel makes the masks and
 * the colour bytes and gives the pixel. */
static int packed_search(int x, int last, unsigned y, unsigned c1, unsigned c2, int match,
                         unsigned flags)
{
    FillScan scan;

    scan.at = packed_row(y);
    scan.x = (unsigned)x;
    scan.to = (unsigned)last;
    scan.c1 = c1;
    scan.c2 = c2;
    scan.flags = (match ? 1 : 0) | flags;
    dev_scan_packed(&scan);
    return scan.found;
}

/* PAINT's row in one go: the first pixel from `x` to `last` that is neither
 * colour, and the run of non-`edge` pixels it is in. */
int gd_run(int x, int last, unsigned y, unsigned edge, unsigned paint, int limit, int *left,
           int *right)
{
    FillScan scan;
    int open, l, r;

    edge = ink(edge);
    paint = ink(paint);
    if (in_memory)
        return pm_run(x, last, y, edge, paint, limit, left, right);
    if (kind == KIND_PACKED4 || kind == KIND_PACKED2) {
        scan.at = packed_row(y);
        scan.x = (unsigned)x;
        scan.to = (unsigned)last;
        scan.c1 = edge;
        scan.c2 = paint;
        scan.flags = kind == KIND_PACKED4 ? 4 : 0;
        scan.limit = (unsigned)limit;
        dev_scan_run(&scan);
        if (scan.found < 0)
            return 0;
        *left = (int)scan.left;
        *right = (int)scan.right;
        return 1;
    }
    open = ops->search(x, last, y, edge, paint, 0);
    if (open < 0)
        return 0;
    l = ops->search(open, 0, y, edge, edge, 1);
    r = ops->search(open, limit, y, edge, edge, 1);
    *left = l < 0 ? 0 : l + 1;
    *right = r < 0 ? limit : r - 1;
    return 1;
}

static int packed4_search(int x, int last, unsigned y, unsigned c1, unsigned c2, int match)
{
    return packed_search(x, last, y, c1, c2, match, 4);
}

static int packed2_search(int x, int last, unsigned y, unsigned c1, unsigned c2, int match)
{
    return packed_search(x, last, y, c1, c2, match, 0);
}

static void packed_move_rows(unsigned to, unsigned from, unsigned count)
{
    BlockOp op;

    if (to < from && !((to ^ from) & 1)) {
        /* a scroll by an even number of rows: the rows of each bank are one
         * block of the bank */
        unsigned bank, offset, n;

        for (bank = 0; bank < 2; bank++) {
            offset = (to ^ bank) & 1;
            if (offset >= count)
                continue;
            n = (count - offset + 1) / 2;
            op.dst = packed_row(to + offset);
            op.src = packed_row(from + offset);
            op.count = n * CGA_ROW_BYTES;
            op.value = 0;
            dev_move(&op);
        }
    } else if (to < from) {
        while (count--) {
            op.dst = packed_row(to++);
            op.src = packed_row(from++);
            op.count = CGA_ROW_BYTES;
            op.value = 0;
            dev_move(&op);
        }
    } else {
        to += count;
        from += count;
        while (count--) {
            op.dst = packed_row(--to);
            op.src = packed_row(--from);
            op.count = CGA_ROW_BYTES;
            op.value = 0;
            dev_move(&op);
        }
    }
}

/* The 4 pixels of a nibble as 4 pixels of 2 bits, 11 where the pixel is set. */
static const u8 nibble_pairs[16] = {0x00, 0x03, 0x0C, 0x0F, 0x30, 0x33, 0x3C, 0x3F,
                                    0xC0, 0xC3, 0xCC, 0xCF, 0xF0, 0xF3, 0xFC, 0xFF};

/* A glyph of 8 pixels a row on the CGA screen: 2 bytes a row of 2-bit pixels, 1
 * of 1-bit ones, the even and the odd rows in their banks. */
static void packed_glyph(unsigned x, unsigned y, const u8 QB_FAR *bits, unsigned height,
                         unsigned foreground, unsigned depth)
{
    Video *at = packed_row(y) + (x * depth >> 3);
    unsigned ink = depth == 2 ? (foreground & 3) * 0x55 : foreground & 1 ? 0xFF : 0, row;

    for (; height; height--, bits++, y++) {
        row = *bits;
        if (depth == 2) {
            at[0] = nibble_pairs[row >> 4] & ink;
            at[1] = nibble_pairs[row & 15] & ink;
        } else {
            at[0] = row & ink;
        }
        if (y & 1)
            at += CGA_ROW_BYTES - CGA_ODD_ROWS;
        else
            at += CGA_ODD_ROWS;
    }
}

static void packed4_plot(unsigned x, unsigned y, unsigned c, unsigned o)
{
    packed_plot(x, y, c, o, 2);
}

static unsigned packed4_read(unsigned x, unsigned y)
{
    return packed_read(x, y, 2);
}

static void packed4_glyph(unsigned x, unsigned y, const u8 QB_FAR *b, unsigned h, unsigned f)
{
    packed_glyph(x, y, b, h, f, 2);
}

static void packed2_plot(unsigned x, unsigned y, unsigned c, unsigned o)
{
    packed_plot(x, y, c, o, 1);
}

static unsigned packed2_read(unsigned x, unsigned y)
{
    return packed_read(x, y, 1);
}

static void packed2_glyph(unsigned x, unsigned y, const u8 QB_FAR *b, unsigned h, unsigned f)
{
    packed_glyph(x, y, b, h, f, 1);
}

static const GdOps packed4_ops = {packed4_plot, packed4_read, packed4_search, packed_move_rows,
                                  packed4_glyph};

static const GdOps packed2_ops = {packed2_plot, packed2_read, packed2_search, packed_move_rows,
                                  packed2_glyph};

/* Box fills: one routine for each kind of mode, chosen once by gd_fill_select
   with the operation patched into the box loop (gdfill.asm), and run on a box
   that has already been clipped to the screen.  The geometry is worked out once
   for the box; each row is a straight run of `op mem, imm`, with the byte at
   each end of a row patched where the pixels in it are not all the box's.  An
   edge byte is old & keep ^ flip, which every operation is: set keeps nothing
   and flips to the pattern; and keeps the pattern; or keeps what the pattern
   lacks and flips to it; xor keeps all and flips. */
static unsigned edge_and(const GdFill *fill, unsigned mask)
{
    return (fill->keep | ~mask) & 0xFF;
}

static unsigned edge_xor(const GdFill *fill, unsigned mask)
{
    return fill->flip & mask;
}

/* The 256-colour mode: a row is `count` bytes, `pitch` apart. */
static void linear_box(const GdFill *fill, unsigned x, unsigned y, unsigned count, unsigned rows)
{
    FillBox box;

    box.dst = pixel_at(x, y);
    box.rows = rows;
    box.middle = count;
    box.steps[0] = box.steps[1] = pitch;
    box.phase = 0;
    box.edges = 0;
    dev_fill_box(&box);
}

/* The CGA modes: the bytes at the ends of a row keep the pixels the box does
   not cover.  Rows alternate between the two banks of the screen. */
static void packed_box(const GdFill *fill, unsigned x, unsigned y, unsigned count, unsigned rows,
                       unsigned bits)
{
    unsigned last = x + count - 1;
    unsigned first_byte = x * bits >> 3, last_byte = last * bits >> 3;
    unsigned left = 0xFF >> (x * bits & 7), right = 0xFF << (8 - bits - (last * bits & 7)) & 0xFF;
    Video *row = packed_row(y);
    FillBox box;

    if (first_byte == last_byte) {
        unsigned keep = edge_and(fill, left & right), flip = edge_xor(fill, left & right);

        while (rows--) {
            Video *at = row + first_byte;

            *at = (u8)(*at & keep ^ flip);
            row += y++ & 1 ? CGA_ROW_BYTES - CGA_ODD_ROWS : CGA_ODD_ROWS;
        }
        return;
    }
    box.dst = row + first_byte;
    box.rows = rows;
    box.middle = last_byte - first_byte - 1;
    box.steps[0] = CGA_ODD_ROWS;
    box.steps[1] = CGA_ROW_BYTES - CGA_ODD_ROWS;
    box.phase = y & 1 ? sizeof(unsigned) : 0;
    box.edges = 1;
    box.left = edge_and(fill, left) | edge_xor(fill, left) << 8;
    box.right = edge_and(fill, right) | edge_xor(fill, right) << 8;
    dev_fill_box(&box);
}

static void packed4_box(const GdFill *fill, unsigned x, unsigned y, unsigned count, unsigned rows)
{
    packed_box(fill, x, y, count, rows, 2);
}

static void packed2_box(const GdFill *fill, unsigned x, unsigned y, unsigned count, unsigned rows)
{
    packed_box(fill, x, y, count, rows, 1);
}

/* The planar modes: the colour goes through the controller's write mode 2, the
   operation through its function, and the bytes at the ends of a row under a
   bit mask.  What a mask covers is written down a whole column, so the mask is
   written three times for a box.  A set has the run of the box loop; the other
   operations read each byte to load the latches (a dword read would load only
   the last one). */
static void planar_edge(Video *at, unsigned rows, unsigned color)
{
    while (rows--) {
        (void)*at;
        *at = (u8)color;
        at += pitch;
    }
}

static void planar_box(const GdFill *fill, unsigned x, unsigned y, unsigned count, unsigned rows)
{
    unsigned last = x + count - 1, color = fill->color, function = fill->operation;
    unsigned left = 0xFF >> (x & 7), right = 0xFF << (7 - (last & 7)) & 0xFF;
    unsigned between = (last >> 3) - (x >> 3);
    Video *at = byte_of(x, y);

    if (function)
        controller(GC_DATA_ROTATE, function << FUNCTION_SHIFT);
    if (between == 0) {
        controller(GC_BIT_MASK, left & right);
        planar_edge(at, rows, color);
    } else {
        controller(GC_BIT_MASK, left);
        planar_edge(at, rows, color);
        controller(GC_BIT_MASK, 0xFF);
        if (between > 1) {
            if (function) {
                unsigned n;

                for (n = 1; n < between; n++)
                    planar_edge(at + n, rows, color);
            } else {
                FillBox box;

                box.dst = at + 1;
                box.rows = rows;
                box.middle = between - 1;
                box.steps[0] = box.steps[1] = pitch;
                box.phase = 0;
                box.edges = 0;
                dev_fill_box(&box);
            }
        }
        controller(GC_BIT_MASK, right);
        planar_edge(at + between, rows, color);
    }
    controller(GC_BIT_MASK, 0xFF);
    if (function)
        controller(GC_DATA_ROTATE, 0);
}

static void (*const boxes[KINDS])(const GdFill *fill, unsigned x, unsigned y, unsigned count,
                                  unsigned rows) = {planar_box, linear_box, packed4_box,
                                                    packed2_box};

/* One pixel: what a box would write to it. */
static void linear_dot_set(const GdFill *fill, unsigned x, unsigned y)
{
    *pixel_at(x, y) = (u8)fill->color;
}

static void linear_dot(const GdFill *fill, unsigned x, unsigned y)
{
    Video *at = pixel_at(x, y);

    *at = (u8)(*at & fill->keep ^ fill->flip);
}

static void packed_dot(const GdFill *fill, unsigned x, unsigned y, unsigned bits)
{
    Video *at = packed_row(y) + (x * bits >> 3);
    unsigned mask = ((1u << bits) - 1) << (8 - bits - (x * bits & 7));

    *at = (u8)(*at & (fill->keep | ~mask) ^ (fill->flip & mask));
}

static void packed4_dot(const GdFill *fill, unsigned x, unsigned y)
{
    packed_dot(fill, x, y, 2);
}

static void packed2_dot(const GdFill *fill, unsigned x, unsigned y)
{
    packed_dot(fill, x, y, 1);
}

static void planar_dot(const GdFill *fill, unsigned x, unsigned y)
{
    Video *at = byte_of(x, y);

    controller(GC_BIT_MASK, 0x80 >> (x & 7));
    (void)*at; /* loads the latches */
    *at = (u8)fill->color;
}

/* Eight points of a circle step at once, without a call for each (set only: the
 * dots' `color` and masks as they are). */
static void linear_octet(const GdFill *fill, unsigned cx, unsigned cy, int u, int v, int w, int z)
{
    Video *c = pixel_at(cx, cy), *a, *b;
    unsigned long vp = (unsigned long)v * pitch, zp = (unsigned long)z * pitch;
    u8 color = (u8)fill->color;

    a = c - vp;
    b = c + vp;
    a[-u] = color;
    a[u] = color;
    b[-u] = color;
    b[u] = color;
    a = c - zp;
    b = c + zp;
    a[-w] = color;
    a[w] = color;
    b[-w] = color;
    b[w] = color;
}

static void packed_octet(const GdFill *fill, unsigned cx, unsigned cy, int u, int v, int w, int z,
                         unsigned bits)
{
    FillOct oct;
    unsigned ys[4], xs[4], i;

    ys[0] = cy - v;
    ys[1] = cy + v;
    ys[2] = cy - z;
    ys[3] = cy + z;
    xs[0] = cx + u;
    xs[1] = cx - u;
    xs[2] = cx + w;
    xs[3] = cx - w;
    for (i = 0; i < 4; i++) {
        oct.row[i] = (ys[i] & 1 ? CGA_ODD_ROWS : 0) + (ys[i] >> 1) * CGA_ROW_BYTES;
        oct.col[i] = xs[i] * bits >> 3;
        oct.mask[i] = ((1u << bits) - 1) << (8 - bits - (xs[i] * bits & 7));
    }
    oct.base = (void QB_FAR *)screen;
    oct.color = fill->flip & 0xFF;
    dev_octet_packed(&oct);
}

static void packed4_octet(const GdFill *fill, unsigned cx, unsigned cy, int u, int v, int w, int z)
{
    packed_octet(fill, cx, cy, u, v, w, z, 2);
}

static void packed2_octet(const GdFill *fill, unsigned cx, unsigned cy, int u, int v, int w, int z)
{
    packed_octet(fill, cx, cy, u, v, w, z, 1);
}

static void planar_octet(const GdFill *fill, unsigned cx, unsigned cy, int u, int v, int w, int z)
{
    FillOct oct;
    unsigned xs[4], i;

    oct.row[0] = (cy - v) * pitch;
    oct.row[1] = (cy + v) * pitch;
    oct.row[2] = (cy - z) * pitch;
    oct.row[3] = (cy + z) * pitch;
    xs[0] = cx + u;
    xs[1] = cx - u;
    xs[2] = cx + w;
    xs[3] = cx - w;
    for (i = 0; i < 4; i++) {
        oct.col[i] = xs[i] >> 3;
        oct.mask[i] = 0x80 >> (xs[i] & 7);
    }
    oct.base = (void QB_FAR *)screen;
    oct.color = fill->color;
    dev_octet_planar(&oct);
}

static void (*const octets[KINDS])(const GdFill *fill, unsigned cx, unsigned cy, int u, int v,
                                   int w, int z) = {planar_octet, linear_octet, packed4_octet,
                                                    packed2_octet};

static void (*const dots[KINDS][2])(const GdFill *fill, unsigned x,
                                    unsigned y) = {{planar_dot, planar_dot},
                                                   {linear_dot, linear_dot_set},
                                                   {packed4_dot, packed4_dot},
                                                   {packed2_dot, packed2_dot}};

/* Lines: the loops of gdline.asm, one for each kind of mode, with Bresenham's
 * terms here.  They draw in the colour, as set. */
static void line_terms(FillLine *line, const GdFill *fill, unsigned dx, unsigned dy)
{
    int major = (int)(dx > dy ? dx : dy), minor = (int)(dx > dy ? dy : dx);

    line->count = (unsigned)major + 1;
    line->minor4 = 4 * minor;
    line->step4 = 4 * (minor - major);
    line->decision = 4 * minor - major;
    line->x_major = dx == 0 ? 2 : dx > dy;
    line->style = fill->style;
}

static void linear_line(const GdFill *fill, unsigned x, unsigned y, unsigned dx, unsigned dy,
                        int step_y)
{
    FillLine line;

    line_terms(&line, fill, dx, dy);
    line.dst = pixel_at(x, y);
    line.ystep = step_y > 0 ? (int)pitch : -(int)pitch;
    dev_line_linear(&line);
}

static void packed_line(const GdFill *fill, unsigned x, unsigned y, unsigned dx, unsigned dy,
                        int step_y, unsigned bits)
{
    FillLine line;

    line_terms(&line, fill, dx, dy);
    line.dst = packed_row(y) + (x * bits >> 3);
    line.pmask = ((1u << bits) - 1) << (8 - bits - (x * bits & 7));
    line.bpp = bits;
    line.color = fill->flip;
    line.ystep = step_y > 0 ? CGA_ROW_BYTES : -CGA_ROW_BYTES;
    dev_line_packed(&line);
}

static void packed4_line(const GdFill *fill, unsigned x, unsigned y, unsigned dx, unsigned dy,
                         int step_y)
{
    packed_line(fill, x, y, dx, dy, step_y, 2);
}

static void packed2_line(const GdFill *fill, unsigned x, unsigned y, unsigned dx, unsigned dy,
                         int step_y)
{
    packed_line(fill, x, y, dx, dy, step_y, 1);
}

static void planar_line(const GdFill *fill, unsigned x, unsigned y, unsigned dx, unsigned dy,
                        int step_y)
{
    FillLine line;

    line_terms(&line, fill, dx, dy);
    line.dst = byte_of(x, y);
    line.pmask = 0x80 >> (x & 7);
    line.ystep = step_y > 0 ? (int)pitch : -(int)pitch;
    line.color = fill->color;
    dev_line_planar(&line);
}

static void (*const lines[KINDS])(const GdFill *fill, unsigned x, unsigned y, unsigned dx,
                                  unsigned dy, int step_y) = {planar_line, linear_line,
                                                              packed4_line, packed2_line};

void gd_dots_begin(const GdFill *fill)
{
    if (kind == KIND_PLANAR && !in_memory && fill->operation)
        controller(GC_DATA_ROTATE, fill->operation << FUNCTION_SHIFT);
}

void gd_dots_end(const GdFill *fill)
{
    if (kind == KIND_PLANAR && !in_memory) {
        controller(GC_BIT_MASK, 0xFF);
        if (fill->operation)
            controller(GC_DATA_ROTATE, 0);
    }
}

void gd_fill_select(GdFill *fill, unsigned color, unsigned operation)
{
    unsigned byte;

    color = ink(color);
    byte = color & 0xFF;

    if (in_memory) {
        pm_fill_select(fill, color, operation);
        return;
    }
    switch (kind) {
    case KIND_PACKED4:
        byte = (color & 3) * 0x55;
        break;
    case KIND_PACKED2:
        byte = (color & 1) * 0xFF;
        break;
    case KIND_PLANAR:
        byte = color & 0x0F;
        break;
    }
    operation &= 3;
    fill->box = boxes[kind];
    fill->dot = dots[kind][operation == 0];
    fill->line = lines[kind];
    fill->octet = octets[kind];
    fill->color = color & 0xFF;
    fill->style = 0xFFFF;
    fill->operation = operation;
    fill->keep = operation == 0 ? 0 : operation == 1 ? byte : operation == 2 ? ~byte & 0xFF : 0xFF;
    fill->flip = operation == 1 ? 0 : byte;
    dev_fill_select(operation, byte);
    dev_line_select(operation, byte);
}

/* GET and PUT.  A sub-byte plane (a CGA mode's pixels, an EGA plane's bits) is
   a run of bits that starts `shift` bits into a screen byte and at bit 0 of an
   array byte, so each byte of one is made of two of the other. */
typedef volatile u32 QB_FAR Video32;

enum { LINEAR_NOT = 4 };

/* rows of width bytes, from a row `sstep` past the last to one `dstep` past it, combined by op (LinearBlit's). */
static void linear_blit(void *dst, const void *src, unsigned width, unsigned rows, int dstep, int sstep,
                        unsigned op)
{
    LinearBlit blit;

    blit.dst = dst;
    blit.src = src;
    blit.count = width;
    blit.rows = rows;
    blit.dstep = dstep;
    blit.sstep = sstep;
    blit.op = op;
    dev_blit(&blit);
}

int gd_solid_box(unsigned x, unsigned y, unsigned count, unsigned rows, unsigned color)
{
    SolidBox box;

    if (kind != KIND_LINEAR || in_memory)
        return 0;
    box.dst = (void *)pixel_at(x, y);
    box.count = count;
    box.rows = rows;
    box.dstep = (int)pitch;
    box.color = ink(color);
    dev_solid(&box);
    return 1;
}

void gd_get(unsigned x, unsigned y, unsigned width, unsigned rows, u8 QB_FAR *out)
{
    unsigned row;

    if (in_memory || vbe_planes()) {
        pm_get(x, y, width, rows, out);
        return;
    }
    if (kind == KIND_LINEAR) {
        linear_blit(out, (const u8 *)pixel_at(x, y), width, rows, (int)width, (int)pitch, 0);
    } else {
        /* a plane (or the CGA modes' one) at a time, a pass of rows with the
         * bits shifted into place */
        unsigned bits = kind == KIND_PLANAR    ? 1
                        : kind == KIND_PACKED4 ? 2
                                               : 1,
                 planes = kind == KIND_PLANAR ? PLANES : 1, plane;
        unsigned total = width * bits, bytes = (total + 7) >> 3, shift = x * bits & 7;
        FillXfer xfer;

        xfer.rows = rows;
        xfer.abytes = bytes;
        xfer.sbytes = (shift + total + 7) >> 3;
        xfer.stride = planes * bytes;
        xfer.shift = shift;
        xfer.last = 0xFF << (bytes * 8 - total) & 0xFF;
        xfer.bank = kind != KIND_PLANAR;
        xfer.step = (int)pitch;
        for (plane = 0; plane < planes; plane++) {
            if (kind == KIND_PLANAR) {
                controller(GC_READ_MAP, plane);
                xfer.screen = byte_of(x, y);
            } else {
                xfer.screen = packed_row(y) + (x * bits >> 3);
            }
            xfer.array = out + plane * bytes;
            dev_get_rows(&xfer);
        }
    }
}

int gd_put_step(unsigned x, unsigned y, unsigned width, unsigned rows, const u8 QB_FAR *in, unsigned step,
                unsigned operation, unsigned invert)
{
    if (kind != KIND_LINEAR || in_memory || vbe.nibbles)
        return 0;

    linear_blit((void *)pixel_at(x, y), in, width, rows, (int)pitch, (int)step, invert ? LINEAR_NOT : operation);
    return 1;
}

void gd_put(unsigned x, unsigned y, unsigned width, unsigned rows, const u8 QB_FAR *in,
            unsigned operation, unsigned invert)
{
    unsigned row;

    if (in_memory || vbe_planes()) {
        pm_put(x, y, width, rows, in, operation, invert);
        return;
    }
    if (kind == KIND_LINEAR) {
        linear_blit((void *)pixel_at(x, y), in, width, rows, (int)pitch, (int)width,
                    invert ? LINEAR_NOT : operation);
    } else {
        /* a plane (or the CGA modes' one) at a time, a pass of rows with the
         * bits shifted into place */
        unsigned bits = kind == KIND_PACKED4 ? 2 : 1, planes = kind == KIND_PLANAR ? PLANES : 1,
                 plane;
        unsigned total = width * bits, bytes = (total + 7) >> 3, shift = x * bits & 7;
        FillXfer xfer;

        xfer.rows = rows;
        xfer.abytes = bytes;
        xfer.sbytes = (shift + total + 7) >> 3;
        xfer.stride = planes * bytes;
        xfer.shift = shift;
        xfer.first = 0xFF >> shift;
        xfer.last = 0xFF << (xfer.sbytes * 8 - shift - total) & 0xFF;
        xfer.bank = kind != KIND_PLANAR;
        xfer.step = (int)pitch;
        dev_put_select(invert ? 4 : operation);
        if (kind != KIND_PLANAR) {
            xfer.screen = packed_row(y) + (x * bits >> 3);
            xfer.array = (void QB_FAR *)in;
            dev_put_rows(&xfer);
            return;
        }
        controller(GC_MODE, 0);
        if (operation)
            controller(GC_DATA_ROTATE, operation << FUNCTION_SHIFT);
        xfer.screen = byte_of(x, y);
        for (plane = 0; plane < planes; plane++) {
            dev_outw(0x3C4, 2 | 1 << plane << 8);
            xfer.rows = rows;
            xfer.array = (void QB_FAR *)(in + plane * bytes);
            dev_put_planar(&xfer);
        }
        dev_outw(0x3C4, 2 | 0x0F << 8);
        if (operation)
            controller(GC_DATA_ROTATE, 0);
        controller(GC_MODE, WRITE_MODE_2);
    }
}
