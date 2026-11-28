' The view out of the cockpit: the sky in bands that roll
' with the horizon, the vale's terrain, then the foes,
' shots and blasts, farthest first.
option explicit

import gfx.draw
from gfx.draw import Vertex
from highvale.model.space import Point3
from highvale.model.land import Land
from highvale.model.foes import Foe, Shot, Blast
from highvale.model.foes import FLAK, DRONE
from highvale.model.foes import ROUND, ROCKET, BURST, BOLT, BLOW
from highvale.land.palette import SKY, SKY_BANDS, FOG
from highvale.land.palette import FLAME, FIRE, HOT, SMOKE
from highvale.land.palette import SMOKE_DARK, TRACER, FOE_RED
from highvale.view.eye import Camera, Spot, place_spot, in_sight
from highvale.view.eye import MID_X, VIEW_W, VIEW_H
from highvale.view.art import Art
from highvale.view.terrain import Terrain, draw_terrain
from highvale.play.combat import blast_life

' a sky band's rows
const BAND_ROWS = 9
' a sprite's pixel, in texels, and its scale's bounds
const SPRITE_TEXEL = 0.5
const LEAST_SCALE = 0.25
const MOST_SCALE = 6
' a thing's kind in the draw list
const IS_FOE = 1
const IS_SHOT = 2
const IS_BLAST = 3
' things drawn at most a frame
const MOST_SEEN = 76
' seconds a round's streak spans
const STREAK = 0.025
' the drones' rotor frames a second
const ROTOR_RATE = 14

' A thing in view: its kind, its index, its spot.
type Seen
    kind as integer
    index as integer
    at_spot as Spot
end type

' The band from row y0 to y1 at the middle, tilted by
' slope, filled.
sub band ( _
    byval dst as long, _
    byval y0 as single, _
    byval y1 as single, _
    byval slope as single, _
    byval colour as integer _
)
    dim p(3) as Vertex
    dim lean as single

    lean = (MID_X + 1) * slope
    p(0).x = -1
    p(0).y = y0 - lean
    p(1).x = VIEW_W
    p(1).y = y0 + lean
    p(2).x = VIEW_W
    p(2).y = y1 + lean
    p(3).x = -1
    p(3).y = y1 - lean
    gfx.draw.poly dst, p(), 0, 4, 0, colour, gfx.draw.FILL
end sub

' The sky's bands down to the horizon, the fog below it.
sub draw_sky (byval dst as long, k as Camera)
    dim i as integer
    dim slope as single
    dim y as single

    slope = sin(k.roll) / cos(k.roll)
    gfx.draw.clear dst, FOG
    y = k.horizon - (SKY_BANDS - 1) * BAND_ROWS
    band dst, y - 4 * VIEW_H, y, slope, SKY

    for i = 1 to SKY_BANDS - 2
        band dst, y, y + BAND_ROWS, slope, SKY + i
        y = y + BAND_ROWS
    next
end sub

' A seen thing added to the list, kept nearest last.
sub add_seen ( _
    seen() as Seen, _
    n as integer, _
    byval kind as integer, _
    byval index as integer, _
    s as Spot _
)
    dim i as integer

    if n > ubound(seen) then exit sub
    i = n

    do while i > 0
        if seen(i - 1).at_spot.ahead >= s.ahead then exit do
        seen(i) = seen(i - 1)
        i = i - 1
    loop

    seen(i).kind = kind
    seen(i).index = index
    seen(i).at_spot = s
    n = n + 1
end sub

' A sprite at a spot, scaled by its distance and turned
' with the horizon.
sub stand ( _
    byval dst as long, _
    s as Spot, _
    byval sprite as long, _
    byval roll as single _
)
    dim size as single

    size = s.size * SPRITE_TEXEL
    if size < LEAST_SCALE then size = LEAST_SCALE
    if size > MOST_SCALE then size = MOST_SCALE
    gfx.draw.sprite_turned dst, s.x, s.y, roll, size, sprite, 0
end sub

' A disc at a spot, r texels across, at least a pixel.
sub disc ( _
    byval dst as long, _
    s as Spot, _
    byval r as single, _
    byval colour as integer _
)
    dim px as single

    px = r * s.size
    if px < 0.6 then px = 0.6
    gfx.draw.circle dst, s.x, s.y, px, colour, gfx.draw.FILL
end sub

sub draw_foe ( _
    byval dst as long, _
    f as Foe, _
    s as Spot, _
    a as Art, _
    k as Camera, _
    byval t as single _
)
    if f.kind = DRONE then
        stand dst, s, a.drone(int(t * ROTOR_RATE) and 1), k.roll
        exit sub
    end if

    if f.hp > 0 then
        stand dst, s, a.flak, k.roll
    else
        stand dst, s, a.wreck, k.roll
    end if
end sub

sub draw_shot ( _
    byval dst as long, _
    sh as Shot, _
    s as Spot, _
    k as Camera _
)
    dim tail as Point3
    dim e as Spot

    select case sh.kind
        case ROUND
            tail.x = sh.place.x - sh.motion.x * STREAK
            tail.y = sh.place.y - sh.motion.y * STREAK
            tail.z = sh.place.z - sh.motion.z * STREAK
            if not place_spot(k, tail, e) then exit sub
            gfx.draw.line dst, s.x, s.y, e.x, e.y, TRACER
        case ROCKET
            disc dst, s, 0.5, FIRE
        case BURST
            disc dst, s, 1.6, SMOKE_DARK
        case BOLT
            disc dst, s, 0.6, FOE_RED
    end select
end sub

sub draw_blast (byval dst as long, b as Blast, s as Spot)
    dim f as single

    f = b.age / blast_life(b.kind)

    if b.kind <> BLOW then
        disc dst, s, 1.2 * (1 - f), HOT
        exit sub
    end if

    if f > 0.6 then
        disc dst, s, 3 + 4 * f, SMOKE
        exit sub
    end if

    disc dst, s, 2 + 6 * f, FLAME
    disc dst, s, 1.4 + 4 * f, FIRE
    disc dst, s, 1.6 - 2 * f, HOT
end sub

' The view: sky, terrain, then what flies or stands in it.
sub draw_scene ( _
    byval dst as long, _
    k as Camera, _
    tv as Terrain, _
    ld as Land, _
    foes() as Foe, _
    shots() as Shot, _
    blasts() as Blast, _
    a as Art, _
    byval t as single _
)
    dim seen(MOST_SEEN - 1) as Seen
    dim n as integer
    dim i as integer
    dim s as Spot

    draw_sky dst, k
    draw_terrain dst, tv, ld, k

    for i = 0 to ubound(foes)
        if foes(i).kind = 0 then continue for
        if not place_spot(k, foes(i).place, s) then continue for
        if not in_sight(ld, k, foes(i).place) then continue for
        add_seen seen(), n, IS_FOE, i, s
    next

    for i = 0 to ubound(shots)
        if shots(i).kind = 0 then continue for
        if not place_spot(k, shots(i).place, s) then continue for
        add_seen seen(), n, IS_SHOT, i, s
    next

    for i = 0 to ubound(blasts)
        if blasts(i).kind = 0 then continue for
        if not place_spot(k, blasts(i).place, s) then continue for
        if not in_sight(ld, k, blasts(i).place) then continue for
        add_seen seen(), n, IS_BLAST, i, s
    next

    for i = 0 to n - 1
        select case seen(i).kind
            case IS_FOE
                draw_foe dst, foes(seen(i).index), seen(i).at_spot, _
                    a, k, t
            case IS_SHOT
                draw_shot dst, shots(seen(i).index), _
                    seen(i).at_spot, k
            case IS_BLAST
                draw_blast dst, blasts(seen(i).index), seen(i).at_spot
        end select
    next
end sub
