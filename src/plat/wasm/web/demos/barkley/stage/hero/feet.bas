' Charlie's feet on the ground: landed on it, run along it,
' off it, against a wall, carried by a platform.
defint A-Z
option explicit

from barkley.model.level import Ground, Level, NONE, TS
from barkley.model.hero import Hero, HH, HW
from barkley.world.terrain.ground import LOOPR, SegAt, Solid, STALL
from barkley.world.terrain.ground import Surf
from barkley.world.movers import Falls

private const COYOTE = .1

' Charlie's side toward dir in a wall: pushed out, and true.
function Wall% (lv as Level, p as Hero, dir as integer)
    dim n as integer, y as integer

    y = cint(p.at.y)

    do while Solid%(lv, cint(p.at.x) + dir * HW, y - 11) _
        or Solid%(lv, cint(p.at.x) + dir * HW, y - HH + 4)
        p.at.x = p.at.x - dir
        n = n + 1
        if n > TS then exit do
    loop

    Wall% = n > 0
end function

' Falling Charlie's feet in the tiles' ground: on the line's
' stretch there, its speed along it kept.
function Land% (lv as Level, gr as Ground, p as Hero, d as integer)
    dim x as integer, y as integer, l as integer, c as integer
    dim r as integer, s as integer

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
    p.at.y = gr.pt(s).at.y _
        + (p.at.x - gr.pt(s).at.x) * gr.pt(s).dir.y / gr.pt(s).dir.x
    Settle gr, p, s, d
    Land% = -1
end function

' Down on stretch s: the speed along it kept.
private sub Settle ( _
    gr as Ground, _
    p as Hero, _
    s as integer, _
    d as integer _
)
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

' Charlie in the air, come from x0, y0, through a curve's
' stretch from its open side: down on it where it can be
' stood on, or is met fast enough to run along; else off it,
' sliding. A loop's far half is not there for one that has
' not run its near half.
function Curve% ( _
    gr as Ground, _
    p as Hero, _
    x0 as single, _
    y0 as single, _
    d as integer _
)
    dim z as integer, s as integer, a as integer, b as integer
    dim near as integer
    dim d0 as single, d1 as single, t as single, f as single

    Curve% = 0

    for z = 0 to gr.n_cv - 1
        if p.at.x < gr.cv(z).x_lo - 8 _
            or p.at.x > gr.cv(z).x_hi + 8 then continue for

        a = gr.cv(z).first
        b = gr.cv(z).last
        LoopHalf gr, p, z, x0, a, b

        for s = a to b
            d1 = (p.at.x - gr.pt(s).at.x) * gr.pt(s).dir.y _
                - (p.at.y - gr.pt(s).at.y) * gr.pt(s).dir.x
            if d1 >= 0 _
                or p.vel.x * gr.pt(s).dir.y - p.vel.y _
                * gr.pt(s).dir.x >= -1 then continue for

            d0 = (x0 - gr.pt(s).at.x) * gr.pt(s).dir.y _
                - (y0 - gr.pt(s).at.y) * gr.pt(s).dir.x
            t = (p.at.x - gr.pt(s).at.x) * gr.pt(s).dir.x _
                + (p.at.y - gr.pt(s).at.y) * gr.pt(s).dir.y
            ' from just behind it too, at a join; not behind
            ' a wall and from further behind a platform,
            ' which moves
            near = d0 > -1 + 3 * (gr.pt(s).kind = 3) _
                and (d0 >= 0 or gr.pt(s).dir.x >= .35)
            if near = 0 or t < -1 _
                or t > gr.pt(s).length + 1 then continue for

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

' Of curve z, a loop, the half that is there for him, from
' stretch a to b: its far one once he has run its near.
private sub LoopHalf ( _
    gr as Ground, _
    p as Hero, _
    z as integer, _
    x0 as single, _
    a as integer, _
    b as integer _
)
    dim far as integer

    if gr.cv(z).far < 0 then exit sub

    far = p.sg >= gr.cv(z).far
    if p.sg >= gr.plat_a then far = x0 > gr.cv(z).x_lo + LOOPR

    if far then
        a = gr.cv(z).far
    else
        b = gr.cv(z).far - 1
    end if
end sub

' Charlie d pixels along the line, over its joins: slowed in
' a sharp hollow, into the air over a sharp crest or off the
' line's end.
sub Along (gr as Ground, p as Hero, d as single)
    dim i as integer, j as integer, a as integer, t as single
    dim cr as single, dt as single

    i = p.sg
    t = (p.at.x - gr.pt(i).at.x) * gr.pt(i).dir.x _
        + (p.at.y - gr.pt(i).at.y) * gr.pt(i).dir.y + d

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
            cr = gr.pt(a).dir.x * gr.pt(a + 1).dir.y _
                - gr.pt(a).dir.y * gr.pt(a + 1).dir.x
            dt = gr.pt(a).dir.x * gr.pt(a + 1).dir.x _
                + gr.pt(a).dir.y * gr.pt(a + 1).dir.y
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

        if cr < 0 and dt < .7 then
            Crease p, t, dt, gr.pt(j).length, j > i
        end if

        i = j
    loop

    p.sg = i
    p.ang = gr.pt(i).rise
    p.at.x = gr.pt(i).at.x + gr.pt(i).dir.x * t
    p.at.y = gr.pt(i).at.y + gr.pt(i).dir.y * t
end sub

' Over a crease that turns the ground up: speed is lost by
' the turn's cosine dt, and so is how far he is past the
' crease on the new stretch, l long; ahead, the crease is at
' its start.
private sub Crease ( _
    p as Hero, _
    t as single, _
    dt as single, _
    l as single, _
    ahead as integer _
)
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

' Off the ground, its speed kept: from a wall or a roof,
' clear of it.
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

' Charlie on a moving platform: carried as far as it went;
' off it as a span falls from under him.
sub Carried (lv as Level, gr as Ground, p as Hero)
    dim m as integer

    if not p.ground then exit sub

    for m = 0 to lv.n_movers - 1
        if lv.movers(m).sg <> p.sg then continue for

        if Falls%(lv, m) then
            Drop gr, p
        else
            p.at.x = p.at.x + lv.movers(m).shift.x
            p.at.y = p.at.y + lv.movers(m).shift.y
        end if

        exit for
    next
end sub
