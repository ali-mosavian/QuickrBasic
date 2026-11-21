' Saltwick Sands: the sea and its tide, crabs, jellyfish,
' gulls and the flume.
defint A-Z
option explicit

from engine.video import Blit, SH, Store, SW
from engine.events import Events
from barkley.model.images import IBONE, ICRAB, IGULL, IRUNNEL, ISPOUT
from barkley.model.images import ISURF
from barkley.model.hero import Hero
from barkley.model.level import Level, NONE, PI, TS
from barkley.model.kinds import Kinds
from barkley.model.game import Game, Loose, Part, Session
from barkley.model.events import EVSOUND, Tell
from barkley.assets.palette import CSEA
from barkley.assets.tunes import SBOOST, SHURT, SSPRING
from barkley.stage.fx.damage import Beaten, BOUNCEV
from barkley.stage.hero.push import Rebound, Shoot
from barkley.stage.things.react import Rest, SHELLT, Turn
from barkley.stage.things.sands import GDIVE
from barkley.stage.hero.hurt import Hurt

const FLUMEV = 460!
const STEPV = 430!

' How fast Charlie goes up off a jellyfish's bell, and how
' far under the sea's top its arms' ends float.
const JELLYV = 480!
' The tide: the steps of its turn; how deep Charlie floats,
' and how deep he wades before he does; his paddling speed
' and its gain; his hop out of the water; the seconds of
' rolling that shake him dry, and his top speed till then;
' the first of the sea's colours.
const TIDEP = 1440

' Down a flume: swept off his feet, curled, and no slower
' than its water runs.
private sub Slide (ev as Events, se as Session, p as Hero)
    if not p.roll then Tell ev, EVSOUND, SBOOST
    Shoot p, FLUMEV
end sub

' A flume's tile or a spout's: stood in its water, he
' slides.
sub MeetFlume ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    p as Hero, _
    i as integer _
)
    dim px as integer, py as integer

    px = cint(p.at.x)
    py = cint(p.at.y)
    if not p.ground or px < lv.things(i).at.x _
        or px >= lv.things(i).at.x + TS then exit sub

    if py <= lv.things(i).at.y - 6 _
        or py >= lv.things(i).at.y + TS + 6 then exit sub

    Slide ev, se, p
end sub

' Charlie at crab i. Down on its back he goes up off it,
' higher off its shell, and it ducks in a while; at its side
' its claws pinch, even a rolling Charlie. In its shell it
' pinches nobody, and a roll knocks it off the sand.
sub MeetCrab ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    lo() as Loose _
)
    dim y as integer, v as single

    y = lv.things(i).at.y
    if lv.things(i).state = 0 _
        or abs(lv.things(i).at.x - p.at.x) >= 15 or p.at.y < y - 22 _
        or p.at.y > y + 10 then exit sub

    if not p.ground and p.vel.y > 0 and p.at.y < y - 4 then
        v = BOUNCEV
        if abs(lv.things(i).state) > 1 then v = STEPV
        Rebound p, v, 0

        if abs(lv.things(i).state) < SHELLT \ 3 then
            Turn lv, i, sgn(lv.things(i).state) * (SHELLT \ 3)
        end if

        Tell ev, EVSOUND, SSPRING
    elseif abs(lv.things(i).state) = 1 then
        Hurt ev, se, g, p, lv.things(i).at.x, lo(), SHURT
    elseif p.roll or p.charging then
        Beaten lv, ev, i, 6
    end if
end sub

' The gulls, a bone under one that carries any; wings still
' in a dive.
sub DrawGulls ( _
    lv as Level, _
    art as Store, _
    se as Session, _
    cx as integer, _
    cy as integer _
)
    dim i as integer, x as integer, y as integer, k as integer

    for i = 0 to lv.n_gulls - 1
        x = cint(lv.gulls(i).at.x) - cx
        y = cint(lv.gulls(i).at.y) - cy
        if lv.gulls(i).st <= 0 or x <= -20 _
            or x >= SW + 20 then continue for

        k = IGULL - 4 * lv.gulls(i).face

        if lv.gulls(i).st <> GDIVE then
            k = k + ((se.ticks \ 8) and 1) * 2
        end if

        Blit art, x - 13, y - 10, k, 26, 20
        if lv.gulls(i).loot then Blit art, x - 5, y + 3, IBONE, 10, 10
    next
end sub

' Where a thing afloat at x lies: h under the sea's top, or
' up over the ground there when the sea is lower.
function Bob% (lv as Level, x as integer, h as integer, up as integer)
    dim y as integer

    y = lv.top(x \ TS) * TS - up
    if lv.tide <> NONE and lv.tide + h < y then y = lv.tide + h
    Bob% = y
end function

' Charlie at jellyfish i. Down on its bell he is thrown
' high, and it stings nobody while it is flat; his feet in
' it, afloat or not, it stings: not a Charlie on a boat's
' deck over it.
sub MeetJelly ( _
    lv as Level, _
    kd as Kinds, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    lo() as Loose, _
    pt() as Part _
)
    dim y as integer

    y = lv.things(i).at.y
    if p.inv > 0 or lv.things(i).state > 1 _
        or abs(lv.things(i).at.x - p.at.x) >= 12 or p.at.y < y - 26 _
        or p.at.y > y + 8 then exit sub

    if not p.ground and not p.swim and p.vel.y > 0 _
        and p.at.y < y - 10 then
        Rebound p, JELLYV, -1
        Rest lv, kd, i
        Tell ev, EVSOUND, SSPRING
    elseif p.at.y > y - 14 then
        p.swim = 0
        Hurt ev, se, g, p, lv.things(i).at.x, lo(), SHURT
    end if
end sub

' The sea's level by the clock: from where the level began
' it to its other end and back, every TIDEP steps.
sub Flow (lv as Level, se as Session)
    lv.tide = lv.tide_lo _
        - cint((lv.tide_lo - lv.tide_hi) _
        * (1 - cos((se.ticks - lv.tide_at) * 2 * PI / TIDEP)) / 2)
end sub

' The sea in each column whose ground is under it: bands of
' water from its surface down behind the column's top tile,
' a run of columns alike at a time.
sub DrawSea (lv as Level, cx as integer, cy as integer)
    dim tx as integer, tx0 as integer, tx1 as integer, y0 as integer
    dim y1 as integer, x0 as integer, x1 as integer

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

' Rows a to b of a band of the sea, of colour c, no lower
' than row last or the screen's foot.
private sub SeaBand ( _
    x0 as integer, _
    x1 as integer, _
    a as integer, _
    b as integer, _
    last as integer, _
    c as integer _
)
    dim y0 as integer, y1 as integer

    y0 = a
    y1 = b
    if y0 < 0 then y0 = 0
    if y1 > last then y1 = last
    if y1 > SH - 1 then y1 = SH - 1
    if y0 <= y1 then line (x0, y0)-(x1, y1), c, BF
end sub

' The water's edge over what is in it: foam and the first of
' the water, in each column of the sea deep enough for them;
' in a shallower one the foam's line alone, and on a slope
' nothing.
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
            line (tx * TS - cx, y + 1)-(tx * TS - cx + TS - 1, _
                y + 1), CSEA, BF
        end if
    next
end sub

' A flume's end at x, y on the screen, over column c: the
' water falling from it to the sea, or to the floor of a dry
' pool, then its trough.
sub DrawSpout ( _
    lv as Level, _
    art as Store, _
    se as Session, _
    x as integer, _
    y as integer, _
    c as integer, _
    cy as integer _
)
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

' A crab's picture by its state: in its shell, or a frame of
' its scuttle.
function CrabPic% (se as Session, s as integer)
    if abs(s) > 1 then
        CrabPic% = ICRAB + 4
    else
        CrabPic% = ICRAB + ((se.ticks \ 8) and 1) * 2
    end if
end function
