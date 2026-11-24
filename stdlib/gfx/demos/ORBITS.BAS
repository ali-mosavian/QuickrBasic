' A sun and its planets at 32 bits. The sun glows, each
' planet trails an arc of its orbit, a moon circles the
' third, and a comet sweeps round, turned to its heading.
' Sized by the screen (stage.bas): W and H may be any size,
' and the sizes below are in units. Up and Down (or + and
' -) change the speed, S smooths the comet. Esc stops it.
import gfx.display, gfx.surface, gfx.draw
from stage import Stage, measure, label

const W = 640
const H = 480
const PLANETS = 4
const STARS = 90
const STAR_R = 0.6
const TRAIL = 1.2
const FULL = 63
const GLOW = 10
const SUN_R = 10
const GLOW_R = 30
const GLOW_STEP = 3
const RING_IN = 3.5
const RING_OUT = 5
const MOON_ORBIT = 12
const MOON_R = 2
const ORBIT_COLOUR = &H1C2434
const MOON_COLOUR = &HC8C8D0
' the comet's picture, in its own pixels
const COMET_W = 24
const COMET_H = 7
' its ellipse, in shares of the screen's width and height:
' half its width and height, and its centre right of the
' sun's
const COMET_A = 0.4
const COMET_B = 0.4
const COMET_SHIFT = 0.05
const COMET_SCALE = 2
' text's distance from the edges, in its own pixels
const MARGIN = 4
const PI = 3.14159265
const ESC = 27

' A planet: its orbit's radius and its own, in units; its
' turn a second and its angle, in radians.
type Planet
    orbit as single
    speed as single
    size as single
    colour as long
    ringed as integer
    angle as single
end type

' A star: its place, a share of the screen's width and
' height, so it keeps it at any size.
type Star
    x as single
    y as single
    shade as integer
end type

declare sub scatter (sky() as Star)
declare sub make_planet ( _
    p as Planet, _
    byval orbit as single, _
    byval speed as single, _
    byval size as single, _
    byval colour as long _
)
declare function new_comet () as long
declare sub draw_sky ( _
    byval dst as long, _
    st as Stage, _
    sky() as Star, _
    byval t as single _
)
declare sub draw_sun (byval dst as long, st as Stage)
declare sub place ( _
    st as Stage, _
    p as Planet, _
    x as single, _
    y as single _
)
declare sub draw_planet (byval dst as long, st as Stage, p as Planet)
declare sub draw_moon ( _
    byval dst as long, _
    st as Stage, _
    p as Planet, _
    byval t as single _
)
declare sub draw_comet ( _
    byval dst as long, _
    st as Stage, _
    byval src as long, _
    byval t as single, _
    byval flags as integer _
)
declare function heading ( _
    byval dx as single, _
    byval dy as single _
) as single
declare sub show_help ( _
    byval dst as long, _
    st as Stage, _
    byval speed as single, _
    byval smooth as integer _
)

canvas& = gfx.display.open(W, H, 32, gfx.display.VSYNC)
randomize timer
dim st as Stage
dim sky(STARS - 1) as Star
dim worlds(PLANETS - 1) as Planet
scatter sky()
make_planet worlds(0), 36, 1.6, 3, gfx.display.rgb(200, 160, 120)
make_planet worlds(1), 58, 1.1, 5, gfx.display.rgb(90, 170, 255)
make_planet worlds(2), 84, 0.7, 6, gfx.display.rgb(80, 220, 120)
make_planet worlds(3), 110, 0.45, 7, gfx.display.rgb(240, 150, 80)
worlds(3).ringed = -1
comet& = new_comet()
smooth% = gfx.draw.SMOOTH
speed! = 1
t! = 0
was# = gfx.display.clock

do
    now# = gfx.display.clock
    dt! = (now# - was#) * speed!
    was# = now#
    t! = t! + dt!
    measure canvas&, st
    gfx.draw.clear canvas&, 0
    draw_sky canvas&, st, sky(), t!
    draw_sun canvas&, st

    for i = 0 to PLANETS - 1
        worlds(i).angle = worlds(i).angle + worlds(i).speed * dt!
        draw_planet canvas&, st, worlds(i)
    next

    draw_moon canvas&, st, worlds(2), t!
    draw_comet canvas&, st, comet&, t!, smooth%
    show_help canvas&, st, speed!, smooth%
    gfx.display.flip
    k$ = ucase$(inkey$)

    if k$ = chr$(ESC) then exit do

    select case k$
        case chr$(0) + "H", "+", "="
            speed! = speed! * 1.5
        case chr$(0) + "P", "-"
            speed! = speed! / 1.5
        case "S"
            smooth% = gfx.draw.SMOOTH - smooth%
    end select
loop

gfx.display.close

' Stars anywhere on the screen, each its own brightness.
sub scatter (sky() as Star)
    for i = 0 to STARS - 1
        sky(i).x = rnd
        sky(i).y = rnd
        sky(i).shade = 60 + int(rnd * 160)
    next
end sub

' Planet p, somewhere on its orbit.
sub make_planet ( _
    p as Planet, _
    byval orbit as single, _
    byval speed as single, _
    byval size as single, _
    byval colour as long _
)
    p.orbit = orbit
    p.speed = speed
    p.size = size
    p.colour = colour
    p.ringed = 0
    p.angle = rnd * 2 * PI
end sub

' The comet: a tail widening to a white head, its anchor
' the head, so it turns about it.
function new_comet () as long
    s& = gfx.surface.new(COMET_W, COMET_H)
    mid% = COMET_H \ 2

    for x = 0 to COMET_W - 1
        v% = 40 + 215 * x / (COMET_W - 1)
        half% = x * mid% \ (COMET_W - 1)
        c& = gfx.display.rgb(v% \ 2, v% * 0.8, v%)
        gfx.draw.line s&, x, mid% - half%, x, mid% + half%, c&
    next

    white& = gfx.display.rgb(255, 255, 255)
    gfx.draw.circle s&, COMET_W - 4, mid%, 3, white&, gfx.draw.FILL
    gfx.surface.set_anchor s&, COMET_W - 4, mid%
    new_comet = s&
end function

' The stars, each twinkling at its own pace, on whole
' pixels so the smallest is one.
sub draw_sky ( _
    byval dst as long, _
    st as Stage, _
    sky() as Star, _
    byval t as single _
)
    r! = STAR_R * st.unit

    for i = 0 to STARS - 1
        v% = sky(i).shade * (0.75 + 0.25 * sin(t * 3 + i))
        c& = gfx.display.rgb(v%, v%, v%)
        x% = int(sky(i).x * st.w)
        y% = int(sky(i).y * st.h)
        gfx.draw.circle dst, x%, y%, r!, c&, gfx.draw.FILL
    next
end sub

' The sun: rings of orange added up into a glow round a
' bright core.
sub draw_sun (byval dst as long, st as Stage)
    orange& = gfx.display.rgb(255, 140, 40)
    core& = gfx.display.rgb(255, 235, 160)
    glowing% = gfx.draw.FILL + gfx.draw.ADD
    gfx.draw.set_alpha GLOW

    for r = GLOW_R to SUN_R + 2 step -GLOW_STEP
        rr! = r * st.unit
        gfx.draw.circle dst, st.cx, st.cy, rr!, orange&, glowing%
    next

    gfx.draw.set_alpha FULL
    sun! = SUN_R * st.unit
    gfx.draw.circle dst, st.cx, st.cy, sun!, core&, gfx.draw.FILL
end sub

' Where planet p is on the screen.
sub place ( _
    st as Stage, _
    p as Planet, _
    x as single, _
    y as single _
)
    x = st.cx + p.orbit * st.unit * cos(p.angle)
    y = st.cy + p.orbit * st.unit * sin(p.angle)
end sub

' Planet p: its orbit faint, an arc trailing it, then the
' planet, and a ring half seen through when it has one.
sub draw_planet (byval dst as long, st as Stage, p as Planet)
    u! = st.unit
    orbit! = p.orbit * u!
    tail! = p.angle - TRAIL
    place st, p, x!, y!
    gfx.draw.circle dst, st.cx, st.cy, orbit!, ORBIT_COLOUR, 0
    gfx.draw.arc dst, st.cx, st.cy, orbit!, tail!, p.angle, p.colour
    gfx.draw.circle dst, x!, y!, p.size * u!, p.colour, gfx.draw.FILL

    if not p.ringed then exit sub

    gfx.draw.set_alpha FULL \ 2

    for r! = (p.size + RING_IN) * u! to (p.size + RING_OUT) * u!
        gfx.draw.circle dst, x!, y!, r!, p.colour, gfx.draw.BLEND
    next

    gfx.draw.set_alpha FULL
end sub

' A moon circling planet p.
sub draw_moon ( _
    byval dst as long, _
    st as Stage, _
    p as Planet, _
    byval t as single _
)
    u! = st.unit
    place st, p, x!, y!
    x! = x! + MOON_ORBIT * u! * cos(t * 4)
    y! = y! + MOON_ORBIT * u! * sin(t * 4)
    r! = MOON_R * u!
    gfx.draw.circle dst, x!, y!, r!, MOON_COLOUR, gfx.draw.FILL
end sub

' The comet on an ellipse round the sun that fills the
' screen, turned to its heading, added to what is under it.
sub draw_comet ( _
    byval dst as long, _
    st as Stage, _
    byval src as long, _
    byval t as single, _
    byval flags as integer _
)
    a! = COMET_A * st.w
    b! = COMET_B * st.h
    v! = t * 0.5
    x! = st.cx + COMET_SHIFT * st.w + a! * cos(v!)
    y! = st.cy + b! * sin(v!)
    turned! = heading(-a! * sin(v!), b! * cos(v!))
    f% = flags + gfx.draw.ADD
    size! = COMET_SCALE * st.unit
    gfx.draw.sprite_turned dst, x!, y!, turned!, size!, src, f%
end sub

' The angle of dx, dy: 0 to the right, clockwise.
function heading ( _
    byval dx as single, _
    byval dy as single _
) as single
    heading = 0

    if dx = 0 and dy = 0 then exit function

    if dx = 0 then
        heading = sgn(dy) * PI / 2
        exit function
    end if

    a! = atn(dy / dx)

    if dx < 0 then a! = a! + PI

    heading = a!
end function

' The keys along dst's top.
sub show_help ( _
    byval dst as long, _
    st as Stage, _
    byval speed as single, _
    byval smooth as integer _
)
    grey& = gfx.display.rgb(200, 200, 200)
    edge% = MARGIN * st.zoom
    t$ = "UP/DN speed" + str$(int(speed * 100) / 100)
    t$ = t$ + "   S smooth "

    if smooth then
        t$ = t$ + "on"
    else
        t$ = t$ + "off"
    end if

    label dst, edge%, edge%, t$, grey&, st.zoom
end sub
