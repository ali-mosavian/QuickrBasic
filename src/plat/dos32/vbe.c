/* VBE's graphics modes for gfx's screen (vbe.h). */
#include "vbe.h"

enum : uint16_t { SUPPORTED = 1, SIZED = 2, GRAPHICS = 0x10, NO_WINDOW = 0x40, LINEAR = 0x80 };
enum : uint16_t { VBE_V3 = 0x300, KB = 1024 };
enum : uint8_t { PACKED = 4, DIRECT = 6, WINDOW_THERE = 1, WINDOW_WRITES = 4 };
/* The block's fields' offsets. */
enum : uint8_t {
    ATTRIBUTES = 0x00,
    WINDOW_A = 0x02,
    GRANULE = 0x04,
    WINDOW_SIZE = 0x06,
    SEGMENT_A = 0x08,
    PITCH = 0x10,
    WIDTH = 0x12,
    HEIGHT = 0x14,
    BITS = 0x19,
    MODEL = 0x1B,
    MASKS = 0x1F, /* red's size and place, green's, blue's */
    BASE = 0x28,
    LINEAR_PITCH = 0x32,
};

static uint32_t u16(const uint8_t *const p) { return p[0] | (uint32_t)p[1] << 8; }

static uint32_t u32(const uint8_t *const p) { return u16(p) | u16(p + 2) << 16; }

/* The depth of a direct colour mode's masks: 0, none of gfx's. VBE 1.2's may give none. */
static uint8_t direct_depth(const uint8_t bits, const uint8_t m[6])
{
    static const struct {
        uint8_t bits, depth, masks[6];
    } formats[] = {
        {15, 15, {5, 10, 5, 5, 5, 0}},
        {16, 15, {5, 10, 5, 5, 5, 0}},
        {16, 16, {5, 11, 6, 5, 5, 0}},
        {32, 32, {8, 16, 8, 8, 8, 0}},
        {15, 15, {}},
        {16, 16, {}},
        {32, 32, {}},
    };

    for (uint32_t i = 0; i < sizeof formats / sizeof formats[0]; i++) {
        bool same = formats[i].bits == bits;

        for (uint32_t j = 0; j < 6 && same; j++)
            same = formats[i].masks[j] == m[j];

        if (same)
            return formats[i].depth;
    }

    return 0;
}

bool vbe_mode(
    const uint16_t number,
    const uint16_t version,
    const uint8_t info[VBE_INFO_BYTES],
    VbeMode *const m
)
{
    const uint32_t attributes = u16(info + ATTRIBUTES);
    const uint8_t bits = info[BITS], model = info[MODEL];
    const uint8_t depth = model == PACKED && bits == 8 ? 8
                          : model == DIRECT            ? direct_depth(bits, info + MASKS)
                                                       : 0;
    const uint32_t need = SUPPORTED | SIZED | GRAPHICS;

    if ((attributes & need) != need || !depth || !u16(info + WIDTH) || !u16(info + HEIGHT))
        return false;

    *m = (VbeMode){
        .number = number,
        .w = (uint16_t)u16(info + WIDTH),
        .h = (uint16_t)u16(info + HEIGHT),
        .pitch = u16(info + PITCH),
        .depth = depth,
        .base = u32(info + BASE),
    };

    m->linear = version >= VBE_V2 && attributes & LINEAR && m->base;

    if (m->linear && version >= VBE_V3 && u16(info + LINEAR_PITCH))
        m->pitch = u16(info + LINEAR_PITCH);

    if (m->linear)
        return m->pitch >= (uint32_t)m->w * vbe_bytes(depth);

    /* the window written: A's, else B's */
    const uint8_t want = WINDOW_THERE | WINDOW_WRITES;
    m->window = (info[WINDOW_A] & want) == want ? 0 : 1;
    m->window_seg = (uint16_t)u16(info + SEGMENT_A + 2 * m->window);
    m->granule = u16(info + GRANULE) * KB;
    m->window_bytes = u16(info + WINDOW_SIZE) * KB;

    return !(attributes & NO_WINDOW) && (info[WINDOW_A + m->window] & want) == want &&
           m->window_seg && m->granule && m->granule <= m->window_bytes &&
           m->pitch >= (uint32_t)m->w * vbe_bytes(depth);
}

VbeMode vbe_mode_13h(void)
{
    return (VbeMode){
        .number = VBE_MODE_13H,
        .w = 320,
        .h = 200,
        .pitch = 320,
        .depth = 8,
        .linear = true,
        .base = 0xA0000,
    };
}

uint8_t vbe_bytes(const uint8_t depth)
{
    return depth == 8 ? 1 : depth == 15 || depth == 16 ? 2 : depth == 32 ? 4 : 0;
}

VbeFit vbe_fit(const VbeMode *const m, const uint16_t w, const uint16_t h)
{
    const uint32_t across = w ? m->w / w : 0, down = h ? m->h / h : 0;
    const uint16_t k = (uint16_t)(across < down ? across : down);

    if (!k)
        return (VbeFit){};

    return (VbeFit){k, (uint16_t)((m->w - w * k) / 2), (uint16_t)((m->h - h * k) / 2)};
}

int32_t vbe_pick(
    const VbeMode *const modes,
    const uint32_t n,
    const uint16_t w,
    const uint16_t h,
    const uint8_t depth
)
{
    int32_t best = -1;
    uint32_t best_spare = 0, best_area = 0;

    for (uint32_t i = 0; i < n; i++) {
        const VbeMode *const m = &modes[i];
        const VbeFit f = vbe_fit(m, w, h);

        if (m->depth != depth || !f.k)
            continue;

        const uint32_t area = (uint32_t)m->w * m->h;
        const uint32_t spare = area - (uint32_t)w * f.k * h * f.k;

        if (best < 0 || spare < best_spare || (spare == best_spare && area < best_area)) {
            best = (int32_t)i;
            best_spare = spare;
            best_area = area;
        }
    }

    return best;
}

int32_t vbe_holding(
    const VbeMode *const modes,
    const uint32_t n,
    const uint16_t w,
    const uint16_t h,
    const uint8_t depth
)
{
    int32_t best = -1;

    for (uint32_t i = 0; i < n; i++) {
        const VbeMode *const m = &modes[i];

        if (m->depth != depth || !m->linear || m->w < w || m->h < h)
            continue;

        if (best < 0 || (uint32_t)m->w * m->h < (uint32_t)modes[best].w * modes[best].h)
            best = (int32_t)i;
    }

    return best;
}

void vbe_widen(
    uint8_t *const to,
    const uint8_t *const from,
    const uint32_t w,
    const uint8_t bytes,
    const uint16_t k
)
{
    uint32_t at = 0;

    for (uint32_t x = 0; x < w; x++)
        for (uint32_t i = 0; i < k; i++)
            for (uint32_t b = 0; b < bytes; b++)
                to[at++] = from[x * bytes + b];
}

uint32_t vbe_window(
    const VbeMode *const m,
    const uint32_t at,
    const uint32_t n,
    uint32_t *const step,
    uint32_t *const in
)
{
    *step = at / m->granule;
    *in = at - *step * m->granule;

    const uint32_t room = m->window_bytes - *in;
    return n < room ? n : room;
}
