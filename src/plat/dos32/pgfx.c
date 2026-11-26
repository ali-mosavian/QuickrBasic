/* gfx's screen on DOS (plat/pgfx.h): VGA's mode 13h and the card's VBE modes.
 *
 * # How it works
 * The page is memory, as on the other hosts. pgfx_show copies it to the mode vbe.c picks: k times
 * its size where the mode holds that, centred. Where video memory holds two of the mode's pages,
 * the copy goes to the one not shown, which 4F07h then shows, at the retrace with vsync; else to
 * the one shown, after the retrace with vsync. A linear frame buffer is mapped once and kept (DPMI
 * 0800h: a host's mappings are few); a VBE 1.2 card's is written through its window (4F05h).
 *
 * The text screen is kept while gfx's is open: its memory, size and colours, set again as it
 * closes. Its cursors are put back in the BIOS's data as each mode is set, which zeroes them, and
 * carried over the closing's mode set as they are then: the runtime moves the BIOS's lazily, when a
 * key is waited for. A program's graphics SCREEN is not kept.
 *
 * A screen played is a 3D accelerator's, its driver's for the card found (accel/accel.h): it plays
 * gfx's FIXED lists, the text screen kept the same.
 */
#include "accel/accel.h"
#include "dos.h"
#include "plat/pclk.h"
#include "plat/pgfx.h"
#include "plat/pmem.h"
#include "plat/pscr.h"
#include "plat/psys.h"
#include "vbe.h"
#include <string.h>

enum : uint16_t {
    VBE_WINDOW = 0x4F05,
    VBE_START = 0x4F07,
    VBE_OK = 0x004F,
    DAC_READ = 0x3C7,
    DAC_WRITE = 0x3C8,
    DAC_DATA = 0x3C9,
};
enum : uint32_t {
    TEXT = 0xB8000,
    TEXT_BYTES = 0x8000,
    BDA_CURSORS = 0x450,
    BDA_SHAPE = 0x460,
    BDA_PAGE = 0x462,
    HZ_FRAMES = 4,
    MICRO = 1000000,
    NO_STEP = 0xFFFFFFFF,
};
enum { CURSORS = 8, COLOURS = 256, DAC_BYTES = 3 * COLOURS };

static struct {
    const VbeMode *modes; /* the ones gfx shows in, 13h's first: null, not listed */
    uint32_t n;
    PdosVbe card;
    uint32_t mapped_base; /* the frame buffer mapped, at mapped */
    uint8_t *mapped;

    /* the screen open: null page, none */
    uint8_t *page, *row; /* the canvas; a row widened, or zeros */
    uint16_t w, h;
    uint8_t depth, bytes;
    bool vsync;
    uint32_t chosen;     /* the mode open picked */
    const VbeMode *mode; /* the one shown in */
    VbeFit fit;
    uint8_t *screen;      /* its memory: null, through the window */
    uint8_t pages, drawn; /* in video memory; the one the next frame goes to */
    uint32_t step;        /* the window's: NO_STEP, unknown */
    float hz;
    bool coloured; /* the DAC holds dac */
    uint32_t dac[COLOURS];

    /* the text screen kept: its memory, then its DAC */
    uint8_t *kept;
    PscrSize size;

    const PgfxPlayer *playing; /* the screen open, an accelerator's: null, none */
} gfx;

/* A VBE function with registers BX, CX, DX: whether it did it. */
static bool vbe(const uint16_t ax, const uint32_t bx, const uint32_t cx, const uint32_t dx)
{
    PdosRmRegs rm = {
        .eax = ax,
        .ebx = bx,
        .ecx = cx,
        .edx = dx,
    };

    return pdos_rm_int(INT_VIDEO, &rm) && (uint16_t)rm.eax == VBE_OK;
}

/* The modes gfx shows in, listed once. */
static void listed(void)
{
    if (!gfx.modes)
        gfx.modes = pdos_vbe_list(&gfx.n, &gfx.card);
}

/* The frame buffer at physical base, all of video memory: null, DPMI would not map it. */
static uint8_t *mapped(const uint32_t base)
{
    if (gfx.mapped && gfx.mapped_base == base)
        return gfx.mapped;

    uint8_t *const at = (uint8_t *)pdos_map(base, gfx.card.memory);

    if (!at)
        return nullptr;

    gfx.mapped_base = base;
    gfx.mapped = at;
    return gfx.mapped;
}

/* n bytes to video memory at `at`. */
static void put(uint32_t at, const uint8_t *from, uint32_t n)
{
    if (gfx.screen) {
        memcpy(gfx.screen + at, from, n);
        return;
    }

    uint8_t *const window = (uint8_t *)pdos_lin(gfx.mode->window_seg * 16u);

    while (n) {
        uint32_t step, in;
        const uint32_t part = vbe_window(gfx.mode, at, n, &step, &in);

        if (step != gfx.step && vbe(VBE_WINDOW, gfx.mode->window, 0, step))
            gfx.step = step;

        memcpy(window + in, from, part);
        at += part;
        from += part;
        n -= part;
    }
}

/* The canvas to page p. */
static void draw(const uint8_t p)
{
    const VbeMode *const m = gfx.mode;
    const uint32_t line = (uint32_t)gfx.w * gfx.bytes, k = gfx.fit.k;
    const uint32_t first = p * m->pitch * m->h + gfx.fit.y * m->pitch + gfx.fit.x * gfx.bytes;

    for (uint32_t y = 0; y < gfx.h; y++) {
        const uint8_t *from = gfx.page + y * line;

        if (k > 1) {
            vbe_widen(gfx.row, from, gfx.w, gfx.bytes, (uint16_t)k);
            from = gfx.row;
        }

        for (uint32_t i = 0; i < k; i++)
            put(first + (y * k + i) * m->pitch, from, line * k);
    }
}

/* The refresh, timed over a few retraces. */
static float timed(void)
{
    pscr_retrace();
    const uint64_t from = pclk_us();

    for (uint32_t i = 0; i < HZ_FRAMES; i++)
        pscr_retrace();

    const uint64_t us = pclk_us() - from;
    return us ? (float)HZ_FRAMES * MICRO / (float)us : 0;
}

/* gfx's mode i set, its pages cleared: false, the card would not. */
static bool shown_in(const uint32_t i)
{
    const VbeMode *const m = &gfx.modes[i];
    const VbeFit fit = vbe_fit(m, gfx.w, gfx.h);
    uint8_t *const row = pmem_resize(gfx.row, m->pitch);

    if (!fit.k || !row)
        return false;

    gfx.row = row;

    if (!pdos_vbe_set(m))
        return false;

    gfx.screen = m->number == VBE_MODE_13H ? (uint8_t *)pdos_lin(m->base)
                 : m->linear               ? mapped(m->base)
                                           : nullptr;

    if (m->linear && !gfx.screen)
        return false;

    gfx.mode = m;
    gfx.fit = fit;
    gfx.step = NO_STEP;
    gfx.coloured = false;
    gfx.pages = m->number != VBE_MODE_13H && 2 * m->pitch * m->h <= gfx.card.memory &&
                        vbe(VBE_START, 0, 0, 0)
                    ? 2
                    : 1;
    gfx.drawn = gfx.pages - 1;
    memset(gfx.row, 0, m->pitch);

    for (uint32_t y = 0; y < gfx.pages * m->h; y++)
        put(y * m->pitch, gfx.row, m->pitch);

    gfx.hz = timed();
    return true;
}

/* The DAC's colours, &HRRGGBB, where they changed. */
static void coloured(const uint32_t *const palette)
{
    uint32_t from = 0, to = COLOURS;

    if (gfx.coloured) {
        while (from < COLOURS && palette[from] == gfx.dac[from])
            from++;
        while (to > from && palette[to - 1] == gfx.dac[to - 1])
            to--;
    }

    psys_outp(DAC_WRITE, (uint8_t)from);

    for (uint32_t i = from; i < to; i++) {
        psys_outp(DAC_DATA, (uint8_t)(palette[i] >> 18 & 63));
        psys_outp(DAC_DATA, (uint8_t)(palette[i] >> 10 & 63));
        psys_outp(DAC_DATA, (uint8_t)(palette[i] >> 2 & 63));
    }

    memcpy(gfx.dac, palette, sizeof gfx.dac);
    gfx.coloured = true;
}

/* The text screen kept: false, no memory. */
static bool kept(void)
{
    gfx.kept = pmem_resize(nullptr, TEXT_BYTES + DAC_BYTES);

    if (!gfx.kept)
        return false;

    gfx.size = pscr_size();
    memcpy(gfx.kept, (const uint8_t *)pdos_lin(TEXT), TEXT_BYTES);
    psys_outp(DAC_READ, 0);

    for (uint32_t i = 0; i < DAC_BYTES; i++)
        gfx.kept[TEXT_BYTES + i] = psys_inp(DAC_DATA);

    return true;
}

/* The text screen as it was kept, its cursors as they are. */
static void restored(void)
{
    uint16_t cursors[CURSORS], shape;
    const uint8_t active = *pdos_lin(BDA_PAGE);

    memcpy(cursors, (const uint8_t *)pdos_lin(BDA_CURSORS), sizeof cursors);
    memcpy(&shape, (const uint8_t *)pdos_lin(BDA_SHAPE), sizeof shape);
    pscr_text(gfx.size);
    memcpy((uint8_t *)pdos_lin(TEXT), gfx.kept, TEXT_BYTES);
    psys_outp(DAC_WRITE, 0);

    for (uint32_t i = 0; i < DAC_BYTES; i++)
        psys_outp(DAC_DATA, gfx.kept[TEXT_BYTES + i]);

    for (uint8_t p = 0; p < CURSORS; p++)
        pscr_cursor(p, (uint8_t)(cursors[p] >> 8), (uint8_t)cursors[p]);

    pscr_shape(shape);
    pscr_page(active);
    pmem_free(gfx.kept);
    gfx.kept = nullptr;
}

static bool played_open(const uint16_t w, const uint16_t h, const bool vsync)
{
    PciFunction card;
    const AccelDriver *const d = accel_driver(&card);

    return d && accel_open(d, &card, w, h, vsync);
}

static const PgfxPlayer played = {
    .shaders = 1 << QX_SHADER_FIXED,
    .y_down = true,
    .open = played_open,
    .close = accel_close,
    .play = accel_play,
    .read = accel_read,
};

void pgfx_set_player(const PgfxPlayer *const p) { (void)p; }

void pgfx_set_display(const PgfxDisplay *const d) { (void)d; }

const PgfxPlayer *pgfx_open_played(
    const uint16_t w,
    const uint16_t h,
    const uint8_t shaders,
    const bool vsync
)
{
    pgfx_close();

    if (!(played.shaders & shaders) || !kept())
        return nullptr;

    if (played.open(w, h, vsync)) {
        gfx.playing = &played;
        return gfx.playing;
    }

    restored(); /* a driver may have set a mode */
    return nullptr;
}

const PgfxPlayer *pgfx_playing(void) { return gfx.playing; }

bool pgfx_open(const uint16_t w, const uint16_t h, const uint8_t depth, const bool vsync)
{
    pgfx_close();

    listed();

    const int32_t i = vbe_pick(gfx.modes, gfx.n, w, h, depth);
    const size_t bytes = (size_t)w * h * vbe_bytes(depth);

    if (i < 0 || !(gfx.page = pmem_resize(nullptr, bytes)))
        return false;

    memset(gfx.page, 0, bytes);
    gfx.w = w;
    gfx.h = h;
    gfx.depth = depth;
    gfx.bytes = vbe_bytes(depth);
    gfx.vsync = vsync;
    gfx.chosen = (uint32_t)i;

    if (kept() && shown_in(gfx.chosen))
        return true;

    if (gfx.kept)
        restored();

    pmem_free(gfx.page);
    pmem_free(gfx.row);
    gfx.page = gfx.row = nullptr;
    return false;
}

void pgfx_close(void)
{
    if (gfx.playing) {
        gfx.playing->close();
        gfx.playing = nullptr;
        restored();
    }

    if (!gfx.page)
        return;

    restored();
    pmem_free(gfx.page);
    pmem_free(gfx.row);
    gfx.page = gfx.row = nullptr;
    gfx.mode = nullptr;
}

uint8_t *pgfx_page(uint32_t *const pitch)
{
    *pitch = (uint32_t)gfx.w * gfx.bytes;
    return gfx.page;
}

void pgfx_show(const uint32_t *const palette)
{
    if (!gfx.page)
        return;

    const uint8_t p = gfx.drawn;

    if (gfx.pages == 1 && gfx.vsync)
        pscr_retrace();

    draw(p);

    if (gfx.pages == 2 && gfx.vsync)
        pscr_retrace();

    if (gfx.depth == 8)
        coloured(palette);

    if (gfx.pages == 2 && vbe(VBE_START, 0, 0, p * gfx.mode->h))
        gfx.drawn = !p;
}

void pgfx_play(const uint32_t *const list, const uint32_t words)
{
    if (gfx.playing)
        gfx.playing->play(list, words);
}

float pgfx_hz(void) { return gfx.playing ? accel_hz() : gfx.page ? gfx.hz : 0; }

bool pgfx_shown(PgfxFrame *const f)
{
    (void)f;
    return false;
}

uint32_t pgfx_monitors(void) { return 1; }

uint32_t pgfx_modes(const uint32_t monitor, QxMode *const out, const uint32_t most)
{
    listed();

    if (monitor != 1)
        return 0;

    for (uint32_t i = 0; i < gfx.n && i < most; i++)
        out[i] = (QxMode){gfx.modes[i].w, gfx.modes[i].h, gfx.modes[i].depth, 0};

    return gfx.n;
}

bool pgfx_mode(const uint32_t monitor, const uint32_t n)
{
    const uint32_t i = monitor && n ? n - 1 : gfx.chosen;

    if (!gfx.page || monitor > 1 || i >= gfx.n || gfx.modes[i].depth != gfx.depth)
        return false;

    if (shown_in(i))
        return true;

    shown_in(gfx.chosen); /* the card would not: the mode open picked again */
    return false;
}

bool pgfx_event(QxEvent *const e)
{
    (void)e;
    return false;
}
