' Lantern Quarter: clotheslines, lantern lines, awnings and chimneys; pouncers.
defint A-Z
option explicit

from barkley.engine.video import Blit, Store, SW
from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck
from barkley.model.images import IPAPER, ISHIRT, ISOCK, ITROUSERS
from barkley.model.hero import Hero
from barkley.model.level import Level, Rope, TS
from barkley.model.game import Game, Loose, Part, Session
from barkley.assets.palette import CLINEN, CPOLE, CQINK, CSOOT
from barkley.assets.tunes import Sfx, SROLL, SSPRING, STWANG
from barkley.stage.fx.parts import Burst
from barkley.stage.fx.damage import Tackle
from barkley.stage.hero.modes import FLUEH
from barkley.stage.zones.downs import CatPic, Prowl

' A clothesline: how far its middle hangs under its ends; his speed up off the first he comes down on, what each
' more in a row adds, and the row that adds no more; how far it gives under him, and the things' steps it swings;
' how far apart its washing is pegged.
const LINESAG = 5
const LINEV = 300!, LINEMORE = 100!, LINEROW = 3
private const LINEGIVE = 8
const LINET = 30
private const WASH = 22

' A lantern line: how far apart its lanterns hang; the speed he slides down it at, and how fast he gathers it.
private const LANTGAP = 18
const SLIDEV = 320!
private const SLIDEACC = 600!

' An awning: half its width; his speed up off it; the things' steps it stays given.
const AWNW = 22
const AWNV = 450!
const AWNT = 8

' A chimney's pot: how near its middle he must come down to go in.
const FLUER = 8

' A pouncer: how near its bin he comes before it leaps; its leap's reach, a cat's beat's, its height and things'
' steps; its state landed, and beaten.
const POUNCER = 56, POUNCEFAR = 40, POUNCEN = 20
private const POUNCEUP = 26
const POUNCEOUT = 100, POUNCEGONE = 30000

' Line r's height u along it: hung, and once trodden given where it was, swinging less each step. A lantern
' line is taut from end to end.
function LineY% (r as Rope, u as integer)
    dim y as single, d as single
    if r.fall then
        LineY% = r.y + (clng(r.fall) * u + r.w \ 2) \ r.w
        exit function
    end if
    y = r.y + LINESAG * 4! * u * (r.w - u) / r.w / r.w
    if r.t > 0 then
        d = u / r.at
        if u > r.at then d = (r.w - u) / (r.w - r.at)
        y = y + LINEGIVE * d * r.t / LINET * cos((LINET - r.t) * .7)
    end if
    LineY% = cint(y)
end function

' The lines: come down on, one throws him up, his way kept, each in a row harder than the last, up to LINEROW;
' on the ground the row is over. From under, he goes through it. On a lantern line he slides.
sub Lines (lv as Level, mu as Deck, se as Session, p as Hero)
    dim i as integer, u as integer, y as integer
    if p.ground or p.swim or p.vine then p.hops = 0
    if p.ground then Glide lv, p
    if p.ground or p.vy <= 0 or p.dead then exit sub
    for i = 0 to lv.nRope - 1
        ' a lantern line is ground, not a spring
        if lv.ropes(i).fall then continue for
        u = cint(p.x) - lv.ropes(i).x
        if u < 1 or u >= lv.ropes(i).w then continue for
        y = LineY%(lv.ropes(i), u)
        if not Trodden%(p, y) then continue for
        Pluck lv.ropes(i), mu, se, p, u, y
        exit sub
    next
end sub

' Stood on a lantern line he slides down it, faster till he goes at SLIDEV: the ground's drag does not hold him.
private sub Glide (lv as Level, p as Hero)
    dim i as integer
    for i = 0 to lv.nRope - 1
        if lv.ropes(i).fall = 0 or lv.ropes(i).sg <> p.sg then continue for
        if p.gsp < SLIDEV then p.gsp = p.gsp + SLIDEACC * STEPT
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

' A chimney that leads on: come down into its pot, he goes down it for the one it leads to. A way out takes no
' one in.
sub MeetChimney (lv as Level, mu as Deck, se as Session, p as Hero, i as integer, pt() as Part)
    if lv.ob(i).s < 1 or p.dead or abs(lv.ob(i).x - p.x) >= FLUER then exit sub
    if not Trodden%(p, lv.ob(i).y - FLUEH) then exit sub
    p.dig = lv.ob(i).s
    p.vy = 0
    Burst se, pt(), csng(lv.ob(i).x), csng(lv.ob(i).y - FLUEH), 8, 90, CSOOT
    Sfx mu, se, SROLL
end sub

' A way out's smoke, from its pot's top at x, y on the screen: three puffs, each rising, drifting and growing in
' its turn.
sub DrawSmoke (se as Session, x as integer, y as integer)
    dim f as integer, t as integer, r as integer, px as integer, py as integer
    for f = 0 to 2
        t = (se.ticks \ 4 + f * 11) and 31
        r = 1 + t \ 12
        px = x + t \ 4
        py = y - 3 - t
        line (px - r, py - r + 1)-(px + r, py + r - 1), CSOOT, BF
        line (px - r + 1, py - r)-(px + r - 1, py + r), CSOOT, BF
    next
end sub

' How high a pouncer is over the roof n steps into its leap.
private function Arc% (n as integer)
    Arc% = POUNCEUP * 4 * n * (POUNCEN - n) \ (POUNCEN * POUNCEN)
end function

' How high pouncer i is over its roof now: in its leap, on its arc.
function Leapt% (lv as Level, i as integer)
    Leapt% = 0
    if abs(lv.ob(i).s) <= POUNCEN then Leapt% = Arc%(abs(lv.ob(i).s) - 1)
end function

' A pouncer's step. In its bin, state 0, till he comes within POUNCER of it; then it leaps at him, its state one
' more than the steps of its leap made, signed the way it goes; landed, POUNCEOUT that way, it prowls as a cat of
' the Mob does; beaten, POUNCEGONE.
sub Pounce (lv as Level, i as integer, p as Hero)
    dim n as integer, d as integer
    if lv.ob(i).s = POUNCEGONE then exit sub
    if lv.ob(i).s = 0 then
        if abs(p.x - lv.ob(i).a) < POUNCER and abs(p.y - lv.ob(i).y) < 40 then Spring lv, i, p
        exit sub
    end if
    n = abs(lv.ob(i).s)
    d = sgn(lv.ob(i).s)
    if n < POUNCEN then
        lv.ob(i).x = lv.ob(i).a + d * POUNCEFAR * n \ POUNCEN
        lv.ob(i).s = d * (n + 1)
        exit sub
    end if
    lv.ob(i).s = d
    Prowl lv, i
    lv.ob(i).s = lv.ob(i).s * POUNCEOUT
end sub

' Pouncer i out of its bin, at him.
sub Spring (lv as Level, i as integer, p as Hero)
    lv.ob(i).s = 1
    if p.x < lv.ob(i).a then lv.ob(i).s = -1
end sub

' A pouncer out of its bin: rolled or charged into it is beaten, else it scratches. In its bin it is not met.
sub MeetPounce (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
    dim y as integer
    if lv.ob(i).s = 0 or lv.ob(i).s = POUNCEGONE or abs(lv.ob(i).x - cint(p.x)) >= 15 then exit sub
    y = lv.ob(i).y - Leapt%(lv, i)
    if abs(y - 6 - cint(p.y) + 13) >= 17 then exit sub
    Tackle lv, mu, se, g, p, i, 6 + Leapt%(lv, i), lo(), pt()
    if lv.ob(i).s = 0 then lv.ob(i).s = POUNCEGONE
end sub

' Pouncer i at x, y on the screen: in its leap the Mob's cat with its back up, on its arc; landed, a cat at a prowl.
sub DrawPouncer (lv as Level, art as Store, se as Session, i as integer, x as integer, y as integer)
    dim s as integer
    s = lv.ob(i).s
    if s = 0 or s = POUNCEGONE then exit sub
    if abs(s) <= POUNCEN then s = sgn(s) * 2 else s = sgn(s)
    Blit art, x - 10, y - 13 - Leapt%(lv, i), CatPic%(se, s), 20, 14
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
        y = lv.ropes(i).y - cy
        DrawPost x, y, lv.top(lv.ropes(i).x \ TS) * TS - cy
        DrawPost x + lv.ropes(i).w, y + lv.ropes(i).fall, lv.top((lv.ropes(i).x + lv.ropes(i).w) \ TS) * TS - cy
        if lv.ropes(i).fall then
            line (x, y)-(x + lv.ropes(i).w, y + lv.ropes(i).fall), CQINK
            Lanterns lv.ropes(i), art, cx, cy
            continue for
        end if
        for u = 8 to lv.ropes(i).w + 7 step 8
            if u > lv.ropes(i).w then u = lv.ropes(i).w
            y1 = LineY%(lv.ropes(i), u) - cy
            line (x + u - 8, y)-(x + u, y1), CLINEN
            y = y1
        next
        Washing lv.ropes(i), art, cx, cy
    next
end sub

' Line r's paper lanterns, one every LANTGAP hung under it.
private sub Lanterns (r as Rope, art as Store, cx as integer, cy as integer)
    dim u as integer
    for u = LANTGAP \ 2 to r.w - 6 step LANTGAP
        Blit art, r.x + u - cx - 4, LineY%(r, u) - cy, IPAPER, 9, 14
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
