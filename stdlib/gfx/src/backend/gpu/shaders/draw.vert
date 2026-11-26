// gfx's one shader, its vertices (gpu/vertex.h's GpuVertex; with DISC, GpuDisc): a pixel's
// centre at whole numbers to the target's clip space, its depth z and its w (perspective's, so
// texels and depth are interpolated as software's); the texel, its rectangle, tint, light, row,
// alpha, stages, disc and sweep passed on, and the position for the disc's test.
// tools/shaders.py makes each host's from it.
#version 450

layout(location = 0) in vec4 at; // x, y, z, w
layout(location = 1) in vec2 texel;
layout(location = 2) in uvec4 rect;
layout(location = 3) in uvec4 tint;  // r, g, b, light
layout(location = 4) in uvec4 more;  // row, alpha, stages' low byte and high
#ifdef DISC
layout(location = 5) in vec4 disc;
layout(location = 6) in vec4 sweep;
#endif

// size: the target's pixels; down: -1 where clip space's y runs up the target's rows, 1 down.
layout(set = 1, binding = 0) uniform Target {
    vec2 size;
    float down;
} target;

layout(location = 0) out vec2 v_texel;
layout(location = 1) flat out uvec4 v_rect;
layout(location = 2) flat out uvec4 v_tint;
layout(location = 3) flat out uvec4 v_more;
layout(location = 4) out vec2 v_at;
layout(location = 5) flat out vec4 v_disc;
layout(location = 6) flat out vec4 v_sweep;

void main()
{
    vec2 clip = (at.xy + 0.5) / target.size * 2.0 - 1.0;
    gl_Position = vec4(clip.x, clip.y * target.down, at.z, 1.0) * at.w;
    v_texel = texel;
    v_rect = rect;
    v_tint = tint;
    v_more = uvec4(more.xy, more.z | more.w << 8, 0u);
    v_at = at.xy;
#ifdef DISC
    v_disc = disc;
    v_sweep = sweep;
#else
    v_disc = vec4(0.0);
    v_sweep = vec4(0.0);
#endif
}
