' A flight down a winding tunnel at 32 bits: rings of
' panels, each a polygon in perspective, dimmed by distance
' to black, and a light at the far end. Sized by the screen
' (stage.bas). Up and Down change the speed, S smooths the
' panels, Esc stops it.
import gfx.display, gfx.surface, gfx.draw
from gfx.draw import Vertex
from stage import Stage, measure, label, on_off$
from meter import Meter, KEPT, count_frame, rate_text$

const W = 640
const H = 480
' the tube: sides round, rings seen, their radius and gap
const SIDES = 16
const RINGS = 30
const RADIUS = 1.4
const GAP = 0.5
' the eye's distance from the screen, in units
const LENS = 200
' tube lengths flown a second, and turns rolled
const SPEED = 3
const ROLL = 0.25
' the tunnel's swing from straight
const SWING = 1.2
' a ring of lit panels every few
const LIT_EVERY = 6
' a panel's texture: its side, and its rivets' inset
const SIDE = 64
const INSET = 6
const FULL = 63
const MARGIN = 4
const PI = 3.14159265
const ESC = 27

declare function new_panel (byval lit as integer) as long
declare function path_x (byval z as single) as single
declare function path_y (byval z as single) as single
declare sub place_rings ( _
    st as Stage, _
    sx() as single, _
    sy() as single, _
    sz() as single, _
    byval travel as single, _
    byval turn as single _
)
declare sub draw_tunnel ( _
    byval dst as long, _
    sx() as single, _
    sy() as single, _
    sz() as single, _
    byval first as long, _
    byval plain as long, _
    byval lit as long, _
    byval flags as integer _
)
declare sub draw_glow ( _
    byval dst as long, _
    st as Stage, _
    byval x as single, _
    byval y as single _
)
declare sub show_help ( _
    byval dst as long, _
    st as Stage, _
    byval pace as single, _
    byval smooth as integer, _
    m as Meter _
)

canvas& = gfx.display.open(W, H, 32, gfx.display.VSYNC)
plain& = new_panel(0)
lit& = new_panel(-1)
dim st as Stage
dim fps as Meter
dim rates(KEPT - 1) as single
dim sx(RINGS, SIDES - 1) as single
dim sy(RINGS, SIDES - 1) as single
dim sz(RINGS) as single
smooth% = 0
pace! = 1
travel! = 0
was# = gfx.display.clock
fps.since = was#

do
    now# = gfx.display.clock
    dt! = now# - was#
    was# = now#
    count_frame fps, rates(), now#
    travel! = travel! + SPEED * pace! * dt!
    measure canvas&, st
    place_rings st, sx(), sy(), sz(), travel!, travel! * ROLL
    first& = int(travel! / GAP)
    flags% = gfx.draw.PERSPECTIVE + smooth%
    gfx.draw.clear canvas&, 0
    ' the far ring's middle, between two opposite corners
    gx! = (sx(RINGS, 0) + sx(RINGS, SIDES \ 2)) / 2
    gy! = (sy(RINGS, 0) + sy(RINGS, SIDES \ 2)) / 2
    draw_tunnel canvas&, sx(), sy(), sz(), first&, plain&, lit&, _
        flags%
    draw_glow canvas&, st, gx!, gy!
    show_help canvas&, st, pace!, smooth%, fps
    gfx.display.flip
    k$ = ucase$(inkey$)

    if k$ = chr$(ESC) then exit do

    select case k$
        case chr$(0) + "H", "+", "="
            pace! = pace! * 1.25
        case chr$(0) + "P", "-"
            pace! = pace! / 1.25
        case "S"
            smooth% = gfx.draw.SMOOTH - smooth%
    end select
loop

gfx.display.close

' A panel: a plate inset in a frame, a rivet at each
' corner; a lit one has a bright band across it.
function new_panel (byval lit as integer) as long
    s& = gfx.surface.new(SIDE, SIDE)
    last% = SIDE - 1
    in% = last% - 2
    gfx.draw.clear s&, &H1C2230
    gfx.draw.rect s&, 2, 2, in%, in%, &H3A4458, gfx.draw.FILL
    gfx.draw.rect s&, 2, 2, in%, in%, &H5A6884, 0
    gfx.draw.line s&, 2, SIDE \ 2, in%, SIDE \ 2, &H2A3242
    rivet& = &H98A4BC

    for c = 0 to 3
        x% = INSET + (last% - 2 * INSET) * (c and 1)
        y% = INSET + (last% - 2 * INSET) * (c \ 2)
        gfx.draw.circle s&, x%, y%, 2, rivet&, gfx.draw.FILL
    next

    if lit then
        top% = SIDE \ 2 - 5
        bottom% = SIDE \ 2 + 5
        f% = gfx.draw.FILL
        gfx.draw.rect s&, 3, top%, in%, bottom%, &HFF9030, f%
        gfx.draw.rect s&, 3, top% + 3, in%, bottom% - 3, &HFFE0A0, f%
    end if

    new_panel = s&
end function

' The tunnel's middle at depth z, across and down.
function path_x (byval z as single) as single
    path_x = SWING * sin(z * 0.21)
end function

function path_y (byval z as single) as single
    path_y = SWING * 0.7 * sin(z * 0.13 + 1)
end function

' Each ring's corners on the screen and its depth: ring 0
' just past the eye, the tube swung by the path less the
' eye's own place on it, and turned.
sub place_rings ( _
    st as Stage, _
    sx() as single, _
    sy() as single, _
    sz() as single, _
    byval travel as single, _
    byval turn as single _
)
    eye! = LENS * st.unit
    frac! = travel / GAP - int(travel / GAP)
    ex! = path_x(travel)
    ey! = path_y(travel)

    for k = 0 to RINGS
        z! = 1 + (k + 1 - frac!) * GAP
        ox! = path_x(travel + z!) - ex!
        oy! = path_y(travel + z!) - ey!
        sz(k) = z!

        for j = 0 to SIDES - 1
            a! = j * 2 * PI / SIDES + turn
            x! = ox! + RADIUS * cos(a!)
            y! = oy! + RADIUS * sin(a!)
            sx(k, j) = st.cx + eye! * x! / z!
            sy(k, j) = st.cy + eye! * y! / z!
        next
    next
end sub

' The tube's panels from the far ring in, each ring's
' light falling with its depth; every LIT_EVERY ring, by
' its place in the tunnel, lit.
sub draw_tunnel ( _
    byval dst as long, _
    sx() as single, _
    sy() as single, _
    sz() as single, _
    byval first as long, _
    byval plain as long, _
    byval lit as long, _
    byval flags as integer _
)
    dim q(3) as Vertex
    far! = sz(RINGS)

    for k = RINGS - 1 to 0 step -1
        level% = FULL * (far! - sz(k)) / (far! - 1)

        if level% < 0 then level% = 0

        gfx.draw.set_light level%
        src& = plain

        if (first + k) mod LIT_EVERY = 0 then src& = lit

        for j = 0 to SIDES - 1
            n = (j + 1) mod SIDES

            for c = 0 to 3
                ring% = k + (c \ 2)
                at% = j

                if c = 1 or c = 2 then at% = n

                q(c).x = sx(ring%, at%)
                q(c).y = sy(ring%, at%)
                q(c).z = sz(ring%)
                q(c).u = SIDE * ((c + 1) \ 2 mod 2)
                q(c).v = SIDE * (c \ 2)
            next

            gfx.draw.poly dst, q(), 0, 4, src&, 0, flags
        next
    next

    gfx.draw.set_light FULL
end sub

' A light at the far end, added over the tunnel in rings,
' so it hazes the panels round it.
sub draw_glow ( _
    byval dst as long, _
    st as Stage, _
    byval x as single, _
    byval y as single _
)
    warm& = &HFF9040
    f% = gfx.draw.FILL + gfx.draw.ADD
    gfx.draw.set_alpha 1

    for r = 120 to 4 step -4
        rr! = r * st.unit
        gfx.draw.circle dst, x, y, rr!, warm&, f%
    next

    gfx.draw.set_alpha FULL
end sub

' The keys along dst's top, the frame rate at its foot.
sub show_help ( _
    byval dst as long, _
    st as Stage, _
    byval pace as single, _
    byval smooth as integer, _
    m as Meter _
)
    grey& = &HC8C8C8
    z% = st.zoom
    edge% = MARGIN * z%
    t$ = "UP/DN speed" + str$(int(pace * 100) / 100)
    t$ = t$ + "   S smooth " + on_off$(smooth)

    label dst, edge%, edge%, t$, grey&, z%
    foot% = st.h - edge% - st.text_h
    label dst, edge%, foot%, rate_text$(m), grey&, z%
end sub
