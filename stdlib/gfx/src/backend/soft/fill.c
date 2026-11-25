/* The fillers (fill.h): one body for every format, compiled for each with the formats and the
   variant constants, so no pixel tests a flag. FILL_TABLE binds them into a format's table. */
#include "core/mem.h"
#include "backend/soft/fill.h"

#if defined(__ARM_NEON)
#include <arm_neon.h>
enum { FILL_VECTORS = 1 };
#else
enum { FILL_VECTORS = 0 };
#endif

/* ---- A texel through the ink ---- */

/* ARGB as a pixel of direct format f, opaque as all drawn into a surface is. */
[[gnu::always_inline]]
static inline uint32_t direct(const PxFormat f, const uint32_t c)
{
    return px_pack(f, c | PX_OPAQUE);
}

/* A mix table's MIX share is scaled by 2^FILL_SCALE over its divisor, rounded up: two summed and
   shifted down are their sum divided, exact for every sum two channels make. */
enum { FILL_SCALE = 24 };

/* Beside the variant's bits: ADDED, BLENDED's mode ADD into a direct target, so each filler's mode
   is a constant to px_mix; ANY, a variant not known before the ink is drawn, each bit read from
   the ink. */
enum { FILL_ADDED = FILL_INKS, FILL_ANY = FILL_INKS << 1 };

/* Variant v's bit, or the ink's when v is ANY. */
[[gnu::always_inline]]
static inline bool has(const uint32_t v, const uint32_t bit, const bool ink)
{
    return v & FILL_ANY ? ink : v & bit;
}

/* c drawn over d, both ARGB, by the alpha and variant v's mode. */
[[gnu::always_inline]]
static inline uint32_t mixed(
    const uint32_t c,
    const uint32_t d,
    const uint32_t v,
    const Ink *const k
)
{
    return px_mix(c, d, has(v, FILL_ADDED, k->mode == BLEND_ADD) ? BLEND_ADD : BLEND_MIX, k->alpha);
}

/* Direct format g's channel ch, 0 red to 2 blue: its bits, and its lowest bit's place. */
[[gnu::always_inline]]
static inline uint32_t bits_of(const PxFormat g, const uint32_t ch)
{
    return g == PX_32 ? 8 : g == PX_16 && ch == 1 ? 6 : 5;
}

[[gnu::always_inline]]
static inline uint32_t place_of(const PxFormat g, const uint32_t ch)
{
    return g == PX_32 ? 16 - 8 * ch : ch == 2 ? 0 : ch == 1 ? 5 : 5 + bits_of(g, 1);
}

/* The entry of pixel t's channel ch, g's, direct, in a table of px_table's: the channels' in
   turn, each by its bits. */
[[gnu::always_inline]]
static inline uint32_t entry_of(const PxFormat g, const uint32_t ch, const uint32_t t)
{
    const uint32_t value = t >> place_of(g, ch) & ((1u << bits_of(g, ch)) - 1);

    return ch == 0   ? value
           : ch == 1 ? (1u << bits_of(g, 0)) + value
                     : (1u << bits_of(g, 0)) + (1u << bits_of(g, 1)) + value;
}

/* Texel t, g's, through px_table's table p: by index from 8 bits, else its channels' entries
   or'd. */
[[gnu::always_inline]]
static inline uint32_t tabled(const PxFormat g, const uint32_t *const p, const uint32_t t)
{
    if (g == PX_8)
        return p[t & 0xFF];

    return p[entry_of(g, 0, t)] | p[entry_of(g, 1, t)] | p[entry_of(g, 2, t)];
}

/* Texel t, h's, direct, blended over there, f's, by the mix table m (fill_mixes): each channel's
   two shares summed, the sum the channel, under ADD the sum's entry. */
[[gnu::always_inline]]
static inline uint32_t mixed_by(
    const PxFormat f,
    const PxFormat h,
    const uint32_t v,
    const uint32_t *const m,
    const uint32_t t,
    const uint32_t there
)
{
    uint32_t out = f == PX_32 ? PX_OPAQUE : 0;

    for (uint32_t ch = 0; ch < 3; ch++) {
        const uint32_t sum = m[entry_of(h, ch, t)] + m[PX_TABLE + entry_of(f, ch, there)];

        out |= v & FILL_ADDED ? m[2 * PX_TABLE + FILL_SUMS * ch + sum]
                              : sum >> FILL_SCALE << place_of(f, ch);
    }

    return out;
}

/* Texel t, g's, through the ink as variant v into f, not blended: an index at 8 bits, else
   ARGB. A filler's direct texel into 8 bits is a cell by the ink's pixel table. */
[[gnu::always_inline]]
static inline uint32_t ink_colour(
    const PxFormat f,
    const PxFormat g,
    const uint32_t v,
    const Ink *const k,
    const uint32_t t
)
{
    if (has(v, FILL_SOLID, k->solid))
        return k->colour;

    if (f != PX_8)
        return g == PX_8 ? k->argb[t & 0xFF] : px_shade(px_argb(g, t), k->shade);

    if (g != PX_8)
        return k->map
            [v & FILL_ANY ? px_cell(px_shade(px_argb(g, t), k->shade)) : tabled(g, k->pixels, t)];

    return has(v, FILL_LUT, k->lut) ? k->lut[t] : t;
}

/* Texel t, g's, through the ink as variant v: f's pixel, blended over there when v is. A filler's
   into a direct target, not blended, is the ink's pixel table's: colour and format at once. */
[[gnu::always_inline]]
static inline uint32_t ink_pixel(
    const PxFormat f,
    const PxFormat g,
    const uint32_t v,
    const Ink *const k,
    const uint32_t t,
    const uint32_t there
)
{
    if (f != PX_8 && !(v & (FILL_ANY | FILL_SOLID | FILL_BLENDED)))
        return tabled(g, k->pixels, t);

    if (f != PX_8 && v & FILL_BLENDED && !(v & (FILL_ANY | FILL_SOLID)))
        return mixed_by(
            f,
            g == PX_8 ? PX_32 : g,
            v,
            k->mixes,
            g == PX_8 ? k->argb[t & 0xFF] : t,
            there
        );

    const uint32_t c = ink_colour(f, g, v, k, t);

    if (!has(v, FILL_BLENDED, k->blend || k->mode))
        return f == PX_8 ? c : direct(f, c);

    return f == PX_8 ? k->blend[c << 8 | there] : direct(f, mixed(c, px_argb(f, there), v, k));
}

[[gnu::always_inline]]
static inline void argb_of(
    const PxFormat g,
    uint32_t *const argb,
    bool *const drawn,
    const uint32_t *const texels,
    const int32_t n,
    const Ink *const k
)
{
    const Ink ink = *k; /* a bool stored may be k's: its own copy is read once */

    for (int32_t i = 0; i < n; i++) {
        drawn[i] = !ink.keyed || !px_key(g, texels[i]);
        argb[i] = ink.solid ? ink.colour : tabled(g, ink.argb, texels[i]);
    }
}

void fill_argb(
    uint32_t *const argb,
    bool *const drawn,
    const uint32_t *const texels,
    const int32_t n,
    const Ink *const k
)
{
    switch (k->source) {
    case PX_8:
        argb_of(PX_8, argb, drawn, texels, n, k);
        break;

    case PX_15:
        argb_of(PX_15, argb, drawn, texels, n, k);
        break;

    case PX_16:
        argb_of(PX_16, argb, drawn, texels, n, k);
        break;

    default:
        argb_of(PX_32, argb, drawn, texels, n, k);
    }
}

/* n texels, g's, through the ink into d, a texel at a time, the variant ANY. */
[[gnu::always_inline]]
static inline void texels_of(
    const PxFormat f,
    const PxFormat g,
    uint8_t *const d,
    const uint32_t *const texels,
    const int32_t n,
    const Ink *const k
)
{
    const uint32_t b = px_bytes(f);
    const bool blended = k->blend || k->mode;

    for (int32_t i = 0; i < n; i++) {
        uint8_t *const p = d + i * (int32_t)b;

        if (!k->keyed || !px_key(g, texels[i]))
            px_store(p, ink_pixel(f, g, FILL_ANY, k, texels[i], blended ? px_load(p, b) : 0), b);
    }
}

/* texels_of, the source's body picked once: a call is often of one texel. */
[[gnu::always_inline]]
static inline void ink_of(
    const PxFormat f,
    uint8_t *const d,
    const uint32_t *const texels,
    const int32_t n,
    const Ink *const k
)
{
    switch (k->source) {
    case PX_8:
        texels_of(f, PX_8, d, texels, n, k);
        break;

    case PX_15:
        texels_of(f, PX_15, d, texels, n, k);
        break;

    case PX_16:
        texels_of(f, PX_16, d, texels, n, k);
        break;

    default:
        texels_of(f, PX_32, d, texels, n, k);
    }
}

[[gnu::always_inline]]
static inline uint32_t paint_of(const PxFormat f, const uint32_t texel, const Ink *const k)
{
    const uint32_t c = ink_colour(f, k->source, FILL_ANY, k, texel);

    return f == PX_8 ? c : direct(f, c);
}

/* ---- The fillers ---- */

/* A filler's kind, by its table's name: a colour (SPAN), a colour blended over the pixels there
   (TINT), a row's texels in order (ROW), a walk's (STEP), a walk's through the ink (INKED). */
typedef enum { KIND_SPAN, KIND_TINT, KIND_ROW, KIND_STEP, KIND_INKED } Kind;

/* A filler's inputs, by kind. Copied in: through a pointer each store could change them, for all
   the compiler knows. */
typedef struct {
    Ink ink;                /* INKED's and TINT's */
    StepsRead walk;         /* STEP's and INKED's */
    const uint8_t *row;     /* ROW's next texel */
    int32_t dir;            /* ROW's bytes to the texel after */
    uint32_t colour;        /* SPAN's pixel */
    const uint8_t *blend;   /* TINT's row of the blend table, into 8 bits */
    const uint32_t *pixels; /* TINT's tint table, into a direct target */
} Inputs;

/* The next texel, g's: none, 0, for a colour's kinds. */
[[gnu::always_inline]]
static inline uint32_t texel_next(const Kind kind, const PxFormat g, Inputs *const in)
{
    if (kind == KIND_SPAN || kind == KIND_TINT)
        return 0;

    if (kind != KIND_ROW)
        return steps_read_next(&in->walk, px_bytes(g));

    const uint32_t t = px_load(in->row, px_bytes(g));
    in->row += in->dir;
    return t;
}

/* The pixel, f's, of texel t, g's, over there as variant v. */
[[gnu::always_inline]]
static inline uint32_t pixel_of(
    const Kind kind,
    const PxFormat f,
    const PxFormat g,
    const uint32_t v,
    const Inputs *const in,
    const uint32_t t,
    const uint32_t there
)
{
    if (kind == KIND_SPAN)
        return in->colour;

    if (kind == KIND_TINT)
        return f == PX_8 ? in->blend[there] : tabled(f, in->pixels, there);

    if (kind == KIND_INKED)
        return ink_pixel(f, g, v, &in->ink, t, there);

    return t;
}

/* A pixel at p, of f, from g as variant v. */
[[gnu::always_inline]]
static inline void pixel_at(
    const Kind kind,
    const PxFormat f,
    const PxFormat g,
    const uint32_t v,
    Inputs *const in,
    uint8_t *const p
)
{
    const uint32_t b = px_bytes(f), t = texel_next(kind, g, in);

    if (!(v & FILL_KEYED) || !px_key(g, t))
        px_store(p, pixel_of(kind, f, g, v, in, t, v & FILL_BLENDED ? px_load(p, b) : 0), b);
}

#if !defined(__ARM_NEON)
/* A row's next word of f's texels, read at once: backward when dir is, its texels then
   reversed. */
[[gnu::always_inline]]
static inline uint32_t row_word(const PxFormat f, Inputs *const in)
{
    const int32_t b = (int32_t)px_bytes(f);
    const bool back = in->dir < 0;
    const uint32_t w = px_load(in->row - (back ? 4 - b : 0), 4);

    in->row += in->dir * (4 / b);

    if (!back)
        return w;

    return b == 1 ? __builtin_bswap32(w) : w >> 16 | w << 16;
}

/* w rotated right by bits. */
[[gnu::always_inline]]
static inline uint32_t rotated(const uint32_t w, const uint32_t bits)
{
    return w >> bits | w << (32 - bits);
}

/* The top bits of w's lanes that are 0, a lane bits wide, 8 or 16. */
[[gnu::always_inline]]
static inline uint32_t zero_tops(const uint32_t w, const uint32_t bits)
{
    const uint32_t low = bits == 8 ? 0x7F7F7F7Fu : 0x7FFF7FFFu;

    return ~((w & low) + low | w | low);
}

/* A word of pixels at p, 4 bytes aligned, of f from g as variant v: built a pixel at a time into
   its top (uGL's or and ror) and stored at once, the keyed pixels kept. A texel as wide as the
   pixel is keyed when 0, so a word of them is keyed at once, by its lanes that are 0. */
[[gnu::always_inline]]
static inline void word_at(
    const Kind kind,
    const PxFormat f,
    const PxFormat g,
    const uint32_t v,
    Inputs *const in,
    uint8_t *const p
)
{
    const uint32_t bits = 8 * px_bytes(f), per = 32 / bits, mask = (1u << bits) - 1;
    const uint32_t top = 1u << (bits - 1);
    const bool lanes = px_bytes(g) == px_bytes(f);
    const uint32_t there = v & FILL_BLENDED ? px_load(p, 4) : 0;
    uint32_t word = 0, texels = 0, keyed = 0; /* keyed: the top bits of the keyed pixels' lanes */

    if (kind == KIND_ROW)
        word = texels = row_word(f, in);
    else
        for (uint32_t j = 0; j < per; j++) {
            const uint32_t t = texel_next(kind, g, in);

            word = rotated(word | pixel_of(kind, f, g, v, in, t, there >> bits * j & mask), bits);
            texels = rotated(texels | t, bits);
            keyed = rotated(keyed | (v & FILL_KEYED && px_key(g, t) ? top : 0), bits);
        }

    if (v & FILL_KEYED && lanes)
        keyed = zero_tops(texels, bits);

    if (keyed) {
        const uint32_t keep = (keyed << 1) - (keyed >> (bits - 1)); /* the lanes, all ones */
        word = (word & ~keep) | ((v & FILL_BLENDED ? there : px_load(p, 4)) & keep);
    }

    px_store(p, word, 4);
}
#endif

#if defined(__ARM_NEON)
/* ---- NEON: 16 bytes of pixels a vector ---- */

/* A vector of f's pixels in the reverse order. */
[[gnu::always_inline]]
static inline uint8x16_t reversed(const PxFormat f, const uint8x16_t t)
{
    if (f == PX_8)
        return vextq_u8(vrev64q_u8(t), vrev64q_u8(t), 8);

    if (f == PX_32) {
        const uint32x4_t w = vrev64q_u32(vreinterpretq_u32_u8(t));
        return vreinterpretq_u8_u32(vextq_u32(w, w, 2));
    }

    const uint16x8_t h = vrev64q_u16(vreinterpretq_u16_u8(t));
    return vreinterpretq_u8_u16(vextq_u16(h, h, 4));
}

/* A vector of f's texels' lanes all ones where drawn, not keyed (px_key). */
[[gnu::always_inline]]
static inline uint8x16_t drawn_lanes(const PxFormat f, const uint8x16_t t)
{
    if (f == PX_8)
        return vtstq_u8(t, t);

    if (f == PX_32)
        return vreinterpretq_u8_u32(vcltzq_s32(vreinterpretq_s32_u8(t)));

    const uint16x8_t h = vreinterpretq_u16_u8(t);
    return vreinterpretq_u8_u16(vtstq_u16(h, h));
}

/* x divided by 63, exact for every x a mix makes (at most 255 * 63): a multiply and shifts. */
[[gnu::always_inline]]
static inline uint16x8_t by63(const uint16x8_t x)
{
    const uint32x4_t low = vmull_n_u16(vget_low_u16(x), 16645), high = vmull_high_n_u16(x, 16645);
    return vshrq_n_u16(vcombine_u16(vshrn_n_u32(low, 16), vshrn_n_u32(high, 16)), 4);
}

/* px_mix's channel, 8 lanes of 16 bits: a's over b's by alpha, added or mixed. */
[[gnu::always_inline]]
static inline uint16x8_t mixed_lanes(
    const bool added,
    const uint16_t alpha,
    const uint16x8_t a,
    const uint16x8_t b
)
{
    if (added)
        return vminq_u16(vaddq_u16(b, by63(vmulq_n_u16(a, alpha))), vdupq_n_u16(255));

    return by63(vmlaq_n_u16(vmulq_n_u16(b, (uint16_t)(PX_FULL - alpha)), a, alpha));
}

/* A 5- or 6-bit channel's lanes widened to 8 bits, as px_widen. */
[[gnu::always_inline]]
static inline uint16x8_t widened(const uint16x8_t c, const uint32_t bits)
{
    return bits == 5 ? vorrq_u16(vshlq_n_u16(c, 3), vshrq_n_u16(c, 2))
                     : vorrq_u16(vshlq_n_u16(c, 2), vshrq_n_u16(c, 4));
}

/* The ARGB colours c, 16 bytes' pixels of them, over the pixels there, f's, direct: px_mix's
   channels a vector at a time, f's pixels again. */
[[gnu::always_inline]]
static inline uint8x16_t mixed_vector(
    const PxFormat f,
    const bool added,
    const uint16_t alpha,
    const uint32_t *const c,
    const uint8x16_t there
)
{
    if (f == PX_32) {
        const uint8x16_t s = vld1q_u8((const uint8_t *)c);
        const uint16x8_t low =
            mixed_lanes(added, alpha, vmovl_u8(vget_low_u8(s)), vmovl_u8(vget_low_u8(there)));
        const uint16x8_t high = mixed_lanes(added, alpha, vmovl_high_u8(s), vmovl_high_u8(there));
        const uint8x16_t out = vcombine_u8(vmovn_u16(low), vmovn_u16(high));

        return vorrq_u8(out, vreinterpretq_u8_u32(vdupq_n_u32(PX_OPAQUE)));
    }

    const uint32_t green = f == PX_16 ? 6 : 5;
    const uint8x8x4_t s = vld4_u8((const uint8_t *)c); /* blue, green, red, alpha */
    const uint16x8_t d = vreinterpretq_u16_u8(there);
    const uint16x8_t r = mixed_lanes(
        added,
        alpha,
        vmovl_u8(s.val[2]),
        widened(vandq_u16(vshlq_u16(d, vdupq_n_s16(-(int16_t)(5 + green))), vdupq_n_u16(31)), 5)
    );
    const uint16x8_t g = mixed_lanes(
        added,
        alpha,
        vmovl_u8(s.val[1]),
        widened(vandq_u16(vshrq_n_u16(d, 5), vdupq_n_u16((uint16_t)((1u << green) - 1))), green)
    );
    const uint16x8_t b =
        mixed_lanes(added, alpha, vmovl_u8(s.val[0]), widened(vandq_u16(d, vdupq_n_u16(31)), 5));
    const uint16x8_t out = vorrq_u16(
        vorrq_u16(
            vshlq_u16(vshrq_n_u16(r, 3), vdupq_n_s16((int16_t)(5 + green))),
            vshlq_u16(vshlq_u16(g, vdupq_n_s16(-(int16_t)(8 - green))), vdupq_n_s16(5))
        ),
        vshrq_n_u16(b, 3)
    );

    return vreinterpretq_u8_u16(out);
}

/* 16 bytes of pixels at p, f's from g as variant v: built a pixel at a time into two 64-bit
   halves and stored at once, the keyed pixels kept by a select. A texel as wide as the pixel is
   keyed when 0, so a vector of them is keyed at once (drawn_lanes). */
[[gnu::always_inline]]
static inline void vector_at(
    const Kind kind,
    const PxFormat f,
    const PxFormat g,
    const uint32_t v,
    Inputs *const in,
    uint8_t *const p
)
{
    const uint32_t b = px_bytes(f), bits = 8 * b, per = 8 / b;
    const uint64_t mask = (uint64_t)-1 >> (64 - bits);
    const bool lanes = px_bytes(g) == b, mixes = f != PX_8 && v & FILL_BLENDED;
    uint64_t word[2] = {}, texels[2] = {}, keyed[2] = {};

    uint32_t colours[16 / 2]; /* a direct target's blended colours, mixed a vector at a time */

    for (uint32_t h = 0; h < 2; h++)
        for (uint32_t j = 0; j < per; j++) {
            const uint8_t *const at = p + (h * per + j) * b;
            const uint32_t t = texel_next(kind, g, in);

            if (mixes)
                colours[h * per + j] =
                    kind == KIND_TINT ? in->colour : ink_colour(f, g, v, &in->ink, t);
            else
                word[h] |=
                    (uint64_t)pixel_of(kind, f, g, v, in, t, v & FILL_BLENDED ? px_load(at, b) : 0)
                    << bits * j;

            texels[h] |= (t & mask) << bits * j;
            keyed[h] |= (v & FILL_KEYED && px_key(g, t) ? mask : 0) << bits * j;
        }

    const uint8x16_t got =
        mixes ? mixed_vector(f, v & FILL_ADDED, in->ink.alpha, colours, vld1q_u8(p))
              : vreinterpretq_u8_u64(vcombine_u64(vcreate_u64(word[0]), vcreate_u64(word[1])));

    if (!(v & FILL_KEYED)) {
        vst1q_u8(p, got);
        return;
    }

    const uint8x16_t drawn =
        lanes
            ? drawn_lanes(
                  f,
                  vreinterpretq_u8_u64(vcombine_u64(vcreate_u64(texels[0]), vcreate_u64(texels[1])))
              )
            : vmvnq_u8(
                  vreinterpretq_u8_u64(vcombine_u64(vcreate_u64(keyed[0]), vcreate_u64(keyed[1])))
              );

    vst1q_u8(p, vbslq_u8(drawn, got, vld1q_u8(p)));
}
#endif

/* n pixels into d, of f from g as variant v: alone until d is aligned, then a word of them at a
   time, the rest alone; a pixel of 32 bits is a word. With NEON, 16 bytes a vector from the
   first, the rest alone. */
[[gnu::always_inline]]
static inline void run_of(
    const Kind kind,
    const PxFormat f,
    const PxFormat g,
    const uint32_t v,
    uint8_t *const d,
    const int32_t n,
    Inputs in
)
{
    const int32_t b = (int32_t)px_bytes(f);
#if defined(__ARM_NEON)
    const int32_t vectors = n / (16 / b) * (16 / b);

    for (int32_t i = 0; i < vectors; i += 16 / b)
        vector_at(kind, f, g, v, &in, d + i * b);

    for (int32_t i = vectors; i < n; i++)
        pixel_at(kind, f, g, v, &in, d + i * b);
#else

    const int32_t per = 4 / b, align = (int32_t)(-(uintptr_t)d & 3) / b;
    const int32_t head = per == 1 || align > n ? n : align, tail = head + (n - head) / per * per;
    uint8_t *const words = d + head * b, *const rest = d + tail * b;

    for (int32_t i = 0; i < head; i++)
        pixel_at(kind, f, g, v, &in, d + i * b);

    for (uint8_t *p = words; p < rest; p += 4)
        word_at(kind, f, g, v, &in, p);

    for (int32_t i = tail; i < n; i++)
        pixel_at(kind, f, g, v, &in, d + i * b);
#endif
}

/* run_of, a blended one's mode picked once. */
[[gnu::always_inline]]
static inline void fill_of(
    const Kind kind,
    const PxFormat f,
    const PxFormat g,
    const uint32_t v,
    uint8_t *const d,
    const int32_t n,
    const Inputs in
)
{
    /* A tint's pixels are tabled, add or mix alike; only its vectors mix. */
    const bool mixes = kind == KIND_INKED || (FILL_VECTORS && kind == KIND_TINT);

    if (mixes && f != PX_8 && v & FILL_BLENDED && in.ink.mode == BLEND_ADD)
        run_of(kind, f, g, v | FILL_ADDED, d, n, in);
    else
        run_of(kind, f, g, v, d, n, in);
}

#if defined(__ARM_NEON)
/* The vectors of row_of's n texels, whole ones: their pixels drawn, and how many. */
[[gnu::always_inline]]
static inline int32_t row_vectors(
    const PxFormat f,
    uint8_t *const d,
    const uint8_t *const s,
    const int32_t n,
    const int32_t dir,
    const bool key
)
{
    const int32_t b = (int32_t)px_bytes(f), per = 16 / b, m = n / per * per;

    for (int32_t i = 0; i < m; i += per) {
        const uint8_t *const at = dir > 0 ? s + i * b : s - (i + per - 1) * b;
        uint8x16_t t = vld1q_u8(at);

        if (dir < 0)
            t = reversed(f, t);

        if (key)
            t = vbslq_u8(drawn_lanes(f, t), t, vld1q_u8(d + i * b));

        vst1q_u8(d + i * b, t);
    }

    return m;
}

/* The vectors of n pixels of colour c, f's, whole ones: stored four at once, then one, and how
   many pixels. */
[[gnu::always_inline]]
static inline int32_t span_vectors(
    const PxFormat f,
    uint8_t *const d,
    const int32_t n,
    const uint32_t c
)
{
    const int32_t b = (int32_t)px_bytes(f), per = 16 / b, m = n / per * per,
                  fours = m / (4 * per) * 4 * per;
    const uint8x16_t t = f == PX_32 ? vreinterpretq_u8_u32(vdupq_n_u32(c))
                                    : vreinterpretq_u8_u16(vdupq_n_u16((uint16_t)c));
    const uint8x16x4_t four = {{t, t, t, t}};

    for (int32_t i = 0; i < fours; i += 4 * per)
        vst1q_u8_x4(d + i * b, four);

    for (int32_t i = fours; i < m; i += per)
        vst1q_u8(d + i * b, t);

    return m;
}
#else
/* No vectors: none drawn so. */
static inline int32_t row_vectors(PxFormat, uint8_t *, const uint8_t *, int32_t, int32_t, bool)
{
    return 0;
}

static inline int32_t span_vectors(PxFormat, uint8_t *, int32_t, uint32_t) { return 0; }
#endif

[[gnu::always_inline]]
static inline void span_of(const PxFormat f, uint8_t *const d, const int32_t n, const uint32_t c)
{
    if (f == PX_8) {
        mem_fill(d, (uint8_t)c, (size_t)n);
        return;
    }

    const int32_t m = span_vectors(f, d, n, c);
    fill_of(KIND_SPAN, f, f, 0, d + m * (int32_t)px_bytes(f), n - m, (Inputs){.colour = c});
}

/* n texels from s, each the next dir texels on; key skips px_key's. */
[[gnu::always_inline]]
static inline void row_of(
    const PxFormat f,
    uint8_t *const d,
    const uint8_t *const s,
    const int32_t n,
    const int32_t dir,
    const bool key
)
{
    const int32_t b = (int32_t)px_bytes(f);

    if (!key && dir == 1) {
        mem_copy(d, s, (size_t)(b * n));
        return;
    }

    const int32_t m = row_vectors(f, d, s, n, dir, key);
    const Inputs in = {.row = s + m * dir * b, .dir = dir * b};

    fill_of(KIND_ROW, f, f, key ? FILL_KEYED : 0, d + m * b, n - m, in);
}

[[gnu::always_inline]]
static inline void steps_of(
    const PxFormat f,
    uint8_t *const d,
    const Pixels *const s,
    const Steps st,
    const int32_t n,
    const bool key
)
{
    const StepsBy by = steps_by(s, st, px_bytes(f));
    const Inputs in = {.walk = steps_read(s, st, &by, px_bytes(f))};

    fill_of(KIND_STEP, f, f, key ? FILL_KEYED : 0, d, n, in);
}

/* ink_of's texels fetched as drawn, from g into f as variant v. */
[[gnu::always_inline]]
static inline void inked_of(
    const PxFormat f,
    const PxFormat g,
    const uint32_t v,
    uint8_t *const d,
    const Pixels *const s,
    const Steps st,
    const int32_t n,
    const Ink *const k
)
{
    const StepsBy by = steps_by(s, st, px_bytes(g));

    fill_of(
        KIND_INKED,
        f,
        g,
        v,
        d,
        n,
        (Inputs){.ink = *k, .walk = steps_read(s, st, &by, px_bytes(g))}

    );
}

const uint32_t *fill_colours(FillColours *const c, const Ink *const k)
{
    if (!c->made || c->from != k->source || c->shade != k->shade) {
        px_table(c->table, k->source, PX_32, nullptr, k->shade);
        c->from = k->source;
        c->shade = k->shade;
        c->made = true;
    }

    return c->table;
}

const uint32_t *fill_mixes(FillMixes *const m, const PxFormat into, const Ink *const k)
{
    const PxFormat from = k->source == PX_8 ? PX_32 : k->source;
    const uint32_t shade = k->source == PX_8 ? PX_WHITE : k->shade, alpha = k->alpha;
    const bool add = k->mode == BLEND_ADD;

    if (m->made && m->from == from && m->into == into && m->shade == shade && m->mode == k->mode &&
        m->alpha == alpha)
        return m->table;

    for (uint32_t ch = 0; ch < 3; ch++) {
        const uint32_t at = 16 - 8 * ch, drop = 8 - bits_of(into, ch);
        const uint32_t divisor = PX_FULL << drop;
        const uint32_t scale = ((1u << FILL_SCALE) + divisor - 1) / divisor;

        for (uint32_t v = 0; v < 1u << bits_of(from, ch); v++) {
            const uint32_t t = v << place_of(from, ch);
            const uint32_t lit = px_channel(px_shade(px_argb(from, t), shade), at);
            m->table[entry_of(from, ch, t)] = add ? lit * alpha / PX_FULL : lit * alpha * scale;
        }

        for (uint32_t v = 0; v < 1u << bits_of(into, ch); v++) {
            const uint32_t there = v << place_of(into, ch);
            const uint32_t b = px_channel(px_argb(into, there), at);
            m->table[PX_TABLE + entry_of(into, ch, there)] =
                add ? b : b * (PX_FULL - alpha) * scale;
        }

        for (uint32_t sum = 0; add && sum < FILL_SUMS; sum++)
            m->table[2 * PX_TABLE + FILL_SUMS * ch + sum] =
                (sum > 255 ? 255 : sum) >> drop << place_of(into, ch);
    }

    m->from = from;
    m->into = into;
    m->shade = shade;
    m->mode = k->mode;
    m->alpha = (uint8_t)alpha;
    m->made = true;
    return m->table;
}

/* tints made for c, ARGB, over each pixel of f there by the ink's mode and alpha, unless they
   are: a channel's mix and pixel are its own, so the table is exact. */
static void tints_of(FillTints *const tints, const PxFormat f, const uint32_t c, const Ink *const k)
{
    if (tints->made && tints->colour == c && tints->mode == k->mode && tints->alpha == k->alpha &&
        tints->format == f)
        return;

    uint32_t *at = tints->pixels;

    for (uint32_t ch = 0; ch < 3; ch++)
        for (uint32_t v = 0; v < 1u << bits_of(f, ch); v++) {
            const uint32_t shift = 16 - 8 * ch;
            const uint32_t mix = px_mix(c, px_widen(v, bits_of(f, ch)) << shift, k->mode, k->alpha);
            *at++ = px_pack(f, (mix & 0xFFu << shift) | (ch ? 0 : PX_OPAQUE));
        }

    tints->colour = c;
    tints->mode = k->mode;
    tints->alpha = k->alpha;
    tints->format = f;
    tints->made = true;
}

/* ink_of's n texels all colour, blended: its colour inked once, into a direct target its tint
   table made once. */
[[gnu::always_inline]]
static inline void tint_of(
    const PxFormat f,
    uint8_t *const d,
    const int32_t n,
    const uint32_t colour,
    const Ink *const k,
    FillTints *const tints
)
{
    const uint32_t c = ink_colour(f, k->source, FILL_ANY, k, colour);
    Inputs in = {.ink = *k, .colour = c};

    if (f == PX_8)
        in.blend = k->blend + (c << 8);
    else {
        tints_of(tints, f, c, k);
        in.pixels = tints->pixels;
    }

    fill_of(KIND_TINT, f, f, FILL_BLENDED, d, n, in);
}

/* Variant k s l b: keyed, SOLID, LUT, BLENDED, each 0 or 1. */
#define FILL_VARIANT(k, s, l, b)                                                                   \
    ((k) * FILL_KEYED | (s) * FILL_SOLID | (l) * FILL_LUT | (b) * FILL_BLENDED)

/* name's inked filler of variant k s l b, from g into f; its slot in the table's row. */
#define FILL_INKED(name, f, g, k, s, l, b)                                                         \
    static void name##_##k##s##l##b(                                                               \
        uint8_t *const d,                                                                          \
        const Pixels *const p,                                                                     \
        const Steps st,                                                                            \
        const int32_t n,                                                                           \
        const Ink *const ink                                                                       \
    )                                                                                              \
    {                                                                                              \
        inked_of(f, g, FILL_VARIANT(k, s, l, b), d, p, st, n, ink);                                \
    }
#define FILL_SLOT(name, f, g, k, s, l, b) [FILL_VARIANT(k, s, l, b)] = name##_##k##s##l##b,

/* X for a variant keyed and not. */
#define FILL_KEYS(X, s, l, b, ...) X(__VA_ARGS__, 0, s, l, b) X(__VA_ARGS__, 1, s, l, b)
/* X for those SOLID and not. */
#define FILL_SOLIDS(X, l, b, ...)                                                                  \
    FILL_KEYS(X, 0, l, b, __VA_ARGS__) FILL_KEYS(X, 1, l, b, __VA_ARGS__)
/* Most pairs': keyed, SOLID and BLENDED, each or not. */
#define FILL_PLAIN(X, ...) FILL_SOLIDS(X, 0, 0, __VA_ARGS__) FILL_SOLIDS(X, 0, 1, __VA_ARGS__)
/* 8 bits from 8 bits: those, and LUT's, which SOLID's colour overrides. */
#define FILL_EIGHT(X, ...)                                                                         \
    FILL_PLAIN(X, __VA_ARGS__) FILL_KEYS(X, 0, 1, 0, __VA_ARGS__) FILL_KEYS(X, 0, 1, 1, __VA_ARGS__)

/* Every target, by X: its table's name and format, and its variants for 8-bit texels. */
#define FILL_TARGETS(X, ...)                                                                       \
    X(fill8, PX_8, FILL_EIGHT, __VA_ARGS__)                                                        \
    X(fill15, PX_15, FILL_PLAIN, __VA_ARGS__)                                                      \
    X(fill16, PX_16, FILL_PLAIN, __VA_ARGS__)                                                      \
    X(fill32, PX_32, FILL_PLAIN, __VA_ARGS__)
/* Target t's every source, by Y: the fillers' name, the formats, the variants. */
#define FILL_SOURCES(t, f, eight, Y)                                                               \
    Y(t##_from8, f, PX_8, eight)                                                                   \
    Y(t##_from15, f, PX_15, FILL_PLAIN)                                                            \
    Y(t##_from16, f, PX_16, FILL_PLAIN)                                                            \
    Y(t##_from32, f, PX_32, FILL_PLAIN)
#define FILL_PAIR(name, f, g, variants) variants(FILL_INKED, name, f, g)
#define FILL_ROW(name, f, g, variants) [f][g] = {variants(FILL_SLOT, name, f, g)},

FILL_TARGETS(FILL_SOURCES, FILL_PAIR)

/* By target, source and variant; a variant fill_variant never gives, null. */
static FillInked *const fill_inked[PX_FORMATS][PX_FORMATS][FILL_INKS] = {
    FILL_TARGETS(FILL_SOURCES, FILL_ROW)
};
/* Format f's table, name: each body above bound to f. */
#define FILL_TABLE(name, f)                                                                        \
    static void name##_span(uint8_t *const d, const int32_t n, const uint32_t c)                   \
    {                                                                                              \
        span_of(f, d, n, c);                                                                       \
    }                                                                                              \
    static void name##_keyed(uint8_t *const d, const uint8_t *const s, const int32_t n)            \
    {                                                                                              \
        row_of(f, d, s, n, 1, true);                                                               \
    }                                                                                              \
    static void name##_keyed_flipped(uint8_t *const d, const uint8_t *const s, const int32_t n)    \
    {                                                                                              \
        row_of(f, d, s, n, -1, true);                                                              \
    }                                                                                              \
    static void name##_opaque(uint8_t *const d, const uint8_t *const s, const int32_t n)           \
    {                                                                                              \
        row_of(f, d, s, n, 1, false);                                                              \
    }                                                                                              \
    static void name##_opaque_flipped(uint8_t *const d, const uint8_t *const s, const int32_t n)   \
    {                                                                                              \
        row_of(f, d, s, n, -1, false);                                                             \
    }                                                                                              \
    static void                                                                                    \
    name##_stepped_keyed(uint8_t *const d, const Pixels *const s, const Steps st, const int32_t n) \
    {                                                                                              \
        steps_of(f, d, s, st, n, true);                                                            \
    }                                                                                              \
    static void name##_stepped_opaque(                                                             \
        uint8_t *const d,                                                                          \
        const Pixels *const s,                                                                     \
        const Steps st,                                                                            \
        const int32_t n                                                                            \
    )                                                                                              \
    {                                                                                              \
        steps_of(f, d, s, st, n, false);                                                           \
    }                                                                                              \
    static void name##_ink(                                                                        \
        uint8_t *const d,                                                                          \
        const uint32_t *const texels,                                                              \
        const int32_t n,                                                                           \
        const Ink *const k                                                                         \
    )                                                                                              \
    {                                                                                              \
        ink_of(f, d, texels, n, k);                                                                \
    }                                                                                              \
    static uint32_t name##_paint(const uint32_t texel, const Ink *const k)                         \
    {                                                                                              \
        return paint_of(f, texel, k);                                                              \
    }                                                                                              \
    static void name##_tint(                                                                       \
        uint8_t *const d,                                                                          \
        const int32_t n,                                                                           \
        const uint32_t colour,                                                                     \
        const Ink *const k,                                                                        \
        FillTints *const tints                                                                     \
    )                                                                                              \
    {                                                                                              \
        tint_of(f, d, n, colour, k, tints);                                                        \
    }                                                                                              \
    const Fill name = {                                                                            \
        .span = name##_span,                                                                       \
        .row =                                                                                     \
            {                                                                                      \
                [0] = name##_keyed,                                                                \
                [FILL_FLIPX] = name##_keyed_flipped,                                               \
                [FILL_OPAQUE] = name##_opaque,                                                     \
                [FILL_OPAQUE | FILL_FLIPX] = name##_opaque_flipped,                                \
            },                                                                                     \
        .step = {[0] = name##_stepped_keyed, [FILL_OPAQUE] = name##_stepped_opaque},               \
        .ink = name##_ink,                                                                         \
        .paint = name##_paint,                                                                     \
        .inked = fill_inked[f],                                                                    \
        .tint = name##_tint,                                                                       \
    }

#define FILL_FORMAT(t, f, ...) FILL_TABLE(t, f);
#define FILL_ENTRY(t, f, ...) [f] = &t,

FILL_TARGETS(FILL_FORMAT)

const Fill *const fill_formats[PX_FORMATS] = {FILL_TARGETS(FILL_ENTRY)};
