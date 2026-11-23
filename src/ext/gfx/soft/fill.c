/* The fillers' tables by format, and a direct target's first steps (fill.h). */
#include "soft/fill.h"

const Fill *const fill_formats[PX_FORMATS] = {
    [PX_8] = &fill8,
    [PX_15] = &fill15,
    [PX_16] = &fill16,
    [PX_32] = &fill32,
};

void fill_argb(
    uint32_t *const argb,
    bool *const drawn,
    const uint32_t *const texels,
    const int32_t n,
    const Ink *const k
)
{
    for (int32_t i = 0; i < n; i++) {
        const uint32_t t = texels[i];
        drawn[i] = !k->keyed || !px_key(k->source, t);

        if (k->solid)
            argb[i] = k->colour;
        else if (k->source == PX_8)
            argb[i] = k->argb[t & 0xFF];
        else
            argb[i] = px_shade(px_argb(k->source, t), k->shade);
    }
}

uint32_t fill_blend(const uint32_t s, const uint32_t d, const Ink *const k)
{
    return px_mix(s, d, k->mode, k->alpha);
}
