' Lantern Quarter: clotheslines and awnings.
defint A-Z
option explicit

from barkley.engine.video import Blit, Store, SW
from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck
from barkley.model.images import ISHIRT, ISOCK, ITROUSERS
from barkley.model.hero import Hero
from barkley.model.level import Level, Rope, TS
from barkley.model.game import Session
from barkley.assets.palette import CLINEN, CPOLE, CQINK
from barkley.assets.tunes import Sfx, SSPRING, STWANG

' A clothesline: how far its middle hangs under its ends; his speed up off the first he comes down on, what each
' more in a row adds, and the row that adds no more; how far it gives under him, and the things' steps it swings;
' how far apart its washing is pegged.
const LINESAG = 5
const LINEV = 300!, LINEMORE = 100!, LINEROW = 3
private const LINEGIVE = 8
const LINET = 30
private const WASH = 22

' An awning: half its width; his speed up off it; the things' steps it stays given.
const AWNW = 22
const AWNV = 450!
const AWNT = 8

' Line r's height u along it: hung, and once trodden given where it was, swinging less each step.
function LineY% (r as Rope, u as integer)
    dim y as single, d as single
    y = r.y + LINESAG * 4! * u * (r.w - u) / r.w / r.w
    if r.t > 0 then
        d = u / r.at
        if u > r.at then d = (r.w - u) / (r.w - r.at)
        y = y + LINEGIVE * d * r.t / LINET * cos((LINET - r.t) * .7)
    end if
    LineY% = cint(y)
end function

' The lines: come down on, one throws him up, his way kept, each in a row harder than the last, up to LINEROW;
' on the ground the row is over. From under, he goes through it.
sub Lines (lv as Level, mu as Deck, se as Session, p as Hero)
    dim i as integer, u as integer, y as integer
    if p.ground or p.swim or p.vine then p.hops = 0
    if p.ground or p.vy <= 0 or p.dead then exit sub
    for i = 0 to lv.nRope - 1
        u = cint(p.x) - lv.ropes(i).x
        if u < 1 or u >= lv.ropes(i).w then continue for
        y = LineY%(lv.ropes(i), u)
        if not Trodden%(p, y) then continue for
        Pluck lv.ropes(i), mu, se, p, u, y
        exit sub
    next
end sub

' Whether his feet, falling, have come down through height y this step.
private function Trodden% (p as Hero, y as integer)
    Trodden% = not p.ground and p.vy > 0 and p.y >= y and p.y - p.vy * STEPT <= y + 2
end function

' Thrown up from height y at v, his way kept.
private sub Toss (p as Hero, y as integer, v as single)
    p.y = y
    p.vy = -v
    p.roll = 0
    p.jumped = 0
    p.sprung = -1
end sub

' Line r trodden u along it, at height y: he is thrown up off it, and it swings.
private sub Pluck (r as Rope, mu as Deck, se as Session, p as Hero, u as integer, y as integer)
    Toss p, y, LINEV + LINEMORE * p.hops
    if p.hops < LINEROW - 1 then p.hops = p.hops + 1
    r.at = u
    r.t = LINET
    Sfx mu, se, STWANG
end sub

' An awning: come down on, its canvas throws him up, his way kept; from under he goes through it.
sub MeetAwning (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    if p.dead or abs(lv.ob(i).x - p.x) >= AWNW or not Trodden%(p, lv.ob(i).y) then exit sub
    Toss p, lv.ob(i).y, AWNV
    lv.ob(i).s = AWNT
    Sfx mu, se, SSPRING
end sub

' The lines' step: one that swings, a step nearer rest.
sub Settle (lv as Level)
    dim i as integer
    for i = 0 to lv.nRope - 1
        if lv.ropes(i).t > 0 then lv.ropes(i).t = lv.ropes(i).t - 1
    next
end sub

' The lines in view: each from post to post, its washing on it.
sub DrawLines (lv as Level, art as Store, cx as integer, cy as integer)
    dim i as integer, u as integer, x as integer, y as integer, y1 as integer
    for i = 0 to lv.nRope - 1
        x = lv.ropes(i).x - cx
        if x >= SW or x + lv.ropes(i).w < 0 then continue for
        DrawPost x, lv.ropes(i).y - cy, lv.top(lv.ropes(i).x \ TS) * TS - cy
        DrawPost x + lv.ropes(i).w, lv.ropes(i).y - cy, lv.top((lv.ropes(i).x + lv.ropes(i).w) \ TS) * TS - cy
        y = lv.ropes(i).y - cy
        for u = 8 to lv.ropes(i).w + 7 step 8
            if u > lv.ropes(i).w then u = lv.ropes(i).w
            y1 = LineY%(lv.ropes(i), u) - cy
            line (x + u - 8, y)-(x + u, y1), CLINEN
            y = y1
        next
        Washing lv.ropes(i), art, cx, cy
    next
end sub

' A line's end at x, y on the screen, the roof under it at y1: a post up from the roof, its crossbar under its
' top; where the roof is no lower, a hook in the wall.
private sub DrawPost (x as integer, y as integer, y1 as integer)
    if y1 < y + 6 then
        line (x - 1, y - 1)-(x + 1, y + 1), CQINK, BF
        exit sub
    end if
    line (x - 2, y - 3)-(x + 2, y1 - 1), CQINK, BF
    line (x - 1, y - 2)-(x + 1, y1 - 1), CPOLE + 1, BF
    line (x - 1, y - 2)-(x - 1, y1 - 1), CPOLE
    line (x + 1, y - 2)-(x + 1, y1 - 1), CPOLE + 2
    line (x - 5, y + 1)-(x + 5, y + 4), CQINK, BF
    line (x - 4, y + 2)-(x + 4, y + 3), CPOLE + 1, BF
    line (x - 4, y + 2)-(x + 4, y + 2), CPOLE
end sub

' Line r's washing, pegged every WASH along it and hung as it hangs: by where it is, a shirt, trousers, a sock,
' or a peg left bare.
private sub Washing (r as Rope, art as Store, cx as integer, cy as integer)
    dim u as integer, x as integer, y as integer
    for u = 16 to r.w - 14 step WASH
        x = r.x + u - cx
        y = LineY%(r, u) - cy - 1
        select case (r.x \ TS + (u \ WASH) * 3) mod 5
        case 0: Blit art, x - 9, y, ISHIRT, 18, 15
        case 2: Blit art, x - 6, y, ITROUSERS, 13, 17
        case 3: Blit art, x - 4, y, ISOCK, 8, 11
        end select
    next
end sub
