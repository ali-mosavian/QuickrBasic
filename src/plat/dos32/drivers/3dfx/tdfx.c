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
};

static Tdfx card;
static const TdfxFamily *family;

void tdfx_store(Tdfx *const c, volatile uint8_t *const at, const uint32_t v)
{
    while (!c->room)
        c->room = tdfx_get(c, SST_STATUS) & c->chip->room;

    c->room--;
    *(volatile uint32_t *)at = v;
}

void tdfx_put(Tdfx *const c, const uint32_t reg, const uint32_t v)
{
    tdfx_store(c, c->regs + reg, v);
}

void tdfx_put_init(const Tdfx *const c, const uint32_t reg, const uint32_t v)
{
    *(volatile uint32_t *)(c->regs + reg) = v;
}

void tdfx_put_float(Tdfx *const c, const uint32_t reg, const float v)
{
    const union {
        float f;
        uint32_t bits;
    } u = {.f = v};

    tdfx_put(c, reg, u.bits);
}

uint32_t tdfx_get(const Tdfx *const c, const uint32_t reg)
{
    return *(const volatile uint32_t *)(c->regs + reg);
}

void tdfx_idle(Tdfx *const c)
{
    const uint64_t until = c->host->clock_us() + TDFX_WAIT_US;

    uint32_t calm = 0;

    tdfx_put(c, SST_NOP, 0);

    while (calm < CALM && c->host->clock_us() < until)
        calm = tdfx_get(c, SST_STATUS) & SST_BUSY ? 0 : calm + 1;

    c->room = 0;
}

void tdfx_idle_fbi(const Tdfx *const c)
{
    const uint64_t until = c->host->clock_us() + TDFX_WAIT_US;

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
static void placed(Tdfx *const c, const uint32_t slot)
{
    const TdfxTexture *const t = &c->textures[slot];

    if (c->placed == slot)
        return;

    tdfx_put(c, SST_TMU0 + SST_TLOD, t->lod);
    tdfx_put(c, SST_TMU0 + SST_TEX_BASE, t->base);
    c->placed = slot;
}

/* The state's words and the aim's clip sent. */
static void sent(Tdfx *const c)
{
    const AccelState *const s = &c->state;
    const TdfxWords w = tdfx_words(s, c->draw);
    const uint32_t x0 = (uint32_t)c->clip.x + c->ox, y0 = (uint32_t)c->clip.y + c->oy;

    tdfx_put(c, SST_CLIP_X, x0 << 16 | (x0 + (uint32_t)c->clip.w));
    tdfx_put(c, SST_CLIP_Y, y0 << 16 | (y0 + (uint32_t)c->clip.h));
    tdfx_put(c, SST_FBZ_COLOUR_PATH, w.colour_path);
    tdfx_put(c, SST_ALPHA_MODE, w.alpha_mode);
    tdfx_put(c, SST_FBZ_MODE, w.fbz_mode);

    if (s->source != ACCEL_COLOUR && s->texture && s->texture < ACCEL_TEXTURES) {
        tdfx_put(c, SST_TMU0 + SST_TEXTURE_MODE, w.texture_mode);
        placed(c, s->texture);
    }
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

    tdfx_put(c, SST_CLIP_X, (uint32_t)r.x << 16 | (uint32_t)(r.x + r.w));
    tdfx_put(c, SST_CLIP_Y, (uint32_t)r.y << 16 | (uint32_t)(r.y + r.h));
    tdfx_put(c, SST_COLOUR1, argb);
    tdfx_put(c, SST_ZA_COLOUR, 0);
    tdfx_put(c, SST_FBZ_MODE, SST_CLIPPED | draw << SST_DRAW_BUFFER | writes);
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

    __builtin_memset(c, 0, sizeof *c); /* a driver has no C library: crt.c's */
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

static bool ACCEL_CALL textured(const uint32_t slot, const uint16_t w, const uint16_t h)
{
    Tdfx *const c = &card;
    const uint32_t at =
        slot < ACCEL_TEXTURES ? tdfx_take(&c->memory, (uint32_t)w * h * TEXEL, ALIGN) : TDFX_NONE;

    if (at == TDFX_NONE)
        return false;

    c->textures[slot] = (TdfxTexture){
        .at = at,
        .w = w,
        .h = h,
        .level = tdfx_level(w, h),
        .lod = tdfx_lod(w, h),
        .base = c->chip->based(c, at - tdfx_skipped(w, h)),
    };

    c->placed = c->placed == slot ? ACCEL_TEXTURES : c->placed;
    return true;
}

static void ACCEL_CALL
uploaded(const uint32_t slot, const AccelRect r, const uint16_t *const texels, const uint32_t pitch)
{
    Tdfx *const c = &card;

    /* the texels are placed as TMU0's registers are as they come */
    tdfx_put(c, SST_TMU0 + SST_TEXTURE_MODE, SST_ARGB1555 << SST_FORMAT | SST_LOCAL);
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

static void ACCEL_CALL drawn(const AccelVertex *const v, const uint32_t n)
{
    Tdfx *const c = &card;
    const bool texel = c->state.source != ACCEL_COLOUR && c->state.texture;
    const uint32_t params = texel ? TDFX_PARAMS : TDFX_S;
    const float scale = texel ? (float)(1u << c->textures[c->state.texture].level) : 1;

    for (uint32_t i = 0; i + 3 <= n; i += 3) {
        TdfxTriangle t;

        if (!tdfx_setup(v + i, (float)c->ox, (float)c->oy, scale, c->state.alpha, &t))
            continue;

        for (uint32_t k = 0; k < 3; k++) {
            tdfx_put_float(c, SST_FVERTEX + 8 * k, t.x[k]);
            tdfx_put_float(c, SST_FVERTEX + 8 * k + 4, t.y[k]);
        }

        for (uint32_t p = 0; p < params; p++) {
            tdfx_put_float(c, SST_FSTART + 4 * p, t.start[p]);
            tdfx_put_float(c, SST_FDX + 4 * p, t.dx[p]);
            tdfx_put_float(c, SST_FDY + 4 * p, t.dy[p]);
        }

        tdfx_put(c, SST_FTRIANGLE, t.command);
    }
}

static void ACCEL_CALL shown(const bool vsync)
{
    Tdfx *const c = &card;

    c->chip->show(c, vsync);
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
    .triangles = drawn,
    .show = shown,
    .carry = carried,
    .read = read_back,
    .clear = filled,
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
