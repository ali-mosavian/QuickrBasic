' A cube turning in perspective at 32 bits, a picture on
' each face, nearer faces hiding farther ones by depth.
' Sized by the screen (stage.bas): W and H may be any size.
' P turns perspective off and on, D depth, S smoothing.
' Esc stops it.
import gfx.display, gfx.surface, gfx.draw
from gfx.draw import Vertex
from stage import Stage, measure, label

const W = 640
const H = 480
const SIDE = 64
const CELL = 8
const FACES = 6
const CORNERS = 8
const AWAY = 3.2
' the eye's distance from the screen, in units
const LENS = 180
' text's distance from the edges, in its own pixels
const MARGIN = 4
const ESC = 27
' each face's four corners, in order round it
const FACE_CORNERS = "013254674026157345102376"
' seconds a frame rate is counted over
const PERIOD = 0.25
' the counted rates shown as their median
const KEPT = 9

type Vec3
    x as single
    y as single
    z as single
end type

' The frame rate: frames counted over each PERIOD, and the
' median of the last KEPT counted, 0 until one is.
type Meter
    since as double
    frames as long
    taken as long
    shown as single
end type

' What the keys turn off and on.
type Options
    perspective as integer
    depth as integer
    smooth as integer
end type

declare function new_face (byval n as integer) as long
declare function tone ( _
    byval hue as integer, _
    byval least as integer, _
    byval more as integer _
) as long
declare sub turn (spun() as Vec3, byval angle as single)
declare sub draw_face ( _
    byval dst as long, _
    st as Stage, _
    byval n as integer, _
    byval src as long, _
    spun() as Vec3, _
    byval flags as integer _
)
declare sub count_frame ( _
    m as Meter, _
    rates() as single, _
    byval now as double _
)
declare function median (rates() as single, byval n as long) as single
declare function flags_of (o as Options) as integer
declare sub toggle (o as Options, byval k as string)
declare sub show_options ( _
    byval dst as long, _
    st as Stage, _
    o as Options, _
    byval rate as single _
)
declare function on_off$ (byval v as integer)

canvas& = gfx.display.open(W, H, 32, gfx.display.VSYNC)
back& = gfx.display.rgb(16, 16, 40)
dim pictures(FACES - 1) as long

for i = 0 to FACES - 1
    pictures(i) = new_face(i)
next

dim spun(CORNERS - 1) as Vec3
dim st as Stage
dim o as Options
dim meter as Meter
dim rates(KEPT - 1) as single
o.perspective = -1
o.depth = -1
angle! = 0
was# = gfx.display.clock
meter.since = was#

do
    now# = gfx.display.clock
    dt! = now# - was#
    was# = now#
    angle! = angle! + dt!
    count_frame meter, rates(), now#
    measure canvas&, st
    turn spun(), angle!
    gfx.draw.clear canvas&, back&
    gfx.surface.clear_depth canvas&
    flags% = flags_of(o)

    for i = 0 to FACES - 1
        draw_face canvas&, st, i, pictures(i), spun(), flags%
    next

    show_options canvas&, st, o, meter.shown
    gfx.display.flip
    k$ = ucase$(inkey$)

    if k$ = chr$(ESC) then exit do

    toggle o, k$
loop

gfx.display.close

' Face n's picture: a checkerboard in its own colour, a
' border and its number.
function new_face (byval n as integer) as long
    s& = gfx.surface.new(SIDE, SIDE)
    hue% = n + 1
    light& = tone(hue%, 40, 200)
    dark& = tone(hue%, 20, 90)
    white& = gfx.display.rgb(255, 255, 255)
    gfx.draw.clear s&, dark&

    for y = 0 to SIDE - 1 step CELL
        for x = (y \ CELL and 1) * CELL to SIDE - 1 step 2 * CELL
            x2 = x + CELL - 1
            y2 = y + CELL - 1
            gfx.draw.rect s&, x, y, x2, y2, light&, gfx.draw.FILL
        next
    next

    gfx.draw.rect s&, 0, 0, SIDE - 1, SIDE - 1, white&, 0
    centre = SIDE / 2 - 4
    gfx.draw.text s&, centre, centre, ltrim$(str$(hue%)), white&
    new_face = s&
end function

' A colour of hue's bits, red, green and blue: least in
' each, and more in those set.
function tone ( _
    byval hue as integer, _
    byval least as integer, _
    byval more as integer _
) as long
    r% = least + more * (hue and 1)
    g% = least + more * (hue \ 2 and 1)
    b% = least + more * (hue \ 4 and 1)
    tone = gfx.display.rgb(r%, g%, b%)
end function

' The cube's corners turned by angle about y and by 0.7 of
' it about x, then set AWAY from the eye.
sub turn (spun() as Vec3, byval angle as single)
    ay! = angle
    ax! = angle * 0.7

    for i = 0 to CORNERS - 1
        x! = (i and 1) * 2 - 1
        y! = (i \ 2 and 1) * 2 - 1
        z! = (i \ 4 and 1) * 2 - 1
        x1! = x! * cos(ay!) + z! * sin(ay!)
        z1! = z! * cos(ay!) - x! * sin(ay!)
        spun(i).x = x1!
        spun(i).y = y! * cos(ax!) - z1! * sin(ax!)
        spun(i).z = y! * sin(ax!) + z1! * cos(ax!) + AWAY
    next
end sub

' Face n drawn from src, its corners projected onto dst
' about its centre.
sub draw_face ( _
    byval dst as long, _
    st as Stage, _
    byval n as integer, _
    byval src as long, _
    spun() as Vec3, _
    byval flags as integer _
)
    dim q(3) as Vertex
    eye! = LENS * st.unit

    for k = 0 to 3
        c = val(mid$(FACE_CORNERS, n * 4 + k + 1, 1))
        z! = spun(c).z
        q(k).x = st.cx + eye! * spun(c).x / z!
        q(k).y = st.cy + eye! * spun(c).y / z!
        q(k).z = z!
        q(k).u = SIDE * ((k + 1) \ 2 mod 2)
        q(k).v = SIDE * (k \ 2)
    next

    gfx.draw.poly dst, q(), 0, 4, src, 0, flags
end sub

' A frame at now counted. A period over, its rate is kept
' and the kept rates' median shown.
sub count_frame ( _
    m as Meter, _
    rates() as single, _
    byval now as double _
)
    m.frames = m.frames + 1
    span# = now - m.since

    if span# < PERIOD then exit sub

    rates(m.taken mod KEPT) = m.frames / span#
    m.taken = m.taken + 1
    m.since = now
    m.frames = 0
    m.shown = median(rates(), m.taken)
end sub

' The median of the rates kept, n of them counted.
function median (rates() as single, byval n as long) as single
    held% = KEPT

    if n < held% then held% = n

    dim sorted(held% - 1) as single

    for i = 0 to held% - 1
        v! = rates(i)
        j = i

        do while j > 0
            if sorted(j - 1) <= v! then exit do

            sorted(j) = sorted(j - 1)
            j = j - 1
        loop

        sorted(j) = v!
    next

    median = sorted(held% \ 2)
end function

' The draw flags o asks for.
function flags_of (o as Options) as integer
    f% = 0

    if o.perspective then f% = f% + gfx.draw.PERSPECTIVE
    if o.depth then f% = f% + gfx.draw.DEPTH_TEST
    if o.depth then f% = f% + gfx.draw.DEPTH_WRITE
    if o.smooth then f% = f% + gfx.draw.SMOOTH

    flags_of = f%
end function

' The option key k turns off or on.
sub toggle (o as Options, byval k as string)
    select case k
        case "P"
            o.perspective = not o.perspective
        case "D"
            o.depth = not o.depth
        case "S"
            o.smooth = not o.smooth
    end select
end sub

' The options along dst's foot, the frame rate at its top.
sub show_options ( _
    byval dst as long, _
    st as Stage, _
    o as Options, _
    byval rate as single _
)
    grey& = gfx.display.rgb(200, 200, 200)
    z% = st.zoom
    edge% = MARGIN * z%
    t$ = "P persp " + on_off$(o.perspective)
    t$ = t$ + "  D depth " + on_off$(o.depth)
    t$ = t$ + "  S smooth " + on_off$(o.smooth)
    foot% = st.h - edge% - st.text_h
    label dst, edge%, foot%, t$, grey&, z%

    if rate <= 0 then exit sub

    fps& = int(rate + 0.5)
    label dst, edge%, edge%, str$(fps&) + " fps", grey&, z%
end sub

function on_off$ (byval v as integer)
    on_off$ = "off"

    if v then on_off$ = "on"
end function
