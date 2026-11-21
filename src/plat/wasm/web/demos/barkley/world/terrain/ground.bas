' The ground's queries: surface, wall, landing, slope, loop, ribbon; the tables they read.
defint A-Z
option explicit

from barkley.engine.clock import STEPT
from barkley.model.hero import Hero, HH, HW
from barkley.model.level import Ground, Level, MH, NONE, PI, TS

const LOOPR = 72
private const COYOTE = .1

' The ground's line: its most points, a loop's and a quarter pipe's stretches, the curves there can be; under STALL
' Charlie cannot stay on a wall or a roof.
const LOOPN = 48, STALL = 150!

const WAVEL = 192
const WADE = 10

' Each tile kind's column heights: 1 solid; 2, 3 rise and fall at 45
' degrees; 4 and 5, 6 and 7 rise and fall half as steep over two tiles.
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

' The tiles' stretch of the line under x, its top within 4 of y; -1 when there is none.
function SegAt% (lv as Level, gr as Ground, x as integer, y as single)
    dim s as integer
    SegAt% = -1
    if x < 0 or x >= lv.wpx then exit function
    s = TileSeg%(gr, x, y)
    if s < 0 then s = PlatSeg%(gr, x, y)
    SegAt% = s
end function

' The tiles' stretch under x, its top within 4 of y; -1 when there is none.
private function TileSeg% (gr as Ground, x as integer, y as single)
    dim s as integer
    TileSeg% = -1
    s = gr.col(x \ TS)
    if s < 0 then exit function
    ' past a loop that stands in the column: the stretch after it
    if x > gr.pt(s + 1).at.x then s = s + LOOPN + 1
    if abs(gr.pt(s).at.y + (x - gr.pt(s).at.x) * gr.pt(s).dir.y / gr.pt(s).dir.x - y) < 4 then TileSeg% = s
end function

' The platform's stretch under x, at y within 4; -1 when there is none.
private function PlatSeg% (gr as Ground, x as integer, y as single)
    dim s as integer
    PlatSeg% = -1
    for s = gr.platA to gr.n - 2
        if gr.pt(s).kind <> 1 or x < gr.pt(s).at.x or x > gr.pt(s + 1).at.x or abs(gr.pt(s).at.y - y) >= 4 then continue for
        PlatSeg% = s
        exit function
    next
end function

' Ground at y is under the sea for good: deeper than Charlie wades, at low water too.
function Sunk% (lv as Level, y as integer)
    dim ebb as integer
    Sunk% = 0
    if lv.tide_lo = NONE then exit function
    ebb = lv.tide_lo
    if lv.tide_hi > ebb then ebb = lv.tide_hi
    Sunk% = y > ebb + WADE
end function

' The ground's top at column sx near y: in y's tile, the one above when
' that is full, the one below when it is empty; NONE when both are, or
' off the level.
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
    Solid% = (y and 15) >= TS - lv.prof(lv.map(x \ TS, y \ TS), x and 15)
end function

' Charlie's side toward dir in a wall: pushed out, and true.
function Wall% (lv as Level, p as Hero, dir as integer)
    dim n as integer, y as integer
    y = cint(p.at.y)
    do while Solid%(lv, cint(p.at.x) + dir * HW, y - 11) or Solid%(lv, cint(p.at.x) + dir * HW, y - HH + 4)
        p.at.x = p.at.x - dir
        n = n + 1
        if n > TS then exit do
    loop
    Wall% = n > 0
end function

' Falling Charlie's feet in the tiles' ground: on the line's stretch there, its speed along it kept.
function Land% (lv as Level, gr as Ground, p as Hero, d as integer)
    dim x as integer, y as integer, l as integer, c as integer, r as integer, s as integer
    x = cint(p.at.x)
    y = cint(p.at.y)
    l = Surf%(lv, x - 6, y)
    c = Surf%(lv, x, y)
    r = Surf%(lv, x + 6, y)
    if l > y or l < y - 14 then l = NONE
    if c > y or c < y - 14 then c = NONE
    if r > y or r < y - 14 then r = NONE
    if l = NONE and c = NONE and r = NONE then
        Land% = 0
        exit function
    end if
    ' by a foot on an edge: onto the edge
    if c <> NONE then
        s = SegAt%(lv, gr, x, csng(c))
    elseif l < r then
        s = SegAt%(lv, gr, x - 6, csng(l))
        ' going off that edge, not onto it: no ground
        if p.vel.x > 0 then s = -1
    else
        s = SegAt%(lv, gr, x + 6, csng(r))
        if p.vel.x < 0 then s = -1
    end if
    if s < 0 then
        Land% = 0
        exit function
    end if
    if p.at.x < gr.pt(s).at.x then p.at.x = gr.pt(s).at.x
    if p.at.x > gr.pt(s + 1).at.x then p.at.x = gr.pt(s + 1).at.x
    p.at.y = gr.pt(s).at.y + (p.at.x - gr.pt(s).at.x) * gr.pt(s).dir.y / gr.pt(s).dir.x
    Settle gr, p, s, d
    Land% = -1
end function

' Down on stretch s: the speed along it kept.
private sub Settle (gr as Ground, p as Hero, s as integer, d as integer)
    p.sg = s
    p.ang = gr.pt(s).rise
    p.gsp = p.vel.x * gr.pt(s).dir.x + p.vel.y * gr.pt(s).dir.y
    if p.jumped and not d and not p.sled then p.roll = 0
    ' hit, it lands where it is and stands afraid a while
    if p.stun then p.gsp = 0
    p.ground = -1
    p.jumped = 0
    p.sprung = 0
    p.coyote = 0
end sub

' Charlie in the air, come from x0, y0, through a curve's stretch from its open side: down on it where it can be
' stood on, or is met fast enough to run along; else off it, sliding. A loop's far half is not there for one that
' has not run its near half.
function Curve% (gr as Ground, p as Hero, x0 as single, y0 as single, d as integer)
    dim z as integer, s as integer, a as integer, b as integer, near as integer
    dim d0 as single, d1 as single, t as single, f as single
    Curve% = 0
    for z = 0 to gr.nz - 1
        if p.at.x < gr.cv(z).l - 8 or p.at.x > gr.cv(z).r + 8 then continue for
        a = gr.cv(z).a
        b = gr.cv(z).b
        LoopHalf gr, p, z, x0, a, b
        for s = a to b
            d1 = (p.at.x - gr.pt(s).at.x) * gr.pt(s).dir.y - (p.at.y - gr.pt(s).at.y) * gr.pt(s).dir.x
            if d1 >= 0 or p.vel.x * gr.pt(s).dir.y - p.vel.y * gr.pt(s).dir.x >= -1 then continue for
            d0 = (x0 - gr.pt(s).at.x) * gr.pt(s).dir.y - (y0 - gr.pt(s).at.y) * gr.pt(s).dir.x
            t = (p.at.x - gr.pt(s).at.x) * gr.pt(s).dir.x + (p.at.y - gr.pt(s).at.y) * gr.pt(s).dir.y
            ' from just behind it too, at a join; not behind a wall
            ' and from further behind a platform, which moves
            near = d0 > -1 + 3 * (gr.pt(s).kind = 3) and (d0 >= 0 or gr.pt(s).dir.x >= .35)
            if near = 0 or t < -1 or t > gr.pt(s).length + 1 then continue for
            if t < 0 then t = 0
            if t > gr.pt(s).length then t = gr.pt(s).length
            p.at.x = gr.pt(s).at.x + gr.pt(s).dir.x * t
            p.at.y = gr.pt(s).at.y + gr.pt(s).dir.y * t
            f = p.vel.x * gr.pt(s).dir.x + p.vel.y * gr.pt(s).dir.y
            if gr.pt(s).dir.x >= .35 or abs(f) >= STALL then
                Settle gr, p, s, d
                Curve% = -1
            else
                p.at.x = p.at.x + gr.pt(s).dir.y
                p.at.y = p.at.y - gr.pt(s).dir.x
                p.vel.x = f * gr.pt(s).dir.x
                p.vel.y = f * gr.pt(s).dir.y
            end if
            exit function
        next
    next
end function

' Of curve z, a loop, the half that is there for him, from stretch a to b: its far one once he has run its near.
private sub LoopHalf (gr as Ground, p as Hero, z as integer, x0 as single, a as integer, b as integer)
    dim far as integer
    if gr.cv(z).m < 0 then exit sub
    far = p.sg >= gr.cv(z).m
    if p.sg >= gr.platA then far = x0 > gr.cv(z).l + LOOPR
    if far then
        a = gr.cv(z).m
    else
        b = gr.cv(z).m - 1
    end if
end sub

' Charlie d pixels along the line, over its joins: slowed in a sharp hollow, into the air over a sharp crest or
' off the line's end.
sub Along (gr as Ground, p as Hero, d as single)
    dim i as integer, j as integer, a as integer, t as single, cr as single, dt as single
    i = p.sg
    t = (p.at.x - gr.pt(i).at.x) * gr.pt(i).dir.x + (p.at.y - gr.pt(i).at.y) * gr.pt(i).dir.y + d
    do
        if t > gr.pt(i).length then
            j = i + 1
            a = i
        elseif t < 0 then
            j = i - 1
            a = j
        else
            exit do
        end if
        if j >= 0 then if gr.pt(j).kind = 0 then j = -1
        if j >= 0 then
            cr = gr.pt(a).dir.x * gr.pt(a + 1).dir.y - gr.pt(a).dir.y * gr.pt(a + 1).dir.x
            dt = gr.pt(a).dir.x * gr.pt(a + 1).dir.x + gr.pt(a).dir.y * gr.pt(a + 1).dir.y
            if cr > 0 and dt < .64 then j = -1
        end if
        if j < 0 then
            if t >= 0 then t = gr.pt(i).length
            if t < 0 then t = 0
            p.at.x = gr.pt(i).at.x + gr.pt(i).dir.x * t
            p.at.y = gr.pt(i).at.y + gr.pt(i).dir.y * t
            Drop gr, p
            exit sub
        end if
        if t < 0 then
            t = t + gr.pt(j).length
        else
            t = t - gr.pt(i).length
        end if
        if cr < 0 and dt < .7 then Crease p, t, dt, gr.pt(j).length, j > i
        i = j
    loop
    p.sg = i
    p.ang = gr.pt(i).rise
    p.at.x = gr.pt(i).at.x + gr.pt(i).dir.x * t
    p.at.y = gr.pt(i).at.y + gr.pt(i).dir.y * t
end sub

' Over a crease that turns the ground up: speed is lost by the turn's cosine dt, and so is how far he is past the
' crease on the new stretch, l long; ahead, the crease is at its start.
private sub Crease (p as Hero, t as single, dt as single, l as single, ahead as integer)
    dim k as single
    k = dt
    if k < 0 then k = 0
    p.gsp = p.gsp * k
    if ahead then
        t = t * k
    else
        t = l - (l - t) * k
    end if
end sub

' Off the ground, its speed kept: from a wall or a roof, clear of it.
sub Drop (gr as Ground, p as Hero)
    dim i as integer
    i = p.sg
    p.ground = 0
    p.vel.x = p.gsp * gr.pt(i).dir.x
    p.vel.y = p.gsp * gr.pt(i).dir.y
    if gr.pt(i).dir.x > .7 then
        p.coyote = COYOTE
    elseif gr.pt(i).dir.x < .35 then
        p.at.x = p.at.x + 2 * gr.pt(i).dir.y
        p.at.y = p.at.y - 2 * gr.pt(i).dir.x
    end if
end sub

' Whether a ribbon starts within 60 ahead of x: its edge is run off, not jumped.
function Ribbon% (gr as Ground, x as integer)
    dim z as integer
    Ribbon% = 0
    for z = 0 to gr.nz - 1
        if gr.cv(z).m < 0 and gr.pt(gr.cv(z).a).kind = 2 and gr.cv(z).l > x - 16 and gr.cv(z).l < x + 60 then Ribbon% = -1
    next
end function

' A ribbon is a strip wound once round a line along its way, each twist 2 * WAVEL long: its near edge rises to a
' crest and falls, and what runs it goes round its inside, on its far side going up and behind it coming down.
' How far round p on one is: 0 upright at its foot, PI upside down under its crest; -1 on none.
function Twist! (gr as Ground, p as Hero)
    dim z as integer, t as single
    Twist! = -1
    if not p.ground then exit function
    for z = 0 to gr.nz - 1
        if gr.cv(z).m <> -2 or p.sg < gr.cv(z).a or p.sg > gr.cv(z).b then continue for
        t = (p.at.x - gr.cv(z).l - 8) / (2 * WAVEL)
        if t < 0 or p.at.x > gr.cv(z).r - 8 then t = 0
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

' Charlie stopped r from x, on the side of it he is.
sub Balk (p as Hero, x as integer, r as integer)
    dim s as integer
    s = 1 + 2 * (p.at.x < x)
    p.at.x = x + s * r
    if p.ground then
        if p.gsp * s < 0 then p.gsp = 0
    elseif p.vel.x * s < 0 then
        p.vel.x = 0
    end if
end sub

' How the ground lies under him, of the five ways what rides it is drawn: -2 to 2, level 0.
function Slant% (p as Hero)
    dim t as integer
    if p.ground then t = cint(p.ang * 8 / PI)
    if t > 2 then t = 2
    if t < -2 then t = -2
    Slant% = t
end function

' How the ground lies at x near y, of the five ways what rides it is drawn: -2 to 2, level 0.
function Lie% (lv as Level, x as integer, y as integer)
    dim a as integer, b as integer, t as integer
    a = Surf%(lv, x - 8, y)
    b = Surf%(lv, x + 8, y)
    if a <> NONE and b <> NONE then t = (a - b) \ 8
    if t > 2 then t = 2
    if t < -2 then t = -2
    Lie% = t
end function
