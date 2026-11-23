/* A zlib stream inflated (inflate.h). */
#include "inflate.h"

enum {
    MAX_BITS = 15,
    LITERALS = 288,
    DISTANCES = 30,
    LENGTH_CODES = 19,
    END = 256,
};

/* The bits read so far, least significant first, as deflate packs them. */
typedef struct {
    const uint8_t *in;
    uint32_t n, at;
    uint32_t bits, count;
    bool past_end;
    uint8_t *out;
    uint32_t size, made;
} Stream;

/* A canonical code: how many codes of each length, then its symbols by code. */
typedef struct {
    uint16_t count[MAX_BITS + 1];
    uint16_t symbol[LITERALS];
} Code;

/* need bits: 0 past the end, which is then short. */
static uint32_t bits(Stream *const s, const uint32_t need)
{
    while (s->count < need) {
        if (s->at == s->n) {
            s->past_end = true;
            return 0;
        }

        s->bits |= (uint32_t)s->in[s->at++] << s->count;
        s->count += 8;
    }

    const uint32_t v = s->bits & ((1u << need) - 1);
    s->bits >>= need;
    s->count -= need;
    return v;
}

/* The code of n symbols' lengths: false, it has more codes of a length than fit. */
static bool code_of(Code *const c, const uint8_t *const lengths, const uint32_t n)
{
    uint16_t offset[MAX_BITS + 1];

    for (uint32_t i = 0; i <= MAX_BITS; i++)
        c->count[i] = 0;

    for (uint32_t i = 0; i < n; i++)
        c->count[lengths[i]]++;

    int32_t left = 1;

    for (uint32_t len = 1; len <= MAX_BITS; len++) {
        left = 2 * left - c->count[len];

        if (left < 0)
            return false;
    }

    offset[1] = 0;

    for (uint32_t len = 1; len < MAX_BITS; len++)
        offset[len + 1] = (uint16_t)(offset[len] + c->count[len]);

    for (uint32_t i = 0; i < n; i++)
        if (lengths[i])
            c->symbol[offset[lengths[i]]++] = (uint16_t)i;

    return true;
}

/* The next symbol in code c, a bit at a time: -1, none. Codes of a length are consecutive, the
   first of each length the last of the one before, plus one, doubled. */
static int32_t decode(Stream *const s, const Code *const c)
{
    int32_t code = 0, first = 0, index = 0;

    for (uint32_t len = 1; len <= MAX_BITS; len++) {
        code |= (int32_t)bits(s, 1);
        const int32_t count = c->count[len];

        if (code - first < count)
            return c->symbol[index + code - first];

        index += count;
        first = (first + count) << 1;
        code <<= 1;

        if (s->past_end)
            return -1;
    }

    return -1;
}

static bool put(Stream *const s, const uint8_t b)
{
    if (s->made == s->size)
        return false;

    s->out[s->made++] = b;
    return true;
}

/* A block's literals and copies, to its end. */
static bool codes(Stream *const s, const Code *const lit, const Code *const dist)
{
    static const uint16_t length_base[] = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                           15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                           67, 83, 99, 115, 131, 163, 195, 227, 258};

    static const uint8_t length_extra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                           2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};

    static const uint16_t dist_base[] = {1,    2,    3,    4,     5,     7,    9,    13,
                                         17,   25,   33,   49,    65,    97,   129,  193,
                                         257,  385,  513,  769,   1025,  1537, 2049, 3073,
                                         4097, 6145, 8193, 12289, 16385, 24577};

    static const uint8_t dist_extra[] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                         6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

    for (;;) {
        const int32_t sym = decode(s, lit);

        if (sym < 0 || s->past_end)
            return false;

        if (sym < END) {
            if (!put(s, (uint8_t)sym))
                return false;

            continue;
        }

        if (sym == END)
            return true;

        const uint32_t l = (uint32_t)sym - END - 1;

        if (l >= sizeof length_base / sizeof *length_base)
            return false;

        const uint32_t len = length_base[l] + bits(s, length_extra[l]);
        const int32_t d = decode(s, dist);

        if (d < 0 || d >= DISTANCES || s->past_end)
            return false;

        const uint32_t back = dist_base[d] + bits(s, dist_extra[d]);

        if (back > s->made || s->past_end)
            return false;

        for (uint32_t i = 0; i < len; i++)
            if (!put(s, s->out[s->made - back]))
                return false;
    }
}

static bool stored(Stream *const s)
{
    s->bits = s->count = 0; /* to the byte */

    if (s->n - s->at < 4)
        return false;

    const uint8_t *const p = s->in + s->at;
    const uint32_t len = p[0] | (uint32_t)p[1] << 8, nlen = p[2] | (uint32_t)p[3] << 8;
    s->at += 4;

    if (len != (~nlen & 0xFFFF) || s->n - s->at < len)
        return false;

    for (uint32_t i = 0; i < len; i++)
        if (!put(s, s->in[s->at++]))
            return false;

    return true;
}

static bool fixed(Stream *const s)
{
    uint8_t lengths[LITERALS + DISTANCES];
    Code lit, dist;

    for (uint32_t i = 0; i < LITERALS; i++)
        lengths[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;

    for (uint32_t i = 0; i < DISTANCES; i++)
        lengths[LITERALS + i] = 5;

    code_of(&lit, lengths, LITERALS);
    code_of(&dist, lengths + LITERALS, DISTANCES);
    return codes(s, &lit, &dist);
}

static bool dynamic(Stream *const s)
{
    static const uint8_t order[LENGTH_CODES] =
        {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

    uint8_t lengths[LITERALS + DISTANCES] = {};

    const uint32_t nlit = bits(s, 5) + 257, ndist = bits(s, 5) + 1, ncode = bits(s, 4) + 4;
    Code lens, lit, dist;

    if (nlit > LITERALS || ndist > DISTANCES)
        return false;

    for (uint32_t i = 0; i < ncode; i++)
        lengths[order[i]] = (uint8_t)bits(s, 3);

    if (s->past_end || !code_of(&lens, lengths, LENGTH_CODES))
        return false;

    for (uint32_t i = 0; i < nlit + ndist;) {
        const int32_t sym = decode(s, &lens);
        uint32_t repeat = 1;
        uint8_t len = (uint8_t)sym;

        if (sym < 0 || s->past_end)
            return false;

        if (sym == 16) {
            if (!i)
                return false;

            len = lengths[i - 1];
            repeat = 3 + bits(s, 2);
        } else if (sym == 17) {
            len = 0;
            repeat = 3 + bits(s, 3);
        } else if (sym == 18) {
            len = 0;
            repeat = 11 + bits(s, 7);
        }

        if (i + repeat > nlit + ndist)
            return false;

        while (repeat--)
            lengths[i++] = len;
    }

    return lengths[END] && code_of(&lit, lengths, nlit) && code_of(&dist, lengths + nlit, ndist) &&
           codes(s, &lit, &dist);
}

/* The stream's Adler-32 over bytes. */
static uint32_t adler(const uint8_t *const p, const uint32_t n)
{
    uint32_t a = 1, b = 0;

    for (uint32_t i = 0; i < n; i++) {
        a = (a + p[i]) % 65521;
        b = (b + a) % 65521;
    }

    return b << 16 | a;
}

bool inflate_zlib(
    const uint8_t *const in,
    const uint32_t n,
    uint8_t *const out,
    const uint32_t size,
    uint32_t *const made
)
{
    if (n < 6 || (in[0] & 0x0F) != 8 || in[0] >> 4 > 7 || (in[0] << 8 | in[1]) % 31 || in[1] & 0x20)
        return false;

    Stream s = {
        .in = in,
        .n = n - 4,
        .at = 2,
        .out = out,
        .size = size,
    };

    for (bool last = false; !last;) {
        last = bits(&s, 1);
        const uint32_t type = bits(&s, 2);
        const bool ok = type == 0 ? stored(&s) : type == 1 ? fixed(&s) : type == 2 && dynamic(&s);

        if (!ok || s.past_end)
            return false;
    }

    const uint8_t *const sum = in + n - 4;
    const uint32_t want = (uint32_t)sum[0] << 24 | (uint32_t)sum[1] << 16 | sum[2] << 8 | sum[3];
    *made = s.made;
    return adler(out, s.made) == want;
}
