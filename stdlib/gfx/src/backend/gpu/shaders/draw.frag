// gfx's one shader, its pixels, in whole numbers as software's ink (soft/ink.h) works them: with
// DISC, only those stdlib/gfx/disc.h's rule covers, worked as it works them; the texel at the
// pixel's centre, kept in its rectangle (u repeating with WRAP); an index through the palette's
// row; keyed; lit and tinted, or the vertex's colour alone (SOLID); with ADD, alpha's share of it,
// which the pipeline adds, as it mixes BLEND's. Into an 8-bit target (EIGHT) it draws an index, in
// alpha, and its colour in row 0: a texel's index through the row, lit and tinted, matched by the
// palette's map as pal_lut does; a colour, matched; SOLID's, the vertex's red; a blend (MIX, ADD),
// of its index's colour in row 0 and the pixel behind's, read from its copy, matched, as
// pal_blend's table. An index texel is in red, or alpha with ALPHA. Into a 15- or 16-bit target
// (FIFTEEN, SIXTEEN) a colour keeps a channel's top bits, as software's pixel does. SMOOTH
// mixes the four texels round the centre, lit, as software's smooth span does, a keyed one adding
// nothing, and draws where they are mostly kept.
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
const uint MIX = 512u;
const uint EIGHT = 1024u;
const uint ALPHA = 2048u;
const uint FIFTEEN = 4096u;
const uint SIXTEEN = 8192u;
const uint TILE = 16384u;

const uint FULL = 63u;

// ESSL's samplers are named by binding (qx/qxlist.h).
layout(set = 2, binding = 0) uniform sampler2D s0;
layout(set = 2, binding = 1) uniform sampler2D s1;
layout(set = 2, binding = 2) uniform sampler2D s2;
#define source s0
#define palette s1
#define behind s2

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

// Row row's colour index.
uvec3 colour_of(uint row, uint index) { return bytes(texelFetch(palette, ivec2(index, row), 0).rgb); }

// Row 0's index nearest c, through the map (palette.h): 5 bits a channel, four cells a texel of
// the palette's rows from 16.
uint nearest(uvec3 c)
{
    uint cell = (c.r >> 3) << 10 | (c.g >> 3) << 5 | c.b >> 3;
    vec4 t = texelFetch(palette, ivec2(cell >> 2 & 255u, 16u + (cell >> 10)), 0);
    return uint(t[cell & 3u] * 255.0 + 0.5);
}

// c lit and tinted: an index's colour as pal_shade, one divide; a direct texel's by its shade,
// the tint times the light over 63, as px_shade.
uvec3 lit(uvec3 c, bool index)
{
    if (v_tint == uvec4(255u, 255u, 255u, FULL))
        return c;

    uvec3 tint = v_tint.rgb;
    uint light = v_tint.a;

    if (index)
        return c * tint * light / (255u * FULL);

    return c * (tint * light / FULL) / 255u;
}

// The index index draws as through the row, lit and tinted, as pal_lut's.
uint looked_up(uint index)
{
    if (v_more.x == 0u && v_tint == uvec4(255u, 255u, 255u, FULL))
        return index;

    return nearest(lit(colour_of(v_more.x, index), true));
}

// An index texel's index, 0 to 1.
float index_in(vec4 c, uint stages) { return (stages & ALPHA) != 0u ? c.a : c.r; }

uint index_of(vec4 c, uint stages) { return uint(index_in(c, stages) * 255.0 + 0.5); }

// Index i drawn into an 8-bit target: its colour in row 0, and itself.
vec4 eight_bit(uint i)
{
    return vec4(texelFetch(palette, ivec2(i, 0), 0).rgb, float(i) / 255.0);
}

// c, 0 to 255 a channel, as the target holds it, 0 to 1: into a 15- or 16-bit one, a channel's
// top 5 bits (green's 6 at 16) widened as px_argb widens them.
vec3 held(uvec3 c, uint stages)
{
    if ((stages & (FIFTEEN | SIXTEEN)) == 0u)
        return vec3(c) / 255.0;

    uvec3 bits = uvec3(5u, (stages & SIXTEEN) != 0u ? 6u : 5u, 5u);
    uvec3 v = c >> (8u - bits);
    return vec3(v << (8u - bits) | v >> (2u * bits - 8u)) / 255.0;
}

// The texel at t, kept in the rectangle (u repeating with WRAP, u and v with TILE).
vec4 fetched(ivec2 t, uint stages)
{
    ivec2 from = ivec2(v_rect.xy), size = ivec2(v_rect.zw);

    if ((stages & WRAP) != 0u)
        t.x = from.x + int(mod(float(t.x - from.x), float(size.x)));

    if ((stages & TILE) != 0u)
        t = from + ((t - from) & (size - 1));

    return texelFetch(source, clamp(t, from, from + size - 1), 0);
}

// Texel c is drawn: not keyed.
bool kept(vec4 c, uint stages)
{
    if ((stages & KEYED) == 0u)
        return true;

    return (stages & INDEXED) != 0u ? index_in(c, stages) >= 0.5 / 255.0 : c.a >= 0.5;
}

// The texel at t, lit: as an index when indices, else a colour; *drawn false where keyed.
uvec3 texel(ivec2 t, uint stages, bool indices, out bool drawn)
{
    vec4 c = fetched(t, stages);
    drawn = kept(c, stages);

    if ((stages & INDEXED) != 0u) {
        uint index = index_of(c, stages);
        return indices ? uvec3(looked_up(index)) : lit(colour_of(v_more.x, index), true);
    }

    uvec3 rgb = lit(bytes(c.rgb), false);
    return indices ? uvec3(nearest(rgb)) : rgb;
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
    uvec3 c0 = texel(t, stages, false, k[0]);
    uvec3 c1 = texel(t + ivec2(1, 0), stages, false, k[1]);
    uvec3 c2 = texel(t + ivec2(0, 1), stages, false, k[2]);
    uvec3 c3 = texel(t + ivec2(1, 1), stages, false, k[3]);
    vec4 top = between(share(c0, k[0]), share(c1, k[1]), f.x);
    vec4 under = between(share(c2, k[2]), share(c3, k[3]), f.x);
    vec4 m = between(top, under, f.y);
    drawn = m.a >= 0.5;
    precise vec3 c = m.rgb / m.a * 255.0 + 0.5;
    return uvec3(c);
}

// c, an index into an 8-bit target, blended with the pixel behind by alpha.
uvec3 blended(uvec3 c, uint stages)
{
    uint alpha = v_more.y;
    uvec3 s = colour_of(0u, c.r);
    uvec3 d = colour_of(0u, index_of(texelFetch(behind, ivec2(gl_FragCoord.xy), 0), ALPHA));
    uvec3 m = (stages & ADD) != 0u ? min(d + s * alpha / FULL, uvec3(255u))
                                   : (d * (FULL - alpha) + s * alpha) / FULL;
    return uvec3(nearest(m));
}

// A simple draw (pipelines.h): SOLID's colour or index; a texel unlit, into a direct target as it
// is, held as the target holds it, an index into an 8-bit one, itself.
void simple(uint stages)
{
    vec4 t = fetched(ivec2(floor(v_texel)), stages);
    bool solid = (stages & SOLID) != 0u;

    if (!kept(t, stages))
        discard;

    if ((stages & EIGHT) != 0u)
        colour = eight_bit(solid ? v_tint.r : index_of(t, stages));
    else if ((stages & (FIFTEEN | SIXTEEN)) != 0u)
        colour = vec4(held(solid ? v_tint.rgb : bytes(t.rgb), stages), 1.0);
    else
        colour = solid ? vec4(vec3(v_tint.rgb) / 255.0, 1.0) : vec4(t.rgb, 1.0);
}

void main()
{
    uint stages = v_more.z;
#ifdef SIMPLE
    simple(stages);
}
#else
    bool eight = (stages & EIGHT) != 0u;

    if ((stages & DISC) != 0u && !covered(stages))
        discard;

    bool solid = (stages & SOLID) != 0u, drawn = true;
    uvec3 c = v_tint.rgb;

    if ((stages & SMOOTH) != 0u) {
        uvec3 m = smoothed(stages, drawn);
        c = solid ? c : eight ? uvec3(nearest(m)) : m;
    } else if (!solid)
        c = texel(ivec2(floor(v_texel)), stages, eight, drawn);
    else if ((stages & KEYED) != 0u)
        drawn = kept(fetched(ivec2(floor(v_texel)), stages), stages);

    if (!drawn)
        discard;

    if ((stages & (ADD | MIX)) != 0u)
        c = eight ? blended(c, stages) : c * v_more.y / FULL;

    colour = eight ? eight_bit(c.r) : vec4(held(c, stages), 1.0);
}
#endif
