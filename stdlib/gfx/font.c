/* Fonts (font.h). */
#include "font.h"
#include "mem.h"
#include "qbrt/llrm/qb/dev/host/vgadata.h"

enum {
    FIRST = 32, /* a sheet's first code */
    CODES = 256,
    GLYPH = 8,
    ACROSS = 16, /* the built-in's glyphs a row */
    SIDE = ACROSS * GLYPH,
};

void font_init(Fonts *const fs)
{
    hdl_init(&fs->table, HDL_FONT);
    fs->builtin = (Font){
        .w = GLYPH,
        .h = GLYPH,
        .columns = ACROSS,
        .count = CODES,
    };

    fs->pixels = nullptr;
}

void font_done(Fonts *const fs)
{
    for (uint32_t slot = 0; slot < fs->table.n; slot++)
        mem_free(hdl_item(&fs->table, hdl_at(&fs->table, slot)));

    hdl_done(&fs->table);
    mem_free(fs->pixels);
    font_init(fs);
}

uint32_t font_new(
    Fonts *const fs,
    const uint32_t sheet,
    const int32_t sheet_w,
    const int32_t sheet_h,
    const int32_t w,
    const int32_t h
)
{
    Font *const f = mem_alloc(sizeof *f);

    if (!f)
        return 0;

    const int32_t columns = sheet_w / w, cells = columns * (sheet_h / h);
    *f = (Font){
        .sheet = sheet,
        .w = w,
        .h = h,
        .columns = columns,
        .first = FIRST,
        .count = (uint32_t)rect_min(cells, CODES - FIRST),
    };

    const uint32_t handle = hdl_add(&fs->table, f);

    if (!handle)
        mem_free(f);

    return handle;
}

const Font *font_get(const Fonts *const fs, const uint32_t h)
{
    return h ? hdl_item(&fs->table, h) : &fs->builtin;
}

/* The runtime's font into a sheet SIDE square. */
static void draw_builtin(uint8_t *const p)
{
    for (uint32_t code = 0; code < CODES; code++) {
        uint8_t *const cell = p + code / ACROSS * GLYPH * SIDE + code % ACROSS * GLYPH;

        for (uint32_t r = 0; r < GLYPH; r++)
            for (uint32_t c = 0; c < GLYPH; c++)
                cell[r * SIDE + c] = qb_vga_font_8[code * GLYPH + r] >> (GLYPH - 1 - c) & 1;
    }
}

bool font_builtin(Fonts *const fs, Pixels *const out)
{
    if (!fs->pixels) {
        if (!(fs->pixels = mem_alloc(SIDE * SIDE)))
            return false;

        draw_builtin(fs->pixels);
    }

    *out = (Pixels){fs->pixels, SIDE, SIDE, SIDE, PX_8};

    return true;
}

bool font_glyph(const Font *const f, const uint8_t code, Rect *const cell)
{
    const uint32_t i = (uint32_t)code - f->first;

    if (code < f->first || i >= f->count)
        return false;

    const int32_t column = (int32_t)i % f->columns, row = (int32_t)i / f->columns;
    *cell = (Rect){column * f->w, row * f->h, f->w, f->h};

    return true;
}
