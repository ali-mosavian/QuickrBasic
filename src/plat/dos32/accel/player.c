/* gfx's GPU lists (qx/qxlist.h's FIXED) played through an accelerator's driver (accel.h).
 *
 * # How it works
 * A texture's texels are kept here as a card takes them (texels.h), from when they are first
 * written or drawn from: the screen's never are. The card holds them as tiles,
 * sent as they are drawn from, and the rows changed since. A texture drawn into is a target of the
 * card's too, whose pixels are then ahead of the texels: read back before it is drawn from. A
 * texture written to while a target is painted onto it from its tiles.
 *
 * The screen is the first RGBA texture made of the screen's size, drawn in the card's screen. Once a frame is
 * shown, the card's next one is undefined: carried from the shown one as it is first drawn in,
 * unless that draw covers all of it opaque.
 *
 * Triangles go to the driver in batches, sent before any other call, which might change what they
 * draw. State and aim are sent when they change.
 *
 * An INDEX texture's tiles are the driver's indexed ones, drawn through the card's palette: made
 * for the target as indices.h holds it, and sent when another than the card's. Into a target
 * holding codes, its texel alone, and a blend drawn where its alpha is at least a half.
 */
#include "plat/dos32/accel/accel.h"
#include "lib/reciprocal.h"
#include "plat/dos32/accel/indices.h"
#include "plat/dos32/accel/texels.h"
#include "plat/pmem.h"
#include "qx/qxlist.h"
#include <string.h>

enum : uint32_t { NONE = UINT32_MAX, WHITE = 0xFFFFFFFF };

enum {
    RADIX = 8,
    FAN = 1 << RADIX,
    LEVELS = 32 / RADIX - 1, /* the trie's below its root */
    BATCH = 3 * 64,
    BAND = 16, /* rows read back at once */
    RGBA = 4,
    ATTRIBUTES = 4,
    RECT = 4,
    QUAD = 4,
    TWO = 6, /* a rectangle's two triangles' corners */
    OPAQUE = 255,
    BITS = 64,
    MILLI = 1000,
    HALF = 128,
};

static_assert((int)ACCEL_COLOUR == QX_FIXED_COLOUR && (int)ACCEL_TEXEL == QX_FIXED_TEXEL);
static_assert((int)ACCEL_MASK == QX_FIXED_MASK);
static_assert((int)ACCEL_NONE == QX_BLEND_NONE && (int)ACCEL_KEEP == QX_BLEND_KEEP);
static_assert((int)ACCEL_MIX == QX_BLEND_MIX && (int)ACCEL_ADD == QX_BLEND_ADD);
static_assert(ACCEL_DEPTH_TEST == QX_DEPTH_TEST && ACCEL_DEPTH_WRITE == QX_DEPTH_WRITE);
static_assert(ACCEL_KEY == QX_FIXED_KEY && ACCEL_LINEAR == QX_FIXED_LINEAR);

/* The list's objects by their ids: a byte of an id a level, so each is found in four steps. */
typedef struct {
    void **root;
} Ids;

typedef void Each(void *object);

typedef struct {
    uint8_t *bytes;
    uint32_t n;
} Buffer;

typedef struct {
    AccelSource source;
    uint8_t flags; /* QX_FIXED_'s */
    AccelBlend blend;
    uint8_t depth;
    uint32_t stride;
    uint32_t at[ATTRIBUTES]; /* each location's offset: NONE, not given */
} Pipeline;

typedef struct {
    uint16_t *texels; /* w by h: null, none yet, every texel 0 */
    uint16_t w, h;
    bool depth; /* a depth texture: no texels */
    bool index; /* INDEX: a texel's low byte its index */
    CutGrid grid;
    uint32_t *tiles;     /* the grid's texture slots, a row across: 0, not made */
    int32_t top, bottom; /* the texel rows its tiles lack: none, top not under bottom */
    uint32_t target;     /* its target's slot, ACCEL_SCREEN the screen's: NONE, none */
    bool deep;           /* the target has a depth buffer */
    bool ahead;          /* the target's pixels are newer than the texels */
    bool blank;          /* every texel 0, as made */
    uint32_t serial;     /* a depth texture's, none other's the same */
    uint32_t held;       /* the depth texture's serial the target's depth buffer holds: 0, none */
    uint32_t used;       /* the play it was last drawn from */
    uint32_t passed;     /* the pass it was last drawn into */
} Texture;

/* A draw's pieces: its texture and state. */
typedef struct {
    const Texture *t;
    AccelState s;
} Pieces;

/* What the card's palette was made of: the target's hold, LOOKUP's words of that count (0, each
   index itself) and COLOURS' of that count (0, not read). */
typedef struct {
    IndicesHeld held;
    uint32_t lookups, colourings;
} Paletted;

static struct {
    const AccelDriver *d; /* null, none open */
    AccelCard card;
    uint16_t w, h;
    bool vsync, discard; /* the screen's QX_SCREEN_VSYNC and QX_SCREEN_DISCARD */
    Ids buffers, textures, pipelines;
    uint32_t screen; /* the texture shown: NONE, none yet */
    bool owed;       /* shown since the card's next frame was carried or drawn over */
    bool shown;      /* by this play */
    uint32_t plays, serials, passes;

    Texture *pass; /* the draws' target: null, none */
    bool pass_deep;
    AccelRect clip;

    /* what the driver was sent: the target aimed at (NONE, none) and its clip; the state */
    uint32_t aimed;
    AccelRect aimed_clip;
    AccelState sent;
    bool stated;
    AccelVertex batch[BATCH];
    uint32_t batched;

    uint64_t slots[ACCEL_TEXTURES / BITS], targets[ACCEL_TARGETS / BITS]; /* taken */
    Texture *target_of[ACCEL_TARGETS];                                    /* each target's */
    void *scratch; /* a tile's rows, or a band read back */
    size_t scratch_n;

    /* LOOKUP's and COLOURS' words, each counted as given; the card's palette and what of (sent,
       none before); the nearest index of each 565 word, of COLOURS' count memo_of */
    uint32_t lookup[INDICES], colours[INDICES], lookups, colourings;
    uint32_t palette[INDICES];
    Paletted paletted;
    bool sent_palette;
    uint16_t *memo;
    uint32_t memo_of;
} p;

static void *zeroed(const size_t bytes)
{
    void *const z = pmem_resize(nullptr, bytes);

    if (z)
        memset(z, 0, bytes);

    return z;
}

static void *id_get(const Ids *const t, const uint32_t id)
{
    void *const *at = t->root;

    for (uint32_t shift = 32 - RADIX; at && shift; shift -= RADIX)
        at = (void *const *)at[id >> shift & (FAN - 1)];

    return at ? at[id & (FAN - 1)] : nullptr;
}

/* id's object made v: false, no memory. */
static bool id_put(Ids *const t, const uint32_t id, void *const v)
{
    void **at = t->root ? t->root : (t->root = zeroed(FAN * sizeof(void *)));

    for (uint32_t shift = 32 - RADIX; at && shift; shift -= RADIX) {
        void **const next = &at[id >> shift & (FAN - 1)];
        at = *next ? *next : (*next = zeroed(FAN * sizeof(void *)));
    }

    if (at)
        at[id & (FAN - 1)] = v;

    return at;
}

/* Each object under page, level levels up from its leaves, to each; the pages freed when done. */
static void walked(void **const page, const uint32_t level, Each *const each, const bool done)
{
    for (uint32_t i = 0; page && i < FAN; i++)
        if (page[i] && level)
            walked(page[i], level - 1, each, done);
        else if (page[i])
            each(page[i]);

    if (done)
        pmem_free(page);
}

/* The first slot of bits' n not taken, taken: 0, none left. */
static uint32_t taken(uint64_t *const bits, const uint32_t n)
{
    for (uint32_t w = 0; w < n / BITS; w++) {
        const uint64_t open = ~bits[w] & (w ? ~0ull : ~1ull); /* slot 0 is none */

        if (open) {
            const uint32_t b = (uint32_t)__builtin_ctzll(open);
            bits[w] |= 1ull << b;
            return w * BITS + b;
        }
    }

    return 0;
}

static void given_back(uint64_t *const bits, const uint32_t slot)
{
    bits[slot / BITS] &= ~(1ull << slot % BITS);
}

static void *scratch(const size_t bytes)
{
    if (bytes > p.scratch_n) {
        void *const s = pmem_resize(p.scratch, bytes);

        if (!s)
            return nullptr;

        p.scratch = s;
        p.scratch_n = bytes;
    }

    return p.scratch;
}

static void flushed(void)
{
    if (p.batched)
        p.d->triangles(p.batch, p.batched);

    p.batched = 0;
}

/* The screen's next frame made the shown one's, if owed. */
static void carried(void)
{
    if (!p.owed)
        return;

    flushed();
    p.d->carry();
    p.owed = false;
}

/* t to be drawn in or read from: the screen's frame carried first. */
static void touched(const Texture *const t)
{
    if (t->target == ACCEL_SCREEN)
        carried();
}

static bool same_rect(const AccelRect a, const AccelRect b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static void aimed(const uint32_t target, const AccelRect clip)
{
    if (target == p.aimed && same_rect(clip, p.aimed_clip))
        return;

    flushed();
    p.d->aim(target, clip);
    p.aimed = target;
    p.aimed_clip = clip;
}

static bool same_state(const AccelState *const a, const AccelState *const b)
{
    return a->texture == b->texture && a->source == b->source && a->flags == b->flags &&
           a->blend == b->blend && a->alpha == b->alpha && a->depth == b->depth;
}

/* Polygon v, n corners round, drawn with state s: a fan of triangles. */
static void drawn(const AccelState *const s, const AccelVertex *const v, const uint32_t n)
{
    if (!p.stated || !same_state(s, &p.sent)) {
        flushed();
        p.d->state(s);
        p.sent = *s;
        p.stated = true;
    }

    for (uint32_t i = 1; i + 1 < n; i++) {
        if (p.batched + 3 > BATCH)
            flushed();

        p.batch[p.batched++] = v[0];
        p.batch[p.batched++] = v[i];
        p.batch[p.batched++] = v[i + 1];
    }
}

/* A quad over pixels x0 up to x1, y0 up to y1, its texels s0, t0 on, at depth 0. */
static void quad(
    AccelVertex v[QUAD],
    const float x0,
    const float y0,
    const float x1,
    const float y1,
    const float s0,
    const float t0
)
{
    const float l = x0 - 0.5f, r = x1 - 0.5f, top = y0 - 0.5f, b = y1 - 0.5f;
    const float s1 = s0 + (x1 - x0), t1 = t0 + (y1 - y0);

    v[0] = (AccelVertex){l, top, 0, 1, s0, t0, WHITE};

    v[1] = (AccelVertex){r, top, 0, 1, s1, t0, WHITE};

    v[2] = (AccelVertex){r, b, 0, 1, s1, t1, WHITE};

    v[3] = (AccelVertex){l, b, 0, 1, s0, t1, WHITE};
}

static void dirtied(Texture *const t, const int32_t top, const int32_t bottom)
{
    const bool none = t->top >= t->bottom;

    t->top = none || top < t->top ? top : t->top;
    t->bottom = none || bottom > t->bottom ? bottom : t->bottom;
}

static uint32_t tiles_of(const Texture *const t) { return (uint32_t)t->grid.across * t->grid.down; }

/* t's tiles given back. */
static void forgotten(Texture *const t)
{
    for (uint32_t i = 0; t->tiles && i < tiles_of(t); i++) {
        if (!t->tiles[i])
            continue;

        flushed();
        p.d->forget(t->tiles[i]);
        given_back(p.slots, t->tiles[i]);
        t->tiles[i] = 0;
    }
}

/* A texture not drawn from in this play: its tiles given back, for room. */
static void evicted(void *const object)
{
    Texture *const t = object;

    if (t->used != p.plays)
        forgotten(t);
}

/* A texture slot made w by h, indexed or not, with every other texture's given back if not: 0, no
   room. */
static uint32_t tile_made(const uint16_t w, const uint16_t h, const bool indexed)
{
    for (uint32_t tries = 0; tries < 2; tries++) {
        const uint32_t slot = taken(p.slots, ACCEL_TEXTURES);

        flushed();

        if (slot && (indexed ? p.d->indexed(slot, w, h) : p.d->texture(slot, w, h)))
            return slot;

        if (slot)
            given_back(p.slots, slot);

        if (!tries)
            walked(p.textures.root, LEVELS, evicted, false);
    }

    return 0;
}

/* t's texels, made 0 as first needed: null, no memory. */
static uint16_t *texels_of(Texture *const t)
{
    if (!t->texels)
        t->texels = zeroed((size_t)t->w * t->h * sizeof *t->texels);

    return t->texels;
}

/* t's tiles made and holding its texels: false, no room. */
static bool tiled(Texture *const t)
{
    const CutGrid *const g = &t->grid;

    if (!texels_of(t))
        return false;

    t->used = p.plays;

    for (uint32_t row = 0; row < g->down; row++)
        for (uint32_t column = 0; column < g->across; column++) {
            uint32_t *const slot = &t->tiles[row * g->across + column];
            const int32_t top = (int32_t)(row * g->tile_h);
            const bool made = !*slot, last = row + 1 == g->down;
            /* the texels a draw reaches, the driver's words two texels wide, or four indices */
            const uint32_t word = t->index ? TEXELS_INDEXED : 2;
            const uint32_t reached = texels_reached(t->w, column * g->tile_w, g->tile_w);
            const uint32_t wide = (reached + word - 1) & ~(word - 1);
            const int32_t tall = (int32_t)texels_reached(t->h, (uint32_t)top, g->tile_h);

            if (!made && t->top >= t->bottom)
                continue;

            if (made && !(*slot = tile_made(g->tile_w, g->tile_h, t->index)))
                return false;

            const int32_t from = t->top - top,
                          to = last && t->bottom >= t->h ? tall : t->bottom - top;
            const int32_t y0 = made || from < 0 ? 0 : from, y1 = made || to > tall ? tall : to;

            if (y0 >= y1)
                continue;

            uint16_t *const rows = scratch((size_t)wide * (uint32_t)(y1 - y0) * sizeof *rows);

            if (!rows)
                return false;

            texels_tile(
                t->texels,
                t->w,
                t->h,
                g,
                column,
                row,
                (uint32_t)y0,
                (uint32_t)y1,
                wide,
                rows
            );
            flushed();
            p.d->upload(*slot, (AccelRect){0, y0, (int32_t)wide, y1 - y0}, rows, wide);
        }

    t->top = t->bottom = 0;
    return true;
}

/* What t's target holds (indices.h): the screen's indices COLOURS' colours. */
static IndicesHeld held_by(const Texture *const t)
{
    return !t->index ? HELD_COLOURS : t->target == ACCEL_SCREEN ? HELD_SHOWN : HELD_CODES;
}

/* The nearest indices' memo, of COLOURS as they are: null, no memory. */
static uint16_t *memo(void)
{
    if (!p.memo)
        p.memo = zeroed(INDICES_CELLS * sizeof *p.memo);
    else if (p.memo_of != p.colourings)
        memset(p.memo, 0, INDICES_CELLS * sizeof *p.memo);

    p.memo_of = p.colourings;
    return p.memo;
}

/* n pixels read back from INDEX texture t's target made their indices in place, each in every
   byte: false, no memory. */
static bool as_indices(const Texture *const t, uint8_t *const rgba, const uint32_t n)
{
    const bool shown = held_by(t) == HELD_SHOWN;
    uint16_t *const m = shown ? memo() : nullptr;

    if (shown && !m)
        return false;

    for (uint32_t i = 0; i < n; i++) {
        uint8_t *const c = rgba + (size_t)i * RGBA;
        memset(c, shown ? indices_nearest(c, p.colours, m) : indices_of_code(c), RGBA);
    }

    return true;
}

/* The card's palette for INDEX texels into a target holding held, through LOOKUP or, own, each
   index itself: sent when it is another than the card's. */
static void paletted(const IndicesHeld held, const bool own)
{
    const Paletted k = {held, own ? 0 : p.lookups, held == HELD_SHOWN ? p.colourings : 0};
    uint32_t words[INDICES];

    if (p.sent_palette && k.held == p.paletted.held && k.lookups == p.paletted.lookups &&
        k.colourings == p.paletted.colourings)
        return;

    indices_palette(words, own ? nullptr : p.lookup, p.colours, held);
    p.paletted = k;

    if (p.sent_palette && !memcmp(words, p.palette, sizeof words))
        return;

    flushed();
    p.d->palette(words);
    memcpy(p.palette, words, sizeof words);
    p.sent_palette = true;
}

/* t's texels made its target's pixels, if ahead: false, no memory. */
static bool pulled(Texture *const t)
{
    if (!t->ahead)
        return true;

    uint16_t *const texels = texels_of(t);
    uint8_t *const band = scratch((size_t)t->w * BAND * RGBA);

    if (!texels || !band)
        return false;

    touched(t);
    flushed();

    for (uint32_t y = 0; y < t->h; y += BAND) {
        const uint32_t n = t->h - y < BAND ? t->h - y : BAND;
        uint16_t *const row = texels + (size_t)y * t->w;
        p.d->read(t->target, (AccelRect){0, (int32_t)y, t->w, (int32_t)n}, band);

        if (!t->index)
            texels_from_rgba(row, t->w, band, (uint32_t)t->w * RGBA, t->w, n);
        else if (!as_indices(t, band, (uint32_t)t->w * n))
            return false;
        else
            for (uint32_t i = 0; i < (uint32_t)t->w * n; i++)
                row[i] = band[(size_t)i * RGBA];
    }

    t->ahead = t->blank = false;
    dirtied(t, 0, t->h);
    return true;
}

/* r of t's target made its texels. */
static void painted(Texture *const t, const AccelRect r)
{
    const CutGrid *const g = &t->grid;

    AccelState s = {
        .source = ACCEL_TEXEL,
        .flags = t->index ? ACCEL_REPLACE : 0,
        .blend = ACCEL_NONE,
        .alpha = OPAQUE,
    };

    AccelVertex v[QUAD];

    if (!tiled(t))
        return;

    touched(t);
    aimed(t->target, r);

    if (t->index)
        paletted(held_by(t), true);

    for (uint32_t row = 0; row < g->down; row++)
        for (uint32_t column = 0; column < g->across; column++) {
            const int32_t left = (int32_t)(column * g->tile_w), top = (int32_t)(row * g->tile_h);
            const int32_t x0 = r.x > left ? r.x : left, y0 = r.y > top ? r.y : top;
            const int32_t right = left + g->tile_w, bottom = top + g->tile_h;
            const int32_t x1 = r.x + r.w < right ? r.x + r.w : right;
            const int32_t y1 = r.y + r.h < bottom ? r.y + r.h : bottom;

            if (x0 >= x1 || y0 >= y1)
                continue;

            s.texture = t->tiles[row * g->across + column];
            quad(
                v,
                (float)x0,
                (float)y0,
                (float)x1,
                (float)y1,
                (float)(x0 - left),
                (float)(y0 - top)
            );
            drawn(&s, v, QUAD);
        }
}

/* t's target's depth buffer made 0, far. */
static void depth_cleared(const Texture *const t)
{
    aimed(t->target, (AccelRect){0, 0, t->w, t->h});
    flushed();
    p.d->clear(0, ACCEL_CLEAR_DEPTH);
}

/* t's target given back; the screen's is kept. */
static void dropped(Texture *const t)
{
    if (t->target == NONE || t->target == ACCEL_SCREEN)
        return;

    flushed();
    p.d->drop(t->target);
    given_back(p.targets, t->target);
    p.target_of[t->target] = nullptr;
    p.aimed = p.aimed == t->target ? NONE : p.aimed;
    t->target = NONE;
}

/* The target drawn into longest ago given back, its pixels pulled into its texels: false, none,
   or no memory. */
static bool target_evicted(void)
{
    Texture *oldest = nullptr;

    for (uint32_t i = 1; i < ACCEL_TARGETS; i++) {
        Texture *const t = p.target_of[i];

        if (t && (!oldest || t->passed < oldest->passed))
            oldest = t;
    }

    if (!oldest || !pulled(oldest))
        return false;

    dropped(oldest);
    return true;
}

/* t a target, with a depth buffer when deep, its pixels its texels, the oldest others given back
   for room: false, no room. */
static bool targeted(Texture *const t, const bool deep)
{
    if (t->target != NONE && (t->deep || !deep))
        return true;

    if (t->target != NONE && !pulled(t))
        return false;

    dropped(t);

    uint32_t slot;

    for (;;) {
        slot = taken(p.targets, ACCEL_TARGETS);
        flushed();

        if (slot && p.d->target(slot, t->w, t->h, deep))
            break;

        if (slot)
            given_back(p.targets, slot);

        if (!target_evicted())
            return false;
    }

    p.target_of[slot] = t;
    t->target = slot;
    t->deep = deep;
    t->held = 0;

    if (!t->blank)
        painted(t, (AccelRect){0, 0, t->w, t->h});

    return true;
}

/* t, the texture shown, drawn in the card's screen from now on. */
static void bound(Texture *const t)
{
    if (t->target == ACCEL_SCREEN || t->w != p.w || t->h != p.h)
        return;

    if (!pulled(t))
        return;

    dropped(t);
    t->target = ACCEL_SCREEN;
    t->deep = true;
    t->held = 0;

    if (!t->blank)
        painted(t, (AccelRect){0, 0, t->w, t->h});
}

static void discarded(void *const object)
{
    Texture *const t = object;

    pmem_free(t->texels);
    pmem_free(t->tiles);
    pmem_free(t);
}

static void buffer_discarded(void *const object)
{
    Buffer *const b = object;

    pmem_free(b->bytes);
    pmem_free(b);
}

static void texture_freed(const uint32_t id)
{
    Texture *const t = id_get(&p.textures, id);

    if (!t)
        return;

    forgotten(t);
    dropped(t);
    p.pass = p.pass == t ? nullptr : p.pass;
    discarded(t);
    id_put(&p.textures, id, nullptr);
}

/* The draw's index at i; its vertices bound-checked as fetched. */
static uint32_t index_at(const Buffer *const ib, const uint32_t i)
{
    uint32_t v;

    memcpy(&v, ib->bytes + (size_t)i * sizeof v, sizeof v);
    return v;
}

/* The draw's vertex at index into *v, its wrap's rectangle to r: false, past the buffer, or its
   w not above 0. A w of 1, a 2D draw's, is not divided. */
static bool fetched(
    const Pipeline *const pl,
    const Buffer *const vb,
    const uint32_t index,
    AccelVertex *const v,
    uint16_t r[RECT]
)
{
    const uint64_t at = (uint64_t)index * pl->stride;

    float xyzw[4], st[2] = {0, 0};

    uint8_t c[4] = {OPAQUE, OPAQUE, OPAQUE, OPAQUE};

    if (at + pl->stride > vb->n)
        return false;

    const uint8_t *const b = vb->bytes + at;

    memcpy(xyzw, b + pl->at[QX_FIXED_AT_POSITION], sizeof xyzw);
    memset(r, 0, RECT * sizeof *r);

    if (pl->at[QX_FIXED_AT_TEXEL] != NONE)
        memcpy(st, b + pl->at[QX_FIXED_AT_TEXEL], sizeof st);

    if (pl->at[QX_FIXED_AT_COLOUR] != NONE)
        memcpy(c, b + pl->at[QX_FIXED_AT_COLOUR], sizeof c);

    if (pl->at[QX_FIXED_AT_RECT] != NONE)
        memcpy(r, b + pl->at[QX_FIXED_AT_RECT], RECT * sizeof *r);

    if (!(xyzw[3] > 0))
        return false;

    const uint32_t argb = (uint32_t)c[3] << 24 | (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];

    *v = (AccelVertex){xyzw[0], xyzw[1], xyzw[2], reciprocal(xyzw[3]), st[0], st[1], argb};

    return true;
}

/* The draw's first two triangles are a rectangle over all of the pass, drawn opaque. */
static bool covers(
    const Pipeline *const pl,
    const Buffer *const vb,
    const Buffer *const ib,
    const uint32_t first,
    const uint32_t count
)
{
    enum { CORNERS = 4 };

    const Texture *const into = p.pass;

    AccelVertex v[TWO];
    uint16_t r[RECT];
    uint32_t omitted[2];

    if (count < TWO || pl->blend != ACCEL_NONE || pl->flags & QX_FIXED_KEY ||
        pl->depth & ACCEL_DEPTH_TEST || !same_rect(p.clip, (AccelRect){0, 0, into->w, into->h}))
        return false;

    for (uint32_t i = 0; i < TWO; i++)
        if (!fetched(pl, vb, index_at(ib, first + i), &v[i], r))
            return false;

    float x0 = v[0].x, x1 = x0, y0 = v[0].y, y1 = y0;

    for (uint32_t i = 1; i < TWO; i++) {
        x0 = v[i].x < x0 ? v[i].x : x0;
        x1 = v[i].x > x1 ? v[i].x : x1;
        y0 = v[i].y < y0 ? v[i].y : y0;
        y1 = v[i].y > y1 ? v[i].y : y1;
    }

    if (x0 > -0.5f || y0 > -0.5f || x1 < into->w - 0.5f || y1 < into->h - 0.5f)
        return false;

    /* each the rectangle's corners but one, those left out opposite */
    for (uint32_t k = 0; k < 2; k++) {
        uint32_t seen = 0;

        for (uint32_t i = 3 * k; i < 3 * k + 3; i++) {
            const bool right = v[i].x == x1, bottom = v[i].y == y1;

            if ((!right && v[i].x != x0) || (!bottom && v[i].y != y0))
                return false;

            seen |= 1u << (right | bottom << 1);
        }

        if (__builtin_popcount(seen) != 3)
            return false;

        omitted[k] = (uint32_t)__builtin_ctz(~seen & ((1u << CORNERS) - 1));
    }

    return (omitted[0] ^ omitted[1]) == 3;
}

/* The colour of a draw that is one solid rectangle over all of its target, written alone, to *argb:
   false, it is not one. */
static bool filled(
    const Pipeline *const pl,
    const Buffer *const vb,
    const Buffer *const ib,
    const uint32_t first,
    const uint32_t count,
    uint32_t *const argb
)
{
    AccelVertex v;
    uint16_t r[RECT];

    if (count != TWO || pl->source != ACCEL_COLOUR || pl->depth ||
        !covers(pl, vb, ib, first, count))
        return false;

    for (uint32_t i = 0; i < TWO; i++) {
        fetched(pl, vb, index_at(ib, first + i), &v, r);

        if (i && v.colour != *argb)
            return false;

        *argb = v.colour;
    }

    *argb = indices_colour(*argb, p.colours, held_by(p.pass));
    return true;
}

/* t's grid for a draw of flags, its wrap rectangle r: the card's own wrap where it can, the
   card's flags to *flags. */
static CutGrid gridded(
    const Texture *const t,
    const uint8_t flags,
    const uint16_t r[RECT],
    uint8_t *out
)
{
    const AccelRect whole = {0, 0, t->w, t->h};

    const AccelRect wrap = r[2] && r[3] ? (AccelRect){r[0], r[1], r[2], r[3]} : whole;

    const bool on_s = flags & QX_FIXED_WRAP_U, on_t = flags & QX_FIXED_WRAP_V;

    CutGrid g = t->grid;

    const bool card_s = on_s && g.across == 1 && t->w == g.tile_w && !wrap.x && wrap.w == t->w;
    const bool card_t = on_t && g.down == 1 && t->h == g.tile_h && !wrap.y && wrap.h == t->h;

    g.wrap = wrap;
    g.wrap_s = on_s && !card_s;
    g.wrap_t = on_t && !card_t;
    *out = (uint8_t)((flags & (ACCEL_KEY | ACCEL_LINEAR)) | (card_s ? ACCEL_WRAP_S : 0) |
                     (card_t ? ACCEL_WRAP_T : 0));
    return g;
}

static void piece(
    void *const ctx,
    const uint32_t column,
    const uint32_t row,
    const AccelVertex *const v,
    const uint32_t n
)
{
    Pieces *const c = ctx;

    c->s.texture = c->t->tiles[row * c->t->grid.across + column];
    drawn(&c->s, v, n);
}

static void op_buffer(const QxListEntry *const e)
{
    const uint32_t *const a = e->arg;

    Buffer *b = id_get(&p.buffers, a[0]);

    if (!b && (b = zeroed(sizeof *b)) && !id_put(&p.buffers, a[0], b)) {
        pmem_free(b);
        return;
    }

    uint8_t *const bytes = b && b->n < a[1] ? pmem_resize(b->bytes, a[1]) : nullptr;

    if (bytes) {
        b->bytes = bytes;
        b->n = a[1];
    }
}

static void op_write(const QxListEntry *const e)
{
    const uint32_t *const a = e->arg;
    Buffer *const b = id_get(&p.buffers, a[0]);

    if (b && a[1] && a[1] <= b->n)
        memcpy(b->bytes, qx_list_pointer(a + 2), a[1]);
}

static void op_texture(const QxListEntry *const e)
{
    const uint32_t *const a = e->arg;
    const uint32_t id = a[0], w = a[1], h = a[2];
    const bool depth = a[3] == QX_TEXTURE_DEPTH, index = a[3] == QX_TEXTURE_INDEX;

    texture_freed(id);

    if (!w || !h || w > UINT16_MAX || h > UINT16_MAX ||
        (!depth && !index && a[3] != QX_TEXTURE_RGBA))
        return;

    Texture *const t = zeroed(sizeof *t);

    if (!t)
        return;

    *t = (Texture){
        .w = (uint16_t)w,
        .h = (uint16_t)h,
        .depth = depth,
        .index = index,
        .target = NONE,
        .blank = true,
    };

    if (depth) {
        t->serial = ++p.serials;
    } else {
        t->grid = texels_grid(t->w, t->h, index, &p.card);
        t->tiles = zeroed(tiles_of(t) * sizeof *t->tiles);
        dirtied(t, 0, t->h);
    }

    if ((!depth && !t->tiles) || !id_put(&p.textures, id, t)) {
        discarded(t);
        return;
    }

    if (p.screen == NONE && !depth && w == p.w && h == p.h)
        p.screen = id;

    if (id == p.screen)
        bound(t);
}

static void op_free(const QxListEntry *const e) { texture_freed(e->arg[0]); }

static void op_upload(const QxListEntry *const e)
{
    const uint32_t *const a = e->arg;
    const uint32_t x = a[1], y = a[2], w = a[3], h = a[4];
    Texture *const t = id_get(&p.textures, a[0]);

    uint16_t *const texels =
        t && !t->depth && w && h && x <= t->w && w <= t->w - x && y <= t->h && h <= t->h - y
            ? texels_of(t)
            : nullptr;

    if (!texels)
        return;

    uint16_t *const to = texels + (size_t)y * t->w + x;

    if (t->index)
        texels_from_indices(to, t->w, qx_list_pointer(a + 6), a[5], w, h);
    else
        texels_from_rgba(to, t->w, qx_list_pointer(a + 6), a[5], w, h);

    t->blank = false;
    dirtied(t, (int32_t)y, (int32_t)(y + h));

    if (t->target != NONE)
        painted(t, (AccelRect){(int32_t)x, (int32_t)y, (int32_t)w, (int32_t)h});
}

static void op_pipeline(const QxListEntry *const e)
{
    static const uint8_t formats[ATTRIBUTES] = {
        QX_ATTRIBUTE_FLOAT4,
        QX_ATTRIBUTE_FLOAT2,
        QX_ATTRIBUTE_UBYTE4,
        QX_ATTRIBUTE_USHORT4,
    };

    static const uint8_t bytes[ATTRIBUTES] = {16, 8, 4, 8};

    const uint32_t *const a = e->arg;
    const uint32_t attributes = (e->args - QX_PIPELINE_ATTRIBUTES) / QX_ATTRIBUTE_WORDS;

    Pipeline pl = {
        .source = (AccelSource)a[QX_FIXED_SOURCE],
        .flags = (uint8_t)a[QX_FIXED_FLAGS],
        .blend = (AccelBlend)(a[1] & 0xFF),
        .depth = (uint8_t)(a[1] >> 8),
        .stride = a[2],
        .at = {NONE, NONE, NONE, NONE},
    };

    if (pl.source < ACCEL_COLOUR || pl.source > ACCEL_MASK || pl.blend > ACCEL_KEEP)
        return;

    for (uint32_t i = 0; i < attributes; i++) {
        const uint32_t *const at = a + QX_PIPELINE_ATTRIBUTES + i * QX_ATTRIBUTE_WORDS;
        const uint32_t location = at[0];

        if (location >= ATTRIBUTES || at[1] != formats[location] || at[2] > pl.stride ||
            bytes[location] > pl.stride - at[2])
            return;

        pl.at[location] = at[2];
    }

    Pipeline *had = id_get(&p.pipelines, a[0]);

    if (pl.at[QX_FIXED_AT_POSITION] == NONE)
        return;

    if (!had && (had = zeroed(sizeof *had)) && !id_put(&p.pipelines, a[0], had)) {
        pmem_free(had);
        return;
    }

    if (had)
        *had = pl;
}

static void op_pass(const QxListEntry *const e)
{
    const uint32_t *const a = e->arg;
    Texture *const t = id_get(&p.textures, a[0]);
    const Texture *const depth = t && e->args > 1 && a[1] ? id_get(&p.textures, a[1]) : nullptr;
    const bool deep = depth && depth->depth && depth->w == t->w && depth->h == t->h;

    p.pass = nullptr;

    if (!t || t->depth || !targeted(t, deep))
        return;

    t->passed = ++p.passes;

    if (deep && t->held != depth->serial) {
        depth_cleared(t);
        t->held = depth->serial;
    }

    p.pass = t;
    p.pass_deep = deep;
    p.clip = (AccelRect){0, 0, t->w, t->h};
}

static void op_clip(const QxListEntry *const e)
{
    const int32_t *const a = (const int32_t *)e->arg;

    if (!p.pass)
        return;

    const int32_t x0 = a[0] > 0 ? a[0] : 0, y0 = a[1] > 0 ? a[1] : 0;
    const int32_t x1 = a[0] + a[2] < p.pass->w ? a[0] + a[2] : p.pass->w;
    const int32_t y1 = a[1] + a[3] < p.pass->h ? a[1] + a[3] : p.pass->h;

    p.clip = (AccelRect){x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0};
}

/* The draw's count indices from first, triangles of pipeline pl from texture t at alpha, into the
   pass holding held, blending as blend. An INDEX texel's alone unless blended, LINEAR not across
   a key a filter would mix; a colour as the pass holds it. */
static void triangles_drawn(
    const Pipeline *const pl,
    const Buffer *const vb,
    const Buffer *const ib,
    const uint32_t first,
    const uint32_t count,
    const Texture *const t,
    const uint8_t alpha,
    const AccelBlend blend
)
{
    const IndicesHeld held = held_by(p.pass);
    const bool index = t && t->index, texel = pl->source == ACCEL_TEXEL;
    const bool alone = index && texel && (blend == ACCEL_NONE || blend == ACCEL_KEEP);
    const uint8_t cut = index && pl->flags & QX_FIXED_KEY ? ACCEL_LINEAR : 0;

    Pieces c = {t, {.source = pl->source, .blend = blend, .alpha = alpha, .depth = pl->depth}};

    if (index && (texel || !p.sent_palette))
        paletted(held, false);

    for (uint32_t i = 0; i + 3 <= count; i += 3) {
        AccelVertex v[3];
        uint16_t r[3][RECT];

        if (!fetched(pl, vb, index_at(ib, first + i), &v[0], r[0]) ||
            !fetched(pl, vb, index_at(ib, first + i + 1), &v[1], r[1]) ||
            !fetched(pl, vb, index_at(ib, first + i + 2), &v[2], r[2]))
            continue;

        for (uint32_t k = 0; k < 3 && !texel; k++)
            v[k].colour = indices_colour(v[k].colour, p.colours, held);

        if (!t) {
            drawn(&c.s, v, 3);
            continue;
        }

        const CutGrid g = gridded(t, (uint8_t)(pl->flags & ~cut), r[0], &c.s.flags);
        c.s.flags |= alone ? ACCEL_REPLACE : 0;
        cut_triangle(&g, v, piece, &c);
    }
}

static void op_draw(const QxListEntry *const e)
{
    const uint32_t *const a = e->arg;
    const Pipeline *const pl = id_get(&p.pipelines, a[0]);
    const Buffer *const vb = id_get(&p.buffers, a[1]), *const ib = id_get(&p.buffers, a[2]);
    const uint32_t first = a[3], count = a[QX_DRAW_COUNT], alpha = a[5];
    const bool texel = pl && pl->source != ACCEL_COLOUR;
    Texture *const into = p.pass;
    Texture *const t =
        texel && e->args > QX_DRAW_TEXTURES ? id_get(&p.textures, a[QX_DRAW_TEXTURES]) : nullptr;

    if (!pl || !vb || !ib || !into || (pl->depth && !p.pass_deep) || !p.clip.w || !p.clip.h ||
        ((uint64_t)first + count) * sizeof(uint32_t) > ib->n)
        return;

    /* into codes a blend is drawn where its alpha is at least a half, as itself */
    const bool coded = held_by(into) == HELD_CODES,
               blended = pl->blend == ACCEL_MIX || pl->blend == ACCEL_ADD;
    const AccelBlend blend = coded && blended ? ACCEL_NONE : pl->blend;

    if ((coded && blended && alpha < HALF) ||
        (texel && (!t || t->depth || !pulled(t) || !tiled(t))))
        return;

    if (pl->blend != ACCEL_KEEP && into->target == ACCEL_SCREEN && p.owed) {
        if (covers(pl, vb, ib, first, count))
            p.owed = false;
        else
            carried();
    }

    aimed(into->target, p.clip);

    uint32_t argb;

    if (!texel && filled(pl, vb, ib, first, count, &argb)) {
        flushed();
        p.d->clear(argb, ACCEL_CLEAR_COLOUR);
    } else
        triangles_drawn(pl, vb, ib, first, count, t, (uint8_t)alpha, blend);

    if (pl->blend != ACCEL_KEEP) {
        into->ahead = true;
        into->blank = false;
    }
}

static void op_show(const QxListEntry *const e)
{
    const Texture *const t = id_get(&p.textures, e->arg[0]);

    if (!t || t->target != ACCEL_SCREEN)
        return;

    carried(); /* shown again undrawn: the next frame, undefined till carried */
    flushed();
    p.d->show(p.vsync);
    p.owed = !p.discard;
    p.shown = true;
}

/* 256 words from a pointer's into to. */
static void words_taken(uint32_t *const to, const uint32_t *const a)
{
    memcpy(to, qx_list_pointer(a), INDICES * sizeof *to);
}

static void op_lookup(const QxListEntry *const e)
{
    words_taken(p.lookup, e->arg);
    p.lookups++;
}

static void op_colours(const QxListEntry *const e)
{
    words_taken(p.colours, e->arg);
    p.colourings++;
}

typedef void Op(const QxListEntry *e);

/* Each op's, and its arguments at least; UNIFORMS, none: a fixed pipeline reads none. */
static const struct {
    Op *run;
    uint32_t args;
} OPS[] = {
    [QX_LIST_BUFFER] = {op_buffer, 3},
    [QX_LIST_WRITE] = {op_write, 4},
    [QX_LIST_TEXTURE] = {op_texture, 4},
    [QX_LIST_FREE] = {op_free, 1},
    [QX_LIST_UPLOAD] = {op_upload, 8},
    [QX_LIST_PIPELINE] = {op_pipeline, QX_PIPELINE_ATTRIBUTES},
    [QX_LIST_PASS] = {op_pass, 1},
    [QX_LIST_CLIP] = {op_clip, 4},
    [QX_LIST_DRAW] = {op_draw, QX_DRAW_TEXTURES},
    [QX_LIST_SHOW] = {op_show, 1},
    [QX_LIST_LOOKUP] = {op_lookup, 2},
    [QX_LIST_COLOURS] = {op_colours, 2},
};

bool accel_open(
    const AccelDriver *const d,
    const PciFunction *const card,
    const uint16_t w,
    const uint16_t h,
    const uint8_t flags
)
{
    AccelCard c = {};

    accel_close();

    if (!d->open(card, w, h, &c))
        return false;

    p.d = d;
    p.card = c;
    p.w = w;
    p.h = h;
    p.vsync = flags & QX_SCREEN_VSYNC;
    p.discard = flags & QX_SCREEN_DISCARD;
    p.screen = p.aimed = NONE;
    p.lookups = p.colourings = 1;

    for (uint32_t i = 0; i < INDICES; i++)
        p.lookup[i] = i;

    return true;
}

void accel_close(void)
{
    if (!p.d)
        return;

    flushed();
    p.d->close();
    walked(p.textures.root, LEVELS, discarded, true);
    walked(p.buffers.root, LEVELS, buffer_discarded, true);
    walked(p.pipelines.root, LEVELS, pmem_free, true);
    pmem_free(p.scratch);
    pmem_free(p.memo);
    memset(&p, 0, sizeof p);
}

bool accel_play(const uint32_t *const list, const uint32_t words)
{
    QxListEntry e;
    uint32_t at = 0;

    if (!p.d)
        return false;

    p.plays++;
    p.shown = false;

    while (qx_list_next(list, words, &at, &e) && e.op != QX_LIST_END)
        if (e.op < sizeof OPS / sizeof *OPS && OPS[e.op].run && e.args >= OPS[e.op].args)
            OPS[e.op].run(&e);

    flushed();
    return p.shown;
}

void accel_read(
    const uint32_t texture,
    const int32_t x,
    const int32_t y,
    const int32_t w,
    const int32_t h,
    uint8_t *const out
)
{
    const Texture *const t = p.d ? id_get(&p.textures, texture) : nullptr;

    if (!t || t->depth || x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > t->w || y + h > t->h)
        return;

    if (!t->ahead && !t->texels) {
        memset(out, 0, (size_t)w * (uint32_t)h * RGBA);
        return;
    }

    if (!t->ahead) {
        const uint16_t *const from = t->texels + (size_t)y * t->w + x;

        if (t->index)
            texels_to_indices(out, from, t->w, (uint32_t)w, (uint32_t)h);
        else
            texels_to_rgba(out, from, t->w, (uint32_t)w, (uint32_t)h);

        return;
    }

    touched(t);
    flushed();
    p.d->read(t->target, (AccelRect){x, y, w, h}, out);

    if (t->index && !as_indices(t, out, (uint32_t)w * (uint32_t)h))
        memset(out, 0, (size_t)w * (uint32_t)h * RGBA);
}

float accel_hz(void) { return p.d ? (float)p.card.millihz / MILLI : 0; }
