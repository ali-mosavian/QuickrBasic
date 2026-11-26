' The land made by rule from a seed: heights by midpoint
' displacement on a grid that wraps, smoothed and shaped
' into valleys, rivers cut where a second such field
' crosses its middle; light by slope, the sun's shadows
' and the water's depth; the ground by slope, height and
' bare patches where a third runs low, woods darker where
' it runs high.
option explicit

from highvale.model.space import Point3
from highvale.model.land import Land, MAP, MAP_MASK, SIDE
from highvale.model.land import LEVELS, PEAK, LIGHTS, SCALE
from highvale.model.land import STEP_PARTS, GRASS_FROM
from highvale.model.land import ROCK_FROM
from highvale.land.palette import WATER, GRASS, ROCK

' a field's first swing, and its swing kept each halving,
' in hundredths: the relief's, and the rivers' course's
const FIRST_SWING = 16384
const RELIEF_ROUGH = 40
const COURSE_ROUGH = 25
' a field's values, and the sea's
const VALUES = 4096
const SEA_LINE = 900
' rivers: their banks' reach from the course's middle,
' and the most they cut, in values
const BANK = 400
const CUT = 700
' rock where a texel's slope over STEEP, in steps a
' texel, its height over HIGH, in steps, and the cover
' under its middle, each over its span, sum past 0; the
' sum eased in over GROUND_SPREAD of a ground's values
const STEEP = 1.6
const STEEP_SPAN = 1.2
const HIGH = 110
const HIGH_SPAN = 80
const BARE_SPAN = 0.5
const GROUND_SPREAD = 64
' woods where the second field is above this level, their
' light this much under the grass's
const WOODS_LINE = 2400
const WOODS_SHADE = 3
' toward the sun, from the north-west and 30 degrees up,
' and the light of ground turned from it, 0 to 1
const SUN_X = -0.612
const SUN_Y = -0.612
const SUN_Z = 0.5
const AMBIENT = 0.3
' a shadow darkens fully this many parts under its line,
' and water a third as much as land
const SOFT = 48
const WATER_SHADOW = 3
' the relief's smoothing passes
const SMOOTHING = 6
' points along a line of sight checked against the ground
const SIGHTS = 12

' The side of level m's rows: its texels and one more.
function level_side (byval m as integer) as long
    level_side = MAP \ 2 ^ m + 1
end function

' Where level m starts in the land's arrays.
function level_base (byval m as integer) as long
    dim j as integer
    dim n as long

    for j = 0 to m - 1
        n = n + level_side(j) * level_side(j)
    next

    level_base = n
end function

' Makes the land's arrays.
sub allot_land (ld as Land)
    redim ld.heights(level_base(LEVELS) - 1)
    redim ld.lights(level_base(LEVELS) - 1)
    redim ld.grounds(level_base(LEVELS) - 1)
end sub

' A field of midpoint displacement over the wrapping grid,
' its swing kept rough hundredths each halving.
sub raise_field (f() as long, byval rough as integer)
    dim span as long
    dim half as long
    dim swing as long
    dim x as long
    dim y as long
    dim x2 as long
    dim y2 as long
    dim sum as long

    f(0) = 0
    span = MAP
    swing = FIRST_SWING

    do while span > 1
        half = span \ 2

        for y = 0 to MAP - 1 step span
            y2 = ((y + span) and MAP_MASK) * MAP

            for x = 0 to MAP - 1 step span
                x2 = (x + span) and MAP_MASK
                sum = f(y * MAP + x) + f(y * MAP + x2)
                sum = sum + f(y2 + x) + f(y2 + x2)
                f((y + half) * MAP + x + half) = _
                    sum \ 4 + (rnd * 2 - 1) * swing
            next
        next

        for y = 0 to MAP - 1 step half
            for x = ((y \ half + 1) and 1) * half to MAP - 1 _
                    step span
                sum = f(((y - half) and MAP_MASK) * MAP + x)
                sum = sum + f(((y + half) and MAP_MASK) * MAP + x)
                sum = sum + f(y * MAP + ((x - half) and MAP_MASK))
                sum = sum + f(y * MAP + ((x + half) and MAP_MASK))
                f(y * MAP + x) = sum \ 4 + (rnd * 2 - 1) * swing
            next
        next

        span = half
        swing = swing * rough \ 100
    loop
end sub

' Each cell the mean of itself and its four neighbours.
sub smooth_field (f() as long)
    dim g(MAP * MAP - 1) as long
    dim x as long
    dim y as long
    dim row as long
    dim up as long
    dim down as long

    for y = 0 to MAP - 1
        row = y * MAP
        up = ((y - 1) and MAP_MASK) * MAP
        down = ((y + 1) and MAP_MASK) * MAP

        for x = 0 to MAP - 1
            g(row + x) = (f(row + x) + f(up + x) + f(down + x) _
                + f(row + ((x - 1) and MAP_MASK)) _
                + f(row + ((x + 1) and MAP_MASK))) \ 5
        next
    next

    f = g
end sub

' The field scaled to 0 to VALUES - 1 in place.
sub level_field (f() as long)
    dim lo as long
    dim hi as long
    dim i as long

    lo = f(0)
    hi = f(0)

    for i = 1 to MAP * MAP - 1
        if f(i) < lo then lo = f(i)
        if f(i) > hi then hi = f(i)
    next

    if hi = lo then hi = lo + 1

    for i = 0 to MAP * MAP - 1
        f(i) = (f(i) - lo) * (VALUES - 1) \ (hi - lo)
    next
end sub

' Height by value, in parts of a step: low land
' flattened into valleys, the sea flat.
sub shape_table (parts() as long)
    dim i as integer
    dim v as single

    for i = 0 to VALUES - 1
        v = i / (VALUES - 1)
        if i < SEA_LINE then v = SEA_LINE / (VALUES - 1)
        parts(i) = int(PEAK * STEP_PARTS * v * sqr(v))
    next
end sub

' The ground a texel's ground value v names.
function ground_of (byval v as integer) as integer
    ground_of = ROCK
    if v < ROCK_FROM then ground_of = GRASS
    if v < GRASS_FROM then ground_of = WATER
end function

' Land's ground value: rock by slope, a step's fall a
' texel, height h, in parts, and cover; near the sea no
' more than its parts over sea allow, so the shore keeps
' to the sea's line.
function land_ground ( _
    byval slope as single, _
    byval h as long, _
    byval sea as long, _
    byval cover as long _
) as integer
    dim stony as single
    dim v as single

    stony = (slope - STEEP) / STEEP_SPAN _
        + (h / STEP_PARTS - HIGH) / HIGH_SPAN _
        + (0.5 - cover / (VALUES - 1)) / BARE_SPAN
    v = ROCK_FROM + stony * GROUND_SPREAD
    if v < GRASS_FROM then v = GRASS_FROM
    if v > 255 then v = 255
    if v > (h - sea) * GRASS_FROM / STEP_PARTS then
        v = (h - sea) * GRASS_FROM / STEP_PARTS
    end if
    land_ground = v
end function

' n kept within 0 to LIGHTS - 1.
function within_light (byval n as integer) as integer
    if n < 0 then n = 0
    if n > LIGHTS - 1 then n = LIGHTS - 1
    within_light = n
end function

' The light of ground rising dx parts of a step over two
' texels east and dy south, shaded 0 to 1 by shadow: the
' sun on it, in LIGHTS levels.
function sunlight ( _
    byval dx as long, _
    byval dy as long, _
    byval shaded as single _
) as integer
    dim gx as single
    dim gy as single
    dim lit as single

    gx = dx * SCALE / (2 * STEP_PARTS)
    gy = dy * SCALE / (2 * STEP_PARTS)
    lit = SUN_Z - gx * SUN_X - gy * SUN_Y
    lit = lit / sqr(gx * gx + gy * gy + 1)
    if lit < 0 then lit = 0
    lit = AMBIENT + (1 - AMBIENT) * lit * (1 - shaded)
    sunlight = int(lit * (LIGHTS - 1) + 0.5)
end function

' How deep each texel of the heights fine lies in the
' shadow of ground toward the sun, 0 to 1: the line of
' the sun's last touch swept from the north-west, falling
' at the sun's angle each diagonal step, twice so it
' carries over the map's wrap.
sub cast_shadows (fine() as long, shade() as single)
    dim edge(MAP * MAP - 1) as long
    dim drop as long
    dim pass as integer
    dim x as long
    dim y as long
    dim i as long
    dim over as long

    drop = SUN_Z / sqr(SUN_X * SUN_X + SUN_Y * SUN_Y) _
        * sqr(2) * STEP_PARTS / SCALE

    for pass = 1 to 2
        for y = 0 to MAP - 1
            for x = 0 to MAP - 1
                i = y * MAP + x
                over = cell_of(edge(), x - 1, y - 1) - drop
                shade(i) = (over - fine(i)) / SOFT
                if shade(i) < 0 then shade(i) = 0
                if shade(i) > 1 then shade(i) = 1
                edge(i) = fine(i)
                if over > fine(i) then edge(i) = over
            next
        next
    next
end sub

' Level m of the land from fields of its side, n: heights
' in parts of a step, lights and grounds; their last row
' and column the first again.
sub put_level ( _
    ld as Land, _
    byval m as integer, _
    hs() as long, _
    ls() as long, _
    gs() as long _
)
    dim n as long
    dim first as long
    dim x as long
    dim y as long
    dim i as long
    dim j as long

    n = level_side(m) - 1
    first = level_base(m)

    for y = 0 to n
        for x = 0 to n
            i = first + y * (n + 1) + x
            j = (y and (n - 1)) * n + (x and (n - 1))
            ld.heights(i) = hs(j)
            ld.lights(i) = ls(j)
            ld.grounds(i) = gs(j)
        next
    next
end sub

' A field of side n made half that, each cell the mean of
' four, in place.
sub halve (f() as long, byval n as long)
    dim x as long
    dim y as long
    dim i as long

    for y = 0 to n \ 2 - 1
        for x = 0 to n \ 2 - 1
            i = 2 * y * n + 2 * x
            f(y * (n \ 2) + x) = _
                (f(i) + f(i + 1) + f(i + n) + f(i + n + 1)) \ 4
        next
    next
end sub

' A field's cell at x, y, the map wrapping.
function cell_of ( _
    f() as long, _
    byval x as long, _
    byval y as long _
) as long
    cell_of = f((y and MAP_MASK) * MAP + (x and MAP_MASK))
end function

' The relief cut by up to CUT where the course runs near
' its middle, its banks eased in: rivers where the land
' is low, valleys where it is high.
sub cut_rivers (relief() as long, course() as long)
    dim i as long
    dim t as single

    for i = 0 to MAP * MAP - 1
        t = abs(course(i) - VALUES \ 2) / BANK
        if t >= 1 then continue for
        relief(i) = relief(i) - CUT * (1 - t * t * (3 - 2 * t))
        if relief(i) < 0 then relief(i) = 0
    next
end sub

' A field raised, smoothed and levelled.
sub make_field (f() as long, byval rough as integer)
    dim i as integer

    raise_field f(), rough

    for i = 1 to SMOOTHING
        smooth_field f()
    next

    level_field f()
end sub

' The land of seed: its heights, its light and its
' ground, at every level.
sub make_land (ld as Land, byval seed as integer)
    dim relief(MAP * MAP - 1) as long
    dim course(MAP * MAP - 1) as long
    dim cover(MAP * MAP - 1) as long
    dim parts(VALUES - 1) as long
    dim fine(MAP * MAP - 1) as long
    dim lit(MAP * MAP - 1) as long
    dim ground(MAP * MAP - 1) as long
    dim shade(MAP * MAP - 1) as single
    dim x as long
    dim y as long
    dim i as long
    dim m as integer
    dim east as long
    dim south as long
    dim light as integer
    dim slope as single

    randomize seed
    make_field relief(), RELIEF_ROUGH
    make_field course(), COURSE_ROUGH
    cut_rivers relief(), course()
    raise_field cover(), RELIEF_ROUGH
    level_field cover()
    shape_table parts()
    ld.sea = parts(0) \ STEP_PARTS

    for i = 0 to MAP * MAP - 1
        fine(i) = parts(relief(i))
    next

    cast_shadows fine(), shade()

    for y = 0 to MAP - 1
        for x = 0 to MAP - 1
            i = y * MAP + x
            east = cell_of(fine(), x + 1, y)
            east = east - cell_of(fine(), x - 1, y)
            south = cell_of(fine(), x, y + 1)
            south = south - cell_of(fine(), x, y - 1)
            light = sunlight(east, south, shade(i))
            slope = sqr(east * east + south * south) _
                / (2 * STEP_PARTS)
            ground(i) = land_ground(slope, fine(i), parts(0), _
                cover(i))

            if relief(i) < SEA_LINE then
                ground(i) = 0
                light = (LIGHTS - 1) * relief(i) \ SEA_LINE
                light = light - shade(i) * LIGHTS / WATER_SHADOW
            elseif ground(i) < ROCK_FROM _
                    and cover(i) > WOODS_LINE then
                light = light - WOODS_SHADE
            end if

            lit(i) = within_light(light)
        next
    next

    for m = 0 to LEVELS - 1
        put_level ld, m, fine(), lit(), ground()
        halve fine(), level_side(m) - 1
        halve lit(), level_side(m) - 1
        halve ground(), level_side(m) - 1
    next
end sub

' The ground's height at x, y, between texels, the map
' wrapping.
function ground_at ( _
    ld as Land, _
    byval x as single, _
    byval y as single _
) as single
    dim ix as long
    dim iy as long
    dim fx as single
    dim fy as single
    dim i as long
    dim a as single
    dim b as single

    ix = int(x)
    iy = int(y)
    fx = x - ix
    fy = y - iy
    i = (iy and MAP_MASK) * SIDE + (ix and MAP_MASK)
    a = ld.heights(i) + (ld.heights(i + 1) - ld.heights(i)) * fx
    i = i + SIDE
    b = ld.heights(i) + (ld.heights(i + 1) - ld.heights(i)) * fx
    ground_at = (a + (b - a) * fy) / STEP_PARTS
end function

' d brought within half the map of 0, as the land wraps.
function wrapped (byval d as single) as single
    if d >= MAP / 2 then d = d - MAP
    if d < -MAP / 2 then d = d + MAP
    wrapped = d
end function

' Whether the ground hides nothing on the line from a to b.
function clear_line (ld as Land, a as Point3, b as Point3) as integer
    dim dx as single
    dim dy as single
    dim i as integer
    dim t as single
    dim z as single

    dx = wrapped(b.x - a.x)
    dy = wrapped(b.y - a.y)
    clear_line = 0

    for i = 1 to SIGHTS - 1
        t = i / SIGHTS
        z = a.z + (b.z - a.z) * t
        if ground_at(ld, a.x + dx * t, a.y + dy * t) > z then
            exit function
        end if
    next

    clear_line = -1
end function
