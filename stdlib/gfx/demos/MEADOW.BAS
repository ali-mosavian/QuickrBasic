' A meadow, busy: a sky, two ranges of hills going by, a
' floor in perspective, blossoms turned, scaled and lit
' swarming over it, glows added on top. Every pixel is
' drawn several times a frame and nothing is read back:
' heavy for a CPU, light for a 3D card. At 8 bits, or 16 to
' compare (DEPTH). Its foot tells the frame rate and what
' drew it. Esc stops it.
import gfx.display, gfx.surface, gfx.draw
from gfx.display import FrameStats
from gfx.draw import Vec2, Vertex
from meter import Meter, KEPT, count_frame, rate_text$

const W = 640
const H = 480
' bits a pixel: 8, its textures indices; or 16, their
' colours
const DEPTH = 8
' the sky's foot, where the floor starts
const HORIZON = 280
' the palette's ramps, each dark to light: the sky, the
' petals' hues, the floor's tones, the hills' ranges, the
' glows
const SKY_AT = 1
const SKY_N = 64
const PETAL_AT = 65
const HUES = 4
const SHADES = 16
const FLOOR_AT = 129
const TONES = 2
const HILL_AT = 161
const RANGES = 2
const GLOW_AT = 193
const GLOW_N = 32
const WHITE = 255
' the textures' sizes
const SKY_W = 4
const HILL_W = 256
const HILL_H = 96
const TILE = 64
const BLOOM = 64
' blossoms and glows
const SWARM = 48
const LIGHTS = 16
const MARGIN = 8
const PI = 3.14159265
const ESC = 27

' A blossom or a glow: the middle of its loop, the loop's
' size, its pace and start round it, its spin, its size and
' its hue.
type Mover
    cx as single
    cy as single
    rx as single
    ry as single
    pace as single
    phase as single
    spin as single
    size as single
    hue as integer
end type

declare sub set_colours (colours() as long)
declare function tone& (byval rgb as long, byval f as single)
declare sub put_texels ( _
    byval s as long, _
    byval across as integer, _
    byval down as integer, _
    px() as byte, _
    colours() as long _
)
declare sub make_sky (byval s as long, colours() as long)
declare sub make_hills ( _
    byval s as long, _
    byval range as integer, _
    colours() as long _
)
declare sub make_floor (byval s as long, colours() as long)
declare sub make_blossom ( _
    byval s as long, _
    byval hue as integer, _
    colours() as long _
)
declare sub make_glow (byval s as long, colours() as long)
declare function angle! (byval dy as single, byval dx as single)
declare sub launch (movers() as Mover, byval size as single)
declare sub draw_sky (byval dst as long, byval s as long)
declare sub draw_hills ( _
    byval dst as long, _
    hills() as long, _
    byval t as single _
)
declare sub draw_floor ( _
    byval dst as long, _
    byval s as long, _
    byval t as single _
)
declare sub draw_blossoms ( _
    byval dst as long, _
    blossoms() as Mover, _
    petals() as long, _
    byval t as single _
)
declare sub draw_glows ( _
    byval dst as long, _
    glows() as Mover, _
    byval s as long, _
    byval t as single _
)

randomize timer
canvas& = gfx.display.open(W, H, DEPTH, gfx.display.VSYNC)
dim colours(255) as long
set_colours colours()
ink& = WHITE

if DEPTH > 8 then ink& = colours(WHITE)

sky& = gfx.surface.new(SKY_W, SKY_N)
make_sky sky&, colours()
dim hills(RANGES - 1) as long

for r = 0 to RANGES - 1
    hills(r) = gfx.surface.new(HILL_W, HILL_H)
    make_hills hills(r), r, colours()
next

floor& = gfx.surface.new(TILE, TILE)
make_floor floor&, colours()
dim petals(HUES - 1) as long

for k = 0 to HUES - 1
    petals(k) = gfx.surface.new(BLOOM, BLOOM)
    make_blossom petals(k), k, colours()
next

glow& = gfx.surface.new(BLOOM, BLOOM)
make_glow glow&, colours()
dim blossoms(SWARM - 1) as Mover
dim glows(LIGHTS - 1) as Mover
launch blossoms(), 1.6
launch glows(), 3
dim fps as Meter
dim rates(KEPT - 1) as single
dim cost as FrameStats
start# = gfx.display.clock
fps.since = start#

do
    now# = gfx.display.clock
    count_frame fps, rates(), now#
    t! = now# - start#
    draw_sky canvas&, sky&
    draw_hills canvas&, hills(), t!
    draw_floor canvas&, floor&, t!
    draw_blossoms canvas&, blossoms(), petals(), t!
    draw_glows canvas&, glows(), glow&, t!
    gfx.display.get_stats cost
    by$ = "  in pages"

    if cost.batches then by$ = "  on the GPU"

    y% = H - MARGIN - 8
    t$ = rate_text$(fps) + by$
    gfx.draw.text canvas&, MARGIN, y%, t$, ink&
    gfx.display.flip

    if inkey$ = chr$(ESC) then exit do
loop

gfx.display.close

' Each ramp's colours: the sky deep blue to a dusk glow;
' the petals pink, violet, gold and white; the floor's two
' greens; the hills' two ranges; the glows black to warm
' white.
sub set_colours (colours() as long)
    for i = 0 to SKY_N - 1
        f! = i / (SKY_N - 1)
        r% = 20 + 230 * f!
        g% = 30 + 150 * f! * f!
        b% = 90 + 50 * f!
        colours(SKY_AT + i) = r% * 65536 + g% * 256 + b%
    next

    dim hue(HUES - 1) as long
    hue(0) = &HFF78B4
    hue(1) = &HAA6EFF
    hue(2) = &HFFC83C
    hue(3) = &HF0F0FF

    for k = 0 to HUES - 1
        for i = 0 to SHADES - 1
            f! = .25 + .75 * i / (SHADES - 1)
            colours(PETAL_AT + k * SHADES + i) = tone&(hue(k), f!)
        next
    next

    for i = 0 to SHADES - 1
        f! = .35 + .65 * i / (SHADES - 1)
        colours(FLOOR_AT + i) = tone&(&H2E5A28, f!)
        colours(FLOOR_AT + SHADES + i) = tone&(&H5A9A44, f!)
        colours(HILL_AT + i) = tone&(&H6A5A9C, f!)
        colours(HILL_AT + SHADES + i) = tone&(&H34503C, f!)
    next

    for i = 0 to GLOW_N - 1
        colours(GLOW_AT + i) = tone&(&HFFDCA0, i / (GLOW_N - 1))
    next

    colours(WHITE) = &HFFFFFF
    gfx.display.set_palette 0, colours()
end sub

' rgb, &HRRGGBB, its channels times f, 0 to 1.
function tone& (byval rgb as long, byval f as single)
    r% = (rgb \ 65536 and 255) * f
    g% = (rgb \ 256 and 255) * f
    b% = (rgb and 255) * f
    tone& = r% * 65536 + g% * 256 + b%
end function

' Indices px(), across by down, written into s: at 8 bits
' as they are; at 16, their colours, two bytes a pixel.
sub put_texels ( _
    byval s as long, _
    byval across as integer, _
    byval down as integer, _
    px() as byte, _
    colours() as long _
)
    if DEPTH = 8 then
        gfx.surface.write s, 0, 0, across, down, px()
        exit sub
    end if

    n& = across
    n& = n& * down
    dim two(2 * n& - 1) as byte

    for i& = 0 to n& - 1
        c& = colours(px(i&))
        r% = c& \ 65536 and 255
        g% = c& \ 256 and 255
        b% = c& and 255
        v& = (r% \ 8) * 2048 + (g% \ 4) * 32 + b% \ 8
        two(2 * i&) = v& and 255
        two(2 * i& + 1) = v& \ 256
    next

    gfx.surface.write s, 0, 0, across, down, two()
end sub

' The sky's ramp, a row an index, top to foot.
sub make_sky (byval s as long, colours() as long)
    dim px(SKY_W * SKY_N - 1) as byte

    for y = 0 to SKY_N - 1
        for x = 0 to SKY_W - 1
            px(y * SKY_W + x) = SKY_AT + y
        next
    next

    put_texels s, SKY_W, SKY_N, px(), colours()
end sub

' A range of hills, its ridge waves that meet across its
' edges, so it repeats; shaded darker down from the ridge,
' clear above it.
sub make_hills ( _
    byval s as long, _
    byval range as integer, _
    colours() as long _
)
    dim px(HILL_W * HILL_H - 1) as byte
    first% = HILL_AT + range * SHADES
    a! = 2 * PI / HILL_W

    for x = 0 to HILL_W - 1
        ridge! = 30 + 14 * sin(a! * x * (2 + range))
        ridge! = ridge! + 9 * sin(a! * x * (5 + range) + 1)
        ridge! = ridge! + 4 * sin(a! * x * 11 + range)

        top% = ridge!

        for y = top% to HILL_H - 1
            shade% = SHADES - 1 - (y - top%) \ 5

            if shade% < 0 then shade% = 0

            px(y * HILL_W + x) = first% + shade%
        next
    next

    put_texels s, HILL_W, HILL_H, px(), colours()
end sub

' The floor's tile: a checker of two greens, each square
' mottled.
sub make_floor (byval s as long, colours() as long)
    dim px(TILE * TILE - 1) as byte
    half% = TILE \ 2

    for y = 0 to TILE - 1
        for x = 0 to TILE - 1
            t% = ((x \ half%) + (y \ half%)) mod TONES
            shade% = 8 + 7 * sin(x * .7 + y * .4) * cos(y * .5)
            px(y * TILE + x) = FLOOR_AT + t% * SHADES + shade%
        next
    next

    put_texels s, TILE, TILE, px(), colours()
end sub

' A five-petalled blossom in hue's shades, lighter to its
' tips, its heart gold; clear round it. Its anchor is its
' middle.
sub make_blossom ( _
    byval s as long, _
    byval hue as integer, _
    colours() as long _
)
    dim px(BLOOM * BLOOM - 1) as byte
    c! = BLOOM / 2
    heart% = PETAL_AT + 2 * SHADES + SHADES - 1

    for y = 0 to BLOOM - 1
        for x = 0 to BLOOM - 1
            dx! = x - c! + .5
            dy! = y - c! + .5
            r! = sqr(dx! * dx! + dy! * dy!)
            reach! = 14 + 16 * abs(cos(2.5 * angle!(dy!, dx!)))

            if r! < 5 then
                px(y * BLOOM + x) = heart%
            elseif r! < reach! then
                shade% = 4 + 11 * r! / reach!
                px(y * BLOOM + x) = PETAL_AT + hue * SHADES + shade%
            end if
        next
    next

    put_texels s, BLOOM, BLOOM, px(), colours()
    gfx.surface.set_anchor s, BLOOM \ 2, BLOOM \ 2
end sub

' The angle of dx, dy from the x axis, -PI / 2 to 3 * PI / 2.
function angle! (byval dy as single, byval dx as single)
    angle! = sgn(dy) * PI / 2

    if dx = 0 then exit function

    a! = atn(dy / dx)

    if dx < 0 then a! = a! + PI

    angle! = a!
end function

' A round glow, brightest in its middle, its anchor.
sub make_glow (byval s as long, colours() as long)
    dim px(BLOOM * BLOOM - 1) as byte
    c! = BLOOM / 2
    reach! = c! - 2

    for y = 0 to BLOOM - 1
        for x = 0 to BLOOM - 1
            dx! = x - c! + .5
            dy! = y - c! + .5
            f! = 1 - sqr(dx! * dx! + dy! * dy!) / reach!

            if f! > 0 then
                px(y * BLOOM + x) = GLOW_AT + (GLOW_N - 1) * f! * f!
            end if
        next
    next

    put_texels s, BLOOM, BLOOM, px(), colours()
    gfx.surface.set_anchor s, BLOOM \ 2, BLOOM \ 2
end sub

' Movers scattered over the screen, up to size times their
' texture's size.
sub launch (movers() as Mover, byval size as single)
    for i = lbound(movers) to ubound(movers)
        movers(i).cx = W * (.1 + .8 * rnd)
        movers(i).cy = H * (.15 + .7 * rnd)
        movers(i).rx = 40 + 120 * rnd
        movers(i).ry = 20 + 80 * rnd
        movers(i).pace = .3 + .7 * rnd
        movers(i).phase = 2 * PI * rnd
        movers(i).spin = 2 * rnd - 1
        movers(i).size = size * (.5 + .5 * rnd)
        movers(i).hue = int(HUES * rnd)
    next
end sub

' The sky stretched over the screen down to the horizon.
sub draw_sky (byval dst as long, byval s as long)
    dim c(3) as Vec2
    c(1).x = W
    c(2).x = W
    c(2).y = HORIZON
    c(3).y = HORIZON
    gfx.draw.quad dst, c(), 0, s, gfx.draw.OPAQUE
end sub

' Each range of hills at twice its size along the horizon,
' the near one going by faster.
sub draw_hills ( _
    byval dst as long, _
    hills() as long, _
    byval t as single _
)
    dim c(3) as Vec2
    span! = 2 * HILL_W

    for r = 0 to RANGES - 1
        went! = t * (25 + 45 * r)
        x! = -(went! - span! * int(went! / span!))
        top! = HORIZON - 2 * HILL_H + 30 * r

        do while x! < W
            c(0).x = x!
            c(0).y = top!
            c(1).x = x! + span!
            c(1).y = top!
            c(2).x = x! + span!
            c(2).y = top! + 2 * HILL_H
            c(3).x = x!
            c(3).y = top! + 2 * HILL_H
            gfx.draw.quad dst, c(), 0, hills(r), 0
            x! = x! + span!
        loop
    next
end sub

' The floor from the horizon to the screen's foot, its
' tiles in perspective coming on.
sub draw_floor ( _
    byval dst as long, _
    byval s as long, _
    byval t as single _
)
    dim q(3) as Vertex
    went! = t * 90
    v! = went! - TILE * int(went! / TILE)
    q(0).y = HORIZON
    q(1).x = W
    q(1).y = HORIZON
    q(2).x = W
    q(2).y = H
    q(3).y = H
    q(0).z = 12
    q(1).z = 12
    q(2).z = 1
    q(3).z = 1
    q(0).u = -12 * W / 4
    q(1).u = 12 * W / 4
    q(2).u = W / 4
    q(3).u = -W / 4
    q(0).v = 12 * TILE * 4 - v!
    q(1).v = 12 * TILE * 4 - v!
    q(2).v = -v!
    q(3).v = -v!
    gfx.draw.poly dst, q(), 0, 4, s, 0, gfx.draw.PERSPECTIVE
end sub

' Each blossom round its loop, turning, its size and light
' pulsing.
sub draw_blossoms ( _
    byval dst as long, _
    blossoms() as Mover, _
    petals() as long, _
    byval t as single _
)
    for i = lbound(blossoms) to ubound(blossoms)
        a! = t * blossoms(i).pace + blossoms(i).phase
        x! = blossoms(i).cx + blossoms(i).rx * sin(a!)
        y! = blossoms(i).cy + blossoms(i).ry * sin(1.3 * a!)
        pulse! = .5 + .5 * sin(2 * a!)
        size! = blossoms(i).size * (.7 + .6 * pulse!)
        turn! = t * blossoms(i).spin
        gfx.draw.set_light 20 + 43 * pulse!
        p& = petals(blossoms(i).hue)
        gfx.draw.sprite_turned dst, x!, y!, turn!, size!, p&, 0
    next

    gfx.draw.reset
end sub

' Each glow round its loop, added to what is under it.
sub draw_glows ( _
    byval dst as long, _
    glows() as Mover, _
    byval s as long, _
    byval t as single _
)
    for i = lbound(glows) to ubound(glows)
        a! = t * glows(i).pace + glows(i).phase
        x! = glows(i).cx + glows(i).rx * sin(a!)
        y! = glows(i).cy + glows(i).ry * cos(a!)
        f% = gfx.draw.ADD
        gfx.draw.sprite_turned dst, x!, y!, 0, glows(i).size, s, f%
    next
end sub
