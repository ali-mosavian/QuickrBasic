' A temple courtyard at dusk at 32 bits: columns round a
' pool, a temple at its head, braziers burning, and the
' camera circling it all. Every face is a polygon in
' perspective, textured, lit by a low sun and hidden by
' depth; the shadows are blended over the paving, the
' water over its basin, the flames added. Sized by the
' screen (stage.bas). Space stops the camera, Esc stops it.
import gfx.display, gfx.surface, gfx.draw
from gfx.draw import Vertex
from stage import Stage, measure, label, on_off$
from meter import Meter, KEPT, count_frame, rate_text$

const W = 640
const H = 480
const PI = 3.14159265
const ESC = 27
const FULL = 63
' text's distance from the edges, in its own pixels
const MARGIN = 4
' a texture's side, in texels, and the water's; it repeats
' past its edges
const SIDE = 64
const WATER_SIDE = 32
' the textures; WATER and FLAME have PHASES frames; HALO
' is the sun's
const PAVING = 0
const BLOCKS = 1
const FLUTES = 2
const MOULDING = 3
const FRIEZE = 4
const ROOF = 5
const GRASS = 6
const MOSAIC = 7
const BRONZE = 8
const GLOW = 9
const WATER = 10
const FLAME = 18
const HALO = 26
const TEXTURES = 27
const PHASES = 8
' a face drawn in a colour, not a texture
const INKED = -1
' how a part is drawn: solid, a shadow, water
const SOLID_PART = 0
const SHADOW_PART = 1
const WATER_PART = 2
' a part this frame: unseen, seen wholly past NEAR, or cut
' by it
const UNSEEN = 0
const PAST_NEAR = 1
const CUT = 2
' a box's faces
const FACE_TOP = 1
const FACE_BOTTOM = 2
const FACE_SOUTH = 4
const FACE_NORTH = 8
const FACE_EAST = 16
const FACE_WEST = 32
const FACE_ALL = 63
' each face's corners, clockwise from its top left: a box's
' corners numbered by their x, y and z bits
const BOX_FACES = "237645106754320173152640"
' the most the world holds
const MOST_VERTICES = 4000
const MOST_FACES = 1000
const MOST_PARTS = 100
const MOST_GROUPS = 64
const MOST_FIRES = 8
const MOST_SHAFTS = 40
' a face's corners at most, cut
const MOST_N = 8

' the sun, toward it: low in the south west (x east, y up,
' z south)
const SUN_X = -0.63
const SUN_Y = 0.34
const SUN_Z = 0.7
' light in shade; the tints of shade and of full sun
const SHADE = 22
const COOL = &H8C9CD0
const WARM = &HFFE2BC
' light inside the temple
const INSIDE = 12
' the shadows' colour, their height over the ground, and
' how much of them, the water and the glows shows
const INK = &H141A30
const SHADOW_Y = 0.03
const SHADOW_ALPHA = 30
const WATER_ALPHA = 44
const GLOW_ALPHA = 16
' water's frames and flames' a second
const RIPPLES = 8
const FLICKER = 11

' the camera's circle: its centre's z, radius and swing,
' height and rise, turns a second and where it starts, and
' the point it looks at
const ORBIT_Z = -8
const ORBIT = 46
const SWAY = 6
const LIFT = 9
const RISE = 6
const TURN = 0.1
const START_TURN = -0.6
const LOOK_Y = 5
const LOOK_Z = -10
' the focal length over the screen's height; the nearest
' and farthest drawn
const LENS = 0.9
const NEAR = 1
const FAR = 900

' texels a unit: the paving's and the pool's, the walls'
' and the roof's, the meadow's
const FINE = 16
const COARSE = 8
const SPARSE = 5
' the plaza's half width, the meadow's
const PLAZA = 24
const MEADOW = 84
' the pool: its half width, north and south edges, the
' curb's width and top, the basin's floor, the water
const POOL_X = 8
const POOL_N = -4
const POOL_S = 12
const CURB = 1
const CURB_TOP = 0.5
const BASIN = -2.4
const WATER_Y = -0.2
' a column: its shaft and capital
const SHAFT_R = 0.9
const SHAFT_SIDES = 8
' a side's band of the shaded flutes, in texels across
const BAND = 32
' the sides drawn: those facing the eye's half
const SHOWN = 4
' a shaft's edges: round once, then on past the sides shown
const SHAFT_EDGES = 12
' half a side's turn, its cosine and sine
const HALF_COS = 0.9238795
const HALF_SIN = 0.3826834
const CAP_HALF = 1.3
const CAP_H = 0.8
' the colonnade: its rows' distance out, the columns' gap,
' their height, and the beam on them
const ROW = 21
const GAP = 7
const COLUMN_H = 8.8
const BEAM_H = 1.6
' the temple: its platform's half width, north and south
' edges, and its steps
const TEMPLE_X = 16
const TEMPLE_N = -46
const TEMPLE_S = -26
const STEP_H = 0.6
const STEP_D = 1.2
const STEPS = 3
' its columns' rows and height; its eaves and ridge
const PORCH_X = 11
const PORCH_N = -41
const PORCH_S = -31
const TALL = 10.2
const EAVES = 13.6
const RIDGE = 17.8
' its cella: half width, north and south, its walls'
' thickness, its door's half width and height
const CELLA_X = 7
const CELLA_N = -39
const CELLA_S = -33
const WALL = 0.8
const DOOR_X = 2.2
const DOOR_H = 9.4
' a brazier: its stand's height, its bowl's half width;
' a flame's and a glow's size
const STAND_H = 2.6
const BOWL = 0.8
const FLAME_W = 3.2
const FLAME_H = 3.2
const GLOW_SIZE = 9

' the sky's bands up from the horizon, their colours and
' lower edges in hundredths of the screen; the land
const BANDS = 8
const SKY = "F0A060D87C62B06070884C78603E7440326A2A28581A1C40"
const SKY_EDGES = "0003071218263650"
const LAND = &H3C4430
' the sun: its haze, in rings in from HAZE_R to HAZE_IN
' units, HAZE_STEP apart, each adding HAZE_ALPHA; its disc
const HAZE = &HFF9040
const HAZE_R = 64
const HAZE_IN = 8
const HAZE_STEP = 4
const HAZE_ALPHA = 2
const SUN_DISC = &HFFE6B4
const DISC_R = 7
' the mountains: peaks round, two ranges, their distances,
' colours and foot; a segment is drawn when its start lies
' ahead, the cosine of its angle from ahead over SEEN_COS
const PEAKS = 36
const RANGES = 2
const FAR_RANGE = 700
const NEAR_RANGE = 450
const FAR_HILLS = &H4C3A62
const NEAR_HILLS = &H342A46
const HILLS_FOOT = -60
const SEEN_COS = 0.35

' the stone's tones
const STONE_A = &HC9B48E
const STONE_B = &HBDA683
const STONE_LIT = &HDECBA4
const STONE_DIM = &HA28C68
const JOINT = &H7E6A4E
const ASHLAR = &HD8C8A4
const ASHLAR_LIT = &HEADCBA
const ASHLAR_DIM = &HBAA884
const MARBLE_HIGH = &HF2E6CA
const MARBLE_LIGHT = &HE2D4B2
const MARBLE_MID = &HCBBB98
const MARBLE_DIM = &HAE9C7A
const GROOVE = &H8E7C5E
' the paint on the frieze
const TRIGLYPH = &H4A5470
const TRIGLYPH_DIM = &H343C54
const PAINT_RED = &H9A3A28
' the roof's tiles
const ROOF_DARK = &H6A2A18
const ROOF_TILE = &HB4583A
const ROOF_LIT = &HD27A52
' the meadow: its blades' roots and tips, dry grass; its
' tussocks' height in texels, and the share of the sun a
' tussock's shadow keeps, blades letting some through
const BLADE_ROOT = &H3D5727
const BLADE_TIP = &H6F8A3E
const STRAW = &H7D764A
const TUSSOCK_H = 1
const SHADOW_KEEPS = 0.5
' the basin's tiles and grout
const MOSAIC_A = &H2E7C84
const MOSAIC_B = &H3A96A0
const MOSAIC_C = &H24646E
const GROUT = &H1C4448
' the braziers
const BRONZE_MID = &H7A5A32
const BRONZE_LIGHT = &H9C7A44
const BRONZE_DARK = &H5A4024
' the water: its own colour, its reflectance head on; the
' wind along u and v, the
' waves across the texture at the spectrum's peak and at
' most, the weakest wave kept against the strongest, the
' slopes' spread, and the steps of a spread in which the
' water's shades are kept
const WATER_BODY = &H0C3848
const HEAD_ON = 0.02
const WIND_U = 0.6
const WIND_V = 0.8
const PEAK_WAVES = 2
const MOST_WAVES = 6
const WEAKEST_WAVE = 0.3
const SLOPE_SPREAD = 0.045
const SLOPE_STEPS = 4
' a flame's layers, outside in
const FLAMES = "B0300CE8641AFFB43CFFF2C8"
const FLAME_HALF = 18

type Vec3
    x as single
    y as single
    z as single
end type

' A flat face of a part: its n corners from vertex first,
' clockwise seen from its front; its texture, or INKED; the
' light and tint the sun gives it; its plane, a normal and
' its offset.
type Face
    part as integer
    first as long
    n as integer
    texture as integer
    light as integer
    tint as long
    nx as single
    ny as single
    nz as single
    offset as single
end type

' A part of the world, culled whole: its first vertex and
' its faces, a sphere round it, how it is drawn, and how it
' is seen this frame.
type Part
    first_vertex as long
    first_face as long
    last_face as long
    centre as Vec3
    radius as single
    look as integer
    seen as integer
end type

' A column's shaft: its part, its axis, and its edges from
' vertex first, each's top then its foot.
type Shaft
    part as integer
    x as single
    z as single
    first as long
end type

' Faces drawn together, order(first) to order(last): of one
' look, light and tint.
type Group
    first as long
    last as long
    look as integer
    light as integer
    tint as long
end type

' The world: its faces' corners, and each as seen this
' frame with its texel; its faces, its parts, and the faces
' in groups; its shafts; its textures; its fires. While it
' is built:
' the most light a face is given, and the point its
' textures are anchored at.
type World
    vertices() as Vec3
    seen() as Vertex
    faces() as Face
    parts() as Part
    groups() as Group
    order() as long
    shafts() as Shaft
    textures() as long
    fires() as Vec3
    n_vertices as long
    n_faces as long
    n_parts as long
    n_groups as integer
    n_shafts as integer
    n_fires as integer
    cap as integer
    anchor as Vec3
end type

' A band of the sky: its colour, and its lower edge in
' hundredths of the screen over the horizon.
type Band
    colour as long
    edge as single
end type

' The camera: its eye and its axes in the world; the
' screen's centre; the focal length in pixels; half the
' screen across and down, and each's reach from the eye.
type Camera
    eye as Vec3
    across as Vec3
    up as Vec3
    ahead as Vec3
    cx as single
    cy as single
    focal as single
    half_w as single
    half_h as single
    reach_w as single
    reach_h as single
end type

canvas& = gfx.display.open(W, H, 32, 0)
dim world as World
dim hills(RANGES - 1, PEAKS - 1) as Vec3
dim sky_bands(BANDS - 1) as Band
dim cam as Camera
dim st as Stage
dim fps as Meter
dim rates(KEPT - 1) as single
dim q(MOST_N - 1) as Vertex
dim r(MOST_N - 1) as Vertex
redim world.textures(TEXTURES - 1)

for i = 0 to TEXTURES - 1
    world.textures(i) = new_texture(i)
next

paint_water world.textures()

build world
raise_ranges hills()
make_bands sky_bands()
moving% = -1
travel! = 0
was# = gfx.display.clock
began# = was#
fps.since = was#

do
    now# = gfx.display.clock
    dt! = now# - was#
    was# = now#
    alive! = now# - began#
    count_frame fps, rates(), now#

    if moving% then travel! = travel! + dt!

    measure canvas&, st
    aim cam, st, travel!
    draw_sky canvas&, cam, st, sky_bands()
    gfx.surface.clear_depth canvas&
    draw_sun canvas&, cam, st, world.textures(HALO), q()
    draw_ranges canvas&, cam, hills(), q()
    ripple% = int(alive! * RIPPLES) mod PHASES
    draw_world canvas&, world.groups(), world.n_groups, _
        world.order(), world.faces(), world.parts(), world.n_parts, _
        world.shafts(), world.n_shafts, world.vertices(), _
        world.seen(), world.textures(), cam, ripple%, r()
    draw_fires canvas&, world.fires(), world.n_fires, _
        world.textures(), cam, alive!, q()
    show_help canvas&, st, moving%, fps
    gfx.display.flip
    k$ = inkey$

    if k$ = chr$(ESC) then exit do
    if k$ = " " then moving% = not moving%
loop

gfx.display.close

' Texture which, painted by rule in flat tones.
function new_texture (byval which as integer) as long
    size% = SIDE
    wide% = SIDE

    if which = HALO then size% = 2 * HAZE_R
    if which = HALO then wide% = size%
    if which = FLUTES then wide% = SHAFT_SIDES * BAND

    if which >= WATER and which < WATER + PHASES then
        size% = WATER_SIDE
        wide% = size%
    end if

    s& = gfx.surface.new(wide%, size%)

    select case which
        case PAVING
            paint_paving s&
        case BLOCKS
            paint_blocks s&
        case FLUTES
            paint_flutes s&
        case MOULDING
            paint_moulding s&
        case FRIEZE
            paint_frieze s&
        case ROOF
            paint_roof s&
        case GRASS
            paint_grass s&
        case MOSAIC
            paint_mosaic s&
        case BRONZE
            paint_bronze s&
        case GLOW
            paint_glow s&
        case WATER to WATER + PHASES - 1
            ' painted together, by paint_water
        case HALO
            paint_halo s&
        case else
            paint_flame s&, which - FLAME
    end select

    new_texture = s&
end function

' Four slabs, two tones, each with a lit and a shaded edge
' and a joint on its top and left; a few worn spots.
sub paint_paving (byval s as long)
    dim k as integer
    dim x0 as integer
    dim y0 as integer
    dim x1 as integer
    dim y1 as integer
    dim stone as long
    half% = SIDE \ 2
    f% = gfx.draw.FILL

    for k = 0 to 3
        x0 = (k and 1) * half%
        y0 = (k \ 2) * half%
        x1 = x0 + half% - 1
        y1 = y0 + half% - 1
        stone = STONE_A

        if k = 1 or k = 2 then stone = STONE_B

        gfx.draw.rect s, x0, y0, x1, y1, stone, f%
        gfx.draw.line s, x0 + 1, y0 + 1, x1 - 1, y0 + 1, STONE_LIT
        gfx.draw.line s, x0 + 1, y0 + 2, x0 + 1, y1 - 1, STONE_LIT
        gfx.draw.line s, x0 + 2, y1, x1, y1, STONE_DIM
        gfx.draw.line s, x1, y0 + 2, x1, y1 - 1, STONE_DIM
        gfx.draw.line s, x0, y0, x1, y0, JOINT
        gfx.draw.line s, x0, y0, x0, y1, JOINT
    next

    for k = 0 to 9
        x0 = (k * 23 + 5) mod SIDE
        y0 = (k * 41 + 9) mod SIDE
        gfx.draw.rect s, x0, y0, x0 + 2, y0, STONE_DIM, f%
    next
end sub

' Courses of blocks in running bond, each lit above and
' shaded below.
sub paint_blocks (byval s as long)
    dim r as integer
    dim c as integer
    dim x as integer
    dim y0 as integer
    dim y1 as integer
    gfx.draw.clear s, ASHLAR

    for r = 0 to 3
        y0 = r * 16
        y1 = y0 + 15
        gfx.draw.line s, 0, y0 + 1, SIDE - 1, y0 + 1, ASHLAR_LIT
        gfx.draw.line s, 0, y1, SIDE - 1, y1, ASHLAR_DIM
        gfx.draw.line s, 0, y0, SIDE - 1, y0, JOINT

        for c = 0 to 1
            x = (r and 1) * 16 + c * 32
            back% = (x + SIDE - 1) mod SIDE
            gfx.draw.line s, back%, y0 + 2, back%, y1, ASHLAR_DIM
            gfx.draw.line s, x, y0, x, y1, JOINT
            gfx.draw.line s, x + 1, y0 + 1, x + 1, y1 - 1, ASHLAR_LIT
        next
    next
end sub

' A band for each side of a shaft, of two flutes: a groove,
' a lit edge, a light and a shaded face; each shaded as the
' sun lights its side, side k's the band k from the right.
sub paint_flutes (byval s as long)
    dim x as integer
    dim k as integer
    dim light as integer
    dim c as long
    dim plain as long
    dim a as single
    dim sun as single
    dim q(3) as Vertex
    plain = gfx.surface.new(SIDE, SIDE)

    for x = 0 to SIDE - 1
        select case x mod 32
            case 0, 1
                c = GROOVE
            case 2 to 4
                c = MARBLE_HIGH
            case 5 to 16
                c = MARBLE_LIGHT
            case 17 to 27
                c = MARBLE_MID
            case else
                c = MARBLE_DIM
        end select

        gfx.draw.line plain, x, 0, x, SIDE - 1, c
    next

    ' side k's band, lit as face_end lights it
    for k = 0 to SHAFT_SIDES - 1
        a = (k + 0.5) * 2 * PI / SHAFT_SIDES
        sun = cos(a) * SUN_X + sin(a) * SUN_Z

        if sun < 0 then sun = 0

        light = SHADE + (FULL - SHADE) * sun
        x = (SHAFT_SIDES - 1 - k) * BAND
        q(0).x = x
        q(1).x = x + BAND
        q(1).u = SIDE
        q(2).x = x + BAND
        q(2).y = SIDE
        q(2).u = SIDE
        q(2).v = SIDE
        q(3).x = x
        q(3).y = SIDE
        q(3).v = SIDE
        gfx.draw.set_light light
        gfx.draw.set_tint mixed(COOL, WARM, sun)
        gfx.draw.poly s, q(), 0, 4, plain, 0, 0
    next

    gfx.draw.set_light FULL
    gfx.draw.set_tint &HFFFFFF
    gfx.surface.free plain
end sub

' A capital's bands, top down: the abacus, the echinus
' shaded under, the necking, the shaft's top.
sub paint_moulding (byval s as long)
    dim y as integer
    dim c as long

    for y = 0 to SIDE - 1
        select case y
            case 0
                c = MARBLE_HIGH
            case 1 to 18
                c = ASHLAR
            case 19, 20
                c = GROOVE
            case 21 to 32
                c = MARBLE_LIGHT
            case 33 to 42
                c = MARBLE_MID
            case 43 to 45
                c = MARBLE_DIM
            case 46, 47
                c = GROOVE
            case 48 to 55
                c = MARBLE_LIGHT
            case else
                c = MARBLE_MID
        end select

        gfx.draw.line s, 0, y, SIDE - 1, y, c
    next
end sub

' The entablature over two bays: a cornice, the
' architrave, and between them a painted triglyph centred
' on each bay's start, a framed metope between.
sub paint_frieze (byval s as long)
    dim b as integer
    dim g as integer
    dim x as integer
    f% = gfx.draw.FILL
    last% = SIDE - 1
    gfx.draw.clear s, STONE_A
    gfx.draw.rect s, 0, 0, last%, 9, ASHLAR, f%
    gfx.draw.line s, 0, 0, last%, 0, MARBLE_HIGH
    gfx.draw.line s, 0, 8, last%, 8, PAINT_RED
    gfx.draw.line s, 0, 9, last%, 9, GROOVE
    gfx.draw.rect s, 0, 52, last%, last%, ASHLAR, f%
    gfx.draw.line s, 0, 52, last%, 52, GROOVE
    gfx.draw.line s, 0, 53, last%, 53, ASHLAR_LIT
    gfx.draw.line s, 0, last%, last%, last%, ASHLAR_DIM

    ' the third bay's triglyph wraps round to the first's
    for b = 0 to 2
        x = b * 32 - 6
        gfx.draw.rect s, x, 10, x + 11, 51, TRIGLYPH, f%

        for g = 0 to 1
            gx% = x + 3 + g * 4
            gfx.draw.rect s, gx%, 12, gx% + 1, 51, TRIGLYPH_DIM, f%
        next

        gfx.draw.rect s, x + 14, 13, x + 29, 48, PAINT_RED, 0
    next
end sub

' Rows of rounded tiles, each row's offset by half a tile,
' each tile lit down its left.
sub paint_roof (byval s as long)
    dim r as integer
    dim c as integer
    dim x as integer
    dim y as integer
    f% = gfx.draw.FILL
    gfx.draw.clear s, ROOF_DARK

    for r = 0 to 3
        y = r * 16

        for c = -1 to 4
            x = c * 16 + (r and 1) * 8 + 8
            gfx.draw.rect s, x - 6, y, x + 6, y + 7, ROOF_TILE, f%
            gfx.draw.circle s, x, y + 7, 6, ROOF_TILE, f%
            gfx.draw.line s, x - 4, y + 1, x - 4, y + 9, ROOF_LIT
        next
    next
end sub

' Grass seen from above, a texel holding many blades: a
' field of tussocks from noise at three scales, lit by its
' normals toward the sun and shadowed where it rises past
' the sun's ray, the light averaging 1; coloured root to
' tip by height, with a grain of more or fewer tips a
' texel, yellower where noise says it is dry. Every step
' wraps, so the texture tiles.
sub paint_grass (byval s as long)
    dim size as integer
    size = gfx.surface.width(s)
    dim heights(size - 1, size - 1) as single
    dim dry(size - 1, size - 1) as single
    dim lit(size - 1, size - 1) as single
    dim texels(size - 1, size - 1) as long
    dim k as integer
    dim x as integer
    dim y as integer
    dim mean as single
    dim level as single

    for k = 0 to 2
        add_noise heights(), size, 8 * 2 ^ k, k + 1, _
            TUSSOCK_H * 2 ^ (2 - k) / 7
    next

    for k = 0 to 1
        add_noise dry(), size, 2 * 2 ^ k, k + 4, 2 ^ (1 - k) / 3
    next

    for y = 0 to size - 1
        for x = 0 to size - 1
            lit(x, y) = grass_light(heights(), size, x, y)
            mean = mean + lit(x, y) / size / size
        next
    next

    for y = 0 to size - 1
        for x = 0 to size - 1
            level = 0.55 * heights(x, y) / TUSSOCK_H _
                + 0.45 * lattice(x, y, 6)
            texels(x, y) = scaled(mixed(mixed(BLADE_ROOT, _
                BLADE_TIP, level), STRAW, 0.7 * small(2 _
                * large(dry(x, y) - 0.45, 0), 1)), _
                lit(x, y) / mean)
        next
    next

    draw_texels s, texels(), size
end sub

' The light on grass's texel (x, y) from the field of
' heights: by its normal toward the sun, less where
' shadowed.
function grass_light ( _
    heights() as single, _
    byval size as integer, _
    byval x as integer, _
    byval y as integer _
) as single
    dim nx as single
    dim nz as single
    dim sun as single
    dim climb as single
    dim reach as single
    dim i as integer
    nx = heights(wrapped(x - 1, size), y) _
        - heights(wrapped(x + 1, size), y)
    nz = heights(x, wrapped(y - 1, size)) _
        - heights(x, wrapped(y + 1, size))
    sun = (nx * SUN_X + 2 * SUN_Y + nz * SUN_Z) _
        / sqr(nx * nx + 4 + nz * nz)

    if sun < 0 then sun = 0

    reach = sqr(SUN_X * SUN_X + SUN_Z * SUN_Z)
    climb = SUN_Y / reach

    for i = 1 to TUSSOCK_H / climb
        if heights(wrapped(x + int(i * SUN_X / reach + 0.5), size), _
            wrapped(y + int(i * SUN_Z / reach + 0.5), size)) _
            > heights(x, y) + i * climb then
            sun = sun * SHADOW_KEEPS
            exit for
        end if
    next

    grass_light = SHADE + (FULL - SHADE) * sun
end function

' Adds weight times value noise to n, size texels a side:
' a lattice of period points a side hashed from seed,
' eased between points; it wraps, so the noise tiles.
sub add_noise ( _
    n() as single, _
    byval size as integer, _
    byval period as integer, _
    byval seed as integer, _
    byval weight as single _
)
    dim grid(period - 1, period - 1) as single
    dim i as integer
    dim j as integer
    dim i1 as integer
    dim j1 as integer
    dim x as integer
    dim y as integer
    dim cell as integer
    dim u as single
    dim v as single
    dim top as single
    dim foot as single
    cell = size \ period

    for j = 0 to period - 1
        for i = 0 to period - 1
            grid(i, j) = lattice(i, j, seed)
        next
    next

    for y = 0 to size - 1
        j = y \ cell
        j1 = (j + 1) mod period
        v = eased((y mod cell) / cell)

        for x = 0 to size - 1
            i = x \ cell
            i1 = (i + 1) mod period
            u = eased((x mod cell) / cell)
            top = grid(i, j) + (grid(i1, j) - grid(i, j)) * u
            foot = grid(i, j1) + (grid(i1, j1) - grid(i, j1)) * u
            n(x, y) = n(x, y) + weight * (top + (foot - top) * v)
        next
    next
end sub

' A value from 0 up to 1 for (i, j) and seed, by a hash.
function lattice ( _
    byval i as integer, _
    byval j as integer, _
    byval seed as integer _
) as single
    dim hash as long
    hash = hashed((seed * 4099& + i) and 65535)
    hash = hashed((hash + j * 257&) and 65535)
    lattice = hash / 65536
end function

' A 16-bit hash of n: two steps of a linear congruential
' generator, each folding its high byte into its low.
function hashed (byval n as long) as long
    n = (n * 25173 + 13849) and 65535
    n = n xor (n \ 256)
    n = (n * 25173 + 13849) and 65535
    hashed = n xor (n \ 256)
end function

' t eased in and out, 0 to 1.
function eased (byval t as single) as single
    eased = t * t * (3 - 2 * t)
end function

' The smaller of a and b.
function small (byval a as single, byval b as single) as single
    small = a

    if b < a then small = b
end function

' The larger of a and b.
function large (byval a as single, byval b as single) as single
    large = a

    if b > a then large = b
end function

' A texel's place along a side of size, a power of two,
' wrapped onto it.
function wrapped ( _
    byval k as integer, _
    byval size as integer _
) as integer
    wrapped = k and (size - 1)
end function

' Colour c times k, channel by channel, held at 255.
function scaled (byval c as long, byval k as single) as long
    dim i as integer
    dim unit as long
    dim part as long
    dim total as long
    unit = 1

    for i = 0 to 2
        part = int(((c \ unit) and 255) * k)

        if part > 255 then part = 255

        total = total + part * unit
        unit = unit * 256
    next

    scaled = total
end function

' Texture s from texels, size a side, a row at a time:
' each run of one colour a line.
sub draw_texels ( _
    byval s as long, _
    texels() as long, _
    byval size as integer _
)
    dim x as integer
    dim y as integer
    dim start as integer

    for y = 0 to size - 1
        start = 0

        for x = 1 to size
            if x = size then
                gfx.draw.line s, start, y, x - 1, y, texels(start, y)
            elseif texels(x, y) <> texels(start, y) then
                gfx.draw.line s, start, y, x - 1, y, texels(start, y)
                start = x
            end if
        next
    next
end sub

' Square tiles of three blues in grout.
sub paint_mosaic (byval s as long)
    dim i as integer
    dim j as integer
    dim c as long
    gfx.draw.clear s, GROUT

    for j = 0 to 7
        for i = 0 to 7
            select case (i * 3 + j * 5) mod 4
                case 1
                    c = MOSAIC_B
                case 2
                    c = MOSAIC_C
                case else
                    c = MOSAIC_A
            end select

            x% = i * 8
            y% = j * 8
            gfx.draw.rect s, x%, y%, x% + 6, y% + 6, c, gfx.draw.FILL
        next
    next
end sub

' Bronze in bands, lit over dark.
sub paint_bronze (byval s as long)
    dim y as integer
    dim c as long

    for y = 0 to SIDE - 1
        select case y mod 16
            case 0 to 2
                c = BRONZE_LIGHT
            case 3, 4
                c = BRONZE_DARK
            case else
                c = BRONZE_MID
        end select

        gfx.draw.line s, 0, y, SIDE - 1, y, c
    next
end sub

' Rings of warm light, brighter inward; outside them, keyed.
sub paint_glow (byval s as long)
    dim k as integer
    dim c as long
    gfx.draw.clear s, 0

    for k = 0 to 7
        c = (32 + k * 31) * 65536& + (12 + k * 20) * 256& + 4 + k * 10
        gfx.draw.circle s, 32, 32, 31 - k * 4, c, gfx.draw.FILL
    next
end sub

' The sun's haze: rings of HAZE, each in more by HAZE_ALPHA
' than the last; outside them, keyed.
sub paint_halo (byval s as long)
    dim r as integer
    dim n as integer
    gfx.draw.clear s, 0

    for r = HAZE_R to HAZE_IN step -HAZE_STEP
        n = n + 1
        gfx.draw.circle s, HAZE_R, HAZE_R, r, _
            mixed(0, HAZE, n * HAZE_ALPHA / FULL), gfx.draw.FILL
    next
end sub

' Water's frames in sources() from a wave spectrum, as
' Tessendorf's ocean: Phillips's spectrum, the waves with
' the wind, each a whole number of times across the
' texture, so every frame tiles, and a whole number of
' cycles a loop, longer waves slower, so PHASES frames
' loop. Each texel is the shade its slopes look up, out to
' four spreads each way. PHASES divides WATER_SIDE, a power
' of two.
sub paint_water (sources() as long)
    dim texels(WATER_SIDE - 1, WATER_SIDE - 1) as long
    dim cosine(WATER_SIDE - 1) as single
    dim skies(-1 to 100) as long
    dim shades(-4 * SLOPE_STEPS to 4 * SLOPE_STEPS, _
        -4 * SLOPE_STEPS to 4 * SLOPE_STEPS) as long
    dim across(4 * MOST_WAVES * (MOST_WAVES + 1)) as integer
    dim down(4 * MOST_WAVES * (MOST_WAVES + 1)) as integer
    dim shift(4 * MOST_WAVES * (MOST_WAVES + 1)) as integer
    dim cycles(4 * MOST_WAVES * (MOST_WAVES + 1)) as integer
    dim height(4 * MOST_WAVES * (MOST_WAVES + 1)) as single
    dim at(4 * MOST_WAVES * (MOST_WAVES + 1)) as integer
    dim m as integer
    dim n as integer
    dim k as integer
    dim used as integer
    dim x as integer
    dim y as integer
    dim frame as integer
    dim waves as single
    dim along as single
    dim strongest as single
    dim spread as single
    dim su as single
    dim sv as single

    for k = 0 to WATER_SIDE - 1
        cosine(k) = cos(k * 2 * PI / WATER_SIDE)
    next

    skies(-1) = LAND

    for k = 0 to 100
        skies(k) = sky_at(k)
    next

    for m = -4 * SLOPE_STEPS to 4 * SLOPE_STEPS
        for n = -4 * SLOPE_STEPS to 4 * SLOPE_STEPS
            shades(m, n) = water_shade(m * SLOPE_SPREAD _
                / SLOPE_STEPS, n * SLOPE_SPREAD / SLOPE_STEPS, _
                skies())
        next
    next

    for m = -MOST_WAVES to MOST_WAVES
        for n = -MOST_WAVES to MOST_WAVES
            waves = sqr(m * m + n * n)

            if waves > 0 and waves <= MOST_WAVES then
                along = (m * WIND_U + n * WIND_V) / waves
            else
                along = 0
            end if

            if along > 0 then
                across(used) = m
                down(used) = n
                height(used) = wave_height(m, n, waves, along)
                shift(used) = int(lattice(m, n, 13) * WATER_SIDE)
                cycles(used) = large(1, _
                    int(sqr(waves / PEAK_WAVES) + 0.5))
                strongest = large(strongest, _
                    abs(height(used)) * waves)
                used = used + 1
            end if
        next
    next

    n = used
    used = 0

    for k = 0 to n - 1
        waves = sqr(across(k) ^ 2 + down(k) ^ 2)

        if abs(height(k)) * waves >= WEAKEST_WAVE * strongest then
            across(used) = across(k)
            down(used) = down(k)
            height(used) = height(k)
            shift(used) = shift(k)
            cycles(used) = cycles(k)
            spread = spread + (height(k) * waves) ^ 2 / 4
            used = used + 1
        end if
    next

    ' heights to the normal's leans in steps: a normal leans
    ' against the slope, so the sign
    spread = -SLOPE_STEPS / sqr(spread)

    for k = 0 to used - 1
        height(k) = height(k) * spread
    next

    for frame = 0 to PHASES - 1
        for y = 0 to WATER_SIDE - 1
            for k = 0 to used - 1
                at(k) = down(k) * y + shift(k) _
                    + cycles(k) * frame * (WATER_SIDE \ PHASES)
            next

            for x = 0 to WATER_SIDE - 1
                su = 0
                sv = 0

                for k = 0 to used - 1
                    along = height(k) _
                        * cosine(at(k) and (WATER_SIDE - 1))
                    su = su + across(k) * along
                    sv = sv + down(k) * along
                    at(k) = at(k) + across(k)
                next

                m = small(large(int(su + 0.5), _
                    -4 * SLOPE_STEPS), 4 * SLOPE_STEPS)
                n = small(large(int(sv + 0.5), _
                    -4 * SLOPE_STEPS), 4 * SLOPE_STEPS)
                texels(x, y) = shades(m, n)
            next
        next

        draw_texels sources(WATER + frame), texels(), WATER_SIDE
    next
end sub

' Water's shade where its normal leans su along u and sv
' along v for 1 up, seen as the camera first sees it, the
' sun at its back: the sky reflected by Fresnel over the
' water's own colour; skies() by height on the screen, in
' hundredths, -1 the land.
function water_shade ( _
    byval su as single, _
    byval sv as single, _
    skies() as long _
) as long
    dim size as single
    dim view_x as single
    dim view_y as single
    dim view_z as single
    dim facing as single
    dim fresnel as single
    dim climb as single
    size = sqr(SUN_X * SUN_X + SUN_Z * SUN_Z)
    view_y = LIFT / sqr(LIFT * LIFT + ORBIT * ORBIT)
    view_x = SUN_X / size * sqr(1 - view_y * view_y)
    view_z = SUN_Z / size * sqr(1 - view_y * view_y)
    size = sqr(su * su + 1 + sv * sv)
    facing = (su * view_x + view_y + sv * view_z) / size
    fresnel = HEAD_ON + (1 - HEAD_ON) * (1 - facing) ^ 5
    ' the reflected ray's rise, then its height on the
    ' screen over the horizon
    climb = 2 * facing / size - view_y
    climb = 100 * LENS * climb _
        / sqr(large(1 - climb * climb, 0.0001))
    water_shade = mixed(WATER_BODY, _
        skies(int(small(large(climb, -1), 100))), fresnel)
end function

' A wave's height from Phillips's spectrum: m and n waves
' across the texture along u and v, waves in all, along
' the wind's share; times a normal deviate by hash.
function wave_height ( _
    byval m as integer, _
    byval n as integer, _
    byval waves as single, _
    byval along as single _
) as single
    dim energy as single
    dim deviate as single
    energy = exp(-2 * (PEAK_WAVES / waves) ^ 2) / waves ^ 4 _
        * along * along
    deviate = sqr(-2 * log(1 - lattice(m, n, 11))) _
        * cos(2 * PI * lattice(m, n, 12))
    wave_height = sqr(energy / 2) * deviate
end function

' The sky's colour edge hundredths of the screen over the
' horizon: between the bands it falls between, as a sky
' shades.
function sky_at (byval edge as single) as long
    dim k as integer
    dim low as single
    dim high as single
    high = 100

    for k = BANDS - 1 to 0 step -1
        low = val(mid$(SKY_EDGES, k * 2 + 1, 2))

        if edge >= low or k = 0 then exit for

        high = low
    next

    sky_at = mixed(hex_at&(SKY, k), _
        hex_at&(SKY, small(k + 1, BANDS - 1)), _
        small((edge - low) / (high - low), 1))
end function

' A flame's frame: four layers, each narrower and shorter
' than the last, swaying; the rest keyed.
sub paint_flame (byval s as long, byval frame as integer)
    dim layer as integer
    dim y as integer
    dim k as single
    dim top as single
    dim t as single
    dim hw as single
    dim x as single
    dim c as long
    ph! = frame * 2 * PI / PHASES
    gfx.draw.clear s, 0

    for layer = 0 to 3
        k = (1 - layer * 0.22) * (1 + 0.06 * sin(ph! + layer * 1.7))
        top = SIDE - 1 - (SIDE - 4) * k
        c = hex_at&(FLAMES, layer)

        for y = int(top) + 1 to SIDE - 1
            t = (y - top) / (SIDE - 1 - top)
            hw = FLAME_HALF * k * t ^ 0.6 * sqr(1 - t ^ 3)
            x = SIDE / 2 + 3 * sin(ph! + t * 4) * (1 - t)

            if hw >= 0.5 then gfx.draw.line s, x - hw, y, x + hw, y, c
        next
    next
end sub

' The k-th colour of t, six hex digits each.
function hex_at& (byval t as string, byval k as integer)
    hex_at& = val("&H" + mid$(t, k * 6 + 1, 6) + "&")
end function

function xyz ( _
    byval x as single, _
    byval y as single, _
    byval z as single _
) as Vec3
    dim v as Vec3
    v.x = x
    v.y = y
    v.z = z
    xyz = v
end function

' a when which is 0, else b.
function pick ( _
    byval a as single, _
    byval b as single, _
    byval which as integer _
) as single
    pick = a

    if which then pick = b
end function

' Colour a moved toward b by t, channel by channel.
function mixed ( _
    byval a as long, _
    byval b as long, _
    byval t as single _
) as long
    dim k as integer
    dim unit as long
    dim ca as long
    dim cb as long
    dim c as long
    unit = 1

    for k = 0 to 2
        ca = (a \ unit) and 255
        cb = (b \ unit) and 255
        c = c + int(ca + (cb - ca) * t) * unit
        unit = unit * 256
    next

    mixed = c
end function

' The world, made once.
sub build (wd as World)
    redim wd.vertices(MOST_VERTICES - 1)
    redim wd.seen(MOST_VERTICES - 1)
    redim wd.faces(MOST_FACES - 1)
    redim wd.parts(MOST_PARTS - 1)
    redim wd.shafts(MOST_SHAFTS - 1)
    redim wd.fires(MOST_FIRES - 1)
    wd.cap = FULL
    build_ground wd
    build_pool wd
    build_colonnade wd
    build_steps wd
    build_porch wd
    build_roof wd
    build_cella wd
    build_braziers wd
    build_shadows wd
    group_faces wd
end sub

' The meadow round the plaza, and the plaza's paving round
' the pool.
sub build_ground (wd as World)
    part_begin wd, SOLID_PART
    ring wd, MEADOW, -MEADOW, MEADOW, PLAZA, -PLAZA, PLAZA, _
        GRASS, SPARSE
    part_end wd
    part_begin wd, SOLID_PART
    ring wd, PLAZA, -PLAZA, PLAZA, POOL_X, POOL_N, POOL_S, _
        PAVING, FINE
    part_end wd
end sub

' A floor at height 0 out to out_x east and west and from
' out_n to out_s, round a hole in_x east and west and from
' in_n to in_s: four strips.
sub ring ( _
    wd as World, _
    byval out_x as single, _
    byval out_n as single, _
    byval out_s as single, _
    byval in_x as single, _
    byval in_n as single, _
    byval in_s as single, _
    byval texture as integer, _
    byval scale as single _
)
    dim wide as single
    dim deep as single
    wide = out_x - in_x
    deep = in_s - in_n
    flat wd, -out_x, out_n, 2 * out_x, in_n - out_n, 0, texture, _
        scale
    flat wd, -out_x, in_s, 2 * out_x, out_s - in_s, 0, texture, _
        scale
    flat wd, -out_x, in_n, wide, deep, 0, texture, scale
    flat wd, in_x, in_n, wide, deep, 0, texture, scale
end sub

' The pool: its curb, down to the basin's floor, then the
' water over it.
sub build_pool (wd as World)
    dim x0 as single
    dim x1 as single
    dim lz as single
    dim sides as integer
    x0 = -POOL_X - CURB
    x1 = POOL_X + CURB
    lz = POOL_S - POOL_N
    sides = FACE_ALL - FACE_BOTTOM
    part_begin wd, SOLID_PART
    box wd, x0, BASIN, POOL_N - CURB, x1, CURB_TOP, POOL_N, _
        BLOCKS, BLOCKS, FINE, FINE, sides
    box wd, x0, BASIN, POOL_S, x1, CURB_TOP, POOL_S + CURB, _
        BLOCKS, BLOCKS, FINE, FINE, sides
    box wd, x0, BASIN, POOL_N, -POOL_X, CURB_TOP, POOL_S, _
        BLOCKS, BLOCKS, FINE, FINE, sides
    box wd, POOL_X, BASIN, POOL_N, x1, CURB_TOP, POOL_S, _
        BLOCKS, BLOCKS, FINE, FINE, sides
    flat wd, -POOL_X, POOL_N, 2 * POOL_X, lz, BASIN, MOSAIC, FINE
    part_end wd
    part_begin wd, WATER_PART
    flat wd, -POOL_X, POOL_N, 2 * POOL_X, lz, WATER_Y, WATER, _
        FINE * WATER_SIDE / SIDE
    part_end wd
end sub

' Columns down the east and west and across the south, a
' beam along each row.
sub build_colonnade (wd as World)
    dim k as integer
    dim p as single
    dim reach as single
    dim inner as single

    for k = -3 to 3
        p = k * GAP
        column wd, -ROW, 0, p, COLUMN_H
        column wd, ROW, 0, p, COLUMN_H

        if abs(k) < 3 then column wd, p, 0, ROW, COLUMN_H
    next

    reach = ROW + CAP_HALF
    inner = ROW - CAP_HALF
    part_begin wd, SOLID_PART
    beam wd, -reach, -reach, -inner, reach, COLUMN_H, GAP, 0, 0
    beam wd, inner, -reach, reach, reach, COLUMN_H, GAP, 0, 0
    beam wd, -inner, inner, inner, reach, COLUMN_H, GAP, 0, 0
    part_end wd
end sub

' Three steps round the temple's platform, each a ring of
' blocks from the ground; then the platform's paving, dim
' inside the cella.
sub build_steps (wd as World)
    dim k as integer
    dim xo as single
    dim xi as single
    dim no as single
    dim ni as single
    dim so as single
    dim si as single
    dim y as single
    dim px as single
    dim pn as single
    dim ps as single
    dim deep as single
    part_begin wd, SOLID_PART

    for k = 0 to STEPS - 1
        xo = TEMPLE_X - STEP_D * k
        xi = xo - STEP_D
        no = TEMPLE_N + STEP_D * k
        ni = no + STEP_D
        so = TEMPLE_S - STEP_D * k
        si = so - STEP_D
        y = STEP_H * (k + 1)
        box wd, -xo, 0, si, xo, y, so, BLOCKS, PAVING, FINE, FINE, _
            FACE_TOP + FACE_SOUTH + FACE_EAST + FACE_WEST
        box wd, -xo, 0, no, xo, y, ni, BLOCKS, PAVING, FINE, FINE, _
            FACE_TOP + FACE_NORTH + FACE_EAST + FACE_WEST
        box wd, -xo, 0, ni, -xi, y, si, BLOCKS, PAVING, FINE, FINE, _
            FACE_TOP + FACE_WEST
        box wd, xi, 0, ni, xo, y, si, BLOCKS, PAVING, FINE, FINE, _
            FACE_TOP + FACE_EAST
    next

    y = STEP_H * STEPS
    px = TEMPLE_X - STEP_D * STEPS
    pn = TEMPLE_N + STEP_D * STEPS
    ps = TEMPLE_S - STEP_D * STEPS
    deep = CELLA_S - CELLA_N
    flat wd, -px, CELLA_S, 2 * px, ps - CELLA_S, y, PAVING, FINE
    flat wd, -px, pn, 2 * px, CELLA_N - pn, y, PAVING, FINE
    flat wd, -px, CELLA_N, px - CELLA_X, deep, y, PAVING, FINE
    flat wd, CELLA_X, CELLA_N, px - CELLA_X, deep, y, PAVING, FINE
    wd.cap = INSIDE
    flat wd, -CELLA_X, CELLA_N, 2 * CELLA_X, deep, y, PAVING, FINE
    wd.cap = FULL
    part_end wd
end sub

' Six columns across the temple's front and back, two more
' down each side, and a beam round them.
sub build_porch (wd as World)
    dim k as integer
    dim x as single
    dim z as single
    dim y as single
    dim e as single
    dim i as single
    dim bay as single
    dim side_bay as single
    y = STEP_H * STEPS
    bay = 2 * PORCH_X / 5
    side_bay = (PORCH_S - PORCH_N) / 3

    for k = 0 to 5
        x = -PORCH_X + k * bay
        column wd, x, y, PORCH_S, TALL
        column wd, x, y, PORCH_N, TALL
    next

    for k = 1 to 2
        z = PORCH_S - k * side_bay
        column wd, -PORCH_X, y, z, TALL
        column wd, PORCH_X, y, z, TALL
    next

    y = y + TALL
    e = PORCH_X + CAP_HALF
    i = PORCH_X - CAP_HALF
    part_begin wd, SOLID_PART
    beam wd, -e, PORCH_S - CAP_HALF, e, PORCH_S + CAP_HALF, y, bay, _
        -PORCH_X, PORCH_S
    beam wd, -e, PORCH_N - CAP_HALF, e, PORCH_N + CAP_HALF, y, bay, _
        -PORCH_X, PORCH_S
    beam wd, -e, PORCH_N + CAP_HALF, -i, PORCH_S - CAP_HALF, y, _
        side_bay, -PORCH_X, PORCH_S
    beam wd, i, PORCH_N + CAP_HALF, e, PORCH_S - CAP_HALF, y, _
        side_bay, -PORCH_X, PORCH_S
    part_end wd
end sub

' The ceiling over the porch, a pediment at each end, and
' the roof's two slopes, overhanging.
sub build_roof (wd as World)
    dim o as Vec3
    dim du as Vec3
    dim dv as Vec3
    dim a as Vec3
    dim b as Vec3
    dim c as Vec3
    dim e as single
    dim zs as single
    dim zn as single
    dim ex as single
    dim ey as single
    dim slope as single
    dim deep as single
    e = PORCH_X + CAP_HALF
    zs = PORCH_S + CAP_HALF
    zn = PORCH_N - CAP_HALF
    part_begin wd, SOLID_PART
    o = xyz(-e, EAVES, zs)
    du = xyz(1, 0, 0)
    dv = xyz(0, 0, -1)
    sheet wd, o, du, dv, 2 * e, zs - zn, BLOCKS, COARSE
    a = xyz(0, RIDGE - 0.2, zs)
    b = xyz(e, EAVES, zs)
    c = xyz(-e, EAVES, zs)
    triangle wd, a, b, c, BLOCKS
    a.z = zn
    b = xyz(-e, EAVES, zn)
    c = xyz(e, EAVES, zn)
    triangle wd, a, b, c, BLOCKS
    ex = e + 0.8
    ey = EAVES - 0.25
    slope = sqr(ex * ex + (RIDGE - ey) * (RIDGE - ey))
    deep = zs - zn + 1.2
    o = xyz(0, RIDGE, zs + 0.6)
    du = xyz(0, 0, -1)
    dv = xyz(ex / slope, (ey - RIDGE) / slope, 0)
    sheet wd, o, du, dv, deep, slope, ROOF, COARSE
    o = xyz(0, RIDGE, zn - 0.6)
    du = xyz(0, 0, 1)
    dv = xyz(-ex / slope, (ey - RIDGE) / slope, 0)
    sheet wd, o, du, dv, deep, slope, ROOF, COARSE
    part_end wd
end sub

' The cella's walls up to the ceiling, a door in the front:
' their outsides lit by the sun, then their insides dim.
sub build_cella (wd as World)
    dim y as single
    dim n1 as single
    dim s0 as single
    dim k as integer
    dim sides as integer
    y = STEP_H * STEPS
    n1 = CELLA_N + WALL
    s0 = CELLA_S - WALL
    part_begin wd, SOLID_PART

    for k = 0 to 1
        sides = FACE_NORTH + FACE_EAST + FACE_WEST

        if k then sides = FACE_SOUTH

        box wd, -CELLA_X, y, CELLA_N, CELLA_X, EAVES, n1, _
            BLOCKS, BLOCKS, COARSE, COARSE, sides
        sides = FACE_WEST

        if k then sides = FACE_EAST

        box wd, -CELLA_X, y, n1, -CELLA_X + WALL, EAVES, s0, _
            BLOCKS, BLOCKS, COARSE, COARSE, sides
        sides = FACE_EAST

        if k then sides = FACE_WEST

        box wd, CELLA_X - WALL, y, n1, CELLA_X, EAVES, s0, _
            BLOCKS, BLOCKS, COARSE, COARSE, sides
        sides = FACE_SOUTH + FACE_WEST + FACE_EAST

        if k then sides = FACE_NORTH

        box wd, -CELLA_X, y, s0, -DOOR_X, EAVES, CELLA_S, _
            BLOCKS, BLOCKS, COARSE, COARSE, sides
        box wd, DOOR_X, y, s0, CELLA_X, EAVES, CELLA_S, _
            BLOCKS, BLOCKS, COARSE, COARSE, sides
        sides = FACE_SOUTH + FACE_BOTTOM

        if k then sides = FACE_NORTH

        box wd, -DOOR_X, y + DOOR_H, s0, DOOR_X, EAVES, CELLA_S, _
            BLOCKS, BLOCKS, COARSE, COARSE, sides
        wd.cap = INSIDE
    next

    wd.cap = FULL
    part_end wd
end sub

' A brazier at each corner of the pool, and one inside the
' cella.
sub build_braziers (wd as World)
    brazier wd, -10, 0, POOL_N - 2
    brazier wd, 10, 0, POOL_N - 2
    brazier wd, -10, 0, POOL_S + 4
    brazier wd, 10, 0, POOL_S + 4
    wd.cap = INSIDE
    brazier wd, 0, STEP_H * STEPS, (CELLA_N + CELLA_S) / 2
    wd.cap = FULL
end sub

' The colonnade's shadows on the ground: each column's, and
' each beam's.
sub build_shadows (wd as World)
    dim k as integer
    dim p as single
    dim reach as single
    dim inner as single
    dim y as single
    part_begin wd, SHADOW_PART

    for k = -3 to 3
        p = k * GAP
        column_shadow wd, -ROW, p
        column_shadow wd, ROW, p

        if abs(k) < 3 then column_shadow wd, p, ROW
    next

    reach = ROW + CAP_HALF
    inner = ROW - CAP_HALF
    y = COLUMN_H + BEAM_H / 2
    shadow_of wd, -reach, -reach, -inner, reach, y
    shadow_of wd, inner, -reach, reach, reach, y
    shadow_of wd, -inner, inner, inner, reach, y
    part_end wd
end sub

' Starts a part drawn as look.
sub part_begin (wd as World, byval look as integer)
    dim p as long
    p = wd.n_parts
    wd.parts(p).look = look
    wd.parts(p).first_vertex = wd.n_vertices
    wd.parts(p).first_face = wd.n_faces
end sub

' Ends the part begun last: a sphere round its vertices.
sub part_end (wd as World)
    dim p as long
    dim i as long
    dim lo as Vec3
    dim hi as Vec3
    dim v as Vec3
    p = wd.n_parts
    lo = wd.vertices(wd.parts(p).first_vertex)
    hi = lo

    for i = wd.parts(p).first_vertex to wd.n_vertices - 1
        v = wd.vertices(i)

        if v.x < lo.x then lo.x = v.x
        if v.y < lo.y then lo.y = v.y
        if v.z < lo.z then lo.z = v.z
        if v.x > hi.x then hi.x = v.x
        if v.y > hi.y then hi.y = v.y
        if v.z > hi.z then hi.z = v.z
    next

    wd.parts(p).centre = xyz((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, _
        (lo.z + hi.z) / 2)
    dx! = hi.x - lo.x
    dy! = hi.y - lo.y
    dz! = hi.z - lo.z
    wd.parts(p).radius = sqr(dx! * dx! + dy! * dy! + dz! * dz!) / 2
    wd.parts(p).last_face = wd.n_faces - 1
    wd.n_parts = p + 1
end sub

' Starts a face of texture.
sub face_begin (wd as World, byval texture as integer)
    dim f as long
    f = wd.n_faces
    wd.faces(f).first = wd.n_vertices
    wd.faces(f).n = 0
    wd.faces(f).texture = texture
end sub

' The face begun last's next corner: at a, texel u, v.
sub face_corner ( _
    wd as World, _
    a as Vec3, _
    byval u as single, _
    byval v as single _
)
    dim i as long
    dim f as long
    i = wd.n_vertices
    f = wd.n_faces
    wd.vertices(i) = a
    wd.seen(i).u = u
    wd.seen(i).v = v
    wd.n_vertices = i + 1
    wd.faces(f).n = wd.faces(f).n + 1
end sub

' The texels at the corners of the face begun last, from
' the anchor: along its top edge, su a unit, and down its
' left edge, sv a unit. Faces in one plane meet seamlessly.
sub anchor_texels ( _
    wd as World, _
    byval su as single, _
    byval sv as single _
)
    dim f as long
    dim k as long
    dim c as integer
    dim a as Vec3
    dim b as Vec3
    dim d as Vec3
    dim size as single
    f = wd.n_faces
    k = wd.faces(f).first
    a = wd.vertices(k)
    b = wd.vertices(k + 1)
    d = wd.vertices(k + wd.faces(f).n - 1)
    ux! = b.x - a.x
    uy! = b.y - a.y
    uz! = b.z - a.z
    size = sqr(ux! * ux! + uy! * uy! + uz! * uz!) / su
    ux! = ux! / size
    uy! = uy! / size
    uz! = uz! / size
    vx! = d.x - a.x
    vy! = d.y - a.y
    vz! = d.z - a.z
    size = sqr(vx! * vx! + vy! * vy! + vz! * vz!) / sv
    vx! = vx! / size
    vy! = vy! / size
    vz! = vz! / size

    for c = 0 to wd.faces(f).n - 1
        a = wd.vertices(k + c)
        px! = a.x - wd.anchor.x
        py! = a.y - wd.anchor.y
        pz! = a.z - wd.anchor.z
        wd.seen(k + c).u = px! * ux! + py! * uy! + pz! * uz!
        wd.seen(k + c).v = px! * vx! + py! * vy! + pz! * vz!
    next
end sub

' Ends the face begun last: its plane from its corners, and
' the sun's light and tint on it, held under the cap.
sub face_end (wd as World)
    dim f as long
    dim k as long
    dim a as Vec3
    dim b as Vec3
    dim c as Vec3
    dim nx as single
    dim ny as single
    dim nz as single
    dim size as single
    dim sun as single
    dim light as single
    f = wd.n_faces
    k = wd.faces(f).first
    a = wd.vertices(k)
    b = wd.vertices(k + 1)
    c = wd.vertices(k + wd.faces(f).n - 1)
    ' down the face crossed with across it: out of its front
    vx! = c.x - a.x
    vy! = c.y - a.y
    vz! = c.z - a.z
    ux! = b.x - a.x
    uy! = b.y - a.y
    uz! = b.z - a.z
    nx = vy! * uz! - vz! * uy!
    ny = vz! * ux! - vx! * uz!
    nz = vx! * uy! - vy! * ux!
    size = sqr(nx * nx + ny * ny + nz * nz)
    nx = nx / size
    ny = ny / size
    nz = nz / size
    wd.faces(f).nx = nx
    wd.faces(f).ny = ny
    wd.faces(f).nz = nz
    wd.faces(f).offset = nx * a.x + ny * a.y + nz * a.z
    sun = nx * SUN_X + ny * SUN_Y + nz * SUN_Z

    if sun < 0 then sun = 0

    light = SHADE + (FULL - SHADE) * sun

    if light > wd.cap then light = wd.cap

    wd.faces(f).part = wd.n_parts
    wd.faces(f).light = light
    wd.faces(f).tint = mixed(COOL, WARM, sun)

    if wd.faces(f).texture = INKED or wd.faces(f).texture = WATER _
    then
        wd.faces(f).light = FULL
        wd.faces(f).tint = &HFFFFFF
    end if

    wd.n_faces = f + 1
end sub

' The faces in groups of one look, light and tint, in
' order(): the solid groups first, then the shadows, then
' the water.
sub group_faces (wd as World)
    dim look as integer
    dim f as long
    dim g as integer
    dim p as integer
    dim start as long
    dim grouped(MOST_FACES - 1) as integer
    dim counts(MOST_GROUPS - 1) as long
    redim wd.groups(MOST_GROUPS - 1)
    redim wd.order(wd.n_faces - 1)

    for look = SOLID_PART to WATER_PART
        for f = 0 to wd.n_faces - 1
            p = wd.faces(f).part

            if wd.parts(p).look <> look then continue for

            g = group_of(wd, look, f)
            grouped(f) = g
            counts(g) = counts(g) + 1
        next
    next

    for g = 0 to wd.n_groups - 1
        wd.groups(g).first = start
        wd.groups(g).last = start - 1
        start = start + counts(g)
    next

    for f = 0 to wd.n_faces - 1
        g = grouped(f)
        wd.groups(g).last = wd.groups(g).last + 1
        wd.order(wd.groups(g).last) = f
    next
end sub

' The group of look with face f's light and tint: made, if
' there is none.
function group_of ( _
    wd as World, _
    byval look as integer, _
    byval f as long _
) as integer
    dim g as integer

    for g = 0 to wd.n_groups - 1
        if wd.groups(g).look <> look then continue for
        if wd.groups(g).light <> wd.faces(f).light then continue for
        if wd.groups(g).tint <> wd.faces(f).tint then continue for

        group_of = g
        exit function
    next

    g = wd.n_groups
    wd.groups(g).look = look
    wd.groups(g).light = wd.faces(f).light
    wd.groups(g).tint = wd.faces(f).tint
    wd.n_groups = g + 1
    group_of = g
end function

' A flat face from corner o, across du for lu and down dv
' for lv, clockwise seen from its front, its texture
' anchored at scale texels a unit.
sub sheet ( _
    wd as World, _
    o as Vec3, _
    du as Vec3, _
    dv as Vec3, _
    byval lu as single, _
    byval lv as single, _
    byval texture as integer, _
    byval scale as single _
)
    dim c as integer
    dim s as single
    dim t as single
    dim a as Vec3
    face_begin wd, texture

    for c = 0 to 3
        s = lu * ((c + 1) \ 2 mod 2)
        t = lv * (c \ 2)
        a.x = o.x + du.x * s + dv.x * t
        a.y = o.y + du.y * s + dv.y * t
        a.z = o.z + du.z * s + dv.z * t
        face_corner wd, a, 0, 0
    next

    anchor_texels wd, scale, scale
    face_end wd
end sub

' A floor facing up at height y, from (x, z) east lx and
' south lz.
sub flat ( _
    wd as World, _
    byval x as single, _
    byval z as single, _
    byval lx as single, _
    byval lz as single, _
    byval y as single, _
    byval texture as integer, _
    byval scale as single _
)
    dim o as Vec3
    dim du as Vec3
    dim dv as Vec3
    o = xyz(x, y, z)
    du = xyz(1, 0, 0)
    dv = xyz(0, 0, 1)
    sheet wd, o, du, dv, lx, lz, texture, scale
end sub

' A box from (x0, y0, z0) to (x1, y1, z1), the faces in
' sides: its top and bottom of cap_texture, the rest of
' texture, anchored at across texels a unit along x and z
' and upward up y; across 0, a whole texture on each face.
sub box ( _
    wd as World, _
    byval x0 as single, _
    byval y0 as single, _
    byval z0 as single, _
    byval x1 as single, _
    byval y1 as single, _
    byval z1 as single, _
    byval texture as integer, _
    byval cap_texture as integer, _
    byval across as single, _
    byval upward as single, _
    byval sides as integer _
)
    dim at(7) as Vec3
    dim i as integer
    dim k as integer
    dim bit as integer

    for i = 0 to 7
        at(i) = xyz(pick(x0, x1, i and 1), pick(y0, y1, i and 2), _
            pick(z0, z1, i and 4))
    next

    bit = 1

    for k = 0 to 5
        if sides and bit then
            if k < 2 then
                box_face wd, at(), k, cap_texture, across, across
            else
                box_face wd, at(), k, texture, across, upward
            end if
        end if

        bit = bit * 2
    next
end sub

' Face k of the box with corners at, anchored at su and sv
' texels a unit; su 0, a whole texture.
sub box_face ( _
    wd as World, _
    at() as Vec3, _
    byval k as integer, _
    byval texture as integer, _
    byval su as single, _
    byval sv as single _
)
    dim c as integer
    dim i as integer
    face_begin wd, texture

    for c = 0 to 3
        i = val(mid$(BOX_FACES, k * 4 + c + 1, 1))
        face_corner wd, at(i), SIDE * ((c + 1) \ 2 mod 2), _
            SIDE * (c \ 2)
    next

    if su > 0 then anchor_texels wd, su, sv

    face_end wd
end sub

' A Doric column standing at (x, y, z), height tall: its
' fluted shaft and its capital under a beam, a part of its
' own.
sub column ( _
    wd as World, _
    byval x as single, _
    byval y as single, _
    byval z as single, _
    byval height as single _
)
    dim top as single
    top = y + height - CAP_H
    part_begin wd, SOLID_PART
    shaft wd, x, z, y, top
    box wd, x - CAP_HALF, top, z - CAP_HALF, x + CAP_HALF, _
        y + height, z + CAP_HALF, MOULDING, BLOCKS, 0, 0, _
        FACE_ALL - FACE_TOP
    part_end wd
end sub

' A shaft of SHAFT_SIDES flat sides round (x, z), from y0
' up to y1: its edges round from east, each's top then its
' foot. Seen from outside, a side's band runs from the next
' edge round to its own.
sub shaft ( _
    wd as World, _
    byval x as single, _
    byval z as single, _
    byval y0 as single, _
    byval y1 as single _
)
    dim k as integer
    dim i as long
    dim a as single
    dim u as single
    dim edge as Vec3
    i = wd.n_vertices
    wd.shafts(wd.n_shafts).part = wd.n_parts
    wd.shafts(wd.n_shafts).x = x
    wd.shafts(wd.n_shafts).z = z
    wd.shafts(wd.n_shafts).first = i
    wd.n_shafts = wd.n_shafts + 1

    for k = 0 to SHAFT_EDGES - 1
        a = k * 2 * PI / SHAFT_SIDES
        u = (2 * SHAFT_SIDES - k) * BAND
        edge = xyz(x + SHAFT_R * cos(a), y1, z + SHAFT_R * sin(a))
        wd.vertices(i) = edge
        wd.seen(i).u = u
        wd.seen(i).v = 0
        edge.y = y0
        wd.vertices(i + 1) = edge
        wd.seen(i + 1).u = u
        wd.seen(i + 1).v = SIDE
        i = i + 2
    next

    wd.n_vertices = i
end sub

' A beam BEAM_H high on the columns at y, over the ground
' from (x0, z0) to (x1, z1): its frieze two bays a texture,
' a triglyph over the column at (ax, az) and every bay on.
sub beam ( _
    wd as World, _
    byval x0 as single, _
    byval z0 as single, _
    byval x1 as single, _
    byval z1 as single, _
    byval y as single, _
    byval bay as single, _
    byval ax as single, _
    byval az as single _
)
    dim top as single
    top = y + BEAM_H
    wd.anchor = xyz(ax, top, az)
    box wd, x0, y, z0, x1, top, z1, FRIEZE, BLOCKS, _
        SIDE / (2 * bay), SIDE / BEAM_H, FACE_ALL
    wd.anchor = xyz(0, 0, 0)
end sub

' A triangle, its apex a then b and c clockwise seen from
' its front, facing north or south, its texture's texels
' COARSE a unit.
sub triangle ( _
    wd as World, _
    a as Vec3, _
    b as Vec3, _
    c as Vec3, _
    byval texture as integer _
)
    face_begin wd, texture
    face_corner wd, a, a.x * COARSE, -a.y * COARSE
    face_corner wd, b, b.x * COARSE, -b.y * COARSE
    face_corner wd, c, c.x * COARSE, -c.y * COARSE
    face_end wd
end sub

' A brazier standing at (x, y, z): a stand, a bowl, and a
' fire on it.
sub brazier ( _
    wd as World, _
    byval x as single, _
    byval y as single, _
    byval z as single _
)
    dim top as single
    top = y + STAND_H
    part_begin wd, SOLID_PART
    box wd, x - 0.25, y, z - 0.25, x + 0.25, top, z + 0.25, _
        BRONZE, BRONZE, 0, 0, FACE_ALL - FACE_BOTTOM
    box wd, x - BOWL, top, z - BOWL, x + BOWL, top + 0.6, _
        z + BOWL, BRONZE, BRONZE, 0, 0, FACE_ALL
    part_end wd
    wd.fires(wd.n_fires) = xyz(x, top + 0.6, z)
    wd.n_fires = wd.n_fires + 1
end sub

' The shadow on the ground of a beam over the ground from
' (x0, z0) to (x1, z1), at height.
sub shadow_of ( _
    wd as World, _
    byval x0 as single, _
    byval z0 as single, _
    byval x1 as single, _
    byval z1 as single, _
    byval height as single _
)
    dim xs(3) as single
    dim zs(3) as single
    dim ox as single
    dim oz as single
    ox = -SUN_X / SUN_Y * height
    oz = -SUN_Z / SUN_Y * height
    xs(0) = x0 + ox
    zs(0) = z0 + oz
    xs(1) = x1 + ox
    zs(1) = z0 + oz
    xs(2) = x1 + ox
    zs(2) = z1 + oz
    xs(3) = x0 + ox
    zs(3) = z1 + oz
    shadow_quad wd, xs(), zs()
end sub

' A column's shadow, from its foot at (x, z) away from the
' sun, as wide as its shaft.
sub column_shadow (wd as World, byval x as single, byval z as single)
    dim xs(3) as single
    dim zs(3) as single
    dim ox as single
    dim oz as single
    dim size as single
    dim px as single
    dim pz as single
    ox = -SUN_X / SUN_Y * COLUMN_H
    oz = -SUN_Z / SUN_Y * COLUMN_H
    size = sqr(ox * ox + oz * oz)
    px = -oz / size * SHAFT_R
    pz = ox / size * SHAFT_R
    xs(0) = x + px
    zs(0) = z + pz
    xs(1) = x - px
    zs(1) = z - pz
    xs(2) = x - px + ox
    zs(2) = z - pz + oz
    xs(3) = x + px + ox
    zs(3) = z + pz + oz
    shadow_quad wd, xs(), zs()
end sub

' A shadow over the ground through four points, turned to
' face up.
sub shadow_quad (wd as World, xs() as single, zs() as single)
    dim c as integer
    dim i as integer
    dim order as string

    ' clockwise seen from above, with x across and z down
    up! = (zs(3) - zs(0)) * (xs(1) - xs(0))
    up! = up! - (xs(3) - xs(0)) * (zs(1) - zs(0))
    order = "0123"

    if up! < 0 then order = "0321"

    face_begin wd, INKED

    for c = 0 to 3
        i = val(mid$(order, c + 1, 1))
        face_corner wd, xyz(xs(i), SHADOW_Y, zs(i)), 0, 0
    next

    face_end wd
end sub

' The mountains' peaks round the plain, far then near: each
' at its height.
sub raise_ranges (hills() as Vec3)
    dim k as integer
    dim a as single
    dim peak as single

    for k = 0 to PEAKS - 1
        a = k * 2 * PI / PEAKS
        peak = 70 + 45 * sin(3 * a + 0.5) + 25 * sin(7 * a + 2)
        peak = peak + 12 * sin(17 * a)
        hills(0, k) = xyz(FAR_RANGE * cos(a), peak, _
            ORBIT_Z + FAR_RANGE * sin(a))
        peak = 25 + 20 * sin(5 * a + 1) + 10 * sin(11 * a + 3)
        peak = peak + 5 * sin(23 * a)
        hills(1, k) = xyz(NEAR_RANGE * cos(a), peak, _
            ORBIT_Z + NEAR_RANGE * sin(a))
    next
end sub

' The sky's bands, read once.
sub make_bands (sky_bands() as Band)
    dim k as integer

    for k = 0 to BANDS - 1
        sky_bands(k).colour = hex_at&(SKY, k)
        sky_bands(k).edge = val(mid$(SKY_EDGES, k * 2 + 1, 2))
    next
end sub

' The camera t seconds round its circle, looking in.
sub aim (cam as Camera, st as Stage, byval t as single)
    dim a as single
    dim r as single
    dim fx as single
    dim fy as single
    dim fz as single
    dim size as single
    a = START_TURN + TURN * t
    r = ORBIT + SWAY * sin(t * 0.21)
    cam.eye.x = r * sin(a)
    cam.eye.y = LIFT + RISE * sin(t * 0.17 + 1)
    cam.eye.z = ORBIT_Z + r * cos(a)
    fx = -cam.eye.x
    fy = LOOK_Y - cam.eye.y
    fz = LOOK_Z - cam.eye.z
    size = sqr(fx * fx + fy * fy + fz * fz)
    cam.ahead.x = fx / size
    cam.ahead.y = fy / size
    cam.ahead.z = fz / size
    ' across: ahead crossed with the world's up, level
    size = sqr(fx * fx + fz * fz)
    cam.across.x = -fz / size
    cam.across.y = 0
    cam.across.z = fx / size
    ' up: across crossed with ahead
    cam.up.x = -cam.across.z * cam.ahead.y
    cam.up.y = cam.across.z * cam.ahead.x - cam.across.x * cam.ahead.z
    cam.up.z = cam.across.x * cam.ahead.y
    cam.cx = st.cx
    cam.cy = st.cy
    cam.focal = st.h * LENS
    cam.half_w = st.w / 2
    cam.half_h = st.h / 2
    cam.reach_w = sqr(cam.focal * cam.focal + cam.half_w * cam.half_w)
    cam.reach_h = sqr(cam.focal * cam.focal + cam.half_h * cam.half_h)
end sub

' The sky in bands down to the horizon, the land under it.
sub draw_sky ( _
    byval dst as long, _
    cam as Camera, _
    st as Stage, _
    sky_bands() as Band _
)
    dim lx as single
    dim lz as single
    dim size as single
    dim horizon as single
    dim lo as single
    dim hi as single
    dim k as integer
    f% = gfx.draw.FILL
    ' the horizon ahead: a level line's height on the screen
    size = sqr(cam.ahead.x * cam.ahead.x + cam.ahead.z * cam.ahead.z)
    lx = cam.ahead.x / size
    lz = cam.ahead.z / size
    up! = lx * cam.up.x + lz * cam.up.z
    away! = lx * cam.ahead.x + lz * cam.ahead.z
    horizon = cam.cy - cam.focal * up! / away!
    gfx.draw.rect dst, 0, horizon, st.w - 1, st.h - 1, LAND, f%

    for k = 0 to BANDS - 1
        lo = horizon - sky_bands(k).edge * st.h / 100

        if lo < 0 then exit for

        hi = 0

        if k < BANDS - 1 then
            hi = horizon - sky_bands(k + 1).edge * st.h / 100
        end if

        c& = sky_bands(k).colour
        gfx.draw.rect dst, 0, hi, st.w - 1, lo, c&, f%
    next
end sub

' The sun low over the land: its haze, glare's texels
' added, then its disc.
sub draw_sun ( _
    byval dst as long, _
    cam as Camera, _
    st as Stage, _
    byval glare as long, _
    q() as Vertex _
)
    dim x as single
    dim y as single
    dim z as single
    dim size as single
    dim c as integer
    z = SUN_X * cam.ahead.x + SUN_Y * cam.ahead.y + _
        SUN_Z * cam.ahead.z

    if z <= 0 then exit sub

    x = SUN_X * cam.across.x + SUN_Z * cam.across.z
    y = SUN_X * cam.up.x + SUN_Y * cam.up.y + SUN_Z * cam.up.z
    x = cam.cx + cam.focal * x / z
    y = cam.cy - cam.focal * y / z
    size = HAZE_R * st.unit

    ' clockwise from the top left
    for c = 0 to 3
        q(c).x = x + size * (2 * ((c + 1) \ 2 mod 2) - 1)
        q(c).y = y + size * (2 * (c \ 2) - 1)
        q(c).u = 2 * HAZE_R * ((c + 1) \ 2 mod 2)
        q(c).v = 2 * HAZE_R * (c \ 2)
    next

    gfx.draw.poly dst, q(), 0, 4, glare, 0, gfx.draw.ADD
    size = DISC_R * st.unit
    gfx.draw.circle dst, x, y, size, SUN_DISC, gfx.draw.FILL
end sub

' The mountain ranges round the plain, the far one first,
' in their colours over the sky: each peak ahead put on the
' screen once, its top and its foot, and the segments
' between two such drawn.
sub draw_ranges ( _
    byval dst as long, _
    cam as Camera, _
    hills() as Vec3, _
    q() as Vertex _
)
    dim g as integer
    dim k as integer
    dim i as integer
    dim known as integer
    dim d as single
    dim size as single
    dim dx as single
    dim dy as single
    dim dz as single
    dim bx as single
    dim by as single
    dim bz as single
    dim s as single
    dim colour as long
    ex! = cam.eye.x
    ey! = cam.eye.y
    ez! = cam.eye.z
    ax! = cam.across.x
    az! = cam.across.z
    ux! = cam.up.x
    uy! = cam.up.y
    uz! = cam.up.z
    fx! = cam.ahead.x
    fy! = cam.ahead.y
    fz! = cam.ahead.z
    cx! = cam.cx
    cy! = cam.cy
    focal! = cam.focal
    size = sqr(fx! * fx! + fz! * fz!)
    lx! = fx! / size
    lz! = fz! / size

    for g = 0 to RANGES - 1
        d = FAR_RANGE
        colour = FAR_HILLS

        if g then d = NEAR_RANGE
        if g then colour = NEAR_HILLS

        known = 0

        ' the last peak again, to close the ring
        for k = 0 to PEAKS
            i = k mod PEAKS
            dx = hills(g, i).x - ex!
            dz = hills(g, i).z - ez!
            ahead! = hills(g, i).x * lx!
            ahead! = ahead! + (hills(g, i).z - ORBIT_Z) * lz!

            if ahead! < SEEN_COS * d then
                known = 0
                continue for
            end if

            bx = dx * ax! + dz * az!
            by = dx * ux! + dz * uz!
            bz = dx * fx! + dz * fz!
            dy = hills(g, i).y - ey!
            s = focal! / (bz + dy * fy!)
            q(1).x = cx! + bx * s
            q(1).y = cy! - (by + dy * uy!) * s
            dy = HILLS_FOOT - ey!
            s = focal! / (bz + dy * fy!)
            q(2).x = cx! + bx * s
            q(2).y = cy! - (by + dy * uy!) * s

            if known then
                gfx.draw.poly dst, q(), 0, 4, 0, colour, gfx.draw.FILL
            end if

            q(0) = q(1)
            q(3) = q(2)
            known = -1
        next
    next
end sub

' The world on the screen. First how the camera sees each
' part: UNSEEN, wholly PAST_NEAR, or CUT by NEAR. Then each
' shaft seen, a strip of the sides facing the eye's half.
' Then the faces group by group, the group's light and tint
' set once: the solid ones, the shadows over them, the water
' at its frame ripple. A face is drawn when its part is seen
' and it faces the eye: its corners on the screen, or, its
' part CUT, cut at NEAR.
sub draw_world ( _
    byval dst as long, _
    groups() as Group, _
    byval n_groups as integer, _
    order() as long, _
    faces() as Face, _
    parts() as Part, _
    byval n_parts as long, _
    shafts() as Shaft, _
    byval n_shafts as integer, _
    vertices() as Vec3, _
    seen() as Vertex, _
    sources() as long, _
    cam as Camera, _
    byval ripple as integer, _
    r() as Vertex _
)
    dim g as integer
    dim p as long
    dim j as long
    dim f as long
    dim i as long
    dim first as long
    dim last as long
    dim eighth as integer
    dim look as integer
    dim sight as integer
    dim flags as integer
    dim shift as integer
    dim t as integer
    dim src as long
    dim colour as long
    dim dx as single
    dim dy as single
    dim dz as single
    dim x as single
    dim y as single
    dim z as single
    dim k as single
    dim radius as single
    ex! = cam.eye.x
    ey! = cam.eye.y
    ez! = cam.eye.z
    ax! = cam.across.x
    az! = cam.across.z
    ux! = cam.up.x
    uy! = cam.up.y
    uz! = cam.up.z
    fx! = cam.ahead.x
    fy! = cam.ahead.y
    fz! = cam.ahead.z
    cx! = cam.cx
    cy! = cam.cy
    focal! = cam.focal
    half_w! = cam.half_w
    half_h! = cam.half_h
    reach_w! = cam.reach_w
    reach_h! = cam.reach_h

    for p = 0 to n_parts - 1
        parts(p).seen = UNSEEN
        radius = parts(p).radius
        dx = parts(p).centre.x - ex!
        dy = parts(p).centre.y - ey!
        dz = parts(p).centre.z - ez!
        z = dx * fx! + dy * fy! + dz * fz!

        if z + radius < NEAR or z - radius > FAR then continue for

        x = abs(dx * ax! + dz * az!)
        y = abs(dx * ux! + dy * uy! + dz * uz!)

        ' past the screen's side or its top or foot
        if x * focal! - z * half_w! > radius * reach_w! then
            continue for
        end if

        if y * focal! - z * half_h! > radius * reach_h! then
            continue for
        end if

        parts(p).seen = PAST_NEAR

        if z - radius < NEAR then parts(p).seen = CUT
    next

    test% = gfx.draw.PERSPECTIVE + gfx.draw.DEPTH_TEST
    flags = test% + gfx.draw.DEPTH_WRITE
    src = sources(FLUTES)

    ' the eye's eighth round the shaft, its sides' normals
    ' turned back half a side: the side before it is the
    ' first SHOWN
    for j = 0 to n_shafts - 1
        sight = parts(shafts(j).part).seen

        if sight = UNSEEN then continue for

        dx = ex! - shafts(j).x
        dz = ez! - shafts(j).z
        x = dx * HALF_COS + dz * HALF_SIN
        z = dz * HALF_COS - dx * HALF_SIN
        eighth = 0

        if z < 0 then
            eighth = 4
            x = -x
            z = -z
        end if

        if x <= 0 then
            eighth = eighth + 2
            k = x
            x = z
            z = -k
        end if

        if z >= x then eighth = eighth + 1

        eighth = (eighth + SHAFT_SIDES - 1) mod SHAFT_SIDES
        first = shafts(j).first + 2 * eighth
        last = first + 2 * SHOWN + 1

        if sight = PAST_NEAR then
            for i = first to last
                dx = vertices(i).x - ex!
                dy = vertices(i).y - ey!
                dz = vertices(i).z - ez!
                z = dx * fx! + dy * fy! + dz * fz!
                k = focal! / z
                seen(i).x = cx! + (dx * ax! + dz * az!) * k
                seen(i).y = cy! - _
                    (dx * ux! + dy * uy! + dz * uz!) * k
                seen(i).z = z
            next

            gfx.draw.triangle_strip dst, seen(), first, _
                2 * SHOWN + 2, src, 0, flags
            continue for
        end if

        for i = first to last
            dx = vertices(i).x - ex!
            dy = vertices(i).y - ey!
            dz = vertices(i).z - ez!
            seen(i).x = dx * ax! + dz * az!
            seen(i).y = dx * ux! + dy * uy! + dz * uz!
            seen(i).z = dx * fx! + dy * fy! + dz * fz!
        next

        for i = first to last - 2
            draw_cut dst, seen(), i, i + 2, cam, src, 0, flags, r()
        next
    next

    look = -1

    for g = 0 to n_groups - 1
        if groups(g).look <> look then
            look = groups(g).look

            select case look
                case SOLID_PART
                    flags = test% + gfx.draw.DEPTH_WRITE
                case SHADOW_PART
                    flags = gfx.draw.FILL + gfx.draw.BLEND + test%
                    gfx.draw.set_alpha SHADOW_ALPHA
                case else
                    flags = gfx.draw.BLEND + test%
                    gfx.draw.set_alpha WATER_ALPHA
                    shift = ripple
            end select
        end if

        gfx.draw.set_light groups(g).light
        gfx.draw.set_tint groups(g).tint

        for j = groups(g).first to groups(g).last
            f = order(j)
            sight = parts(faces(f).part).seen

            if sight = UNSEEN then continue for
            ' the eye behind its plane
            if faces(f).nx * ex! + faces(f).ny * ey! + _
                faces(f).nz * ez! <= faces(f).offset then continue for

            first = faces(f).first
            last = first + faces(f).n - 1
            t = faces(f).texture
            src = 0
            colour = INK

            if t <> INKED then
                src = sources(t + shift)
                colour = 0
            end if

            if sight = PAST_NEAR then
                for i = first to last
                    dx = vertices(i).x - ex!
                    dy = vertices(i).y - ey!
                    dz = vertices(i).z - ez!
                    z = dx * fx! + dy * fy! + dz * fz!
                    k = focal! / z
                    seen(i).x = cx! + (dx * ax! + dz * az!) * k
                    seen(i).y = cy! - _
                        (dx * ux! + dy * uy! + dz * uz!) * k
                    seen(i).z = z
                next

                gfx.draw.poly dst, seen(), first, faces(f).n, src, _
                    colour, flags
                continue for
            end if

            for i = first to last
                dx = vertices(i).x - ex!
                dy = vertices(i).y - ey!
                dz = vertices(i).z - ez!
                seen(i).x = dx * ax! + dz * az!
                seen(i).y = dx * ux! + dy * uy! + dz * uz!
                seen(i).z = dx * fx! + dy * fy! + dz * fz!
            next

            draw_cut dst, seen(), first, last, cam, src, colour, _
                flags, r()
        next
    next

    gfx.draw.set_alpha FULL
    gfx.draw.set_light FULL
    gfx.draw.set_tint &HFFFFFF
end sub

' The polygon seen(first) to seen(last), in the camera's
' space, cut at NEAR into r, then on the screen.
sub draw_cut ( _
    byval dst as long, _
    seen() as Vertex, _
    byval first as long, _
    byval last as long, _
    cam as Camera, _
    byval src as long, _
    byval colour as long, _
    byval flags as integer, _
    r() as Vertex _
)
    dim i as long
    dim nxt as long
    dim m as integer
    dim k as single
    m = 0

    ' each corner past NEAR, and where each edge crosses it
    for i = first to last
        nxt = i + 1

        if nxt > last then nxt = first

        if seen(i).z >= NEAR then
            r(m) = seen(i)
            m = m + 1
        end if

        if (seen(i).z >= NEAR) = (seen(nxt).z >= NEAR) then
            continue for
        end if

        k = (NEAR - seen(i).z) / (seen(nxt).z - seen(i).z)
        r(m).x = seen(i).x + k * (seen(nxt).x - seen(i).x)
        r(m).y = seen(i).y + k * (seen(nxt).y - seen(i).y)
        r(m).z = NEAR
        r(m).u = seen(i).u + k * (seen(nxt).u - seen(i).u)
        r(m).v = seen(i).v + k * (seen(nxt).v - seen(i).v)
        m = m + 1
    next

    if m < 3 then exit sub

    for i = 0 to m - 1
        r(i).x = cam.cx + cam.focal * r(i).x / r(i).z
        r(i).y = cam.cy - cam.focal * r(i).y / r(i).z
    next

    gfx.draw.poly dst, r(), 0, m, src, colour, flags
end sub

' The fires: every glow, then every flame at its frame,
' added over what is nearer, each upright and facing the
' eye. The camera never comes near a fire, so one with a
' corner behind NEAR is behind the eye: not drawn.
sub draw_fires ( _
    byval dst as long, _
    fires() as Vec3, _
    byval n as integer, _
    sources() as long, _
    cam as Camera, _
    byval t as single, _
    q() as Vertex _
)
    dim at(MOST_FIRES - 1) as Vec3
    dim i as integer
    dim c as integer
    dim kind as integer
    dim src as long
    dim dx as single
    dim dy as single
    dim dz as single
    dim foot as single
    dim wide as single
    dim high as single
    dim x as single
    dim y as single
    dim z as single
    dim k as single
    ex! = cam.eye.x
    ey! = cam.eye.y
    ez! = cam.eye.z
    ax! = cam.across.x
    az! = cam.across.z
    ux! = cam.up.x
    uy! = cam.up.y
    uz! = cam.up.z
    fx! = cam.ahead.x
    fy! = cam.ahead.y
    fz! = cam.ahead.z
    cx! = cam.cx
    cy! = cam.cy
    focal! = cam.focal
    f% = gfx.draw.PERSPECTIVE + gfx.draw.ADD + gfx.draw.DEPTH_TEST
    beat% = int(t * FLICKER)

    ' each fire in the camera's space
    for i = 0 to n - 1
        dx = fires(i).x - ex!
        dy = fires(i).y - ey!
        dz = fires(i).z - ez!
        at(i).x = dx * ax! + dz * az!
        at(i).y = dx * ux! + dy * uy! + dz * uz!
        at(i).z = dx * fx! + dy * fy! + dz * fz!
    next

    for kind = 0 to 1
        foot = FLAME_H / 2 - GLOW_SIZE / 2
        wide = GLOW_SIZE
        high = GLOW_SIZE
        gfx.draw.set_alpha GLOW_ALPHA

        if kind then
            foot = 0
            wide = FLAME_W
            high = FLAME_H
            gfx.draw.set_alpha FULL
        end if

        for i = 0 to n - 1
            src = sources(GLOW)

            if kind then
                src = sources(FLAME + (beat% + i) mod PHASES)
            end if

            ' clockwise from the top left, as the eye sees it:
            ' across is level, so the corners are aside in x
            for c = 0 to 3
                y = foot + high * (1 - c \ 2)
                z = at(i).z + fy! * y

                if z < NEAR then exit for

                k = focal! / z
                x = at(i).x + ((c + 1) \ 2 mod 2 - 0.5) * wide
                q(c).x = cx! + x * k
                q(c).y = cy! - (at(i).y + uy! * y) * k
                q(c).z = z
                q(c).u = SIDE * ((c + 1) \ 2 mod 2)
                q(c).v = SIDE * (c \ 2)
            next

            if c < 4 then continue for

            gfx.draw.poly dst, q(), 0, 4, src, 0, f%
        next
    next
end sub

' The key along dst's top, the frame rate at its foot.
sub show_help ( _
    byval dst as long, _
    st as Stage, _
    byval moving as integer, _
    m as Meter _
)
    grey& = &HC8C8C8
    z% = st.zoom
    edge% = MARGIN * z%
    t$ = "SPACE camera " + on_off$(moving)
    label dst, st.top, edge%, edge%, t$, grey&, z%
    foot% = st.h - edge% - st.text_h
    label dst, st.foot, edge%, foot%, rate_text$(m), grey&, z%
end sub
