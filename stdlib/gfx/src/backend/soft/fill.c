/* The fillers (fill.h): one body for every format, compiled for each with the formats and the
   variant constants, so no pixel tests a flag. FILL_TABLE binds them into a format's table. */
#include "core/mem.h"
#include "backend/soft/fill.h"
#include "backend/soft/scanner.h"

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

#if !defined(__i386__)
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

/* n pixels into d over the depth zp from z by dz along the read, f's texels: each shown only nearer
   when z tests, and its depth set when z writes, a keyed texel neither. */
[[gnu::always_inline]]
static inline void deep_of(
    const PxFormat f,
    const bool key,
    const uint32_t z,
    uint8_t *const d,
    uint16_t *const zp,
    StepsRead *const rd,
    const int32_t n,
    int32_t zd,
    const int32_t dz
)
{
    const uint32_t b = px_bytes(f);

    for (int32_t i = 0; i < n; i++, zd = depth_on(zd, dz, 1)) {
        const uint32_t t = steps_read_next(rd, b);
        const uint16_t here = depth_of(zd);

        if ((z & ROW_TEST && !depth_shows(zp[i], here)) || (key && px_key(f, t)))
            continue;

        px_store(d + i * b, t, b);

        if (z & ROW_WRITE)
            zp[i] = here;
    }
}

#endif

/* deep_of's, each texel held inside s: a walk that leaves it. */
[[gnu::always_inline]]
static inline void held_of(
    const PxFormat f,
    const bool key,
    const uint32_t z,
    uint8_t *const d,
    uint16_t *const zp,
    const Pixels *const s,
    Steps st,
    const int32_t n,
    int32_t zd,
    const int32_t dz
)
{
    const uint32_t b = px_bytes(f);

    for (int32_t i = 0; i < n; i++, steps_on(&st), zd = depth_on(zd, dz, 1)) {
        const uint16_t here = depth_of(zd);

        if (z & ROW_TEST && !depth_shows(zp[i], here))
            continue;

        const uint32_t t = steps_texel(s, &st);

        if (key && px_key(f, t))
            continue;

        px_store(d + i * b, t, b);

        if (z & ROW_WRITE)
            zp[i] = here;
    }
}

#if defined(__i386__)
/* The walks in asm, uGL's hlinet8 and hlinetp8_z. A plain walk's loop: esi the texel's place and
   edx u's and v's fractions (StepsRead's), ecx the pixel's index up from -n to the row's end in
   edi, its depth's in ebp. A pixel's depth, z's high half, is tested before its texel is read; the
   place steps a row on v's borrow and a texel on u's carry. Every register is the loop's, so all
   else it reads is in walk_frame, at its absolute place. */
typedef struct {
    uint32_t dv;        /* v's step's fraction, its low half 0 */
    uint32_t du;        /* u's step's fraction, the low half */
    int32_t steps[2];   /* the place's step a pixel: a row more on v's borrow, or not */
    int32_t z, dz;      /* the depth, 16.16, and its step a pixel */
    int32_t n, done, m; /* a perspective row's pixels, those walked, and its run's */
    int32_t pu, pv;     /* u and v at the run's first pixel, 16.16 */
    uint32_t sw, sh;    /* the texture's sides, 16.16; 0 when one reaches 32768: every run held */
    int32_t pitch;      /* in texels */
    uintptr_t at;       /* the texture's first texel's place, in texels */
    uint8_t *d;         /* the row's first pixel */
    uint16_t *zp;       /* and its depth */
    int32_t held[4];    /* the run left to held_of: its u, v, du and dv */
    float dpun, dpvn, dpzn, unit; /* unit: 65536 */
    uint8_t uvw[30]; /* u/z, v/z and 1/z as the x87 holds them, 80 bits each, across a held run */
    double u, v, w;  /* u/z, v/z and 1/z at the row's first pixel */
} WalkFrame;

/* clang-format off */

/* WalkFrame's places, the asm's names for them. */
#define WF_dv 0
#define WF_du 4
#define WF_steps 8
#define WF_z 16
#define WF_here 18
#define WF_dz 20
#define WF_n 24
#define WF_done 28
#define WF_m 32
#define WF_pu 36
#define WF_pv 40
#define WF_sw 44
#define WF_sh 48
#define WF_pitch 52
#define WF_at 56
#define WF_d 60
#define WF_zp 64
#define WF_held 68
#define WF_dpun 84
#define WF_dpvn 88
#define WF_dpzn 92
#define WF_unit 96
#define WF_uvw 100
#define WF_u 136
#define WF_v 144
#define WF_w 152
#define WF_CHECK(f) static_assert(offsetof(WalkFrame, f) == WF_##f);
WF_CHECK(dv) WF_CHECK(du) WF_CHECK(steps) WF_CHECK(z) WF_CHECK(dz) WF_CHECK(n) WF_CHECK(done)
WF_CHECK(m) WF_CHECK(pu) WF_CHECK(pv) WF_CHECK(sw) WF_CHECK(sh) WF_CHECK(pitch) WF_CHECK(at)
WF_CHECK(d) WF_CHECK(zp) WF_CHECK(held) WF_CHECK(dpun) WF_CHECK(dpvn) WF_CHECK(dpzn)
WF_CHECK(unit) WF_CHECK(uvw) WF_CHECK(u) WF_CHECK(v) WF_CHECK(w)
#define WF_STR(n) #n
#define WF_AT(n) WF_STR(n)
/* Field f of walk_frame, operand [f]; at + bytes past it. */
#define WF(f) WF_AT(WF_##f) "+%[f]"
#define WFP(f, at) WF_AT(WF_##f) "+" #at "+%[f]"

static WalkFrame walk_frame = {.unit = 65536.0f};

#define WALK_TEXEL_1 "movzbl (%%esi), %%ebx\n\t"
#define WALK_TEXEL_2 "movzwl (,%%esi,2), %%ebx\n\t"
#define WALK_TEXEL_4 "movl (,%%esi,4), %%ebx\n\t"
#define WALK_KEY_0_1 ""
#define WALK_KEY_0_2 ""
#define WALK_KEY_0_4 ""
#define WALK_KEY_1_1 "testb %%bl, %%bl\n\tjz 2f\n\t"
#define WALK_KEY_1_2 "testw %%bx, %%bx\n\tjz 2f\n\t"
#define WALK_KEY_1_4 "testl %%ebx, %%ebx\n\tjns 2f\n\t"
#define WALK_STORE_1 "movb %%bl, (%%edi,%%ecx)\n\t"
#define WALK_STORE_2 "movw %%bx, (%%edi,%%ecx,2)\n\t"
#define WALK_STORE_4 "movl %%ebx, (%%edi,%%ecx,4)\n\t"
#define WALK_HERE "movw " WF(here) ", %%ax\n\tmovl " WF(dz) ", %%ebx\n\taddl %%ebx, " WF(z) "\n\t"
#define WALK_HERE_0 ""
#define WALK_HERE_1 WALK_HERE
#define WALK_HERE_2 WALK_HERE
#define WALK_HERE_3 WALK_HERE
#define WALK_TEST "cmpw %%ax, (%%ebp,%%ecx,2)\n\tjae 2f\n\t"
#define WALK_TEST_0 ""
#define WALK_TEST_1 WALK_TEST
#define WALK_TEST_2 ""
#define WALK_TEST_3 WALK_TEST
#define WALK_WRITE "movw %%ax, (%%ebp,%%ecx,2)\n\t"
#define WALK_WRITE_0 ""
#define WALK_WRITE_1 ""
#define WALK_WRITE_2 WALK_WRITE
#define WALK_WRITE_3 WALK_WRITE
#define WALK_STEP                                                                                  \
    "2:\n\t"                                                                                       \
    "addl " WF(dv) ", %%edx\n\t"                                                                   \
    "sbbl %%ebx, %%ebx\n\t"                                                                        \
    "addw " WF(du) ", %%dx\n\t"                                                                    \
    "adcl " WFP(steps, 4) "(,%%ebx,4), %%esi\n\t"                                                  \
    "incl %%ecx\n\t"                                                                               \
    "jnz 1b\n\t"
#define WALK_LOOP(b, k, z)                                                                         \
    "1:\n\t" WALK_HERE_##z WALK_TEST_##z WALK_TEXEL_##b WALK_KEY_##k##_##b WALK_STORE_##b          \
        WALK_WRITE_##z WALK_STEP
#define WALK_KEYS(X, ...) X(__VA_ARGS__, 0) X(__VA_ARGS__, 1)
#define WALK_VARIANTS(X)                                                                           \
    WALK_KEYS(X, 1, 0) WALK_KEYS(X, 1, 1) WALK_KEYS(X, 1, 2) WALK_KEYS(X, 1, 3)                    \
    WALK_KEYS(X, 2, 0) WALK_KEYS(X, 2, 1) WALK_KEYS(X, 2, 2) WALK_KEYS(X, 2, 3)                    \
    WALK_KEYS(X, 4, 0) WALK_KEYS(X, 4, 1) WALK_KEYS(X, 4, 2) WALK_KEYS(X, 4, 3)

#define WALK_PLAIN(B, Z, K)                                                                        \
    if (b == B && key == K && z == Z) {                                                            \
        __asm__ volatile("movl %[zend], %%ebp\n\t" WALK_LOOP(B, K, Z)                              \
                         : "+S"(at), "+d"(uv), "+c"(i), "+D"(end), [f] "+m"(walk_frame)            \
                         : [zend] "m"(zend)                                                        \
                         : "eax", "ebx", "ebp", "cc", "memory");                                   \
        return;                                                                                    \
    }
/* clang-format on */

/* A plain walk of n >= 1 pixels from texel place at and fractions uv, keyed or not, in depth mode
   z, to the pixels before end and depths before zend: walk_frame's steps and depth set. */
[[gnu::always_inline]]
static inline void walk_plain(
    const uint32_t b,
    const bool key,
    const uint32_t z,
    uintptr_t at,
    uint32_t uv,
    const int32_t n,
    uint8_t *end,
    uint16_t *const zend
)
{
    int32_t i = -n;

    WALK_VARIANTS(WALK_PLAIN)
}

/* clang-format off */
/* A perspective row's runs, from walk_frame's done, as persp_row's: u, v and w on the x87, its
   divide at each run's end as clang's, so the same bits; each run's steps and whether it stays
   inside the texture as steps_inside's in 32 bits; a plain one walked, a held one stopping the
   walk with its steps in walk_frame's held, u, v and w kept in uvw. First, from u, v and w; again,
   past the held run, from uvw. */
#define WALK_ENTRY                                                                                 \
    "cmpl $0, " WF(done) "\n\t"                                                                    \
    "jne 6f\n\t"                                                                                   \
    "fldl " WF(w) "\n\t"                                                                           \
    "fldl " WF(v) "\n\t"                                                                           \
    "fldl " WF(u) "\n\t"                                                                           \
    "fld %%st(2)\n\t"                                                                              \
    "fdivrs " WF(unit) "\n\t"                                                                      \
    "fld %%st(1)\n\t"                                                                              \
    "fmul %%st(1), %%st\n\t"                                                                       \
    "fistpl " WF(pu) "\n\t"                                                                        \
    "fmul %%st(2), %%st\n\t"                                                                       \
    "fistpl " WF(pv) "\n\t"                                                                        \
    "addl $32768, " WF(pu) "\n\t"                                                                  \
    "addl $32768, " WF(pv) "\n\t"                                                                  \
    "jmp 4f\n"                                                                                     \
    "6:\n\t"                                                                                       \
    "fldt " WFP(uvw, 20) "\n\t"                                                                    \
    "fldt " WFP(uvw, 10) "\n\t"                                                                    \
    "fldt " WF(uvw) "\n"                                                                           \
    "4:\n\t"
/* m, the divide (u, v and w on: st0 u, st1 v, st2 w), du in eax and dv in ebx from pu in esi and
   pv in edi, then whether the walk leaves the texture: 7f if it does. */
#define WALK_DIVIDE                                                                                \
    "movl " WF(n) ", %%ecx\n\t"                                                                    \
    "subl " WF(done) ", %%ecx\n\t"                                                                 \
    "cmpl %[p], %%ecx\n\t"                                                                         \
    "jle 5f\n\t"                                                                                   \
    "movl %[p], %%ecx\n"                                                                           \
    "5:\n\t"                                                                                       \
    "movl %%ecx, " WF(m) "\n\t"                                                                    \
    "movl " WF(pu) ", %%esi\n\t"                                                                   \
    "movl " WF(pv) ", %%edi\n\t"                                                                   \
    "fadds " WF(dpun) "\n\t"                                                                       \
    "fxch %%st(1)\n\t"                                                                             \
    "fadds " WF(dpvn) "\n\t"                                                                       \
    "fxch %%st(2)\n\t"                                                                             \
    "fadds " WF(dpzn) "\n\t"                                                                       \
    "fld %%st(0)\n\t"                                                                              \
    "fdivrs " WF(unit) "\n\t"                                                                      \
    "fld %%st(2)\n\t"                                                                              \
    "fmul %%st(1), %%st\n\t"                                                                       \
    "fistpl " WF(pu) "\n\t"                                                                        \
    "fmul %%st(3), %%st\n\t"                                                                       \
    "fistpl " WF(pv) "\n\t"                                                                        \
    "fxch %%st(2)\n\t"                                                                             \
    "fxch %%st(1)\n\t"                                                                             \
    "movl " WF(pu) ", %%eax\n\t"                                                                   \
    "addl $32768, %%eax\n\t"                                                                       \
    "movl %%eax, " WF(pu) "\n\t"                                                                   \
    "movl " WF(pv) ", %%ebx\n\t"                                                                   \
    "addl $32768, %%ebx\n\t"                                                                       \
    "movl %%ebx, " WF(pv) "\n\t"                                                                   \
    "subl %%esi, %%eax\n\t"                                                                        \
    "sarl %[s], %%eax\n\t"                                                                         \
    "subl %%edi, %%ebx\n\t"                                                                        \
    "sarl %[s], %%ebx\n\t"                                                                         \
    "leal -1(%%ecx), %%edx\n\t"                                                                    \
    "movl %%eax, %%ebp\n\t"                                                                        \
    "imull %%edx, %%ebp\n\t"                                                                       \
    "jo 7f\n\t"                                                                                    \
    "addl %%esi, %%ebp\n\t"                                                                        \
    "jo 7f\n\t"                                                                                    \
    "cmpl " WF(sw) ", %%ebp\n\t"                                                                   \
    "jae 7f\n\t"                                                                                   \
    "cmpl " WF(sw) ", %%esi\n\t"                                                                   \
    "jae 7f\n\t"                                                                                   \
    "movl %%ebx, %%ebp\n\t"                                                                        \
    "imull %%edx, %%ebp\n\t"                                                                       \
    "jo 7f\n\t"                                                                                    \
    "addl %%edi, %%ebp\n\t"                                                                        \
    "jo 7f\n\t"                                                                                    \
    "cmpl " WF(sh) ", %%ebp\n\t"                                                                   \
    "jae 7f\n\t"                                                                                   \
    "cmpl " WF(sh) ", %%edi\n\t"                                                                   \
    "jae 7f\n\t"
/* A plain run's steps, place and fractions (steps_by's and steps_read's), done on by m, and the
   ends of its b-byte pixels and its depths. */
#define WALK_SETUP(b)                                                                              \
    "movl %%eax, " WF(du) "\n\t"                                                                   \
    "movl %%ebx, %%edx\n\t"                                                                        \
    "shll $16, %%edx\n\t"                                                                          \
    "movl %%edx, " WF(dv) "\n\t"                                                                   \
    "sarl $16, %%ebx\n\t"                                                                          \
    "imull " WF(pitch) ", %%ebx\n\t"                                                               \
    "sarl $16, %%eax\n\t"                                                                          \
    "addl %%eax, %%ebx\n\t"                                                                        \
    "movl %%ebx, " WFP(steps, 4) "\n\t"                                                            \
    "addl " WF(pitch) ", %%ebx\n\t"                                                                \
    "movl %%ebx, " WF(steps) "\n\t"                                                                \
    "movl %%edi, %%eax\n\t"                                                                        \
    "sarl $16, %%eax\n\t"                                                                          \
    "imull " WF(pitch) ", %%eax\n\t"                                                               \
    "addl " WF(at) ", %%eax\n\t"                                                                   \
    "movl %%esi, %%edx\n\t"                                                                        \
    "sarl $16, %%edx\n\t"                                                                          \
    "addl %%edx, %%eax\n\t"                                                                        \
    "movl %%edi, %%edx\n\t"                                                                        \
    "shll $16, %%edx\n\t"                                                                          \
    "movw %%si, %%dx\n\t"                                                                          \
    "movl %%eax, %%esi\n\t"                                                                        \
    "movl " WF(done) ", %%eax\n\t"                                                                 \
    "addl %%ecx, %%eax\n\t"                                                                        \
    "movl %%eax, " WF(done) "\n\t"                                                                 \
    "negl %%ecx\n\t"                                                                               \
    "movl " WF(d) ", %%edi\n\t"                                                                    \
    "leal (%%edi,%%eax," #b "), %%edi\n\t"
#define WALK_ZEND "movl " WF(zp) ", %%ebp\n\tleal (%%ebp,%%eax,2), %%ebp\n\t"
#define WALK_ZEND_0 ""
#define WALK_ZEND_1 WALK_ZEND
#define WALK_ZEND_2 WALK_ZEND
#define WALK_ZEND_3 WALK_ZEND
/* The next run, or the row done: u, v and w popped. A held run: they kept, and its steps. */
#define WALK_NEXT                                                                                  \
    "movl " WF(done) ", %%eax\n\t"                                                                 \
    "cmpl " WF(n) ", %%eax\n\t"                                                                    \
    "jl 4b\n\t"                                                                                    \
    "fstp %%st(0)\n\t"                                                                             \
    "fstp %%st(0)\n\t"                                                                             \
    "fstp %%st(0)\n\t"                                                                             \
    "jmp 8f\n"                                                                                     \
    "7:\n\t"                                                                                       \
    "fstpt " WF(uvw) "\n\t"                                                                        \
    "fstpt " WFP(uvw, 10) "\n\t"                                                                   \
    "fstpt " WFP(uvw, 20) "\n\t"                                                                   \
    "movl %%esi, " WF(held) "\n\t"                                                                 \
    "movl %%edi, " WFP(held, 4) "\n\t"                                                             \
    "movl %%eax, " WFP(held, 8) "\n\t"                                                             \
    "movl %%ebx, " WFP(held, 12) "\n"                                                              \
    "8:"
#define WALK_PERSP(B, Z, K)                                                                        \
    if (b == B && key == K && z == Z) {                                                            \
        __asm__ volatile(WALK_ENTRY WALK_DIVIDE WALK_SETUP(B) WALK_ZEND_##Z WALK_LOOP(B, K, Z)     \
                             WALK_NEXT                                                             \
                         : [f] "+m"(walk_frame)                                                    \
                         : [p] "i"(SCAN_SUBDIVP),                                                  \
                           [s] "i"(SCAN_SUBDIVS)                                                   \
                         : "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "cc", "memory", "st",  \
                           "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)");         \
        return;                                                                                    \
    }
/* clang-format on */

[[gnu::always_inline]]
static inline void walk_persp(const uint32_t b, const bool key, const uint32_t z)
{
    WALK_VARIANTS(WALK_PERSP)
}
#endif

/* A row's, read by its runs, taken from Rows once a row: an asm run's memory clobber reloads none
   of it. */
typedef struct {
    uint8_t *d;
    uint16_t *zp;
    Pixels src;
    int32_t dz;
} Row;

static inline Row row_of_rows(
    const PxFormat f,
    const uint32_t z,
    const Rows *const r,
    const int32_t y,
    const int32_t x
)
{
    return (Row){
        r->dst.p + y * r->dst.pitch + (uint32_t)x * px_bytes(f),
        z ? r->depth.p + y * r->depth.row + x : nullptr,
        r->src,
        r->dz,
    };
}

/* A run of n pixels at x of the row along st, f's texels: deep_of's, steps_of's without depth, or
   held_of's. */
[[gnu::always_inline]]
static inline void run_at(
    const PxFormat f,
    const bool key,
    const uint32_t z,
    const bool held,
    const Row *const w,
    const int32_t x,
    const int32_t n,
    const Steps st,
    const int32_t zd
)
{
    const uint32_t b = px_bytes(f);
    uint8_t *const d = w->d + (uint32_t)x * b;
    uint16_t *const zp = z ? w->zp + x : nullptr;

    if (held)
        held_of(f, key, z, d, zp, &w->src, st, n, zd, w->dz);
#if defined(__i386__)
    else if (n > 0) {
        const StepsBy by = steps_by(&w->src, st, b);
        const StepsRead rd = steps_read(&w->src, st, &by, b);

        walk_frame.dv = by.dv;
        walk_frame.du = by.du;
        walk_frame.steps[0] = (int32_t)(by.step + by.pitch);
        walk_frame.steps[1] = (int32_t)by.step;
        walk_frame.z = zd;
        walk_frame.dz = w->dz;
        walk_plain(b, key, z, rd.at, rd.uv, n, d + (uint32_t)n * b, z ? zp + n : nullptr);
    }
#else
    else if (!z)
        steps_of(f, d, &w->src, st, n, key);
    else {
        const StepsBy by = steps_by(&w->src, st, b);
        StepsRead rd = steps_read(&w->src, st, &by, b);

        deep_of(f, key, z, d, zp, &rd, n, zd, w->dz);
    }
#endif
}

/* RowAffine's, f's texels into f. */
[[gnu::always_inline]]
static inline void affine_row(
    const PxFormat f,
    const bool key,
    const uint32_t z,
    const uint32_t walk,
    const Rows *const r,
    const int32_t y,
    const int32_t x0,
    const int32_t x1,
    const int32_t u,
    const int32_t v,
    const int32_t zd
)
{
    const Steps st = walk == ROW_WRAPPED ? steps_from(u, v, r->du, r->dv, r->wrap, r->wrap_v)
                                         : (Steps){u, v, r->du, r->dv, 0, 0};

    const Row w = row_of_rows(f, z, r, y, x0);

    run_at(f, key, z, walk != ROW_PLAIN, &w, 0, x1 - x0, st, zd);
}

/* RowPersp's, f's texels into f: runs of SCAN_SUBDIVP, each a walk between two divides, held where
   it leaves the texture. */
[[gnu::always_inline]]
static inline void persp_row(
    const PxFormat f,
    const bool key,
    const uint32_t z,
    const Rows *const r,
    const int32_t y,
    const int32_t x0,
    const int32_t x1,
    double u,
    double v,
    double w,
    const int32_t zd
)
{
    const int32_t n = x1 - x0;
    const Row row = row_of_rows(f, z, r, y, x0);
#if defined(__i386__)
    /* in asm, but a held run: held_of's, then the asm again past it */
    const uint32_t b = px_bytes(f);
    const bool small = row.src.w < 32768 && row.src.h < 32768;

    if (n <= 0)
        return;

    walk_frame.n = n;
    walk_frame.done = 0;
    walk_frame.z = zd;
    walk_frame.dz = row.dz;
    walk_frame.sw = small ? (uint32_t)row.src.w << 16 : 0;
    walk_frame.sh = small ? (uint32_t)row.src.h << 16 : 0;
    walk_frame.pitch = (int32_t)(row.src.pitch / b);
    walk_frame.at = (uintptr_t)row.src.p / b;
    walk_frame.d = row.d;
    walk_frame.zp = row.zp;
    walk_frame.dpun = r->dpun;
    walk_frame.dpvn = r->dpvn;
    walk_frame.dpzn = r->dpzn;
    walk_frame.u = u;
    walk_frame.v = v;
    walk_frame.w = w;

    for (;;) {
        walk_persp(b, key, z);

        const int32_t at = walk_frame.done, m = walk_frame.m;

        if (at >= n)
            return;

        const int32_t *const h = walk_frame.held;
        const Steps st = steps_from(h[0], h[1], h[2], h[3], r->wrap, r->wrap_v);

        held_of(
            f,
            key,
            z,
            row.d + (uint32_t)at * b,
            z ? row.zp + at : nullptr,
            &row.src,
            st,
            m,
            walk_frame.z,
            row.dz
        );
        walk_frame.z = depth_on(walk_frame.z, row.dz, m);
        walk_frame.done = at + m;

        if (at + m >= n)
            return;
    }
#else

    const float dpun = r->dpun, dpvn = r->dpvn, dpzn = r->dpzn;
    ScanUV p = scan_pdiv(u, v, w);

    for (int32_t done = 0; done < n; done += SCAN_SUBDIVP) {
        const int32_t m = n - done < SCAN_SUBDIVP ? n - done : SCAN_SUBDIVP;

        u += dpun;
        v += dpvn;
        w += dpzn;

        const ScanUV l = scan_pdiv(u, v, w);
        const Steps st = steps_from(
            p.u, p.v, scan_step(p.u, l.u), scan_step(p.v, l.v), r->wrap, r->wrap_v
        );

        const int32_t at = depth_on(zd, row.dz, done);

        if (steps_inside(&st, &row.src, m))
            run_at(f, key, z, false, &row, done, m, st, at);
        else
            run_at(f, key, z, true, &row, done, m, st, at);

        p = l;
    }
#endif
}

/* ---- SMOOTH's and the general row fillers ---- */

/* The texel at u, v, whole texels, held inside s; each wrapped when it wraps. */
static uint32_t texel_at(
    const Pixels *const s,
    const int32_t u,
    const int32_t v,
    const bool wrap,
    const bool wrap_v
)
{
    const uint32_t bytes = px_bytes(s->format);
    const int32_t across = wrap ? steps_wrap(u, s->w) : steps_held(u, s->w);
    const int32_t down = wrap_v ? steps_wrap(v, s->h) : steps_held(v, s->h);

    return px_load(s->p + down * s->pitch + (uint32_t)across * bytes, bytes);
}

/* The four texels from u, v to u + 1, v + 1, whole texels, of s, g's: read from one place when
   all are inside, else each held or wrapped. */
[[gnu::always_inline]]
static inline void corners(
    uint32_t *const out,
    const PxFormat g,
    const Pixels *const s,
    const int32_t u,
    const int32_t v,
    const bool wrap,
    const bool wrap_v
)
{
    const uint32_t bytes = px_bytes(g);

    if ((uint32_t)u < (uint32_t)(s->w - 1) && (uint32_t)v < (uint32_t)(s->h - 1)) {
        const uint8_t *const p = s->p + (uint32_t)v * s->pitch + (uint32_t)u * bytes;

        out[0] = px_load(p, bytes);
        out[1] = px_load(p + bytes, bytes);
        out[2] = px_load(p + s->pitch, bytes);
        out[3] = px_load(p + s->pitch + bytes, bytes);
        return;
    }

    out[0] = texel_at(s, u, v, wrap, wrap_v);
    out[1] = texel_at(s, u + 1, v, wrap, wrap_v);
    out[2] = texel_at(s, u, v + 1, wrap, wrap_v);
    out[3] = texel_at(s, u + 1, v + 1, wrap, wrap_v);
}

/* A colour's channels, 0 to 1, and its coverage: 1 drawn, 0 keyed. */
typedef struct {
    float r, g, b, a;
} Mix;

/* A channel, 0 to 1: c times a 255th, made once. */
#define UNIT(c) (float)(c) * (1.0f / 255),
#define UNITS4(c) UNIT(c) UNIT(c + 1) UNIT(c + 2) UNIT(c + 3)
#define UNITS16(c) UNITS4(c) UNITS4(c + 4) UNITS4(c + 8) UNITS4(c + 12)
#define UNITS64(c) UNITS16(c) UNITS16(c + 16) UNITS16(c + 32) UNITS16(c + 48)
static const float units[256] = {UNITS64(0) UNITS64(64) UNITS64(128) UNITS64(192)};

[[gnu::always_inline]]
static inline Mix mix_of(const uint32_t argb, const bool drawn)
{
    if (!drawn)
        return (Mix){0, 0, 0, 0};

    return (Mix){units[argb >> 16 & 0xFF], units[argb >> 8 & 0xFF], units[argb & 0xFF], 1};
}

/* GLSL's mix: a to b by f. */
[[gnu::always_inline]]
static inline Mix mix(const Mix a, const Mix b, const float f)
{
    const float e = 1 - f;
    return (Mix){a.r * e + b.r * f, a.g * e + b.g * f, a.b * e + b.b * f, a.a * e + b.a * f};
}

/* Four texels' ARGB c, those drawn, mixed by fu across and fv down as the GPU's shader mixes
   them: a keyed texel adds nothing. */
[[gnu::always_inline]]
static inline Mix mixed_four(
    const uint32_t *const c,
    const bool *const drawn,
    const float fu,
    const float fv
)
{
    const Mix top = mix(mix_of(c[0], drawn[0]), mix_of(c[1], drawn[1]), fu);
    const Mix under = mix(mix_of(c[2], drawn[2]), mix_of(c[3], drawn[3]), fu);
    return mix(top, under, fv);
}

static uint32_t channel(const float c) { return (uint32_t)(c * 255 + 0.5f); }

static uint32_t opaque(const float r, const float g, const float b)
{
    return PX_OPAQUE | channel(r) << 16 | channel(g) << 8 | channel(b);
}

/* mixed's colour divided by its coverage into *argb: false, under half covered, not drawn. */
static bool partly(
    const uint32_t *const c,
    const bool *const drawn,
    const float fu,
    const float fv,
    uint32_t *const argb
)
{
    const Mix m = mixed_four(c, drawn, fu, fv);

    if (m.a < 0.5f)
        return false;

    *argb = opaque(m.r / m.a, m.g / m.a, m.b / m.a);
    return true;
}

/* partly's, four drawn: they cover it all, exactly 1, so none is divided. */
[[gnu::always_inline]]
static inline bool covered(
    const uint32_t *const c,
    const bool *const drawn,
    const float fu,
    const float fv,
    uint32_t *const argb
)
{
    if (!(drawn[0] & drawn[1] & drawn[2] & drawn[3]))
        return partly(c, drawn, fu, fv, argb);

    const Mix m = mixed_four(c, (const bool[]){true, true, true, true}, fu, fv);

    *argb = opaque(m.r, m.g, m.b);
    return true;
}

/* The first of m pixels from i not shown: m, none. */
static int32_t shown_to(const bool *const shown, int32_t i, const int32_t m)
{
    while (i < m && shown[i])
        i++;

    return i;
}

enum { SMOOTH_CHUNK = 64 };

/* n pixels at x of row y SMOOTH's way, g's texels into f, SMOOTH_CHUNK at a time: the four
   texels round each pixel's place through the ink but its blend, mixed, the place half a texel
   back and its fractions the weights; the mixes shown over the depth there drawn a run at a time,
   their depth set. A SOLID mix is its colour, where covered. */
[[gnu::always_inline]]
static inline void smooth_of(
    const PxFormat f,
    const PxFormat g,
    const Rows *const r,
    const int32_t y,
    const int32_t px,
    const int32_t n,
    Steps st,
    int32_t z
)
{
    const uint32_t bytes = px_bytes(f);
    const int32_t dz = r->dz;
    uint16_t *const d = r->depths ? r->depth.p + y * r->depth.row + px : nullptr;
    uint8_t *const out = r->dst.p + y * r->dst.pitch + (uint32_t)px * bytes;
    Ink k = *r->ink, ink = *r->ink;
    k.source = g;
    ink.keyed = false;

    if (!k.solid) {
        ink.source = PX_32;
        ink.shade = PX_WHITE;
    }

    for (int32_t at = 0; at < n; at += SMOOTH_CHUNK) {
        const int32_t m = n - at < SMOOTH_CHUNK ? n - at : SMOOTH_CHUNK;
        uint32_t texels[4 * SMOOTH_CHUNK], c[4 * SMOOTH_CHUNK], argb[SMOOTH_CHUNK];
        bool drawn[4 * SMOOTH_CHUNK], shown[SMOOTH_CHUNK];
        float fu[SMOOTH_CHUNK], fv[SMOOTH_CHUNK];

        for (int32_t i = 0; i < m; i++, steps_on(&st)) {
            const int32_t pu = (int32_t)((uint32_t)st.u - 0x8000u),
                          pv = (int32_t)((uint32_t)st.v - 0x8000u);

            fu[i] = (float)(pu & 0xFFFF) / 65536;
            fv[i] = (float)(pv & 0xFFFF) / 65536;
            corners(texels + 4 * i, g, &r->src, pu >> 16, pv >> 16, r->wrap, r->wrap_v);
        }

        fill_argb(c, drawn, texels, 4 * m, &k);

        for (int32_t i = 0; i < m; i++, z = depth_on(z, dz, 1)) {
            const uint16_t here = depth_of(z);
            uint16_t *const there = d ? d + at + i : nullptr;

            shown[i] = (!there || !(r->depths & ROW_TEST) || depth_shows(*there, here)) &&
                       covered(c + 4 * i, drawn + 4 * i, fu[i], fv[i], argb + i);

            if (shown[i] && there && r->depths & ROW_WRITE)
                *there = here;
        }

        for (int32_t i = 0; i < m;) {
            const int32_t end = shown_to(shown, i, m);

            if (end > i)
                ink_of(f, out + (at + i) * (int32_t)bytes, argb + i, end - i, &ink);

            i = end + 1;
        }
    }
}

/* smooth_of, the source's body picked once a row. */
[[gnu::always_inline]]
static inline void smooth_row(
    const PxFormat f,
    const Rows *const r,
    const int32_t y,
    const int32_t x,
    const int32_t n,
    const Steps st,
    const int32_t z
)
{
    switch (r->src.format) {
    case PX_8:
        smooth_of(f, PX_8, r, y, x, n, st, z);
        break;

    case PX_15:
        smooth_of(f, PX_15, r, y, x, n, st, z);
        break;

    case PX_16:
        smooth_of(f, PX_16, r, y, x, n, st, z);
        break;

    default:
        smooth_of(f, PX_32, r, y, x, n, st, z);
    }
}

/* n pixels at x of row y along st, g's texels into f through the ink a pixel at a time: each texel
   held inside the texture, u wrapped when it wraps; shown only nearer and its depth set as Rows'
   depths say, a keyed texel neither. */
[[gnu::always_inline]]
static inline void general_of(
    const PxFormat f,
    const PxFormat g,
    const Rows *const r,
    const int32_t y,
    const int32_t x,
    const int32_t n,
    Steps st,
    int32_t z
)
{
    const uint32_t b = px_bytes(f), depths = r->depths;
    const int32_t dz = r->dz;
    const Ink k = *r->ink; /* a pixel stored may be the ink's, for all the compiler knows */
    const bool blended = k.blend || k.mode;
    uint8_t *const d = r->dst.p + y * r->dst.pitch + (uint32_t)x * b;
    uint16_t *const zp = depths ? r->depth.p + y * r->depth.row + x : nullptr;

    for (int32_t i = 0; i < n; i++, steps_on(&st), z = depth_on(z, dz, 1)) {
        const uint16_t here = depth_of(z);

        if (depths & ROW_TEST && !depth_shows(zp[i], here))
            continue;

        const uint32_t t = steps_texel(&r->src, &st);

        if (k.keyed && px_key(g, t))
            continue;

        uint8_t *const p = d + i * (int32_t)b;

        px_store(p, ink_pixel(f, g, FILL_ANY, &k, t, blended ? px_load(p, b) : 0), b);

        if (depths & ROW_WRITE)
            zp[i] = here;
    }
}

/* general_of, the source's body picked once a row. */
[[gnu::always_inline]]
static inline void general_row(
    const PxFormat f,
    const Rows *const r,
    const int32_t y,
    const int32_t x,
    const int32_t n,
    const Steps st,
    const int32_t z
)
{
    switch (r->src.format) {
    case PX_8:
        general_of(f, PX_8, r, y, x, n, st, z);
        break;

    case PX_15:
        general_of(f, PX_15, r, y, x, n, st, z);
        break;

    case PX_16:
        general_of(f, PX_16, r, y, x, n, st, z);
        break;

    default:
        general_of(f, PX_32, r, y, x, n, st, z);
    }
}

/* A run of n pixels at x of row y along st, the general filler's or SMOOTH's: one body a format,
   shared by its affine and perspective rows. */
typedef void FillRun(const Rows *r, int32_t y, int32_t x, int32_t n, Steps st, int32_t z);

/* A perspective row as persp_row walks it, each run drawn by run. */
[[gnu::always_inline]]
static inline void persp_runs(
    FillRun *const run,
    const Rows *const r,
    const int32_t y,
    const int32_t x0,
    const int32_t x1,
    double u,
    double v,
    double w,
    const int32_t zd
)
{
    const int32_t n = x1 - x0;
    ScanUV p = scan_pdiv(u, v, w);

    for (int32_t done = 0; done < n; done += SCAN_SUBDIVP) {
        const int32_t m = n - done < SCAN_SUBDIVP ? n - done : SCAN_SUBDIVP;

        u += r->dpun;
        v += r->dpvn;
        w += r->dpzn;

        const ScanUV l = scan_pdiv(u, v, w);
        const Steps st = steps_from(
            p.u, p.v, scan_step(p.u, l.u), scan_step(p.v, l.v), r->wrap, r->wrap_v
        );
        const int32_t at = depth_on(zd, r->dz, done);

        run(r, y, x0 + done, m, st, at);
        p = l;
    }
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

/* RowFlat's, r's pixel: each shown only nearer and its depth set as z says. */
[[gnu::always_inline]]
static inline void flat_row(
    const PxFormat f,
    const uint32_t z,
    const Rows *const r,
    const int32_t y,
    const int32_t x0,
    const int32_t x1,
    int32_t zd
)
{
    const uint32_t b = px_bytes(f), c = r->colour;
    uint8_t *const d = r->dst.p + y * r->dst.pitch + (uint32_t)x0 * b;

    if (!z) {
        span_of(f, d, x1 - x0, c);
        return;
    }

    uint16_t *const zp = r->depth.p + y * r->depth.row + x0;

    for (int32_t i = 0; i < x1 - x0; i++, zd = depth_on(zd, r->dz, 1)) {
        const uint16_t here = depth_of(zd);

        if (z & ROW_TEST && !depth_shows(zp[i], here))
            continue;

        px_store(d + i * b, c, b);

        if (z & ROW_WRITE)
            zp[i] = here;
    }
}

/* RowFlat's, r's colour blended over the pixels there; with depth, Rows' depths, in runs between
   the pixels it hides. */
[[gnu::always_inline]]
static inline void tinted_row(
    const PxFormat f,
    const bool deep,
    const Rows *const r,
    const int32_t y,
    const int32_t x0,
    const int32_t x1,
    int32_t zd
)
{
    const uint32_t b = px_bytes(f), z = r->depths;
    const int32_t n = x1 - x0;
    uint8_t *const d = r->dst.p + y * r->dst.pitch + (uint32_t)x0 * b;
    const Inputs in = {
        .ink = *r->ink,
        .colour = r->colour,
        .blend = r->blend,
        .pixels = r->tints,
    };

    if (!deep) {
        fill_of(KIND_TINT, f, f, FILL_BLENDED, d, n, in);
        return;
    }

    uint16_t *const zp = r->depth.p + y * r->depth.row + x0;
    int32_t from = 0;

    for (int32_t i = 0; i < n; i++, zd = depth_on(zd, r->dz, 1)) {
        const uint16_t here = depth_of(zd);

        if (z & ROW_TEST && !depth_shows(zp[i], here)) {
            fill_of(KIND_TINT, f, f, FILL_BLENDED, d + from * b, i - from, in);
            from = i + 1;
        } else if (z & ROW_WRITE)
            zp[i] = here;
    }

    fill_of(KIND_TINT, f, f, FILL_BLENDED, d + from * b, n - from, in);
}

/* FillFlat's: flat by depth, or tinted when the ink blends, deep with depth. */
[[gnu::always_inline]]
static inline RowFlat *flat_pick(
    const PxFormat f,
    Rows *const r,
    const uint32_t colour,
    FillTints *const tints,
    RowFlat *const *const flat,
    RowFlat *const tinted,
    RowFlat *const deep
)
{
    const Ink *const k = r->ink;

    if (!(k->blend || k->mode)) {
        r->colour = paint_of(f, colour, k);
        return flat[r->depths];
    }

    r->colour = ink_colour(f, k->source, FILL_ANY, k, colour);

    if (f == PX_8)
        r->blend = k->blend + (r->colour << 8);
    else {
        tints_of(tints, f, r->colour, k);
        r->tints = tints->pixels;
    }

    return r->depths ? deep : tinted;
}

/* Variant k s l b: keyed, SOLID, LUT, BLENDED, each 0 or 1. */
#define FILL_VARIANT(k, s, l, b)                                                                   \
    ((k) * FILL_KEYED | (s) * FILL_SOLID | (l) * FILL_LUT | (b) * FILL_BLENDED)

/* name's inked filler of variant k s l b, from g into f; its slot in the table's row. */
#define FILL_INKED(name, f, g, k, s, l, b)                                                         \
    static void name##_##k##s##l##b(                                                               \
        const Rows *const r,                                                                       \
        const int32_t y,                                                                           \
        const int32_t x0,                                                                          \
        const int32_t x1,                                                                          \
        const int32_t u,                                                                           \
        const int32_t v,                                                                           \
        [[maybe_unused]] const int32_t z                                                           \
    )                                                                                              \
    {                                                                                              \
        uint8_t *const d = r->dst.p + y * r->dst.pitch + (uint32_t)x0 * px_bytes(f);               \
        const Steps st = {u, v, r->du, r->dv, 0, 0};                                               \
                                                                                                   \
        inked_of(f, g, FILL_VARIANT(k, s, l, b), d, &r->src, st, x1 - x0, r->ink);                 \
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
static RowAffine *const fill_inked[PX_FORMATS][PX_FORMATS][FILL_INKS] = {
    FILL_TARGETS(FILL_SOURCES, FILL_ROW)
};
/* Format f's row fillers, name's: affine by opacity o, walk h and depth z; persp by o and z. */
#define FILL_AFFINE(name, f, o, h, z)                                                              \
    static void name##_affine_##o##z##h(                                                           \
        const Rows *const r,                                                                       \
        const int32_t y,                                                                           \
        const int32_t x0,                                                                          \
        const int32_t x1,                                                                          \
        const int32_t u,                                                                           \
        const int32_t v,                                                                           \
        const int32_t zd                                                                           \
    )                                                                                              \
    {                                                                                              \
        affine_row(f, !(o), z, h, r, y, x0, x1, u, v, zd);                                         \
    }
#define FILL_PERSP(name, f, o, z)                                                                  \
    static void name##_persp_##o##z(                                                               \
        const Rows *const r,                                                                       \
        const int32_t y,                                                                           \
        const int32_t x0,                                                                          \
        const int32_t x1,                                                                          \
        const double u,                                                                            \
        const double v,                                                                            \
        const double w,                                                                            \
        const int32_t zd                                                                           \
    )                                                                                              \
    {                                                                                              \
        persp_row(f, !(o), z, r, y, x0, x1, u, v, w, zd);                                          \
    }
/* X for each depth, ROW_'s. */
#define FILL_DEPTHS(X, ...) X(__VA_ARGS__, 0) X(__VA_ARGS__, 1) X(__VA_ARGS__, 2) X(__VA_ARGS__, 3)
#define FILL_ROWS(name, f)                                                                         \
    FILL_DEPTHS(FILL_AFFINE, name, f, 0, 0)                                                        \
    FILL_DEPTHS(FILL_AFFINE, name, f, 0, 1)                                                        \
    FILL_DEPTHS(FILL_AFFINE, name, f, 0, 2)                                                        \
    FILL_DEPTHS(FILL_AFFINE, name, f, 1, 0)                                                        \
    FILL_DEPTHS(FILL_AFFINE, name, f, 1, 1)                                                        \
    FILL_DEPTHS(FILL_AFFINE, name, f, 1, 2)                                                        \
    FILL_DEPTHS(FILL_PERSP, name, f, 0)                                                            \
    FILL_DEPTHS(FILL_PERSP, name, f, 1)
#define FILL_AFFINE_Z(name, o, z)                                                                  \
    {name##_affine_##o##z##0, name##_affine_##o##z##1, name##_affine_##o##z##2}
#define FILL_AFFINE_O(name, o)                                                                     \
    {FILL_AFFINE_Z(name, o, 0),                                                                    \
     FILL_AFFINE_Z(name, o, 1),                                                                    \
     FILL_AFFINE_Z(name, o, 2),                                                                    \
     FILL_AFFINE_Z(name, o, 3)}
#define FILL_PERSP_O(name, o)                                                                      \
    {name##_persp_##o##0, name##_persp_##o##1, name##_persp_##o##2, name##_persp_##o##3}

/* Format f's flat row filler, name's, by depth z. */
#define FILL_FLAT(name, f, z)                                                                      \
    static void name##_flat_##z(                                                                   \
        const Rows *const r,                                                                       \
        const int32_t y,                                                                           \
        const int32_t x0,                                                                          \
        const int32_t x1,                                                                          \
        const int32_t zd                                                                           \
    )                                                                                              \
    {                                                                                              \
        flat_row(f, z, r, y, x0, x1, zd);                                                          \
    }
#define FILL_FLAT_Z(name) {name##_flat_0, name##_flat_1, name##_flat_2, name##_flat_3}

/* Format f's general and SMOOTH row fillers, name's. */
#define FILL_GENERAL(name, f)                                                                      \
    [[gnu::noinline]]                                                                              \
    static void name##_general_run(                                                                \
        const Rows *const r,                                                                       \
        const int32_t y,                                                                           \
        const int32_t x,                                                                           \
        const int32_t n,                                                                           \
        const Steps st,                                                                            \
        const int32_t z                                                                            \
    )                                                                                              \
    {                                                                                              \
        general_row(f, r, y, x, n, st, z);                                                         \
    }                                                                                              \
    [[gnu::noinline]]                                                                              \
    static void name##_smooth_run(                                                                 \
        const Rows *const r,                                                                       \
        const int32_t y,                                                                           \
        const int32_t x,                                                                           \
        const int32_t n,                                                                           \
        const Steps st,                                                                            \
        const int32_t z                                                                            \
    )                                                                                              \
    {                                                                                              \
        smooth_row(f, r, y, x, n, st, z);                                                          \
    }                                                                                              \
    static void name##_general(                                                                    \
        const Rows *const r,                                                                       \
        const int32_t y,                                                                           \
        const int32_t x0,                                                                          \
        const int32_t x1,                                                                          \
        const int32_t u,                                                                           \
        const int32_t v,                                                                           \
        const int32_t z                                                                            \
    )                                                                                              \
    {                                                                                              \
        name##_general_run(                                                                        \
            r, y, x0, x1 - x0, steps_from(u, v, r->du, r->dv, r->wrap, r->wrap_v), z               \
        );                                                                                         \
    }                                                                                              \
    static void name##_general_persp(                                                              \
        const Rows *const r,                                                                       \
        const int32_t y,                                                                           \
        const int32_t x0,                                                                          \
        const int32_t x1,                                                                          \
        const double u,                                                                            \
        const double v,                                                                            \
        const double w,                                                                            \
        const int32_t z                                                                            \
    )                                                                                              \
    {                                                                                              \
        persp_runs(name##_general_run, r, y, x0, x1, u, v, w, z);                                  \
    }                                                                                              \
    static void name##_smooth(                                                                     \
        const Rows *const r,                                                                       \
        const int32_t y,                                                                           \
        const int32_t x0,                                                                          \
        const int32_t x1,                                                                          \
        const int32_t u,                                                                           \
        const int32_t v,                                                                           \
        const int32_t z                                                                            \
    )                                                                                              \
    {                                                                                              \
        name##_smooth_run(                                                                         \
            r, y, x0, x1 - x0, steps_from(u, v, r->du, r->dv, r->wrap, r->wrap_v), z               \
        );                                                                                         \
    }                                                                                              \
    static void name##_smooth_persp(                                                               \
        const Rows *const r,                                                                       \
        const int32_t y,                                                                           \
        const int32_t x0,                                                                          \
        const int32_t x1,                                                                          \
        const double u,                                                                            \
        const double v,                                                                            \
        const double w,                                                                            \
        const int32_t z                                                                            \
    )                                                                                              \
    {                                                                                              \
        persp_runs(name##_smooth_run, r, y, x0, x1, u, v, w, z);                                   \
    }

/* Format f's table, name: each body above bound to f. */
#define FILL_TABLE(name, f)                                                                        \
    FILL_ROWS(name, f)                                                                             \
    FILL_GENERAL(name, f)                                                                          \
    FILL_DEPTHS(FILL_FLAT, name, f)                                                                \
    static void name##_tinted(                                                                     \
        const Rows *const r,                                                                       \
        const int32_t y,                                                                           \
        const int32_t x0,                                                                          \
        const int32_t x1,                                                                          \
        const int32_t z                                                                            \
    )                                                                                              \
    {                                                                                              \
        tinted_row(f, false, r, y, x0, x1, z);                                                     \
    }                                                                                              \
    static void name##_tinted_deep(                                                                \
        const Rows *const r,                                                                       \
        const int32_t y,                                                                           \
        const int32_t x0,                                                                          \
        const int32_t x1,                                                                          \
        const int32_t z                                                                            \
    )                                                                                              \
    {                                                                                              \
        tinted_row(f, true, r, y, x0, x1, z);                                                      \
    }                                                                                              \
    static RowFlat *name##_pick_flat(Rows *const r, const uint32_t colour, FillTints *const tints) \
    {                                                                                              \
        static RowFlat *const flat[ROW_DEPTHS] = FILL_FLAT_Z(name);                                \
                                                                                                   \
        return flat_pick(f, r, colour, tints, flat, name##_tinted, name##_tinted_deep);            \
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
    static void name##_ink(                                                                        \
        uint8_t *const d,                                                                          \
        const uint32_t *const texels,                                                              \
        const int32_t n,                                                                           \
        const Ink *const k                                                                         \
    )                                                                                              \
    {                                                                                              \
        ink_of(f, d, texels, n, k);                                                                \
    }                                                                                              \
    const Fill name = {                                                                            \
        .pick_flat = name##_pick_flat,                                                             \
        .flat = FILL_FLAT_Z(name),                                                                 \
        .row =                                                                                     \
            {                                                                                      \
                [0] = name##_keyed,                                                                \
                [FILL_FLIPX] = name##_keyed_flipped,                                               \
                [FILL_OPAQUE] = name##_opaque,                                                     \
                [FILL_OPAQUE | FILL_FLIPX] = name##_opaque_flipped,                                \
            },                                                                                     \
        .ink = name##_ink,                                                                         \
        .inked = fill_inked[f],                                                                    \
        .affine = {FILL_AFFINE_O(name, 0), FILL_AFFINE_O(name, 1)},                                \
        .persp = {FILL_PERSP_O(name, 0), FILL_PERSP_O(name, 1)},                                   \
        .general = name##_general,                                                                 \
        .smooth = name##_smooth,                                                                   \
        .general_persp = name##_general_persp,                                                     \
        .smooth_persp = name##_smooth_persp,                                                       \
    }

#define FILL_FORMAT(t, f, ...) FILL_TABLE(t, f);
#define FILL_ENTRY(t, f, ...) [f] = &t,

FILL_TARGETS(FILL_FORMAT)

const Fill *const fill_formats[PX_FORMATS] = {FILL_TARGETS(FILL_ENTRY)};
