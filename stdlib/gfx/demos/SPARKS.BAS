' Fireworks at 32 bits: rockets rise over a town and burst
' into thousands of sparks, each one glow sprite tinted its
' colour, added to the sky and fading; the sky fades a
' little each frame, so they leave trails. Sized by the
' screen (stage.bas). Space sends a rocket, Esc stops it.
import gfx.display, gfx.surface, gfx.draw
from stage import Stage, measure, label
from meter import Meter, KEPT, count_frame, rate_text$

const W = 640
const H = 480
const MOST = 4000
const AIRBORNE = 8
' sparks a burst sends out, at least and at most more
const BURST_LEAST = 160
const BURST_MORE = 140
' seconds between rockets sent by themselves
const EVERY = 0.7
' units a second: a rocket's climb, a burst's speed
const CLIMB = 210
const BLAST = 90
' units a second each second: the pull down
const FALL = 70
' a spark's speed lost a second, a share
const DRAG = 0.9
' a spark's seconds, at least and at most more
const LIFE_LEAST = 1.2
const LIFE_MORE = 1.0
' the sky's light kept a second, a share
const AFTERGLOW = 0.0005
' the glow's sprite: its side; drawn this many units; a
' spark's alpha at its brightest
const GLOW_SIDE = 16
const SPARK_SIZE = 0.45
const SPARK_ALPHA = 44
const HOUSES = 24
' the built-in font's height
const GLYPH_H = 8
const FULL = 63
const MARGIN = 4
const PI = 3.14159265
const ESC = 27

' A spark: where, how fast, its colour, and its seconds
' lived and to live.
type Spark
    x as single
    y as single
    dx as single
    dy as single
    colour as long
    age as single
    life as single
end type

' A rocket on its way up: where, how fast, its colour,
' and its seconds left to burst; none when 0.
type Rocket
    x as single
    y as single
    dx as single
    dy as single
    colour as long
    fuse as single
end type

declare function new_glow () as long
declare sub make_town (roofs() as single)
declare sub launch (rockets() as Rocket, st as Stage)
declare sub move_rockets ( _
    rockets() as Rocket, _
    sparks() as Spark, _
    n as long, _
    byval dt as single _
)
declare sub burst ( _
    sparks() as Spark, _
    n as long, _
    r as Rocket _
)
declare sub move_sparks ( _
    sparks() as Spark, _
    n as long, _
    byval dt as single _
)
declare sub fade_sky ( _
    byval dst as long, _
    st as Stage, _
    byval dt as single _
)
declare sub draw_rockets ( _
    byval dst as long, _
    st as Stage, _
    rockets() as Rocket, _
    byval glow as long _
)
declare sub draw_sparks ( _
    byval dst as long, _
    st as Stage, _
    sparks() as Spark, _
    byval n as long, _
    byval glow as long _
)
declare sub draw_town ( _
    byval dst as long, _
    st as Stage, _
    roofs() as single _
)
declare function bright (byval c as long) as long
declare sub show_help ( _
    byval dst as long, _
    st as Stage, _
    byval n as long, _
    m as Meter _
)
declare sub blank ( _
    byval dst as long, _
    byval x as single, _
    byval y as single, _
    byval t as string, _
    byval zoom as integer _
)

canvas& = gfx.display.open(W, H, 32, gfx.display.VSYNC)
randomize timer
glow& = new_glow()
dim st as Stage
dim fps as Meter
dim rates(KEPT - 1) as single
dim sparks(MOST - 1) as Spark
dim rockets(AIRBORNE - 1) as Rocket
dim roofs(HOUSES - 1) as single
make_town roofs()
n& = 0
due! = 0
was# = gfx.display.clock
fps.since = was#
measure canvas&, st
gfx.draw.clear canvas&, 0

do
    now# = gfx.display.clock
    dt! = now# - was#
    was# = now#
    count_frame fps, rates(), now#
    measure canvas&, st
    due! = due! - dt!

    if due! <= 0 then
        launch rockets(), st
        due! = EVERY * (0.5 + rnd)
    end if

    move_rockets rockets(), sparks(), n&, dt!
    move_sparks sparks(), n&, dt!
    fade_sky canvas&, st, dt!
    draw_rockets canvas&, st, rockets(), glow&
    draw_sparks canvas&, st, sparks(), n&, glow&
    draw_town canvas&, st, roofs()
    show_help canvas&, st, n&, fps
    gfx.display.flip
    k$ = inkey$

    if k$ = chr$(ESC) then exit do
    if k$ = " " then launch rockets(), st
loop

gfx.display.close

' The glow: white, brightest at its middle, its anchor
' there.
function new_glow () as long
    s& = gfx.surface.new(GLOW_SIDE, GLOW_SIDE)
    mid% = GLOW_SIDE \ 2

    for r = mid% to 1 step -1
        f! = 1 - r / (mid% + 1)
        v% = 255 * f! * f!
        c& = gfx.display.rgb(v%, v%, v%)
        gfx.draw.circle s&, mid%, mid%, r, c&, gfx.draw.FILL
    next

    gfx.surface.set_anchor s&, mid%, mid%
    new_glow = s&
end function

' The town's roofs, each house's height in units.
sub make_town (roofs() as single)
    for i = 0 to HOUSES - 1
        roofs(i) = 10 + rnd * 26
    next
end sub

' A rocket from the town at a free place, if one is free,
' leaning a little, in one of the colours.
sub launch (rockets() as Rocket, st as Stage)
    for i = 0 to AIRBORNE - 1
        if rockets(i).fuse <= 0 then exit for
    next

    if i = AIRBORNE then exit sub

    rockets(i).x = (0.15 + rnd * 0.7) * st.w / st.unit
    rockets(i).y = st.h / st.unit
    rockets(i).dx = (rnd - 0.5) * 40
    rockets(i).dy = -CLIMB * (0.8 + rnd * 0.3)
    rockets(i).fuse = 1 + rnd * 0.6

    select case int(rnd * 6)
        case 0
            rockets(i).colour = &HFF4030
        case 1
            rockets(i).colour = &HFFC040
        case 2
            rockets(i).colour = &H40FF60
        case 3
            rockets(i).colour = &H4090FF
        case 4
            rockets(i).colour = &HD050FF
        case else
            rockets(i).colour = &HFFFFFF
    end select
end sub

' Each rocket climbs, slowing; its fuse out, it bursts.
sub move_rockets ( _
    rockets() as Rocket, _
    sparks() as Spark, _
    n as long, _
    byval dt as single _
)
    for i = 0 to AIRBORNE - 1
        if rockets(i).fuse > 0 then
            rockets(i).x = rockets(i).x + rockets(i).dx * dt
            rockets(i).y = rockets(i).y + rockets(i).dy * dt
            rockets(i).dy = rockets(i).dy + FALL * dt
            rockets(i).fuse = rockets(i).fuse - dt

            if rockets(i).fuse <= 0 then burst sparks(), n, rockets(i)
        end if
    next
end sub

' Sparks out of r in every direction, at speeds that fill
' a ball, while there is room.
sub burst ( _
    sparks() as Spark, _
    n as long, _
    r as Rocket _
)
    count% = BURST_LEAST + int(rnd * BURST_MORE)

    for k = 1 to count%
        if n >= MOST then exit sub

        a! = rnd * 2 * PI
        v! = BLAST * sqr(rnd)
        sparks(n).x = r.x
        sparks(n).y = r.y
        sparks(n).dx = r.dx * 0.3 + v! * cos(a!)
        sparks(n).dy = r.dy * 0.3 + v! * sin(a!)
        sparks(n).colour = r.colour

        if rnd < 0.1 then sparks(n).colour = bright(r.colour)

        sparks(n).age = 0
        sparks(n).life = LIFE_LEAST + rnd * LIFE_MORE
        n = n + 1
    next
end sub

' Each spark falls and slows; one past its life takes the
' last one's place.
sub move_sparks ( _
    sparks() as Spark, _
    n as long, _
    byval dt as single _
)
    keep! = 1 - DRAG * dt

    if keep! < 0 then keep! = 0

    i& = 0

    do while i& < n
        sparks(i&).age = sparks(i&).age + dt

        if sparks(i&).age >= sparks(i&).life then
            n = n - 1
            sparks(i&) = sparks(n)
        else
            sparks(i&).dx = sparks(i&).dx * keep!
            sparks(i&).dy = sparks(i&).dy * keep! + FALL * dt
            sparks(i&).x = sparks(i&).x + sparks(i&).dx * dt
            sparks(i&).y = sparks(i&).y + sparks(i&).dy * dt
            i& = i& + 1
        end if
    loop
end sub

' The sky darkened by what it keeps of its light over dt,
' so a spark's trail fades at any frame rate. Black: a
' blend's rounding leaves a few levels above its colour.
sub fade_sky ( _
    byval dst as long, _
    st as Stage, _
    byval dt as single _
)
    alpha% = FULL * (1 - AFTERGLOW ^ dt) + 0.5

    if alpha% < 1 then alpha% = 1

    f% = gfx.draw.FILL + gfx.draw.BLEND
    gfx.draw.set_alpha alpha%
    gfx.draw.rect dst, 0, 0, st.w - 1, st.h - 1, 0, f%
    gfx.draw.set_alpha FULL
end sub

' Each rocket on its way up: a small glow, its colour
' lightened.
sub draw_rockets ( _
    byval dst as long, _
    st as Stage, _
    rockets() as Rocket, _
    byval glow as long _
)
    u! = st.unit
    f% = gfx.draw.ADD + gfx.draw.SMOOTH

    for i = 0 to AIRBORNE - 1
        if rockets(i).fuse > 0 then
            gfx.draw.set_tint bright(rockets(i).colour)
            x! = rockets(i).x * u!
            y! = rockets(i).y * u!
            size! = SPARK_SIZE * u! * 0.7
            gfx.draw.sprite_turned dst, x!, y!, 0, size!, glow, f%
        end if
    next

    gfx.draw.set_tint &HFFFFFF
end sub

' Each spark: the glow tinted its colour, added, fading
' as it ages.
sub draw_sparks ( _
    byval dst as long, _
    st as Stage, _
    sparks() as Spark, _
    byval n as long, _
    byval glow as long _
)
    u! = st.unit
    size! = SPARK_SIZE * u!
    f% = gfx.draw.ADD + gfx.draw.SMOOTH

    for i& = 0 to n - 1
        remain! = 1 - sparks(i&).age / sparks(i&).life
        gfx.draw.set_alpha 1 + int((SPARK_ALPHA - 1) * remain!)
        gfx.draw.set_tint sparks(i&).colour
        x! = sparks(i&).x * u!
        y! = sparks(i&).y * u!
        gfx.draw.sprite_turned dst, x!, y!, 0, size!, glow, f%
    next

    gfx.draw.set_alpha FULL
    gfx.draw.set_tint &HFFFFFF
end sub

' The town along dst's foot, dark against the sky, a
' lit window in some houses.
sub draw_town ( _
    byval dst as long, _
    st as Stage, _
    roofs() as single _
)
    u! = st.unit
    wide! = st.w / HOUSES
    foot! = st.h - 1

    for i = 0 to HOUSES - 1
        x1! = i * wide!
        x2! = x1! + wide! - 1
        top! = foot! - roofs(i) * u!
        f% = gfx.draw.FILL
        gfx.draw.rect dst, x1!, top!, x2!, foot!, &H0A0C14, f%

        if i mod 3 = 1 then
            wx! = x1! + wide! / 2 - u!
            wy! = top! + 4 * u!
            wx2! = wx! + 2 * u!
            wy2! = wy! + 2 * u!
            gfx.draw.rect dst, wx!, wy!, wx2!, wy2!, &HE0B050, f%
        end if
    next
end sub

' c lightened halfway to white.
function bright (byval c as long) as long
    r% = (c \ 65536 and 255)
    g% = (c \ 256 and 255)
    b% = (c and 255)
    r% = (r% + 255) \ 2
    g% = (g% + 255) \ 2
    b% = (b% + 255) \ 2
    bright = r% * 65536 + g% * 256 + b%
end function

' The keys and the sparks counted along dst's top, the
' frame rate at its foot, over the town.
sub show_help ( _
    byval dst as long, _
    st as Stage, _
    byval n as long, _
    m as Meter _
)
    grey& = &HC8C8C8
    z% = st.zoom
    edge% = MARGIN * z%
    t$ = "SPACE rocket   sparks" + str$(n)
    foot% = st.h - edge% - st.text_h
    r$ = rate_text$(m)
    blank dst, edge%, edge%, t$, z%
    blank dst, edge%, foot%, r$, z%
    label dst, st.top, edge%, edge%, t$, grey&, z%
    label dst, st.foot, edge%, foot%, r$, grey&, z%
end sub

' Black under where text t goes at x, y, so the last
' frame's, kept by the sky, does not show through.
sub blank ( _
    byval dst as long, _
    byval x as single, _
    byval y as single, _
    byval t as string, _
    byval zoom as integer _
)
    across! = gfx.draw.text_width(t + "  ") * zoom
    y2! = y + GLYPH_H * zoom
    gfx.draw.rect dst, x, y, x + across!, y2!, 0, gfx.draw.FILL
end sub
