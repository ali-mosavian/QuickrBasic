// gfx's one shader, its vertices (gpu/vertex.h's GpuVertex; with DISC, GpuDisc): a pixel's
// centre at whole numbers to the target's clip space; the texel, its rectangle, tint, light, row,
// stages, disc and sweep passed on, and the position for the disc's test.
// tools/shaders.py makes each host's from it.
#version 450

layout(location = 0) in vec2 at;
layout(location = 1) in vec2 texel;
layout(location = 2) in uvec4 rect;
layout(location = 3) in uvec4 tint;  // r, g, b, light
layout(location = 4) in uvec4 more;  // row, stages
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
layout(location = 2) out vec4 v_tint;
layout(location = 3) flat out uvec2 v_more;
layout(location = 4) out vec2 v_at;
layout(location = 5) flat out vec4 v_disc;
layout(location = 6) flat out vec4 v_sweep;

void main()
{
    vec2 clip = (at + 0.5) / target.size * 2.0 - 1.0;
    gl_Position = vec4(clip.x, clip.y * target.down, 0.0, 1.0);
    v_texel = texel;
    v_rect = rect;
    v_tint = vec4(vec3(tint.rgb) / 255.0, float(tint.a) / 63.0);
    v_more = more.xy;
    v_at = at;
#ifdef DISC
    v_disc = disc;
    v_sweep = sweep;
#else
    v_disc = vec4(0.0);
    v_sweep = vec4(0.0);
#endif
}
