/* What 3dfx's drivers share (tdfx.h). */
#include "plat/dos32/drivers/3dfx/tdfx.h"
#include "plat/dos32/drivers/3dfx/regs.h"
#include "plat/dos32/drivers/3dfx/setup.h"
#include <stddef.h>

enum : uint32_t {
    SIDE = 256,
    LEAST = 2, /* sides of 4 up: smaller levels are padded on some cards */
    ASPECT = 3,
    ALIGN = 16, /* texBaseAddr's: 8 bytes on SST-1 and CVG, 16 on H3 */
    TEXEL = 2,
    PIXEL_AT = 1, /* the frame buffer window's: a pixel */
    CALM = 3,     /* status reads idle in a row: a read can pass the FIFO's last write */
    FBI_CALM = 6,
    LOD_OFF = 8 * 4 << 6 | 8 * 4, /* a TMU's lodmax and lodmin past every level: not drawn from */
    FIVE = 0x1F,
    SIX = 0x3F,
    OPAQUE = 255,
    COLOURS = TDFX_COLOURS,
    RUN = 8, /* palette entries a run of registers */
    PALETTE_WRITE = 1u << 31,
    INDEX_AT = 23, /* a palette entry's index, less its low bit the register's */
    RGB = 0xFFFFFF,
    DROPPED = 0x070307, /* the bits of a colour a 565 pixel drops */
};

static Tdfx card;
static const TdfxFamily *family;

static_assert(
    SST_FVERTEX + TDFX_START * 4 == SST_FSTART && SST_FVERTEX + TDFX_DX * 4 == SST_FDX &&
    SST_FVERTEX + TDFX_DY * 4 == SST_FDY && SST_FVERTEX + TDFX_COMMAND * 4 == SST_FTRIANGLE
);

TdfxPicks tdfx_picks(const uint32_t mask)
{
    TdfxPicks p = {mask, 0, {}};

    for (uint32_t i = 0, left = mask; left; i++, left >>= 1)
        if (left & 1)
            p.at[p.n++] = (uint8_t)i;

    return p;
}

void tdfx_put(Tdfx *const c, const uint32_t reg, const uint32_t v)
{
    static const TdfxPicks one = {1, 1, {0}};

    c->chip->put(c, reg, &one, &v);
}

void tdfx_put_run(Tdfx *const c, const uint32_t reg, const uint32_t *const v, const uint32_t n)
{
    const TdfxPicks p = tdfx_picks((1u << n) - 1);

    c->chip->put(c, reg, &p, v);
}

void tdfx_put_init(const Tdfx *const c, const uint32_t reg, const uint32_t v)
{
    *(volatile uint32_t *)(c->regs + reg) = v;
}

uint32_t tdfx_get(const Tdfx *const c, const uint32_t reg)
{
    return *(const volatile uint32_t *)(c->regs + reg);
}

bool tdfx_combined(Tdfx *const c, const uint32_t phys, const uint32_t bytes)
{
    const AccelHost *const host = c->host;

    c->fence = host->combine && host->combine(phys, bytes) ? host->fence : nullptr;
    return c->fence;
}

void tdfx_uncombined(Tdfx *const c)
{
    if (c->fence)
        c->host->uncombine();

    c->fence = nullptr;
}

void tdfx_idle(Tdfx *const c)
{
    const uint64_t until = c->host->clock_us() + TDFX_WAIT_US;

    uint32_t calm = 0;

    tdfx_put(c, SST_NOP, 0);
    tdfx_fenced(c);

    while (calm < CALM && c->host->clock_us() < until)
        calm = *c->status & c->chip->busy ? 0 : calm + 1;
}

void tdfx_idle_fbi(const Tdfx *const c)
{
    const uint64_t until = c->host->clock_us() + TDFX_WAIT_US;

    tdfx_fenced(c);

    for (uint32_t calm = 0; calm < FBI_CALM && c->host->clock_us() < until;)
        calm = tdfx_get(c, SST_STATUS) & SST_FBI_BUSY ? 0 : calm + 1;
}

/* The colour buffer slot's draws go to: the screen's, the one not shown; a target's, 0. */
static uint32_t buffer_of(const Tdfx *const c, const uint32_t slot)
{
    return slot == ACCEL_SCREEN ? c->shown ^ 1 : 0;
}

static uint32_t row_of(const Tdfx *const c, const uint32_t slot)
{
    return slot == ACCEL_SCREEN ? c->top : c->targets[slot].y;
}

static uint32_t column_of(const Tdfx *const c, const uint32_t slot)
{
    return slot == ACCEL_SCREEN ? c->left : 0;
}

/* TMU0's registers placing texture slot. */
/* Each held register's place. */
static const uint32_t HELD_AT[TDFX_HELD] = {
    SST_CLIP_X,
    SST_CLIP_Y,
    SST_FBZ_COLOUR_PATH,
    SST_ALPHA_MODE,
    SST_FBZ_MODE,
    SST_SETUP_MODE,
    SST_TMU0 + SST_TEXTURE_MODE,
};

/* Held register which put v, unless it holds v: a state change sends only what it changes. */
static void held_put(Tdfx *const c, const TdfxHeld which, const uint32_t v)
{
    if (c->known >> which & 1 && c->held[which] == v)
        return;

    tdfx_put(c, HELD_AT[which], v);
    c->held[which] = v;
    c->known |= 1u << which;
}

static void placed(Tdfx *const c, const uint32_t slot)
{
    const TdfxTexture *const t = &c->textures[slot];

    if (c->placed == slot)
        return;

    tdfx_put(c, SST_TMU0 + SST_TLOD, t->lod);
    tdfx_put(c, SST_TMU0 + SST_TEX_BASE, t->base);
    c->placed = slot;
    c->repaletted = false; /* placed, its texels are looked up afresh */
}

/* The state's way, of one colour or not, to w: only the picks it sends worked out. */
static void way_of(const Tdfx *const c, const bool one, TdfxWay *const w)
{
    const uint32_t params = one ? c->params & ~TDFX_COLOURED : c->params;

    w->params = params;
    w->path = one ? c->path | SST_PATH_COLOUR0 : c->path;

    if (!family->unit) {
        w->sent = tdfx_picks(tdfx_sent(params));
        w->started = tdfx_picks(tdfx_started_sent(params));
        return;
    }

    const uint32_t corner = tdfx_corner_sent(params);

    w->mode = tdfx_unit_mode(params);
    w->corner = tdfx_picks(corner);

    if (!c->chip->corners) {
        w->begins = tdfx_picks(corner | 1u << TDFX_VBEGIN);
        w->draws = tdfx_picks(corner | 1u << TDFX_VDRAW);
    }
}

/* The state's words and the aim's clip sent, with its way by the corners' colours: its ways kept
   while its parameters and path are, as a texture's change keeps them. */
static void sent(Tdfx *const c)
{
    const AccelState *const s = &c->state;
    const bool texel = s->source != ACCEL_COLOUR && s->texture && s->texture < ACCEL_TEXTURES;
    const TdfxWords w = tdfx_words(s, c->draw, texel && c->textures[s->texture].indexed);
    const uint32_t x0 = (uint32_t)c->clip.x + c->ox, y0 = (uint32_t)c->clip.y + c->oy;
    const uint32_t params = tdfx_params(s, texel);

    if (!c->way || params != c->params || w.colour_path != c->path) {
        c->params = params;
        c->path = w.colour_path;
        way_of(c, false, &c->ways[0]);
        c->coloured = false;
    }

    c->way = &c->ways[0];

    held_put(c, TDFX_HELD_CLIP_X, x0 << 16 | (x0 + (uint32_t)c->clip.w));
    held_put(c, TDFX_HELD_CLIP_Y, y0 << 16 | (y0 + (uint32_t)c->clip.h));
    held_put(c, TDFX_HELD_PATH, c->path);
    held_put(c, TDFX_HELD_ALPHA, w.alpha_mode);
    held_put(c, TDFX_HELD_FBZ, w.fbz_mode);

    if (family->unit) /* a fan's corners and three a triangle alike: each triangle begins */
        held_put(c, TDFX_HELD_SETUP, c->way->mode | SST_UNIT_FAN);

    if (texel) {
        held_put(c, TDFX_HELD_TEXTURE_MODE, w.texture_mode);
        placed(c, s->texture);
    }
}

/* The way a run of triangles worked out here is drawn, the registers set for it: color0 the colour,
   theirs if one the state reads; the one-colour way worked out when first taken. */
static const TdfxWay *way_for(Tdfx *const c, const bool one_colour, const uint32_t colour)
{
    const bool one = one_colour && c->params & TDFX_COLOURED;
    TdfxWay *const w = &c->ways[one];

    if (one && !c->coloured) {
        way_of(c, true, w);
        c->coloured = true;
    }

    if (one) {
        const uint32_t argb = tdfx_argb(colour, c->state.alpha);

        if (argb != c->colour0) {
            tdfx_put(c, SST_COLOUR0, argb);
            c->colour0 = argb;
        }
    }

    if (c->way != w) {
        held_put(c, TDFX_HELD_PATH, w->path);
        c->way = w;
    }

    return w;
}

void tdfx_cleared(
    Tdfx *const c,
    const uint32_t buffer,
    const AccelRect r,
    const uint32_t argb,
    const uint8_t what
)
{
    const uint32_t draw = c->chip->drawn_in(c, buffer);
    const uint32_t writes = (what & ACCEL_CLEAR_COLOUR ? SST_RGB_WRITE : 0) |
                            (what & ACCEL_CLEAR_DEPTH ? SST_DEPTH | SST_AUX_WRITE : 0);

    held_put(c, TDFX_HELD_CLIP_X, (uint32_t)r.x << 16 | (uint32_t)(r.x + r.w));
    held_put(c, TDFX_HELD_CLIP_Y, (uint32_t)r.y << 16 | (uint32_t)(r.y + r.h));
    tdfx_put(c, SST_COLOUR1, argb);
    tdfx_put(c, SST_ZA_COLOUR, 0);
    held_put(c, TDFX_HELD_FBZ, SST_CLIPPED | draw << SST_DRAW_BUFFER | writes);
    tdfx_put(c, SST_FASTFILL, 0);
    c->draw = c->chip->drawn_in(c, c->buffer);
    sent(c);
}

/* A pixel of 565 as bytes R, G, B and A. */
static void spread(const uint32_t p, uint8_t *const out)
{
    const uint32_t r = p >> 11 & FIVE, g = p >> 5 & SIX, b = p & FIVE;

    out[0] = (uint8_t)(r << 3 | r >> 2);
    out[1] = (uint8_t)(g << 2 | g >> 4);
    out[2] = (uint8_t)(b << 3 | b >> 2);
    out[3] = OPAQUE;
}

uint32_t tdfx_clut(const uint32_t i)
{
    /* entry i is red and blue's 5 bits i, green's 6 bits 2i; past the last, white */
    const uint32_t five = i < TDFX_CLUT - 1 ? i << 3 | i >> 2 : OPAQUE;
    const uint32_t six = i < TDFX_CLUT - 1 ? i << 3 | i >> 3 : OPAQUE;

    return i << 24 | five << 16 | six << 8 | five;
}

void tdfx_gamma(Tdfx *const c)
{
    for (uint32_t i = 0; i < TDFX_CLUT; i++) {
        tdfx_put_init(c, SST_CLUT, tdfx_clut(i));
        tdfx_idle_fbi(c);
    }
}

static bool ACCEL_CALL
opened(const PciFunction *const pci, const uint16_t w, const uint16_t h, AccelCard *const out)
{
    Tdfx *const c = &card;
    const AccelHost *const host = c->host;

    __builtin_memset(c, 0, sizeof *c); /* a driver has no C library: mem.asm's */
    c->host = host;
    c->chip = family->chip;
    c->pci = *pci;

    if (!family->open(c, w, h))
        return false;

    c->w = w;
    c->h = h;
    c->left = (uint16_t)((c->mode_w - w) / 2);
    c->top = (uint16_t)((c->mode_h - h) / 2);
    c->memory.size = c->texture_bytes;
    c->below.size = c->rows - c->mode_h;
    c->placed = ACCEL_TEXTURES;
    c->aimed = ACCEL_SCREEN;
    c->buffer = buffer_of(c, ACCEL_SCREEN);
    c->draw = c->chip->drawn_in(c, c->buffer);
    c->clip = (AccelRect){0, 0, w, h};
    c->ox = c->left;
    c->oy = c->top;

    tdfx_put(c, SST_FOG_MODE, 0);
    tdfx_put(c, SST_COLOUR0, 0);
    tdfx_put(c, SST_TMU0 + SST_TDETAIL, 0);
    tdfx_put(c, SST_TMU1 + SST_TLOD, LOD_OFF);
    const AccelRect mode = {0, 0, (int32_t)c->mode_w, (int32_t)c->mode_h};

    tdfx_cleared(c, 0, mode, 0, ACCEL_CLEAR_COLOUR | ACCEL_CLEAR_DEPTH);
    tdfx_cleared(c, 1, mode, 0, ACCEL_CLEAR_COLOUR | ACCEL_CLEAR_DEPTH);

    *out = (AccelCard){
        .side = SIDE,
        .least = LEAST,
        .aspect = ASPECT,
        .memory = c->texture_bytes,
        .millihz = c->millihz,
    };

    return true;
}

static void ACCEL_CALL closed(void)
{
    tdfx_idle(&card);
    family->close(&card);
}

/* Texture slot made w by h, a byte a texel when indexed: false, no room. */
static bool made(const uint32_t slot, const uint16_t w, const uint16_t h, const bool indexed)
{
    Tdfx *const c = &card;
    const uint32_t texel = indexed ? 1 : TEXEL;
    const uint32_t at =
        slot < ACCEL_TEXTURES ? tdfx_take(&c->memory, (uint32_t)w * h * texel, ALIGN) : TDFX_NONE;

    if (at == TDFX_NONE)
        return false;

    c->textures[slot] = (TdfxTexture){
        .at = at,
        .w = w,
        .h = h,
        .indexed = indexed,
        .level = tdfx_level(w, h),
        .lod = tdfx_lod(w, h),
        .base = c->chip->based(c, at - tdfx_skipped(w, h, texel)),
    };

    c->placed = c->placed == slot ? ACCEL_TEXTURES : c->placed;
    return true;
}

static bool ACCEL_CALL textured(const uint32_t slot, const uint16_t w, const uint16_t h)
{
    return made(slot, w, h, false);
}

static bool ACCEL_CALL indexed(const uint32_t slot, const uint16_t w, const uint16_t h)
{
    return made(slot, w, h, true);
}

static void ACCEL_CALL
uploaded(const uint32_t slot, const AccelRect r, const uint16_t *const texels, const uint32_t pitch)
{
    Tdfx *const c = &card;

    /* the texels are placed, and their format taken, as TMU0's registers are as they come */
    const AccelState placing = {.texture = slot, .source = ACCEL_TEXEL};
    const TdfxWords w = tdfx_words(&placing, 0, c->textures[slot].indexed);

    held_put(c, TDFX_HELD_TEXTURE_MODE, w.texture_mode);
    placed(c, slot);
    c->chip->upload(c, &c->textures[slot], r, texels, pitch);

    /* the draws' texture as it was */
    if (c->state.source != ACCEL_COLOUR)
        sent(c);
}

static void ACCEL_CALL forgotten(const uint32_t slot)
{
    tdfx_give(&card.memory, card.textures[slot].at);
    card.placed = card.placed == slot ? ACCEL_TEXTURES : card.placed;
}

static bool ACCEL_CALL
targeted(const uint32_t slot, const uint16_t w, const uint16_t h, const bool depth)
{
    Tdfx *const c = &card;
    const uint32_t at =
        slot && slot < ACCEL_TARGETS && w <= c->stride ? tdfx_take(&c->below, h, 1) : TDFX_NONE;

    if (at == TDFX_NONE)
        return false;

    c->targets[slot] = (TdfxTarget){c->mode_h + at, w, h, depth};
    tdfx_cleared(
        c,
        buffer_of(c, slot),
        (AccelRect){0, (int32_t)(c->mode_h + at), w, h},
        0,
        ACCEL_CLEAR_COLOUR | (depth ? ACCEL_CLEAR_DEPTH : 0)
    );
    return true;
}

static void ACCEL_CALL dropped(const uint32_t slot)
{
    tdfx_give(&card.below, card.targets[slot].y - card.mode_h);
    card.targets[slot] = (TdfxTarget){};
}

static void ACCEL_CALL aimed(const uint32_t slot, const AccelRect clip)
{
    Tdfx *const c = &card;

    c->aimed = slot;
    c->buffer = buffer_of(c, slot);
    c->draw = c->chip->drawn_in(c, c->buffer);
    c->ox = column_of(c, slot);
    c->oy = row_of(c, slot);
    c->clip = clip;
    sent(c);
}

static void ACCEL_CALL stated(const AccelState *const s)
{
    card.state = *s;
    sent(&card);
}

/* n corners, packed in v, to the setup unit's registers, each its words at their places and a
   command: the first of three beginning a triangle, the others drawing it. */
static void cornered(Tdfx *const c, const uint32_t *const v, const uint32_t n, const bool fan)
{
    const TdfxWay *const w = c->way;
    const uint32_t words = w->corner.n;

    TdfxVertex at; /* put reads only the words it picks */

    at.word[TDFX_VDRAW] = at.word[TDFX_VBEGIN] = 0;

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t k = 0; k < words; k++)
            at.word[w->corner.at[k]] = v[i * words + k];

        c->chip->put(c, SST_CORNER, (fan ? i : i % 3) ? &w->draws : &w->begins, at.word);
    }
}

/* A draw's: its move, texels' scale, parameters and alpha's share, w's way. */
static TdfxDraw draw_of(const Tdfx *const c, const TdfxWay *const w)
{
    const bool texel = w->params >> TDFX_S & 1;
    const uint32_t level = texel ? c->textures[c->state.texture].level : 0;

    return (TdfxDraw){(float)c->ox, (float)c->oy, level, w->params, c->state.alpha};
}

/* n polygons from v, polygon i sides[i] corners round, to the setup unit: each of more than three
   one fan, the triangles BATCH a time, as many as a PKT3 takes. */
static void unit_drawn(
    Tdfx *const c,
    const AccelVertex *v,
    const uint8_t *const sides,
    const uint32_t n
)
{
    enum { BATCH = 5 };

    static_assert(ACCEL_SIDES <= 3 * BATCH); /* a PKT3's corners, 15, and packed's */

    const TdfxWay *const w = c->way;
    const TdfxDraw d = draw_of(c, w);
    const uint32_t words = w->corner.n;

    for (uint32_t i = 0; i < n;) {
        const bool fan = sides[i] > 3;

        uint32_t k = 1; /* the triangles of one packet */

        while (!fan && k < BATCH && i + k < n && sides[i + k] == 3)
            k++;

        const uint32_t m = fan ? sides[i] : 3 * k;

        uint32_t packed[3 * BATCH * TDFX_CORNER_WORDS];
        uint32_t *const to = c->chip->corners ? c->chip->corners(c, m, fan) : packed;

        if (fan)
            tdfx_corners(v, m, &d, to);

        for (uint32_t j = 0; !fan && j < k; j++)
            tdfx_corners(v + 3 * j, 3, &d, to + 3 * words * j);

        if (!c->chip->corners)
            cornered(c, packed, m, fan);

        v += m;
        i += fan ? 1 : k;
    }
}

/* Whether v's n corners are one colour as params reads it: not, none read. */
static bool one_colour(const AccelVertex *const v, const uint32_t n, const uint32_t params)
{
    const uint32_t read =
        (params & 1u << TDFX_R ? 0xFFFFFF : 0) | (params & 1u << TDFX_A ? 0xFF000000 : 0);
    const uint32_t first = v[0].colour & read;

    for (uint32_t i = 1; i < n; i++)
        if ((v[i].colour & read) != first)
            return false;

    return read;
}

/* n polygons from v worked out here, polygon i sides[i] corners round, as a fan's triangles: one
   of one colour through color0, else iterated. A quad of one plane, as a sprite, is sent its
   gradients once, as Glide's grDrawPlanarPolygon: its second triangle its corners and starts. */
static void fans_drawn(
    Tdfx *const c,
    const AccelVertex *v,
    const uint8_t *const sides,
    const uint32_t n
)
{
    TdfxDraw d = draw_of(c, c->way);
    TdfxCorner at[ACCEL_SIDES];
    TdfxTriangle t;

    for (uint32_t i = 0; i < n; v += sides[i++]) {
        const uint32_t m = sides[i];
        const TdfxWay *const w = way_for(c, one_colour(v, m, c->params), v->colour);

        d.params = w->params;
        tdfx_cornered(v, m, &d, at);

        if (m == 4 && tdfx_planar(at, d.params)) {
            if (!tdfx_triangle(&at[0], &at[1], &at[2], d.params, &t))
                continue;

            c->chip->put(c, SST_FVERTEX, &w->sent, t.word);
            tdfx_started(&at[0], &at[2], &at[3], d.params, t.turn, &t);
            c->chip->put(c, SST_FVERTEX, &w->started, t.word);
            continue;
        }

        for (uint32_t j = 1; j + 1 < m; j++)
            if (tdfx_triangle(&at[0], &at[j], &at[j + 1], d.params, &t))
                c->chip->put(c, SST_FVERTEX, &w->sent, t.word);
    }
}

/* By the setup unit, each polygon's corners: it works out the colour's gradients itself, and runs
   would break its batches. Else each polygon's triangles, a fan's, worked out here. */
static void ACCEL_CALL drawn(const AccelVertex *v, const uint8_t *const sides, const uint32_t n)
{
    Tdfx *const c = &card;

    if (c->repaletted) {
        c->repaletted = false;
        c->chip->paletted(c);
    }

    if (family->unit)
        unit_drawn(c, v, sides, n);
    else
        fans_drawn(c, v, sides, n);
}

static void ACCEL_CALL shown(const bool vsync)
{
    Tdfx *const c = &card;

    c->chip->show(c, vsync);
    tdfx_fenced(c); /* the swap sent now, not when the CPU's buffers next fill */
    c->shown ^= 1;

    /* the screen's buffer is the other now; a target's stays, its draw buffer as shown */
    c->buffer = buffer_of(c, c->aimed);
    const uint32_t draw = c->chip->drawn_in(c, c->buffer);

    if (draw != c->draw) {
        c->draw = draw;
        sent(c);
    }
}

static void ACCEL_CALL carried(void) { card.chip->carry(&card); }

/* Index 0's colour, its dropped bits unlike every other colour's: the chroma key's. */
static uint32_t keyed(const uint32_t *const colours)
{
    const uint32_t top = colours[0] & RGB & ~DROPPED;

    for (uint32_t low = DROPPED;; low = (low - 1) & DROPPED) {
        uint32_t i = 1;

        while (i < COLOURS && (colours[i] & RGB) != (top | low))
            i++;

        if (i == COLOURS || !low)
            return top | low;
    }
}

/* Palette entries from up to to sent as set, index 0's colour as the key: each in the register
   its index's low three bits pick. */
static void entries_sent(Tdfx *const c, const uint32_t from, const uint32_t to)
{
    for (uint32_t at = from; at < to;) {
        const uint32_t k = at % RUN, n = to - at < RUN - k ? to - at : RUN - k;
        uint32_t run[RUN];

        for (uint32_t j = 0; j < n; j++) {
            const uint32_t i = at + j;
            run[j] = PALETTE_WRITE | (i & ~1u) << INDEX_AT | (i ? c->palette[i] : c->key);
        }

        tdfx_put_run(c, SST_TMU0 + SST_PALETTE + k * 4, run, n);
        at += n;
    }
}

/* n entries from first set and sent; the key found again, and sent with index 0, once one of them
   took it or index 0's top bits changed. What holds texels looked up in the palette is flushed
   before the next draw. */
static void ACCEL_CALL
paletted(const uint32_t *const colours, const uint32_t first, const uint32_t n)
{
    Tdfx *const c = &card;
    const uint32_t to = first < COLOURS && n <= COLOURS - first ? first + n : COLOURS;

    bool taken = !c->keyed;

    for (uint32_t i = first; i < to; i++) {
        c->palette[i] = colours[i] & RGB;
        taken |= i ? c->palette[i] == c->key : ((c->palette[0] ^ c->key) & ~DROPPED) != 0;
    }

    const uint32_t key = taken ? keyed(c->palette) : c->key;
    const bool rekeyed = !c->keyed || key != c->key;

    c->key = key;
    c->keyed = true;
    c->repaletted = true;

    if (rekeyed && first)
        entries_sent(c, 0, 1);

    entries_sent(c, first, to);

    if (rekeyed)
        tdfx_put(c, SST_CHROMA_KEY, key);
}

static void ACCEL_CALL filled(const uint32_t argb, const uint8_t what)
{
    Tdfx *const c = &card;
    const AccelRect clip = c->clip;

    tdfx_cleared(
        c,
        c->buffer,
        (AccelRect){clip.x + (int32_t)c->ox, clip.y + (int32_t)c->oy, clip.w, clip.h},
        argb,
        what
    );
}

static void ACCEL_CALL read_back(const uint32_t slot, const AccelRect r, uint8_t *const out)
{
    Tdfx *const c = &card;
    const uint32_t top = row_of(c, slot) + (uint32_t)r.y, x0 = column_of(c, slot) + (uint32_t)r.x;
    const uint32_t x1 = x0 + (uint32_t)r.w;
    const volatile uint8_t *const pixels = c->chip->pixels(c, buffer_of(c, slot));

    tdfx_idle(c);

    for (uint32_t y = 0; y < (uint32_t)r.h; y++) {
        uint8_t *to = out + (size_t)y * (uint32_t)r.w * 4;

        for (uint32_t x = x0; x < x1;) {
            const uint32_t pair = *(const volatile uint32_t *)(pixels + (top + y) * c->lfb_row +
                                                               ((x & ~1u) << PIXEL_AT));

            for (uint32_t k = x & 1; k < 2 && x < x1; k++, x++, to += 4)
                spread(pair >> (16 * k) & 0xFFFF, to);
        }
    }
}

static AccelDriver table = {
    .size = sizeof(AccelDriver),
    .abi = ACCEL_ABI,
    .open = opened,
    .close = closed,
    .texture = textured,
    .upload = uploaded,
    .forget = forgotten,
    .target = targeted,
    .drop = dropped,
    .aim = aimed,
    .state = stated,
    .fans = drawn,
    .show = shown,
    .carry = carried,
    .read = read_back,
    .clear = filled,
    .indexed = indexed,
    .palette = paletted,
};

const AccelDriver *tdfx_driver(const AccelHost *const host, const TdfxFamily *const f)
{
    if (host->size < sizeof *host || host->abi != ACCEL_ABI)
        return nullptr;

    card.host = host;
    family = f;
    table.name = f->name;
    return &table;
}
