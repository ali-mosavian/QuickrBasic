' A cube turning in perspective at 32 bits, a texture on
' each face, nearer faces hiding farther ones by depth.
' Sized by the screen (stage.bas): W and H may be any size.
' P turns perspective off and on, D depth, S smoothing.
' Esc stops it.
import gfx.display, gfx.surface, gfx.draw
from gfx.draw import Vertex
from stage import Stage, measure, label, on_off$
from meter import Meter, KEPT, count_frame, rate_text$

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

type Vec3
    x as single
    y as single
    z as single
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
declare sub turn ( _
    spun() as Vec3, _
    byval angle as single, _
    st as Stage _
)
declare function flags_of (o as Options) as integer
declare sub toggle (o as Options, byval k as string)
declare sub show_options ( _
    byval dst as long, _
    st as Stage, _
    o as Options, _
    m as Meter _
)

canvas& = gfx.display.open(W, H, 32, gfx.display.VSYNC)
back& = gfx.display.rgb(16, 16, 40)
dim textures(FACES - 1) as long

for i = 0 to FACES - 1
    textures(i) = new_face(i)
next

dim spun(CORNERS - 1) as Vec3
' the faces' corners end to end, four a face
dim quads(FACES * 4 - 1) as Vertex
dim corner(FACES * 4 - 1) as integer
dim st as Stage
dim o as Options
dim fps as Meter
dim rates(KEPT - 1) as single
o.perspective = -1
o.depth = -1
angle! = 0

for i = 0 to FACES * 4 - 1
    corner(i) = val(mid$(FACE_CORNERS, i + 1, 1))
    k = i mod 4
    quads(i).u = SIDE * ((k + 1) \ 2 mod 2)
    quads(i).v = SIDE * (k \ 2)
next

was# = gfx.display.clock
fps.since = was#

do
    now# = gfx.display.clock
    dt! = now# - was#
    was# = now#
    angle! = angle! + dt!
    count_frame fps, rates(), now#
    measure canvas&, st
    turn spun(), angle!, st

    for i = 0 to FACES * 4 - 1
        c = corner(i)
        quads(i).x = spun(c).x
        quads(i).y = spun(c).y
        quads(i).z = spun(c).z
    next

    gfx.draw.clear canvas&, back&
    gfx.surface.clear_depth canvas&
    flags% = flags_of(o)

    for i = 0 to FACES - 1
        first = i * 4
        src& = textures(i)
        gfx.draw.poly canvas&, quads(), first, 4, src&, 0, flags%
    next

    show_options canvas&, st, o, fps
    gfx.display.flip
    k$ = ucase$(inkey$)

    if k$ = chr$(ESC) then exit do

    toggle o, k$
loop

gfx.display.close

' Face n's texture: a checkerboard in its own colour, a
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
' it about x, set AWAY from the eye and projected onto st
' about its centre: x and y there, z the depth.
sub turn ( _
    spun() as Vec3, _
    byval angle as single, _
    st as Stage _
)
    cos_y! = cos(angle)
    sin_y! = sin(angle)
    cos_x! = cos(angle * 0.7)
    sin_x! = sin(angle * 0.7)
    eye! = LENS * st.unit

    for i = 0 to CORNERS - 1
        x! = (i and 1) * 2 - 1
        y! = (i \ 2 and 1) * 2 - 1
        z! = (i \ 4 and 1) * 2 - 1
        x1! = x! * cos_y! + z! * sin_y!
        z1! = z! * cos_y! - x! * sin_y!
        y2! = y! * cos_x! - z1! * sin_x!
        z2! = y! * sin_x! + z1! * cos_x! + AWAY
        spun(i).x = st.cx + eye! * x1! / z2!
        spun(i).y = st.cy + eye! * y2! / z2!
        spun(i).z = z2!
    next
end sub

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
    m as Meter _
)
    grey& = gfx.display.rgb(200, 200, 200)
    z% = st.zoom
    edge% = MARGIN * z%
    t$ = "P persp " + on_off$(o.perspective)
    t$ = t$ + "  D depth " + on_off$(o.depth)
    t$ = t$ + "  S smooth " + on_off$(o.smooth)
    foot% = st.h - edge% - st.text_h
    label dst, st.foot, edge%, foot%, t$, grey&, z%
    label dst, st.top, edge%, edge%, rate_text$(m), grey&, z%
end sub
