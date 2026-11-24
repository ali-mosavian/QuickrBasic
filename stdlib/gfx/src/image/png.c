/* PNG to pixels (image.h). */
#include "image/image.h"
#include "image/inflate.h"
#include "core/mem.h"

enum {
    SIGNATURE = 8,
    HEAD = 13,
    MOST = 1 << 14, /* pixels a side */
    INDEXED = 3,
    KEY = 0x80, /* alpha under it is keyed */
    NO_KEY = -1,
    KEYS = -2, /* more than one */
};

/* IHDR's, and the chunks' that colour it. */
typedef struct {
    uint32_t w, h;
    uint8_t depth, type, channels;
    const uint8_t *palette, *trns; /* PLTE's and tRNS's bytes */
    uint32_t colours, ntrns;
} Png;

static uint32_t be16(const uint8_t *const p) { return (uint32_t)p[0] << 8 | p[1]; }

static uint32_t be32(const uint8_t *const p) { return be16(p) << 16 | be16(p + 2); }

static bool is(const uint8_t *const chunk, const char *const type)
{
    return chunk[4] == type[0] && chunk[5] == type[1] && chunk[6] == type[2] && chunk[7] == type[3];
}

/* The chunk at, its data's length within n: null, none or cut short. */
static const uint8_t *chunk(
    const uint8_t *const data,
    const uint32_t n,
    const uint32_t at,
    uint32_t *const len
)
{
    if (at > n || n - at < 12)
        return nullptr;

    *len = be32(data + at);
    return *len <= n - at - 12 ? data + at : nullptr;
}

/* The header and the colours: false, not one this reads (interlaced, an unknown type). */
static bool header(const uint8_t *const data, const uint32_t n, Png *const p, uint32_t *const idat)
{
    static const uint8_t magic[SIGNATURE] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

    uint32_t len;
    const uint8_t *c = chunk(data, n, SIGNATURE, &len);

    if (!c)
        return false;

    for (uint32_t i = 0; i < SIGNATURE; i++)
        if (data[i] != magic[i])
            return false;

    if (!is(c, "IHDR") || len != HEAD)
        return false;

    const uint8_t *const h = c + 8;
    static const uint8_t channels[] = {1, 0, 3, 1, 2, 0, 4};

    *p = (Png){
        .w = be32(h),
        .h = be32(h + 4),
        .depth = h[8],
        .type = h[9],
    };

    if (p->type > 6 || !channels[p->type] || h[10] || h[11] || h[12] || !p->w || !p->h ||
        p->w > MOST || p->h > MOST)
        return false;

    p->channels = channels[p->type];
    const uint8_t d = p->depth;
    const bool depth_ok = p->type == 0         ? d == 1 || d == 2 || d == 4 || d == 8 || d == 16
                          : p->type == INDEXED ? d == 1 || d == 2 || d == 4 || d == 8
                                               : d == 8 || d == 16;

    if (!depth_ok)
        return false;

    *idat = 0;

    for (uint32_t at = SIGNATURE + 12 + HEAD; (c = chunk(data, n, at, &len)); at += 12 + len) {
        if (is(c, "PLTE")) {
            p->palette = c + 8;
            p->colours = len / 3;
        } else if (is(c, "tRNS")) {
            p->trns = c + 8;
            p->ntrns = len;
        } else if (is(c, "IDAT"))
            *idat += len;
        else if (is(c, "IEND"))
            break;
    }

    return *idat && (p->type != INDEXED || (p->palette && p->colours <= 256));
}

/* The image's IDAT chunks before IEND, bytes of them, joined: null, no memory. */
static uint8_t *joined(const uint8_t *const data, const uint32_t n, const uint32_t bytes)
{
    uint8_t *const out = mem_alloc(bytes);
    uint32_t len, made = 0;
    const uint8_t *c;

    for (uint32_t at = SIGNATURE; out && (c = chunk(data, n, at, &len)) && !is(c, "IEND");

         at += 12 + len)
        if (is(c, "IDAT")) {
            mem_copy(out + made, c + 8, len);
            made += len;
        }

    return out;
}

static uint8_t paeth(const uint8_t a, const uint8_t b, const uint8_t c)
{
    const int32_t p = a + b - c;
    const int32_t pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p,
                  pc = p > c ? p - c : c - p;
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

/* The rows' filters undone in place: each row its filter's byte, then row bytes; bpp the bytes
   a pixel, at least 1. false: a filter unknown. */
static bool unfilter(uint8_t *const raw, const uint32_t h, const uint32_t row, const uint32_t bpp)
{
    for (uint32_t y = 0; y < h; y++) {
        uint8_t *const r = raw + y * (row + 1) + 1;
        const uint8_t *const up = y ? r - (row + 1) : nullptr;
        const uint8_t filter = r[-1];

        if (filter > 4)
            return false;

        for (uint32_t i = 0; i < row; i++) {
            const uint8_t a = i >= bpp ? r[i - bpp] : 0, b = up ? up[i] : 0;
            const uint8_t c = up && i >= bpp ? up[i - bpp] : 0;
            const uint8_t add = filter == 1   ? a
                                : filter == 2 ? b
                                : filter == 3 ? (uint8_t)((a + b) / 2)
                                : filter == 4 ? paeth(a, b, c)
                                              : 0;
            r[i] = (uint8_t)(r[i] + add);
        }
    }

    return true;
}

/* Sample i of a row of depth bits, as it is. */
static uint32_t sample(const uint8_t *const r, const uint32_t i, const uint8_t depth)
{
    if (depth == 16)
        return (uint32_t)r[2 * i] << 8 | r[2 * i + 1];

    if (depth == 8)
        return r[i];

    const uint32_t per = 8 / depth, shift = 8 - depth * (i % per + 1);
    return r[i / per] >> shift & ((1u << depth) - 1);
}

/* A sample of depth bits as 8. */
static uint32_t eight(const uint32_t v, const uint8_t depth)
{
    return depth == 16 ? v >> 8 : depth == 8 ? v : v * 255 / ((1u << depth) - 1);
}

/* The alpha of index i: tRNS's, else opaque. */
static uint32_t index_alpha(const Png *const p, const uint32_t i)
{
    return i < p->ntrns ? p->trns[i] : 0xFF;
}

/* An indexed image's one keyed index: NO_KEY, KEYS. */
static int32_t keyed_index(const Png *const p)
{
    int32_t found = NO_KEY;

    for (uint32_t i = 0; i < p->colours; i++)
        if (index_alpha(p, i) < KEY)
            found = found == NO_KEY ? (int32_t)i : KEYS;

    return found;
}

/* An indexed image at 8 bits, its keyed index, if any, swapped with 0, the key's. */
static ImgStatus indexed(
    const Png *const p,
    const uint8_t *const raw,
    const uint32_t row,
    const int32_t key,
    Image *const out
)
{
    const uint32_t swap = key > 0 ? (uint32_t)key : 0;
    uint8_t *const pixels = mem_alloc(p->w * p->h);
    uint32_t *const palette = mem_alloc(256 * sizeof *palette);

    if (!pixels || !palette) {
        mem_free(pixels);
        mem_free(palette);
        return IMG_NO_MEMORY;
    }

    for (uint32_t i = 0; i < p->colours; i++) {
        const uint8_t *const c = p->palette + 3 * i;
        const uint32_t to = i == swap ? 0 : i == 0 ? swap : i;
        palette[to] = (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];
    }

    for (uint32_t y = 0; y < p->h; y++)
        for (uint32_t x = 0; x < p->w; x++) {
            const uint32_t i = sample(raw + y * (row + 1) + 1, x, p->depth);
            pixels[y * p->w + x] = (uint8_t)(i == swap ? 0 : i == 0 ? swap : i);
        }

    *out = (Image){
        .w = (int32_t)p->w,
        .h = (int32_t)p->h,
        .format = PX_8,
        .pixels = pixels,
        .palette = palette,
    };

    return IMG_OK;
}

/* Pixel x of a row as ARGB: grey, colour or index, its alpha the image's, or tRNS's. */
static uint32_t argb(const Png *const p, const uint8_t *const r, const uint32_t x)
{
    const uint8_t d = p->depth;
    const uint32_t first = sample(r, x * p->channels, d);

    if (p->type == INDEXED) {
        const uint8_t *const c = p->palette + 3 * (first < p->colours ? first : 0);
        return index_alpha(p, first) << 24 | (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];
    }

    const bool colour = p->type == 2 || p->type == 6, alpha = p->type == 4 || p->type == 6;
    const uint32_t r_ = first, g = colour ? sample(r, x * p->channels + 1, d) : first;
    const uint32_t b = colour ? sample(r, x * p->channels + 2, d) : first;
    uint32_t a = alpha ? eight(sample(r, x * p->channels + p->channels - 1, d), d) : 0xFF;

    if (!alpha && p->trns) {
        const uint32_t need = colour ? 6 : 2;
        const uint8_t *const t = p->trns;
        const bool match = p->ntrns >= need && be16(t) == r_ &&
                           (!colour || (be16(t + 2) == g && be16(t + 4) == b));
        a = match ? 0 : a;
    }

    return a << 24 | eight(r_, d) << 16 | eight(g, d) << 8 | eight(b, d);
}

static ImgStatus direct(
    const Png *const p,
    const uint8_t *const raw,
    const uint32_t row,
    Image *const out
)
{
    uint8_t *const pixels = mem_alloc(p->w * p->h * 4);

    if (!pixels)
        return IMG_NO_MEMORY;

    for (uint32_t y = 0; y < p->h; y++)
        for (uint32_t x = 0; x < p->w; x++) {
            const uint32_t c = argb(p, raw + y * (row + 1) + 1, x);
            mem_copy(pixels + 4 * (y * p->w + x), &c, 4);
        }

    *out = (Image){
        .w = (int32_t)p->w,
        .h = (int32_t)p->h,
        .format = PX_32,
        .pixels = pixels,
    };

    return IMG_OK;
}

ImgStatus img_png(const uint8_t *const data, const uint32_t n, Image *const out)
{
    Png p;
    uint32_t idat;

    if (!header(data, n, &p, &idat))
        return IMG_NOT_ONE;

    const uint32_t bits = (uint32_t)p.channels * p.depth, row = (p.w * bits + 7) / 8;
    const uint32_t bytes = p.h * (row + 1), bpp = bits < 8 ? 1 : bits / 8;
    const int32_t key = keyed_index(&p);
    const bool eight = p.type == INDEXED && key != KEYS;
    uint8_t *const z = joined(data, n, idat), *const raw = z ? mem_alloc(bytes) : nullptr;
    uint32_t made = 0;
    ImgStatus status = !raw ? IMG_NO_MEMORY : IMG_NOT_ONE;

    if (raw && inflate_zlib(z, idat, raw, bytes, &made) && made == bytes &&
        unfilter(raw, p.h, row, bpp))
        status = eight ? indexed(&p, raw, row, key, out) : direct(&p, raw, row, out);

    mem_free(z);
    mem_free(raw);
    return status;
}
