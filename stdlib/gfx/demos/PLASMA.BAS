' A plasma at 8 bits, drawn once and set moving by its
' palette alone: each frame turns the colours its indexes
' show. Bars of fixed colours rise and fall over it.
' Sized by the screen (stage.bas). P changes the palette,
' B hides the bars, Esc stops it.
import gfx.display, gfx.surface, gfx.draw
from stage import Stage, measure, label_shaded
from meter import Meter, KEPT, count_frame, rate_text$

const W = 640
const H = 480
' the plasma's indexes, turned through each frame
const CYCLE = 192
' indexes it turns through a second
const SPIN = 48
' the fixed indexes: black, the bars' shades, grey, white
const BLACK = 192
const SHADES = 193
const GREY = 254
const WHITE = 255
const BARS = 3
' shades in a bar, dark to light
const RAMP = 12
' a bar's swing about the centre, and its pace, in units
const SWING = 80
const PACE = 1.3
const PALETTES = 3
const MARGIN = 4
const PI = 3.14159265
const ESC = 27

declare sub make_plasma (byval s as long, st as Stage)
declare function plasma_at ( _
    byval x as single, _
    byval y as single _
) as single
declare sub fix_colours (colours() as long)
declare sub turn_colours ( _
    colours() as long, _
    byval kind as integer, _
    byval shift as single _
)
declare function shade_of ( _
    byval kind as integer, _
    byval f as single _
) as long
declare function part (byval v as single) as integer
declare sub draw_bars ( _
    byval dst as long, _
    st as Stage, _
    byval t as single _
)
declare sub show_help ( _
    byval dst as long, _
    st as Stage, _
    m as Meter _
)

canvas& = gfx.display.open(W, H, 8, gfx.display.VSYNC)
dim st as Stage
dim colours(255) as long
dim fps as Meter
dim rates(KEPT - 1) as single
measure canvas&, st
plasma& = gfx.surface.new(st.w, st.h)
make_plasma plasma&, st
fix_colours colours()
kind% = 0
striped% = -1
t! = 0
was# = gfx.display.clock
fps.since = was#

do
    now# = gfx.display.clock
    t! = t! + (now# - was#)
    was# = now#
    count_frame fps, rates(), now#
    turn_colours colours(), kind%, t! * SPIN
    gfx.display.set_palette 0, colours()
    gfx.draw.sprite canvas&, 0, 0, plasma&, gfx.draw.OPAQUE

    if striped% then draw_bars canvas&, st, t!

    show_help canvas&, st, fps
    gfx.display.flip
    k$ = ucase$(inkey$)

    if k$ = chr$(ESC) then exit do

    select case k$
        case "P"
            kind% = (kind% + 1) mod PALETTES
        case "B"
            striped% = not striped%
    end select
loop

gfx.display.close

' The plasma's indexes written into s, a byte a pixel,
' measured in units so it looks the same at any size.
sub make_plasma (byval s as long, st as Stage)
    n& = st.w
    n& = n& * st.h
    dim px(n& - 1) as byte
    at& = 0

    for y = 0 to st.h - 1
        for x = 0 to st.w - 1
            v! = plasma_at(x / st.unit, y / st.unit)
            px(at&) = int(v! * CYCLE) mod CYCLE
            at& = at& + 1
        next
    next

    gfx.surface.write s, 0, 0, st.w, st.h, px()
end sub

' The plasma at x, y in units: four waves summed, 0 to
' under 1.
function plasma_at ( _
    byval x as single, _
    byval y as single _
) as single
    dx! = x - 160
    dy! = y - 120
    v! = sin(x / 16) + sin(y / 12)
    v! = v! + sin((x + y) / 22)
    v! = v! + sin(sqr(dx! * dx! + dy! * dy!) / 9)
    plasma_at = (v! + 4) / 8.0001
end function

' The fixed colours: black, the bars' shades, grey, white.
sub fix_colours (colours() as long)
    colours(BLACK) = 0

    for bar = 0 to BARS - 1
        hue% = bar + 1

        for k = 0 to RAMP - 1
            v% = 40 + 215 * k \ (RAMP - 1)
            wash% = 120 * k \ (RAMP - 1)
            r% = part(v% * (hue% and 1) + wash%)
            g% = part(v% * (hue% \ 2 and 1) + wash%)
            b% = part(v% * (hue% \ 4 and 1) + wash%)
            c& = r% * 65536 + g% * 256 + b%
            colours(SHADES + bar * RAMP + k) = c&
        next
    next

    colours(GREY) = &HA0A0A0
    colours(WHITE) = &HFFFFFF
end sub

' The plasma's colours turned by shift indexes, in palette
' kind.
sub turn_colours ( _
    colours() as long, _
    byval kind as integer, _
    byval shift as single _
)
    for i = 0 to CYCLE - 1
        f! = (i + shift) / CYCLE
        colours(i) = shade_of(kind, f! - int(f!))
    next
end sub

' Palette kind's colour at f, 0 to 1 round: a rainbow, a
' fire, a sea. The last two go and come back, so the turn
' has no seam.
function shade_of ( _
    byval kind as integer, _
    byval f as single _
) as long
    a! = 2 * PI * f
    t! = 1 - abs(2 * f - 1)

    select case kind
        case 0
            r% = part(128 + 127 * sin(a!))
            g% = part(128 + 127 * sin(a! + 2 * PI / 3))
            b% = part(128 + 127 * sin(a! + 4 * PI / 3))
        case 1
            r% = part(765 * t!)
            g% = part(765 * t! - 255)
            b% = part(765 * t! - 510)
        case else
            r% = part(30 * t!)
            g% = part(40 + 180 * t!)
            b% = part(90 + 165 * t!)
    end select

    shade_of = r% * 65536 + g% * 256 + b%
end function

' v as a colour's part, 0 to 255.
function part (byval v as single) as integer
    part = 0

    if v <= 0 then exit function

    part = 255

    if v >= 255 then exit function

    part = int(v)
end function

' The bars: each a ramp of shades up to its middle and
' down, a line of units each, swinging on its own beat.
sub draw_bars ( _
    byval dst as long, _
    st as Stage, _
    byval t as single _
)
    u! = st.unit
    right! = st.w - 1

    for b = 0 to BARS - 1
        beat! = t * PACE + b * 2 * PI / BARS
        top! = st.cy + (SWING * sin(beat!) - RAMP) * u!

        for k = 0 to 2 * RAMP - 2
            shade% = RAMP - 1 - abs(k - (RAMP - 1))
            c% = SHADES + b * RAMP + shade%
            y1! = top! + k * u!
            y2! = y1! + u! - 1
            gfx.draw.rect dst, 0, y1!, right!, y2!, c%, gfx.draw.FILL
        next
    next
end sub

' The keys along dst's top, the frame rate at its foot,
' each over a shadow.
sub show_help ( _
    byval dst as long, _
    st as Stage, _
    m as Meter _
)
    z% = st.zoom
    edge% = MARGIN * z%
    foot% = st.h - edge% - st.text_h
    t$ = "P palette   B bars"
    label_shaded dst, edge%, edge%, t$, WHITE, BLACK, z%
    r$ = rate_text$(m)
    label_shaded dst, edge%, foot%, r$, GREY, BLACK, z%
end sub
