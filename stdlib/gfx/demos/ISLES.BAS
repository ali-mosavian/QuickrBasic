' Islands at 32 bits, seen by a camera that drifts, turns
' and zooms: a map of tiles drawn in one call, its water
' moving because every water cell shows one view that
' slides over a strip of waves. Boats sail it, clouds
' blend over it. Sized by the screen (stage.bas). T stops
' the turn, Up and Down zoom, S smooths, Esc stops it.
import gfx.display, gfx.surface, gfx.draw
from stage import Stage, measure, label_shaded, on_off$
from meter import Meter, KEPT, count_frame, rate_text$

const W = 640
const H = 480
' the map: its cells across and down, a cell's side
const COLS = 128
const ROWS = 128
const TILE = 16
' the ground's kinds, low to high
const DEEP = 0
const SHALLOW = 1
const SAND = 2
const GRASS = 3
const FOREST = 4
const ROCK = 5
const SNOW = 6
const KINDS = 7
' the land's pictures, on one sheet
const LANDS = 5
' cells between the land's knots: its islands, its shores
const COARSE_GAP = 8
const FINE_GAP = 4
' pixels the waves move a second
const WAVE_PACE = 10
const FLEET = 14
' a boat's pixels a second
const SAIL = 22
const PUFFS = 10
' a cloud's pixels a second
const WIND = 14
' the camera's zoom: about, and its swing, in units
const ZOOM_MID = 0.7
const ZOOM_SWING = 0.3
const TURN = 0.08
const FULL = 63
const MARGIN = 4
const PI = 3.14159265
const ESC = 27

' A boat: where, and its heading.
type Boat
    x as single
    y as single
    angle as single
end type

' A cloud: where, and its size in pixels.
type Puff
    x as single
    y as single
    size as single
end type

declare function new_waves () as long
declare function new_lands () as long
declare sub dots ( _
    byval s as long, _
    byval x0 as integer, _
    byval every as integer, _
    byval colour as long _
)
declare sub make_ground (ground() as integer)
declare sub scatter (knots() as single, byval n as integer)
declare function noise_at ( _
    knots() as single, _
    byval x as single, _
    byval y as single _
) as single
declare function ease (byval f as single) as single
declare function kind_of (byval high as single) as integer
declare sub lay_map ( _
    map() as long, _
    ground() as integer, _
    pictures() as long _
)
declare function is_water ( _
    ground() as integer, _
    byval x as single, _
    byval y as single _
) as integer
declare sub launch (boats() as Boat, ground() as integer)
declare sub sail ( _
    boats() as Boat, _
    ground() as integer, _
    byval dt as single _
)
declare function new_boat () as long
declare sub draw_boats ( _
    byval dst as long, _
    boats() as Boat, _
    byval hull as long, _
    byval flags as integer _
)
declare sub drift (clouds() as Puff, byval dt as single)
declare sub draw_clouds (byval dst as long, clouds() as Puff)
declare sub puff ( _
    byval dst as long, _
    byval x as single, _
    byval y as single, _
    byval r as single _
)
declare sub show_help ( _
    byval dst as long, _
    st as Stage, _
    byval turning as integer, _
    byval smooth as integer, _
    m as Meter _
)

canvas& = gfx.display.open(W, H, 32, gfx.display.VSYNC)
randomize timer
dim st as Stage
dim fps as Meter
dim rates(KEPT - 1) as single
measure canvas&, st
' the world drawn through a view of the screen, so its
' camera turns the world and not the keys over it
world& = gfx.surface.new_view(canvas&, 0, 0, st.w, st.h)
waves& = new_waves()
sheet& = new_lands()
hull& = new_boat()
dim pictures(KINDS - 1) as long
pictures(DEEP) = gfx.surface.new_view(waves&, 0, 0, TILE, TILE)
pictures(SHALLOW) = gfx.surface.new_view(waves&, 0, TILE, TILE, TILE)
dim cut(LANDS - 1) as long
n% = gfx.surface.new_views(sheet&, TILE, TILE, cut())

for i = 0 to LANDS - 1
    pictures(SAND + i) = cut(i)
next

dim ground(COLS * ROWS - 1) as integer
dim map(COLS * ROWS - 1) as long
make_ground ground()
lay_map map(), ground(), pictures()
dim boats(FLEET - 1) as Boat
launch boats(), ground()
dim clouds(PUFFS - 1) as Puff

for i = 0 to PUFFS - 1
    clouds(i).x = rnd * COLS * TILE
    clouds(i).y = rnd * ROWS * TILE
    clouds(i).size = 30 + rnd * 40
next

turning% = -1
smooth% = 0
zoomed! = 1
angle! = 0
t! = 0
was# = gfx.display.clock
fps.since = was#

do
    now# = gfx.display.clock
    dt! = now# - was#
    was# = now#
    t! = t! + dt!
    count_frame fps, rates(), now#

    if turning% then angle! = angle! + TURN * dt!

    swell% = int(t! * WAVE_PACE) mod TILE
    gfx.surface.set_view pictures(DEEP), swell%, 0
    gfx.surface.set_view pictures(SHALLOW), TILE - 1 - swell%, TILE
    sail boats(), ground(), dt!
    drift clouds(), dt!
    span! = COLS * TILE
    cx! = span! * (0.5 + 0.3 * sin(t! * 0.05))
    cy! = span! * (0.5 + 0.3 * sin(t! * 0.037 + 1))
    swing! = ZOOM_SWING * sin(t! * 0.2)
    zoom! = st.unit * zoomed! * (ZOOM_MID + swing!)
    gfx.surface.set_camera_turned world&, cx!, cy!, angle!, zoom!
    gfx.draw.clear canvas&, 0
    gfx.draw.tiles world&, 0, 0, map(), COLS, TILE, TILE, smooth%
    draw_boats world&, boats(), hull&, smooth%
    draw_clouds world&, clouds()
    show_help canvas&, st, turning%, smooth%, fps
    gfx.display.flip
    k$ = ucase$(inkey$)

    if k$ = chr$(ESC) then exit do

    select case k$
        case "T"
            turning% = not turning%
        case "S"
            smooth% = gfx.draw.SMOOTH - smooth%
        case chr$(0) + "H", "+", "="
            zoomed! = zoomed! * 1.25
        case chr$(0) + "P", "-"
            zoomed! = zoomed! / 1.25
    end select
loop

gfx.display.close

' A strip of waves two tiles across: deep water above,
' shallow below, crests every tile, so a tile's view at
' any place on it joins its neighbours.
function new_waves () as long
    s& = gfx.surface.new(2 * TILE, 2 * TILE)
    f% = gfx.draw.FILL
    right% = 2 * TILE - 1
    gfx.draw.rect s&, 0, 0, right%, TILE - 1, &H1E4E8C, f%
    gfx.draw.rect s&, 0, TILE, right%, 2 * TILE - 1, &H3A86C0, f%

    for x = 0 to 2 * TILE - 1
        a! = 2 * PI * x / TILE
        y1% = 4 + int(1.5 * sin(a!) + 0.5)
        y2% = 11 + int(1.5 * sin(a! + PI) + 0.5)
        gfx.draw.pset s&, x, y1%, &H3A70B0
        gfx.draw.pset s&, x, y2%, &H3A70B0
        gfx.draw.pset s&, x, TILE + y1%, &H8CC8E8
        gfx.draw.pset s&, x, TILE + y2%, &H8CC8E8
    next

    new_waves = s&
end function

' The land's tiles in a row: sand, grass, forest, rock,
' snow, each a flat tone with marks placed by rule.
function new_lands () as long
    s& = gfx.surface.new(LANDS * TILE, TILE)
    f% = gfx.draw.FILL
    last% = TILE - 1
    tones$ = "D8C27C4C9A3C3E8A34807A6CECF0F4"

    for i = 0 to LANDS - 1
        c& = val("&H" + mid$(tones$, i * 6 + 1, 6))
        x0% = i * TILE
        gfx.draw.rect s&, x0%, 0, x0% + last%, last%, c&, f%
    next

    dots s&, 0, 5, &HB8A060
    dots s&, TILE, 7, &H3A7E2E
    dots s&, 4 * TILE, 9, &HB8D0E8
    x0% = 2 * TILE

    for k = 0 to 1
        tx% = x0% + 4 + 8 * k
        ty% = 5 + 6 * k
        gfx.draw.rect s&, tx%, ty% + 2, tx% + 1, ty% + 5, &H5A3A1E, f%
        gfx.draw.circle s&, tx%, ty%, 3, &H1E5A22, f%
    next

    x0% = 3 * TILE
    gfx.draw.line s&, x0% + 2, 4, x0% + 9, 9, &H5E584E
    gfx.draw.line s&, x0% + 9, 9, x0% + 13, 6, &H5E584E
    gfx.draw.line s&, x0% + 4, 13, x0% + 10, 12, &H9A9488
    new_lands = s&
end function

' Marks on the tile at x0: a pixel where a rule of its
' place, squared so they scatter, falls on every so many.
sub dots ( _
    byval s as long, _
    byval x0 as integer, _
    byval every as integer, _
    byval colour as long _
)
    for y = 0 to TILE - 1
        for x = 0 to TILE - 1
            if (x * x * 3 + x * y + y * 5) mod every = 0 then
                gfx.draw.pset s, x0 + x, y, colour
            end if
        next
    next
end sub

' Each cell's kind, by the height of two noises: one
' coarse, for the islands, and one finer, for their shores.
sub make_ground (ground() as integer)
    nc% = COLS \ COARSE_GAP + 1
    nf% = COLS \ FINE_GAP + 1
    dim coarse(nc% - 1, nc% - 1) as single
    dim fine(nf% - 1, nf% - 1) as single
    scatter coarse(), nc%
    scatter fine(), nf%

    for r = 0 to ROWS - 1
        for c = 0 to COLS - 1
            a! = noise_at(coarse(), c / COARSE_GAP, r / COARSE_GAP)
            b! = noise_at(fine(), c / FINE_GAP, r / FINE_GAP)
            ground(r * COLS + c) = kind_of(0.65 * a! + 0.35 * b!)
        next
    next
end sub

' A noise's knots, n by n, each anywhere 0 to 1.
sub scatter (knots() as single, byval n as integer)
    for j = 0 to n - 1
        for i = 0 to n - 1
            knots(i, j) = rnd
        next
    next
end sub

' The noise at x, y, in knots: its four knots round the
' point, eased between.
function noise_at ( _
    knots() as single, _
    byval x as single, _
    byval y as single _
) as single
    i% = int(x)
    j% = int(y)
    fx! = ease(x - i%)
    fy! = ease(y - j%)
    a! = knots(i%, j%)
    b! = knots(i% + 1, j%)
    c! = knots(i%, j% + 1)
    d! = knots(i% + 1, j% + 1)
    top! = a! + (b! - a!) * fx!
    bottom! = c! + (d! - c!) * fx!
    noise_at = top! + (bottom! - top!) * fy!
end function

' f, 0 to 1, eased at both ends.
function ease (byval f as single) as single
    ease = f * f * (3 - 2 * f)
end function

' The kind of ground at height high.
function kind_of (byval high as single) as integer
    kind_of = SNOW

    if high < 0.76 then kind_of = ROCK
    if high < 0.67 then kind_of = FOREST
    if high < 0.6 then kind_of = GRASS
    if high < 0.53 then kind_of = SAND
    if high < 0.5 then kind_of = SHALLOW
    if high < 0.44 then kind_of = DEEP
end function

' Each cell's picture, its kind's.
sub lay_map ( _
    map() as long, _
    ground() as integer, _
    pictures() as long _
)
    for i& = 0 to COLS * ROWS - 1
        map(i&) = pictures(ground(i&))
    next
end sub

' Whether the world's point x, y is on water.
function is_water ( _
    ground() as integer, _
    byval x as single, _
    byval y as single _
) as integer
    is_water = 0
    c% = int(x / TILE)
    r% = int(y / TILE)

    if c% < 0 or r% < 0 or c% >= COLS or r% >= ROWS then exit function

    is_water = ground(r% * COLS + c%) <= SHALLOW
end function

' The boats, each on water somewhere, heading anywhere.
sub launch (boats() as Boat, ground() as integer)
    span! = COLS * TILE

    for i = 0 to FLEET - 1
        do
            boats(i).x = rnd * span!
            boats(i).y = rnd * span!
        loop until is_water(ground(), boats(i).x, boats(i).y)

        boats(i).angle = rnd * 2 * PI
    next
end sub

' Each boat sails on, turning away from land ahead.
sub sail ( _
    boats() as Boat, _
    ground() as integer, _
    byval dt as single _
)
    stride! = SAIL * dt

    for i = 0 to FLEET - 1
        a! = boats(i).angle
        x! = boats(i).x + stride! * cos(a!)
        y! = boats(i).y + stride! * sin(a!)
        ahead_x! = boats(i).x + TILE * cos(a!)
        ahead_y! = boats(i).y + TILE * sin(a!)

        if is_water(ground(), ahead_x!, ahead_y!) then
            boats(i).x = x!
            boats(i).y = y!
        else
            boats(i).angle = a! + PI / 2 + rnd * PI
        end if
    next
end sub

' A boat, facing right: a hull and a sail, its anchor
' its middle.
function new_boat () as long
    s& = gfx.surface.new(14, 10)
    f% = gfx.draw.FILL
    gfx.draw.rect s&, 1, 6, 12, 8, &H6A4020, f%
    gfx.draw.rect s&, 3, 5, 11, 6, &H8A5A30, f%
    gfx.draw.line s&, 6, 0, 6, 5, &H40301C
    gfx.draw.rect s&, 7, 1, 10, 4, &HF4F0E4, f%
    gfx.surface.set_anchor s&, 7, 5
    new_boat = s&
end function

' Each boat turned to its heading.
sub draw_boats ( _
    byval dst as long, _
    boats() as Boat, _
    byval hull as long, _
    byval flags as integer _
)
    for i = 0 to FLEET - 1
        x! = boats(i).x
        y! = boats(i).y
        a! = boats(i).angle
        gfx.draw.sprite_turned dst, x!, y!, a!, 1, hull, flags
    next
end sub

' Each cloud carried by the wind, round the map.
sub drift (clouds() as Puff, byval dt as single)
    span! = COLS * TILE

    for i = 0 to PUFFS - 1
        clouds(i).x = clouds(i).x + WIND * dt

        if clouds(i).x > span! then clouds(i).x = clouds(i).x - span!
    next
end sub

' Each cloud: three puffs of white blended over the land,
' each in rings, thicker to its middle.
sub draw_clouds (byval dst as long, clouds() as Puff)
    gfx.draw.set_alpha 7

    for i = 0 to PUFFS - 1
        x! = clouds(i).x
        y! = clouds(i).y
        r! = clouds(i).size
        puff dst, x!, y!, r!
        puff dst, x! + r!, y! + r! / 4, r! * 0.7
        puff dst, x! - r!, y! + r! / 3, r! * 0.6
    next

    gfx.draw.set_alpha FULL
end sub

' A puff of radius r at x, y: rings of white, smaller on
' each, so its edge is soft.
sub puff ( _
    byval dst as long, _
    byval x as single, _
    byval y as single, _
    byval r as single _
)
    f% = gfx.draw.FILL + gfx.draw.BLEND

    for k = 0 to 3
        gfx.draw.circle dst, x, y, r * (1 - k * 0.2), &HFFFFFF, f%
    next
end sub

' The keys along dst's top, the frame rate at its foot.
sub show_help ( _
    byval dst as long, _
    st as Stage, _
    byval turning as integer, _
    byval smooth as integer, _
    m as Meter _
)
    grey& = &HF0F0F0
    z% = st.zoom
    edge% = MARGIN * z%
    t$ = "T turn " + on_off$(turning)
    t$ = t$ + "  UP/DN zoom  S smooth " + on_off$(smooth)
    label_shaded dst, edge%, edge%, t$, grey&, 0, z%
    foot% = st.h - edge% - st.text_h
    label_shaded dst, edge%, foot%, rate_text$(m), grey&, 0, z%
end sub
