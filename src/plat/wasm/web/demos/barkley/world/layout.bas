' What a level is laid with: columns, pits, shelves, scenery and things.
defint A-Z
option explicit

from barkley.model.level import KBONE, KBUSH, KFLOWER, KLAMP, KMOVE, KPLAT, KPOST, KSEAT, KSPAN, KSPRING, KTANK
from barkley.model.level import KTRAM, KTREE, KVENT, Level, MH, MW, Realm, TS
from barkley.world.terrain.ground import Sunk
from barkley.world.terrain.track import LimbUp, LIMBW, LimbX, NLIMB

const PITD = 5

' A moving platform, its middle's left end at x, y; reach a, kind k; e the edge before it.
sub AddMover (lv as Level, x as integer, y as integer, a as integer, k as integer, e as integer)
    select case k
    case 6
        AddObj lv, KSPAN, x, y
    case 7
        AddObj lv, KSEAT, x, y
    case 8
        AddObj lv, KTRAM, x, y
    case else
        AddObj lv, KMOVE, x, y
    end select
    lv.movers(lv.n_movers).at.x = x
    lv.movers(lv.n_movers).at.y = y
    lv.movers(lv.n_movers).reach = a
    lv.movers(lv.n_movers).kind = k
    lv.movers(lv.n_movers).edge = e
    lv.n_movers = lv.n_movers + 1
end sub

' What grows past x is pulled up.
sub Weed (lv as Level, x as integer)
    dim i as integer
    for i = 0 to lv.n_things - 1
        if lv.things(i).kind >= KTREE and lv.things(i).kind <= KLAMP and lv.things(i).at.x > x then lv.things(i).kind = 0
    next
end sub

' Nothing grows about column c, to hide what stands there.
sub Clearing (lv as Level, c as integer)
    Weed lv, c * TS - 28
    lv.fallow = c + 3
end sub

' Flowers, bushes and lantern plants along the flats, by the column; none before column lv.fallow.
sub Scatter (lv as Level, zn as Realm, c as integer, s as integer)
    if c < 8 or c < lv.fallow then exit sub
    if zn.decked then
        ' lamp posts, tanks and valve stacks along the decks
        select case (c * 37 + 11) mod 23
        case 0, 12
            AddObj lv, KPOST, c * TS + 8, s
        case 5
            AddObj lv, KTANK, c * TS + 8, s
        case 17
            AddObj lv, KVENT, c * TS + 8, s
        end select
        exit sub
    end if
    select case (c * 37 + 11) mod 19
    case 0, 7
        AddObj lv, KFLOWER, c * TS + 8, s
    case 3
        AddObj lv, KBUSH, c * TS + 8, s
    case 11
        AddObj lv, KLAMP, c * TS + 8, s
    end select
end sub

' Column c solid g tiles up from the bottom; kind over 1 a slope on top.
sub Column (lv as Level, c as integer, g as integer, kind as integer)
    dim r as integer
    for r = MH - g to MH - 1
        lv.map(c, r) = 1
    next
    lv.glaze(c) = lv.icing
    if kind > 1 then
        lv.map(c, MH - g - 1) = kind
        lv.top(c) = MH - g - 1
        lv.vis(c) = kind + 2
    else
        lv.top(c) = MH - g
        lv.vis(c) = 1 + (c and 1)
    end if
end sub

' Under a jump or a platform, not a fall to nothing: n columns from c of floor PITD under the ground g, bones on
' it, a spring at its far wall.
sub Pit (lv as Level, c as integer, g as integer, n as integer)
    dim i as integer, y as integer
    y = (MH - g + PITD) * TS
    for i = 0 to n - 1
        Column lv, c + i, g - PITD, 1
        if i > 1 and i < n - 3 and (i and 1) = 0 and not Sunk%(lv, y) then AddObj lv, KBONE, (c + i) * TS + 8, y - 14
    next
    if not Sunk%(lv, y) then AddObj lv, KSPRING, (c + n - 2) * TS + 8, y
end sub

' A shelf w wide, n tiles over the ground, s, at column c; g the ground's height in tiles.
sub Shelf (lv as Level, c as integer, g as integer, s as integer, n as integer, w as integer)
    dim i as integer
    for i = 0 to w - 1
        lv.map(c + i, MH - g - n - 1) = 1
        AddObj lv, KPLAT, (c + i) * TS, s - (n + 1) * TS
    next
end sub

' The great oak's limbs' flats, solid to stand on; g the ground's height in tiles at its foot.
sub Limbs (lv as Level, g as integer)
    dim n as integer, i as integer, c as integer
    for n = 0 to NLIMB - 1
        c = (lv.oak.x + LimbX%(n)) \ TS
        for i = 0 to LIMBW \ TS - 1
            lv.map(c + i, MH - g - LimbUp%(n) \ TS) = 1
        next
    next
end sub

sub AddObj (lv as Level, k as integer, x as integer, y as integer)
    lv.things(lv.n_things).kind = k
    lv.things(lv.n_things).at.x = x
    lv.things(lv.n_things).at.y = y
    lv.things(lv.n_things).home = x
    lv.n_things = lv.n_things + 1
end sub

' Whether thing i stands rooted in the ground: of a kind from lo to the lamp, or a post.
function Rooted% (lv as Level, i as integer, lo as integer)
    Rooted% = (lv.things(i).kind >= lo and lv.things(i).kind <= KLAMP) or lv.things(i).kind >= KPOST
end function

' The things of kind k told each other in pairs: a thing's state its other end, counted from 1. With no way
' back the second's is 0: a way out that takes no one in.
sub Pair (lv as Level, k as integer, back as integer)
    dim i as integer, j as integer
    for i = 0 to lv.n_things - 1
        if lv.things(i).kind <> k or lv.things(i).state >= 0 then continue for
        for j = i + 1 to lv.n_things - 1
            if lv.things(j).kind <> k or lv.things(j).state <> lv.things(i).state then continue for
            lv.things(i).state = j + 1
            lv.things(j).state = 0
            if back then lv.things(j).state = i + 1
            exit for
        next
    next
end sub

' The things by x, and first(c) the first at or past column c.
sub SortObjs (lv as Level)
    dim i as integer, j as integer, c as integer
    for i = 1 to lv.n_things - 1
        j = i
        do while j > 0
            if lv.things(j - 1).at.x <= lv.things(j).at.x then exit do
            swap lv.things(j - 1), lv.things(j)
            j = j - 1
        loop
    next
    i = 0
    for c = 0 to MW
        do while i < lv.n_things
            if lv.things(i).at.x >= c * TS then exit do
            i = i + 1
        loop
        lv.first(c) = i
    next
    for i = 0 to lv.n_things - 1
        lv.things(i).home = lv.things(i).at.x
    next
end sub
