' Saltwick Sands: the sea and its tide, crabs, jellyfish, gulls and the flume.
defint A-Z
option explicit

from barkley.engine.video import Blit, SH, Store, SW
from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck
from barkley.model.images import IBONE, ICRAB, IGULL, IRUNNEL, ISPOUT, ISURF
from barkley.model.hero import Hero
from barkley.model.level import Bird, Level, NONE, PI, TS
from barkley.model.game import Game, Loose, NLOOSE, Part, Session
from barkley.assets.palette import CSEA, CWHITE
from barkley.assets.tunes import SBOOST, Sfx, SHURT, SPOP, SSPRING, SSQUAWK
from barkley.stage.fx.parts import Burst
from barkley.stage.fx.damage import Beaten, BOUNCEV, Hurt
from barkley.stage.hero.moves import GSCARED, SHELLT

const FLUMEV = 460!
const STEPV = 430!

' How fast Charlie goes up off a jellyfish's bell, and how far under the sea's top its arms' ends float.
const JELLYV = 480!

private const GULLSEE = 100, DIVEV = 210!, CLIMBV = 90!, GULLREST = 2.5, GULLTAKE = 5
const GHOVER = 1, GDIVE = 2, GCLIMB = 3

' The tide: the steps of its turn; how deep Charlie floats, and how deep he wades before he does; his paddling
' speed and its gain; his hop out of the water; the seconds of rolling that shake him dry, and his top speed
' till then; the first of the sea's colours.
const TIDEP = 1440

' Down a flume: swept off his feet, curled, and no slower than its water runs.
private sub Slide (mu as Deck, se as Session, p as Hero)
    if not p.roll then Sfx mu, se, SBOOST
    p.roll = -1
    p.face = 0
    if p.gsp < FLUMEV then p.gsp = FLUMEV
end sub

' A flume's tile or a spout's: stood in its water, he slides.
sub MeetFlume (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    dim px as integer, py as integer
    px = cint(p.at.x)
    py = cint(p.at.y)
    if not p.ground or px < lv.ob(i).x or px >= lv.ob(i).x + TS then exit sub
    if py > lv.ob(i).y - 6 and py < lv.ob(i).y + TS + 6 then Slide mu, se, p
end sub

' Charlie at crab i. Down on its back he goes up off it, higher off its shell, and it ducks in a while; at its
' side its claws pinch, even a rolling Charlie. In its shell it pinches nobody, and a roll knocks it off the sand.
sub MeetCrab (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
    dim y as integer
    y = lv.ob(i).y
    if lv.ob(i).s = 0 or abs(lv.ob(i).x - p.at.x) >= 15 or p.at.y < y - 22 or p.at.y > y + 10 then exit sub
    if not p.ground and p.vel.y > 0 and p.at.y < y - 4 then
        p.vel.y = -BOUNCEV
        if abs(lv.ob(i).s) > 1 then p.vel.y = -STEPV
        p.jumped = 0
        if abs(lv.ob(i).s) < SHELLT \ 3 then lv.ob(i).s = sgn(lv.ob(i).s) * (SHELLT \ 3)
        Sfx mu, se, SSPRING
    elseif abs(lv.ob(i).s) = 1 then
        Hurt mu, se, g, p, lv.ob(i).x, lo(), SHURT
    elseif p.roll or p.charging then
        Beaten lv, mu, se, g, pt(), i, 6
    end if
end sub

' Gull b toward x, y at v: there, or nearer. Whether it is there.
private function Glide% (b as Bird, x as single, y as single, v as single)
    dim dx as single, dy as single, d as single
    dx = x - b.at.x
    dy = y - b.at.y
    d = sqr(dx * dx + dy * dy)
    if dx <> 0 then b.face = dx < 0
    if d <= v * STEPT then
        b.at.x = x
        b.at.y = y
        Glide% = -1
    else
        b.at.x = b.at.x + dx / d * v * STEPT
        b.at.y = b.at.y + dy / d * v * STEPT
        Glide% = 0
    end if
end function

' Gull b lets go of the bones it carries: they fall loose, to be picked up.
private sub Spill (se as Session, b as Bird, lo() as Loose)
    dim i as integer
    for i = 0 to NLOOSE - 1
        if b.loot = 0 then exit for
        if lo(i).t > 0 then continue for
        lo(i).at.x = b.at.x
        lo(i).at.y = b.at.y
        lo(i).vel.x = (b.loot * 37 mod 90) - 45
        lo(i).vel.y = -40
        lo(i).t = 4
        se.loose_n = se.loose_n + 1
        b.loot = b.loot - 1
    next
    b.loot = 0
end sub

' Gull b, the n'th, for a step. It hovers about its home till Charlie, with bones, passes under: then dives at
' where he will be, takes bones if it reaches him, and climbs home with them, to dive no more. A bark scares it
' home and makes it drop what it took; a rolling Charlie knocks it out of the air.
private sub Gull (mu as Deck, se as Session, b as Bird, n as integer, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim x as single, y as single, lead as single
    x = b.home.x + 24 * sin(se.ticks * .02 + n * 2)
    y = b.home.y + 4 * sin(se.ticks * .07 + n)
    select case b.st
    case GHOVER
        b.face = cos(se.ticks * .02 + n * 2) < 0
        b.at.x = x
        b.at.y = y
        if b.rest > 0 then b.rest = b.rest - STEPT
        if b.rest <= 0 and b.loot = 0 and g.bones > 0 and abs(p.at.x - x) < GULLSEE and p.at.y > y + 30 then
            lead = (p.at.y - 13 - y) / DIVEV * 60
            b.aim.x = p.at.x + (p.at.x - p.last_x) * lead
            b.aim.y = p.at.y - 13
            b.st = GDIVE
            Sfx mu, se, SSQUAWK
        end if
    case GDIVE: if Glide%(b, b.aim.x, b.aim.y, DIVEV) then b.st = GCLIMB
    case GCLIMB
        if Glide%(b, x, y, CLIMBV) then
            b.st = GHOVER
            b.rest = GULLREST
        end if
    case GSCARED
        Spill se, b, lo()
        b.st = GCLIMB
    end select
    if abs(b.at.x - p.at.x) >= 13 or abs(b.at.y - p.at.y + 13) >= 15 then exit sub
    if p.roll or p.charging then
        Spill se, b, lo()
        b.st = 0
        g.score = g.score + 100
        Burst se, pt(), b.at.x, b.at.y, 10, 130, CWHITE
        Sfx mu, se, SPOP
    elseif b.st = GDIVE then
        b.loot = g.bones
        if b.loot > GULLTAKE then b.loot = GULLTAKE
        g.bones = g.bones - b.loot
        b.st = GCLIMB
        Sfx mu, se, SSQUAWK
    end if
end sub

sub Flock (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim i as integer
    for i = 0 to lv.nGulls - 1
        if lv.gulls(i).st > 0 then Gull mu, se, lv.gulls(i), i, g, p, lo(), pt()
    next
end sub

' The gulls, a bone under one that carries any; wings still in a dive.
sub DrawGulls (lv as Level, art as Store, se as Session, cx as integer, cy as integer)
    dim i as integer, x as integer, y as integer, k as integer
    for i = 0 to lv.nGulls - 1
        x = cint(lv.gulls(i).at.x) - cx
        y = cint(lv.gulls(i).at.y) - cy
        if lv.gulls(i).st <= 0 or x <= -20 or x >= SW + 20 then continue for
        k = IGULL - 4 * lv.gulls(i).face
        if lv.gulls(i).st <> GDIVE then k = k + ((se.ticks \ 8) and 1) * 2
        Blit art, x - 13, y - 10, k, 26, 20
        if lv.gulls(i).loot then Blit art, x - 5, y + 3, IBONE, 10, 10
    next
end sub

' Where a thing afloat at x lies: h under the sea's top, or up over the ground there when the sea is lower.
function Bob% (lv as Level, x as integer, h as integer, up as integer)
    dim y as integer
    y = lv.top(x \ TS) * TS - up
    if lv.tide <> NONE and lv.tide + h < y then y = lv.tide + h
    Bob% = y
end function

' Charlie at jellyfish i. Down on its bell he is thrown high, and it stings nobody while it is flat; his feet in
' it, afloat or not, it stings: not a Charlie on a boat's deck over it.
sub MeetJelly (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
    dim y as integer
    y = lv.ob(i).y
    if p.inv > 0 or lv.ob(i).s > 1 or abs(lv.ob(i).x - p.at.x) >= 12 or p.at.y < y - 26 or p.at.y > y + 8 then exit sub
    if not p.ground and not p.swim and p.vel.y > 0 and p.at.y < y - 10 then
        p.vel.y = -JELLYV
        p.jumped = 0
        p.sprung = -1
        lv.ob(i).s = 10
        Sfx mu, se, SSPRING
    elseif p.at.y > y - 14 then
        p.swim = 0
        Hurt mu, se, g, p, lv.ob(i).x, lo(), SHURT
    end if
end sub

' The sea's level by the clock: from where the level began it to its other end and back, every TIDEP steps.
sub Flow (lv as Level, se as Session)
    lv.tide = lv.tideLo - cint((lv.tideLo - lv.tideHi) * (1 - cos((se.ticks - lv.tideAt) * 2 * PI / TIDEP)) / 2)
end sub

' The sea in each column whose ground is under it: bands of water from its surface down behind the column's top
' tile, a run of columns alike at a time.
sub DrawSea (lv as Level, cx as integer, cy as integer)
    dim tx as integer, tx0 as integer, tx1 as integer, y0 as integer, y1 as integer, x0 as integer, x1 as integer
    y0 = lv.tide - cy
    if y0 >= SH then exit sub
    tx1 = cx \ TS + SW \ TS
    if tx1 > lv.w - 1 then tx1 = lv.w - 1
    tx = cx \ TS
    do while tx <= tx1
        if lv.top(tx) * TS > lv.tide then
            tx0 = tx
            do while tx < tx1 and lv.top(tx + 1) = lv.top(tx0)
                tx = tx + 1
            loop
            y1 = (lv.top(tx0) + 1) * TS - 1 - cy
            x0 = tx0 * TS - cx
            x1 = tx * TS + TS - 1 - cx
            SeaBand x0, x1, y0, y0 + 1, y1, CSEA + 1
            SeaBand x0, x1, y0 + 2, y0 + 7, y1, CSEA + 2
            SeaBand x0, x1, y0 + 8, y0 + 25, y1, CSEA + 3
            SeaBand x0, x1, y0 + 26, SH - 1, y1, CSEA + 4
        end if
        tx = tx + 1
    loop
end sub

' Rows a to b of a band of the sea, of colour c, no lower than row last or the screen's foot.
private sub SeaBand (x0 as integer, x1 as integer, a as integer, b as integer, last as integer, c as integer)
    dim y0 as integer, y1 as integer
    y0 = a
    y1 = b
    if y0 < 0 then y0 = 0
    if y1 > last then y1 = last
    if y1 > SH - 1 then y1 = SH - 1
    if y0 <= y1 then line (x0, y0)-(x1, y1), c, BF
end sub

' The water's edge over what is in it: foam and the first of the water, in each column of the sea deep enough
' for them; in a shallower one the foam's line alone, and on a slope nothing.
sub DrawSurf (lv as Level, art as Store, cx as integer, cy as integer)
    dim tx as integer, tx1 as integer, y as integer, deep as integer
    y = lv.tide - cy - 1
    if y >= SH or y < -8 then exit sub
    tx1 = cx \ TS + SW \ TS
    if tx1 > lv.w - 1 then tx1 = lv.w - 1
    for tx = cx \ TS to tx1
        deep = lv.top(tx) * TS - lv.tide
        if deep <= 0 or lv.vis(tx) > 2 then continue for
        if deep >= 7 then
            Blit art, tx * TS - cx, y, ISURF, TS, 8
        else
            line (tx * TS - cx, y + 1)-(tx * TS - cx + TS - 1, y + 1), CSEA, BF
        end if
    next
end sub

' A flume's end at x, y on the screen, over column c: the water falling from it to the sea, or to the floor of a
' dry pool, then its trough.
sub DrawSpout (lv as Level, art as Store, se as Session, x as integer, y as integer, c as integer, cy as integer)
    dim e as integer, yy as integer, k as integer
    k = ((se.ticks \ 3) and 3) * 2
    e = lv.top(c + 1) * TS
    if lv.tide <> NONE and lv.tide < e then e = lv.tide + 2
    e = e - cy
    if e > y then
        Blit art, x + TS - 1, y - 8, IRUNNEL + k, 5, 8
        for yy = e - 8 to y step -8
            Blit art, x + TS - 1, yy, IRUNNEL + k, 5, 8
        next
    end if
    Blit art, x, y - 8, ISPOUT + k, TS, 13
end sub

' A crab's picture by its state: in its shell, or a frame of its scuttle.
function CrabPic% (se as Session, s as integer)
    if abs(s) > 1 then
        CrabPic% = ICRAB + 4
    else
        CrabPic% = ICRAB + ((se.ticks \ 8) and 1) * 2
    end if
end function
