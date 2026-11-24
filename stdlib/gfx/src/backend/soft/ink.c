/* Inks (ink.h). */
#include "backend/soft/ink.h"
#include "gfx_qx.h"

bool ink_begin(
    Ink *const k,
    Palette *const pal,
    const Style *const p,
    const int32_t flags,
    const PxFormat format
)
{
    const bool eight = format == PX_8;
    const BlendMode mode = flags & GFX_DRAW_ADD     ? BLEND_ADD
                           : flags & GFX_DRAW_BLEND ? BLEND_MIX
                                                    : BLEND_NONE;
    const uint32_t solid = pal_shade(p->tint, p->light, PAL_WHITE);
    uint8_t index = 0;
    *k = (Ink){
        .source = eight ? PX_8 : PX_32,
        .keyed = !(flags & GFX_DRAW_OPAQUE),
        .solid = flags & GFX_DRAW_SOLID,
        .smooth = flags & GFX_DRAW_SMOOTH,
        .colour = PX_OPAQUE | solid,
        .shade = pal_shade(PAL_WHITE, p->light, p->tint),
        .mode = eight ? BLEND_NONE : mode,
        .alpha = (uint8_t)p->alpha,
    };

    if (eight && !pal_lut(pal, p->palette_row, p->light, p->tint, &k->lut))
        return false;

    if (eight && k->solid && !pal_index(pal, solid, &index))
        return false;

    if (eight && mode && !(k->blend = pal_blend(pal, mode, p->alpha)))
        return false;

    k->colour = eight ? index : k->colour;
    k->effects = k->solid || mode || (eight ? k->lut != nullptr : k->shade != PAL_WHITE);
    return true;
}

bool ink_source(
    Ink *const k,
    Palette *const pal,
    const Style *const p,
    const PxFormat into,
    const PxFormat from
)
{
    const bool eight = into == PX_8;
    k->source = from;

    const bool colours = from != PX_8 || k->smooth; /* matched to row 0 at 8 bits */

    if (eight && colours && !k->map && !(k->map = pal_map(pal)))
        return false;

    return (eight && !k->smooth) || from != PX_8 || k->argb ||
           (k->argb = pal_argb(pal, p->palette_row, p->light, p->tint));
}

void ink_solid(Ink *const k, const Style *const p, const PxFormat into, const uint32_t colour)
{
    k->solid = k->effects = true;
    k->colour = into == PX_8 ? (k->lut ? k->lut[colour] : colour)
                             : PX_OPAQUE | pal_shade(colour, p->light, p->tint);
}
