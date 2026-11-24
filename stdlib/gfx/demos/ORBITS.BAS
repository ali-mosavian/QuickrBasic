' A sun and its planets at 32 bits. The sun glows, each
' planet trails an arc of its orbit, a moon circles the
' third, and a comet sweeps past turned to its heading.
' Up and Down (or + and -) change the speed, S smooths the
' comet. Esc stops it.
import gfx.display, gfx.surface, gfx.draw

const W = 320
const H = 240
const CX = 160
const CY = 120
const PLANETS = 4
const STARS = 90
const TRAIL = 1.2
const FULL = 63
const GLOW = 10
const ORBIT_COLOUR = &H1C2434
const MOON_COLOUR = &HC8C8D0
const COMET_W = 24
const COMET_H = 7
const COMET_A = 135
const COMET_B = 95
const COMET_SHIFT = 30
const PI = 3.14159265
const ESC = 27

type Planet
    orbit as single
    speed as single
    size as single
    colour as long
    ringed as integer
    angle as single
end type

type Star
    x as integer
    y as integer
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
    sky() as Star, _
    byval t as single _
)
declare sub draw_sun (byval dst as long)
declare sub draw_planet (byval dst as long, p as Planet)
declare sub draw_moon ( _
    byval dst as long, _
    p as Planet, _
    byval t as single _
)
declare sub draw_comet ( _
    byval dst as long, _
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
    byval speed as single, _
    byval smooth as integer _
)

canvas& = gfx.display.open(W, H, 32, gfx.display.VSYNC)
randomize timer
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
    gfx.draw.clear canvas&, 0
    draw_sky canvas&, sky(), t!
    draw_sun canvas&

    for i = 0 to PLANETS - 1
        worlds(i).angle = worlds(i).angle + worlds(i).speed * dt!
        draw_planet canvas&, worlds(i)
    next

    draw_moon canvas&, worlds(2), t!
    draw_comet canvas&, comet&, t!, smooth%
    show_help canvas&, speed!, smooth%
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
        sky(i).x = int(rnd * W)
        sky(i).y = int(rnd * H)
        sky(i).shade = 60 + int(rnd * 160)
    next
end sub

' Planet p, at the start of its orbit.
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

' The stars, each twinkling at its own pace.
sub draw_sky (byval dst as long, sky() as Star, byval t as single)
    for i = 0 to STARS - 1
        v% = sky(i).shade * (0.75 + 0.25 * sin(t * 3 + i))
        c& = gfx.display.rgb(v%, v%, v%)
        gfx.draw.pset dst, sky(i).x, sky(i).y, c&
    next
end sub

' The sun: rings of orange added up into a glow round a
' bright core.
sub draw_sun (byval dst as long)
    orange& = gfx.display.rgb(255, 140, 40)
    core& = gfx.display.rgb(255, 235, 160)
    glowing% = gfx.draw.FILL + gfx.draw.ADD
    gfx.draw.set_alpha GLOW

    for r = 30 to 12 step -3
        gfx.draw.circle dst, CX, CY, r, orange&, glowing%
    next

    gfx.draw.set_alpha FULL
    gfx.draw.circle dst, CX, CY, 10, core&, gfx.draw.FILL
end sub

' Planet p: its orbit faint, an arc trailing it, then the
' planet, and a ring half seen through when it has one.
sub draw_planet (byval dst as long, p as Planet)
    x! = CX + p.orbit * cos(p.angle)
    y! = CY + p.orbit * sin(p.angle)
    gfx.draw.circle dst, CX, CY, p.orbit, ORBIT_COLOUR, 0
    tail! = p.angle - TRAIL
    gfx.draw.arc dst, CX, CY, p.orbit, tail!, p.angle, p.colour
    gfx.draw.circle dst, x!, y!, p.size, p.colour, gfx.draw.FILL

    if not p.ringed then exit sub

    gfx.draw.set_alpha FULL \ 2
    gfx.draw.circle dst, x!, y!, p.size + 4, p.colour, gfx.draw.BLEND
    gfx.draw.circle dst, x!, y!, p.size + 5, p.colour, gfx.draw.BLEND
    gfx.draw.set_alpha FULL
end sub

' A moon circling planet p.
sub draw_moon (byval dst as long, p as Planet, byval t as single)
    x! = CX + p.orbit * cos(p.angle) + 12 * cos(t * 4)
    y! = CY + p.orbit * sin(p.angle) + 12 * sin(t * 4)
    gfx.draw.circle dst, x!, y!, 2, MOON_COLOUR, gfx.draw.FILL
end sub

' The comet on an ellipse round the sun, turned to its
' heading, added to what is under it.
sub draw_comet ( _
    byval dst as long, _
    byval src as long, _
    byval t as single, _
    byval flags as integer _
)
    u! = t * 0.5
    x! = CX + COMET_SHIFT + COMET_A * cos(u!)
    y! = CY + COMET_B * sin(u!)
    a! = heading(-COMET_A * sin(u!), COMET_B * cos(u!))
    f% = flags + gfx.draw.ADD
    gfx.draw.sprite_turned dst, x!, y!, a!, 2, src, f%
end sub

' The angle of dx, dy: 0 to the right, clockwise.
function heading (byval dx as single, byval dy as single) as single
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
    byval speed as single, _
    byval smooth as integer _
)
    grey& = gfx.display.rgb(200, 200, 200)
    t$ = "UP/DN speed" + str$(int(speed * 100) / 100)
    t$ = t$ + "   S smooth "

    if smooth then
        t$ = t$ + "on"
    else
        t$ = t$ + "off"
    end if

    gfx.draw.text dst, 4, 4, t$, grey&
end sub
