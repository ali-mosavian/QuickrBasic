' The ground's queries: surface, wall, landing, slope, loop,
' ribbon; the tables they read.
defint A-Z
option explicit

from engine.clock import STEPT
from barkley.model.hero import Hero
from barkley.model.level import Ground, Level, MH, NONE, PI, TS

const LOOPR = 72

' The ground's line: its most points, a loop's and a quarter
' pipe's stretches, the curves there can be; under STALL
' Charlie cannot stay on a wall or a roof.
const LOOPN = 48
const STALL = 150!

const WAVEL = 192
const WADE = 10

' Each tile kind's column heights: 1 solid; 2, 3 rise and
' fall at 45 degrees; 4 and 5, 6 and 7 rise and fall half as
' steep over two tiles.
sub LoadTables (lv as Level)
    dim i as integer

    for i = 0 to TS - 1
        lv.prof(1, i) = TS
        lv.prof(2, i) = i + 1
        lv.prof(3, i) = TS - i
        lv.prof(4, i) = (i + 2) \ 2
        lv.prof(5, i) = 8 + (i + 2) \ 2
        lv.prof(6, i) = TS - i \ 2
        lv.prof(7, i) = 8 - i \ 2
    next
end sub

' The angle of ground that runs the way u, v.
function Bearing! (u as single, v as single)
    if u > 0 then
        Bearing! = atn(-v / u)
    elseif u < 0 then
        Bearing! = atn(-v / u) + PI
    else
        Bearing! = -sgn(v) * PI / 2
    end if
end function

' The tiles' stretch of the line under x, its top within 4
' of y; -1 when there is none.
function SegAt% (lv as Level, gr as Ground, x as integer, y as single)
    dim s as integer

    SegAt% = -1
    if x < 0 or x >= lv.wpx then exit function

    s = TileSeg%(gr, x, y)
    if s < 0 then s = PlatSeg%(gr, x, y)
    SegAt% = s
end function

' The tiles' stretch under x, its top within 4 of y; -1 when
' there is none.
private function TileSeg% (gr as Ground, x as integer, y as single)
    dim s as integer

    TileSeg% = -1
    s = gr.col(x \ TS)
    if s < 0 then exit function

    ' past a loop that stands in the column: the stretch
    ' after it
    if x > gr.pt(s + 1).at.x then s = s + LOOPN + 1

    if abs(gr.pt(s).at.y _
        + (x - gr.pt(s).at.x) * gr.pt(s).dir.y / gr.pt(s).dir.x _
        - y) >= 4 then exit function

    TileSeg% = s
end function

' The platform's stretch under x, at y within 4; -1 when
' there is none.
private function PlatSeg% (gr as Ground, x as integer, y as single)
    dim s as integer

    PlatSeg% = -1

    for s = gr.plat_a to gr.n - 2
        if gr.pt(s).kind <> 1 or x < gr.pt(s).at.x _
            or x > gr.pt(s + 1).at.x _
            or abs(gr.pt(s).at.y - y) >= 4 then continue for

        PlatSeg% = s
        exit function
    next
end function

' Ground at y is under the sea for good: deeper than Charlie
' wades, at low water too.
function Sunk% (lv as Level, y as integer)
    dim ebb as integer

    Sunk% = 0
    if lv.tide_lo = NONE then exit function

    ebb = lv.tide_lo
    if lv.tide_hi > ebb then ebb = lv.tide_hi
    Sunk% = y > ebb + WADE
end function

' The ground's top at column sx near y: in y's tile, the one
' above when that is full, the one below when it is empty;
' NONE when both are, or off the level.
function Surf% (lv as Level, sx as integer, y as integer)
    dim tx as integer, ty as integer, i as integer, h as integer

    Surf% = NONE
    if sx < 0 or sx >= lv.wpx or y < TS then exit function

    tx = sx \ TS
    i = sx and 15
    ty = y \ TS
    if ty >= MH - 1 then exit function

    h = lv.prof(lv.map(tx, ty), i)

    if h = TS then
        Surf% = ty * TS - lv.prof(lv.map(tx, ty - 1), i)
    elseif h = 0 then
        h = lv.prof(lv.map(tx, ty + 1), i)
        if h then Surf% = (ty + 2) * TS - h
    else
        Surf% = (ty + 1) * TS - h
    end if
end function

' Past the level's ends a wall; above and below it air.
function Solid% (lv as Level, x as integer, y as integer)
    if x < 0 or x >= lv.wpx then
        Solid% = -1
        exit function
    end if

    if y < 0 or y >= MH * TS then
        Solid% = 0
        exit function
    end if

    Solid% = (y and 15) >= TS - lv.prof(lv.map(x \ TS, y \ TS), _
        x and 15)
end function

' Whether a ribbon starts within 60 ahead of x: its edge is
' run off, not jumped.
function Ribbon% (gr as Ground, x as integer)
    dim z as integer

    Ribbon% = 0

    for z = 0 to gr.n_cv - 1
        if gr.cv(z).far >= 0 or gr.pt(gr.cv(z).first).kind <> 2 _
            then continue for
        if gr.cv(z).x_lo <= x - 16 or gr.cv(z).x_lo >= x + 60 _
            then continue for

        Ribbon% = -1
    next
end function

' A ribbon is a strip wound once round a line along its way,
' each twist 2 * WAVEL long: its near edge rises to a crest
' and falls, and what runs it goes round its inside, on its
' far side going up and behind it coming down. How far round
' p on one is: 0 upright at its foot, PI upside down under
' its crest; -1 on none.
function Twist! (gr as Ground, p as Hero)
    dim z as integer, t as single

    Twist! = -1
    if not p.ground then exit function

    for z = 0 to gr.n_cv - 1
        if gr.cv(z).far <> -2 or p.sg < gr.cv(z).first _
            or p.sg > gr.cv(z).last then continue for

        t = (p.at.x - gr.cv(z).x_lo - 8) / (2 * WAVEL)
        if t < 0 or p.at.x > gr.cv(z).x_hi - 8 then t = 0
        Twist! = (t - int(t)) * 2 * PI
        exit function
    next
end function

' v toward 0 by f a second, not past it.
sub Slow (v as single, f as single)
    if v > 0 then
        v = v - f * STEPT
        if v < 0 then v = 0
    elseif v < 0 then
        v = v + f * STEPT
        if v > 0 then v = 0
    end if
end sub

' How the ground lies under him, of the five ways what rides
' it is drawn: -2 to 2, level 0.
function Slant% (p as Hero)
    dim t as integer

    if p.ground then t = cint(p.ang * 8 / PI)
    if t > 2 then t = 2
    if t < -2 then t = -2
    Slant% = t
end function

' How the ground lies at x near y, of the five ways what
' rides it is drawn: -2 to 2, level 0.
function Lie% (lv as Level, x as integer, y as integer)
    dim a as integer, b as integer, t as integer

    a = Surf%(lv, x - 8, y)
    b = Surf%(lv, x + 8, y)
    if a <> NONE and b <> NONE then t = (a - b) \ 8
    if t > 2 then t = 2
    if t < -2 then t = -2
    Lie% = t
end function
