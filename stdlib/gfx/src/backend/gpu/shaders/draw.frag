// gfx's one shader, its pixels, in whole numbers as software's ink (soft/ink.h) works them: with
// DISC, only those stdlib/gfx/disc.h's rule covers, worked as it works them; the texel at the
// pixel's centre, kept in its rectangle (u repeating with WRAP); an index through the palette's
// row; keyed; lit and tinted, or the vertex's colour alone (SOLID); with ADD, alpha's share of it,
// which the pipeline adds. SMOOTH mixes the four texels round the centre, lit, as software's
// smooth span does, a keyed one adding nothing, and draws where they are mostly kept.
#version 450

const uint INDEXED = 1u;
const uint KEYED = 2u;
const uint SOLID = 4u;
const uint WRAP = 8u;
const uint DISC = 16u;
const uint SWEEP = 32u;
const uint WIDE = 64u;
const uint SMOOTH = 128u;
const uint ADD = 256u;

const uint FULL = 63u;

// ESSL's samplers are named by binding (qx/qxlist.h).
layout(set = 2, binding = 0) uniform sampler2D s0;
layout(set = 2, binding = 1) uniform sampler2D s1;
#define source s0
#define palette s1

layout(location = 0) in vec2 v_texel;
layout(location = 1) flat in uvec4 v_rect;
layout(location = 2) flat in uvec4 v_tint; // r, g, b, light
layout(location = 3) flat in uvec4 v_more; // row, alpha, stages
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

uvec3 bytes(vec3 c) { return uvec3(c * 255.0 + 0.5); }

// c lit and tinted: an index's colour as pal_shade, one divide; a direct texel's by its shade,
// the tint times the light over 63, as px_shade.
uvec3 lit(uvec3 c, bool index)
{
    uvec3 tint = v_tint.rgb;
    uint light = v_tint.a;

    if (index)
        return c * tint * light / (255u * FULL);

    return c * (tint * light / FULL) / 255u;
}

// The texel at t, kept in the rectangle, lit; *drawn false where keyed.
uvec3 texel(ivec2 t, uint stages, out bool drawn)
{
    ivec2 from = ivec2(v_rect.xy), size = ivec2(v_rect.zw);

    if ((stages & WRAP) != 0u)
        t.x = from.x + int(mod(float(t.x - from.x), float(size.x)));

    t = clamp(t, from, from + size - 1);
    vec4 c = texelFetch(source, t, 0);

    if ((stages & INDEXED) != 0u) {
        uint index = uint(c.r * 255.0 + 0.5);
        drawn = (stages & KEYED) == 0u || index != 0u;
        return lit(bytes(texelFetch(palette, ivec2(index, v_more.x), 0).rgb), true);
    }

    drawn = (stages & KEYED) == 0u || c.a >= 0.5;
    return lit(bytes(c.rgb), false);
}

// A texel's share of a mix: its colour, 0 to 1, and its coverage.
vec4 share(uvec3 c, bool drawn)
{
    float n = drawn ? 1.0 / 255.0 : 0.0;
    return vec4(vec3(c) * n, drawn ? 1.0 : 0.0);
}

// a to b by f, as software's mix: each product and sum rounded once.
vec4 between(vec4 a, vec4 b, float f)
{
    precise float e = 1.0 - f;
    precise vec4 m = a * e + b * f;
    return m;
}

// The four texels round the centre mixed; *drawn false where under half covered.
uvec3 smoothed(uint stages, out bool drawn)
{
    vec2 p = v_texel - 0.5;
    vec2 f = fract(p);
    ivec2 t = ivec2(floor(p));
    bool k[4];
    uvec3 c0 = texel(t, stages, k[0]);
    uvec3 c1 = texel(t + ivec2(1, 0), stages, k[1]);
    uvec3 c2 = texel(t + ivec2(0, 1), stages, k[2]);
    uvec3 c3 = texel(t + ivec2(1, 1), stages, k[3]);
    vec4 top = between(share(c0, k[0]), share(c1, k[1]), f.x);
    vec4 under = between(share(c2, k[2]), share(c3, k[3]), f.x);
    vec4 m = between(top, under, f.y);
    drawn = m.a >= 0.5;
    precise vec3 c = m.rgb / m.a * 255.0 + 0.5;
    return uvec3(c);
}

void main()
{
    uint stages = v_more.z;

    if ((stages & DISC) != 0u && !covered(stages))
        discard;

    bool drawn;
    uvec3 c = (stages & SMOOTH) != 0u ? smoothed(stages, drawn)
                                      : texel(ivec2(floor(v_texel)), stages, drawn);

    if (!drawn)
        discard;

    if ((stages & SOLID) != 0u)
        c = v_tint.rgb;

    if ((stages & ADD) != 0u)
        c = c * v_more.y / FULL;

    colour = vec4(vec3(c) / 255.0, 1.0);
}
