// Made by tools/shaders.py from src/plat/gpu/draw.*: run make shaders, never edit.
export const DRAW_VERT = `#version 300 es

struct Target
{
    vec2 size;
    float down;
};

uniform Target target;

layout(location = 0) in vec2 at;
out vec2 v_texel;
layout(location = 1) in vec2 texel;
flat out uvec4 v_rect;
layout(location = 2) in uvec4 rect;
out vec4 v_tint;
layout(location = 3) in uvec4 tint;
flat out uvec2 v_more;
layout(location = 4) in uvec4 more;
out vec2 v_at;
flat out vec4 v_disc;
flat out vec4 v_sweep;

void main()
{
    vec2 clip = (((at + vec2(0.5)) / target.size) * 2.0) - vec2(1.0);
    gl_Position = vec4(clip.x, clip.y * target.down, 0.0, 1.0);
    v_texel = texel;
    v_rect = rect;
    v_tint = vec4(vec3(tint.xyz) / vec3(255.0), float(tint.w) / 63.0);
    v_more = more.xy;
    v_at = at;
    v_disc = vec4(0.0);
    v_sweep = vec4(0.0);
}

`;
export const DISC_VERT = `#version 300 es

struct Target
{
    vec2 size;
    float down;
};

uniform Target target;

layout(location = 0) in vec2 at;
out vec2 v_texel;
layout(location = 1) in vec2 texel;
flat out uvec4 v_rect;
layout(location = 2) in uvec4 rect;
out vec4 v_tint;
layout(location = 3) in uvec4 tint;
flat out uvec2 v_more;
layout(location = 4) in uvec4 more;
out vec2 v_at;
flat out vec4 v_disc;
layout(location = 5) in vec4 disc;
flat out vec4 v_sweep;
layout(location = 6) in vec4 sweep;

void main()
{
    vec2 clip = (((at + vec2(0.5)) / target.size) * 2.0) - vec2(1.0);
    gl_Position = vec4(clip.x, clip.y * target.down, 0.0, 1.0);
    v_texel = texel;
    v_rect = rect;
    v_tint = vec4(vec3(tint.xyz) / vec3(255.0), float(tint.w) / 63.0);
    v_more = more.xy;
    v_at = at;
    v_disc = disc;
    v_sweep = sweep;
}

`;
export const DRAW_FRAG = `#version 300 es
precision mediump float;
precision highp int;

uniform highp sampler2D source;
uniform highp sampler2D palette;

in highp vec2 v_at;
flat in highp vec4 v_disc;
flat in highp vec4 v_sweep;
flat in uvec2 v_more;
flat in uvec4 v_rect;
in highp vec2 v_texel;
layout(location = 0) out highp vec4 colour;
in highp vec4 v_tint;

bool within(highp float dx, highp float dy, highp float r)
{
    highp float d = (dx * dx) + (dy * dy);
    highp float e = r * r;
    bool _35 = r > 0.0;
    bool _51;
    if (_35)
    {
        bool _40 = d < e;
        bool _50;
        if (!_40)
        {
            _50 = (d == e) && (dx < 0.0);
        }
        else
        {
            _50 = _40;
        }
        _51 = _50;
    }
    else
    {
        _51 = _35;
    }
    return _51;
}

bool covered(uint stages)
{
    highp vec2 p = floor(v_at + vec2(0.5));
    highp float dx = p.x - v_disc.x;
    highp float dy = p.y - v_disc.y;
    highp float param = dx;
    highp float param_1 = dy;
    highp float param_2 = v_disc.z;
    bool _91 = !within(param, param_1, param_2);
    bool _104;
    if (!_91)
    {
        highp float param_3 = dx;
        highp float param_4 = dy;
        highp float param_5 = v_disc.w;
        _104 = within(param_3, param_4, param_5);
    }
    else
    {
        _104 = _91;
    }
    if (_104)
    {
        return false;
    }
    if ((stages & 32u) == 0u)
    {
        return true;
    }
    highp float from = (v_sweep.x * dy) - (v_sweep.y * dx);
    highp float to = (dx * v_sweep.w) - (dy * v_sweep.z);
    bool _143;
    if ((stages & 64u) != 0u)
    {
        _143 = !((from < 0.0) && (to < 0.0));
    }
    else
    {
        _143 = (from >= 0.0) && (to >= 0.0);
    }
    return _143;
}

void main()
{
    uint stages = v_more.y;
    bool _171 = (stages & 16u) != 0u;
    bool _178;
    if (_171)
    {
        uint param = stages;
        _178 = !covered(param);
    }
    else
    {
        _178 = _171;
    }
    if (_178)
    {
        discard;
    }
    ivec2 from = ivec2(v_rect.xy);
    ivec2 size = ivec2(v_rect.zw);
    ivec2 t = ivec2(floor(v_texel));
    if ((stages & 8u) != 0u)
    {
        t.x = from.x + int(mod(float(t.x - from.x), float(size.x)));
    }
    t = clamp(t, from, (from + size) - ivec2(1));
    highp vec4 c = texelFetch(source, t, 0);
    if ((stages & 1u) != 0u)
    {
        int index = int((c.x * 255.0) + 0.5);
        if (((stages & 2u) != 0u) && (index == 0))
        {
            discard;
        }
        c = texelFetch(palette, ivec2(index, int(v_more.x)), 0);
    }
    else
    {
        bool _276 = (stages & 2u) != 0u;
        bool _282;
        if (_276)
        {
            _282 = c.w < 0.5;
        }
        else
        {
            _282 = _276;
        }
        if (_282)
        {
            discard;
        }
    }
    if ((stages & 4u) != 0u)
    {
        c.x = 1.0;
        c.y = 1.0;
        c.z = 1.0;
    }
    colour = vec4((c.xyz * v_tint.xyz) * v_tint.w, 1.0);
}

`;
