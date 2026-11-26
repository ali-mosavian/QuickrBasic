/* Palette rows, the nearest index and the 8-bit tables (palette.h). */
#include "core/palette.h"
#include "core/mem.h"
#include "qbrt/llrm/qb/dev/host/vgadata.h"

enum {
    MAP_BITS = 5, /* px_cell's */
    MAP_CELLS = PAL_CELLS,
    LIGHTS = PAL_FULL + 1,
    BLEND_CELLS = PAL_COLOURS * PAL_COLOURS,
};

static_assert(MAP_CELLS == 1 << (3 * MAP_BITS), "a cell of each 5-bit colour");

/* A VGA DAC's 6 bits as 8. */
static uint32_t eight(const uint8_t six) { return (uint32_t)(six << 2 | six >> 4); }

static uint32_t rgb_of(const uint32_t r, const uint32_t g, const uint32_t b)
{
    return r << 16 | g << 8 | b;
}

void pal_default(Palette *const p)
{
    mem_fill(p, 0, sizeof *p);

    for (uint32_t i = 0; i < PAL_COLOURS; i++) {
        const unsigned char *const c = qb_vga_palette[i];
        p->rows[0][i] = rgb_of(eight(c[0]), eight(c[1]), eight(c[2]));
    }
}

/* Row 0's tables gone: made again when next asked for. */
static void forget(Palette *const p)
{
    mem_free(p->map);
    mem_free(p->light);
    p->map = p->light = nullptr;

    for (uint32_t i = 0; i < PAL_BLENDS; i++) {
        mem_free(p->blends[i].table);
        p->blends[i] = (PalBlend){};
    }
}

void pal_done(Palette *const p)
{
    forget(p);
    mem_free(p->lut);
    mem_free(p->argb);
    mem_free(p->pixels);
    p->lut = nullptr;
    p->argb = nullptr;
    p->pixels = nullptr;
}

void pal_set(Palette *const p, const int32_t row, const uint32_t *const colours, const uint32_t n)
{
    mem_copy(p->rows[row], colours, (size_t)n * sizeof *colours);
    p->changed[row]++;

    if (!row)
        forget(p);
}

uint8_t pal_nearest(const uint32_t *const row, const uint32_t rgb)
{
    uint8_t best = 0;
    int32_t least = INT32_MAX;

    for (uint32_t i = 0; i < PAL_COLOURS && least; i++) {
        const int32_t r = (int32_t)px_channel(row[i], 16) - (int32_t)px_channel(rgb, 16);
        const int32_t g = (int32_t)px_channel(row[i], 8) - (int32_t)px_channel(rgb, 8);
        const int32_t b = (int32_t)px_channel(row[i], 0) - (int32_t)px_channel(rgb, 0);
        const int32_t d = r * r + g * g + b * b;

        if (d < least) {
            least = d;
            best = (uint8_t)i;
        }
    }

    return best;
}

/* The map, made from row 0 when not there: each cell's centre's nearest index. */
const uint8_t *pal_map(Palette *const p)
{
    if (p->map || !(p->map = mem_alloc(MAP_CELLS)))
        return p->map;

    const uint32_t step = 1 << (8 - MAP_BITS), half = step / 2, top = (1 << MAP_BITS) - 1;

    for (uint32_t i = 0; i < MAP_CELLS; i++) {
        const uint32_t r = (i >> (2 * MAP_BITS)) * step + half;
        const uint32_t g = (i >> MAP_BITS & top) * step + half, b = (i & top) * step + half;
        p->map[i] = pal_nearest(p->rows[0], rgb_of(r, g, b));
    }

    return p->map;
}

bool pal_index(Palette *const p, const uint32_t rgb, uint8_t *const index)
{
    const uint8_t *const m = pal_map(p);

    if (m)
        *index = m[px_cell(rgb)];

    return m;
}

uint32_t pal_shade(const uint32_t rgb, const int32_t light, const uint32_t tint)
{
    const uint32_t l = (uint32_t)light;
    uint32_t out = 0;

    for (uint32_t shift = 0; shift < 24; shift += 8)
        out |= px_channel(rgb, shift) * px_channel(tint, shift) * l / (255 * PAL_FULL) << shift;

    return out;
}

/* The light table, made from row 0 when not there: null, no memory. */
static const uint8_t *light(Palette *const p)
{
    if (p->light)
        return p->light;

    const uint8_t *const m = pal_map(p);

    if (!m || !(p->light = mem_alloc(LIGHTS * PAL_COLOURS)))
        return nullptr;

    for (int32_t l = 0; l < LIGHTS; l++)
        for (uint32_t i = 0; i < PAL_COLOURS; i++)
            p->light[l * PAL_COLOURS + i] = m[px_cell(pal_shade(p->rows[0][i], l, PAL_WHITE))];

    return p->light;
}

/* The key of row, light and tint as the rows are now. */
static PalKey key(const Palette *const p, const int32_t row, const int32_t l, const uint32_t tint)
{
    return (PalKey){row, l, tint, {p->changed[0], p->changed[row]}};
}

static bool same(const PalKey a, const PalKey b)
{
    return a.row == b.row && a.light == b.light && a.tint == b.tint &&
           a.changed[0] == b.changed[0] && a.changed[1] == b.changed[1];
}

bool pal_lut(
    Palette *const p,
    const int32_t row,
    const int32_t l,
    const uint32_t tint,
    const uint8_t **const lut
)
{
    *lut = nullptr;

    if (!row && tint == PAL_WHITE && l == PAL_FULL)
        return true;

    if (!row && tint == PAL_WHITE) {
        const uint8_t *const table = light(p);
        *lut = table ? table + l * PAL_COLOURS : nullptr;
        return table;
    }

    const PalKey k = key(p, row, l, tint);

    if (!p->lut || !same(p->lut_key, k)) {
        const uint8_t *const m = pal_map(p);

        if (!m || (!p->lut && !(p->lut = mem_alloc(PAL_COLOURS))))
            return false;

        for (uint32_t i = 0; i < PAL_COLOURS; i++)
            p->lut[i] = m[px_cell(pal_shade(p->rows[row][i], l, tint))];

        p->lut_key = k;
    }

    *lut = p->lut;
    return true;
}

const uint32_t *pal_argb(Palette *const p, const int32_t row, const int32_t l, const uint32_t tint)
{
    const PalKey k = key(p, row, l, tint);

    if (p->argb && same(p->argb_key, k))
        return p->argb;

    if (!p->argb && !(p->argb = mem_alloc(PAL_COLOURS * sizeof *p->argb)))
        return nullptr;

    for (uint32_t i = 0; i < PAL_COLOURS; i++)
        p->argb[i] = PX_OPAQUE | pal_shade(p->rows[row][i], l, tint);

    p->argb_key = k;
    return p->argb;
}

const uint32_t *pal_pixels(
    Palette *const p,
    const int32_t row,
    const int32_t l,
    const uint32_t tint,
    const PxFormat from,
    const PxFormat into
)
{
    const PalKey k = key(p, row, l, tint);

    if (p->pixels && same(p->pixels_key, k) && p->pixels_from == from && p->pixels_into == into)
        return p->pixels;

    const uint32_t *const argb = from == PX_8 ? pal_argb(p, row, l, tint) : nullptr;

    if ((from == PX_8 && !argb) ||
        (!p->pixels && !(p->pixels = mem_alloc(PX_TABLE * sizeof *p->pixels))))
        return nullptr;

    px_table(p->pixels, from, into, argb, pal_shade(PAL_WHITE, l, tint));
    p->pixels_key = k;
    p->pixels_from = from;
    p->pixels_into = into;
    return p->pixels;
}

const uint8_t *pal_blend(Palette *const p, const BlendMode mode, const int32_t alpha)
{
    PalBlend *oldest = &p->blends[0];
    p->asked++;

    for (uint32_t i = 0; i < PAL_BLENDS; i++) {
        PalBlend *const b = &p->blends[i];

        if (b->table && b->mode == mode && b->alpha == alpha) {
            b->used = p->asked;
            return b->table;
        }

        oldest = b->used < oldest->used ? b : oldest;
    }

    const uint8_t *const m = pal_map(p);

    if (!m || (!oldest->table && !(oldest->table = mem_alloc(BLEND_CELLS))))
        return nullptr;

    for (uint32_t s = 0; s < PAL_COLOURS; s++)
        for (uint32_t d = 0; d < PAL_COLOURS; d++)
            oldest->table[s << 8 | d] =
                m[px_cell(px_mix(p->rows[0][s], p->rows[0][d], mode, (uint32_t)alpha))];

    *oldest = (PalBlend){oldest->table, mode, (uint8_t)alpha, p->asked};

    return oldest->table;
}
