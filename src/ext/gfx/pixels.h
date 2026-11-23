/* Pixels and rectangles: what a surface gives the software backend to draw in or from; the
 * pixels' formats, and colours mixed.
 *
 * # How it works
 * A pixel is an index (8 bits), 15 bits (x555), 16 (565) or 32 (&HAARRGGBB), little-endian. As a
 * colour each is ARGB: 15 and 16 bits widen a channel by repeating its top bits, and hold no
 * alpha (&HFF). A colour's channels mix in whole numbers, alpha and light 0 to 63.
 */
#ifndef GFX_PIXELS_H
#define GFX_PIXELS_H
#include <stdint.h>

typedef enum : uint8_t { PX_8, PX_15, PX_16, PX_32, PX_FORMATS } PxFormat;

typedef enum : uint8_t { BLEND_NONE, BLEND_MIX, BLEND_ADD } BlendMode;

enum { PX_FULL = 63, PX_WHITE = 0xFFFFFF, PX_OPAQUE = 0xFF000000u };

/* w by h pixels of format; p the top left, pitch bytes a row. */
typedef struct {
    uint8_t *p;
    uint32_t pitch;
    int32_t w, h;
    PxFormat format;
} Pixels;

static inline uint32_t px_bytes(const PxFormat f) { return f == PX_8 ? 1 : f == PX_32 ? 4 : 2; }

static inline uint32_t px_bits(const PxFormat f)
{
    return f == PX_8 ? 8 : f == PX_15 ? 15 : f == PX_16 ? 16 : 32;
}

static inline uint32_t px_channel(const uint32_t c, const uint32_t shift)
{
    return c >> shift & 0xFF;
}

/* A channel of bits widened to 8, its top bits repeated below. */
static inline uint32_t px_widen(const uint32_t v, const uint32_t bits)
{
    return v << (8 - bits) | v >> (2 * bits - 8);
}

/* A pixel of a direct format as ARGB. */
static inline uint32_t px_argb(const PxFormat f, const uint32_t v)
{
    if (f == PX_32)
        return v;

    const uint32_t g = f == PX_16 ? 6 : 5, r_at = 5 + g;
    return PX_OPAQUE | px_widen(v >> r_at & 31, 5) << 16 |
           px_widen(v >> 5 & ((1u << g) - 1), g) << 8 | px_widen(v & 31, 5);
}

/* ARGB as a pixel of a direct format. */
static inline uint32_t px_pack(const PxFormat f, const uint32_t c)
{
    if (f == PX_32)
        return c;

    const uint32_t g = f == PX_16 ? 6 : 5;
    return px_channel(c, 16) >> 3 << (5 + g) | px_channel(c, 8) >> (8 - g) << 5 |
           px_channel(c, 0) >> 3;
}

/* A texel a keyed draw skips: 0, or 32 bits' alpha under a half. */
static inline bool px_key(const PxFormat f, const uint32_t v)
{
    return f == PX_32 ? v < 0x80000000u : !v;
}

/* c's channels multiplied by shade's, &HRRGGBB; c's alpha kept. */
static inline uint32_t px_shade(const uint32_t c, const uint32_t shade)
{
    if ((shade & PX_WHITE) == PX_WHITE)
        return c;

    uint32_t out = c & PX_OPAQUE;

    for (uint32_t shift = 0; shift < 24; shift += 8)
        out |= px_channel(c, shift) * px_channel(shade, shift) / 255 << shift;

    return out;
}

/* s drawn over d by mode at alpha: their channels mixed, or added. */
static inline uint32_t px_mix(
    const uint32_t s,
    const uint32_t d,
    const BlendMode mode,
    const uint32_t alpha
)
{
    uint32_t out = PX_OPAQUE;

    for (uint32_t shift = 0; shift < 24; shift += 8) {
        const uint32_t a = px_channel(s, shift), b = px_channel(d, shift);
        const uint32_t v = mode == BLEND_ADD ? b + a * alpha / PX_FULL
                                             : (b * (PX_FULL - alpha) + a * alpha) / PX_FULL;
        out |= (v > 255 ? 255 : v) << shift;
    }

    return out;
}

/* The cell of rgb in a map of 5 bits a channel. */
static inline uint32_t px_cell(const uint32_t rgb)
{
    return px_channel(rgb, 16) >> 3 << 10 | px_channel(rgb, 8) >> 3 << 5 | px_channel(rgb, 0) >> 3;
}

typedef struct {
    int32_t x, y, w, h;
} Rect;

/* A point; a coordinate names a pixel's centre. */
typedef struct {
    float x, y;
} Vec;

/* A shape's corner on the target, and the texel there: u and v in texels from the top left. */
typedef struct {
    float x, y, u, v;
} Vertex;

static inline int32_t rect_max(const int32_t a, const int32_t b) { return a > b ? a : b; }

static inline int32_t rect_min(const int32_t a, const int32_t b) { return a < b ? a : b; }

/* Where a and b overlap: w or h 0, nowhere. */
static inline Rect rect_meet(const Rect a, const Rect b)
{
    const int32_t x = rect_max(a.x, b.x), y = rect_max(a.y, b.y);
    const int32_t r = rect_min(a.x + a.w, b.x + b.w), d = rect_min(a.y + a.h, b.y + b.h);
    return (Rect){x, y, rect_max(r - x, 0), rect_max(d - y, 0)};
}
#endif
