// gfx's one shader, its pixels: with DISC, only those stdlib/gfx/disc.h's rule covers, worked
// as it works them; the texel at the pixel's centre, kept in its rectangle (u repeating with
// WRAP); an index through the palette's row; keyed, or the tint alone (SOLID); times the tint and
// the light. Blending is the pipeline's.
#version 450

const uint INDEXED = 1u;
const uint KEYED = 2u;
const uint SOLID = 4u;
const uint WRAP = 8u;
const uint DISC = 16u;
const uint SWEEP = 32u;
const uint WIDE = 64u;

layout(set = 2, binding = 0) uniform sampler2D source;
layout(set = 2, binding = 1) uniform sampler2D palette;

layout(location = 0) in vec2 v_texel;
layout(location = 1) flat in uvec4 v_rect;
layout(location = 2) in vec4 v_tint;
layout(location = 3) flat in uvec2 v_more;
layout(location = 4) in vec2 v_at;
layout(location = 5) flat in vec4 v_disc;
layout(location = 6) flat in vec4 v_sweep;

layout(location = 0) out vec4 colour;

// d, from the centre, is within r: each product and sum rounded once (precise), as in C.
bool within(float dx, float dy, float r)
{
    precise float d = dx * dx + dy * dy;
    precise float e = r * r;
    return r > 0.0 && (d < e || (d == e && dx < 0.0));
}

// The pixel at v_at is in the disc's band, and its sweep.
bool covered(uint stages)
{
    vec2 p = floor(v_at + 0.5);
    precise float dx = p.x - v_disc.x;
    precise float dy = p.y - v_disc.y;

    if (!within(dx, dy, v_disc.z) || within(dx, dy, v_disc.w))
        return false;

    if ((stages & SWEEP) == 0u)
        return true;

    precise float from = v_sweep.x * dy - v_sweep.y * dx;
    precise float to = dx * v_sweep.w - dy * v_sweep.z;
    return (stages & WIDE) != 0u ? !(from < 0.0 && to < 0.0) : from >= 0.0 && to >= 0.0;
}

void main()
{
    uint stages = v_more.y;

    if ((stages & DISC) != 0u && !covered(stages))
        discard;
    ivec2 from = ivec2(v_rect.xy), size = ivec2(v_rect.zw);
    ivec2 t = ivec2(floor(v_texel));

    if ((stages & WRAP) != 0u)
        t.x = from.x + int(mod(float(t.x - from.x), float(size.x)));

    t = clamp(t, from, from + size - 1);
    vec4 c = texelFetch(source, t, 0);

    if ((stages & INDEXED) != 0u) {
        int index = int(c.r * 255.0 + 0.5);

        if ((stages & KEYED) != 0u && index == 0)
            discard;

        c = texelFetch(palette, ivec2(index, int(v_more.x)), 0);
    } else if ((stages & KEYED) != 0u && c.a < 0.5)
        discard;

    if ((stages & SOLID) != 0u)
        c.rgb = vec3(1.0);

    colour = vec4(c.rgb * v_tint.rgb * v_tint.a, 1.0);
}
