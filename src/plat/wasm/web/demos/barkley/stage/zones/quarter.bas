' Lantern Quarter: clotheslines, lantern lines, awnings,
' chimneys and mortars; pouncers, rope-walkers and
' snatchers.
defint A-Z
option explicit

from engine.video import Blit, DrawLine, FillBox, Store, SW
from engine.clock import STEPT
from engine.events import Events
from barkley.model.images import IBURST, ICAT, ICATF, IFLOWER, IMORTAR
from barkley.model.images import IPAPER, ISHIRT, ISNATCH, ISOCK, ITALL
from barkley.model.images import ITIPPER, ITROUSERS
from barkley.model.hero import Hero
from barkley.model.level import KTIPPER, Level, NONE, Rope, TS
from barkley.model.kinds import Kinds
from barkley.model.game import Game, Loose, PULSES, Session
from barkley.model.events import EVBURST, EVSOUND, Tell, TellAt
from barkley.assets.palette import CLANT, CLINEN, CPOLE, CQINK, CSOOT
from barkley.assets.palette import CWHITE
from barkley.assets.tunes import SHURT, SROLL, SSPRING, STWANG
from barkley.stage.fx.parts import PSOOT
from barkley.stage.fx.damage import Beaten, Tackle
from barkley.stage.hero.modes import FLUEH
from barkley.stage.zones.downs import CatPic
from barkley.stage.things.react import Rest, Turn
from barkley.stage.hero.push import Descend, Gather, Hop, Toss
from barkley.stage.things.quarter import BLASTW, Fired, FLASHP, Leapt
from barkley.stage.things.quarter import LINEMORE, LINEROW, LINEV
from barkley.stage.things.quarter import LineY, MORTARBAR, MORTARH
from barkley.stage.things.quarter import MORTARREACH, MORTARV, MORTARW
from barkley.stage.things.quarter import PotFall, POTR, POTX
from barkley.stage.things.quarter import POUNCEGONE, POUNCEN, SHELLUP
from barkley.stage.things.quarter import SILLUP, Twang, WASH
from barkley.stage.hero.hurt import Hurt

' A lantern line: how far apart its lanterns hang; the speed
' he slides down it at, and how fast he gathers it.
private const LANTGAP = 18
const SLIDEV = 320!
private const SLIDEACC = 600!

' An awning: half its width; his speed up off it; the
' things' steps it stays given.
const AWNW = 22
const AWNV = 450!
const AWNT = 8

' A chimney's pot: how near its middle he must come down to
' go in.
const FLUER = 8

' The lines: come down on, one throws him up, his way kept,
' each in a row harder than the last, up to LINEROW; on the
' ground the row is over. From under, he goes through it. On
' a lantern line he slides.
sub Lines (lv as Level, ev as Events, se as Session, p as Hero)
    dim i as integer, u as integer, y as integer

    if p.ground or p.swim or p.vine then p.hops = 0
    if p.ground then Glide lv, p
    if p.ground or p.vel.y <= 0 or p.dead then exit sub

    for i = 0 to lv.n_ropes - 1
        ' a lantern line is ground, not a spring
        if lv.ropes(i).fall then continue for

        u = cint(p.at.x) - lv.ropes(i).at.x
        if u < 1 or u >= lv.ropes(i).w then continue for

        y = LineY%(lv.ropes(i), u)
        if not Trodden%(p, y) then continue for

        Pluck lv.ropes(i), ev, se, p, u, y

        ' the cat walking it is shaken off
        if lv.ropes(i).cat then
            Shaken lv, ev, lv.ropes(i).cat - 1
        end if

        exit sub
    next
end sub

' Stood on a lantern line he slides down it, faster till he
' goes at SLIDEV: the ground's drag does not hold him.
private sub Glide (lv as Level, p as Hero)
    dim i as integer

    for i = 0 to lv.n_ropes - 1
        if lv.ropes(i).fall = 0 _
            or lv.ropes(i).sg <> p.sg then continue for

        Gather p, SLIDEV, SLIDEACC
        exit sub
    next
end sub

' Whether his feet, falling, have come down through height y
' this step.
private function Trodden% (p as Hero, y as integer)
    Trodden% = not p.ground and p.vel.y > 0 and p.at.y >= y _
        and p.at.y - p.vel.y * STEPT <= y + 2
end function

' Line r trodden u along it, at height y: he is thrown up
' off it, and it swings.
private sub Pluck ( _
    r as Rope, _
    ev as Events, _
    se as Session, _
    p as Hero, _
    u as integer, _
    y as integer _
)
    Toss p, y, LINEV + LINEMORE * p.hops
    Hop p, LINEROW
    Twang r, u
    Tell ev, EVSOUND, STWANG
end sub

' An awning: come down on, its canvas throws him up, his way
' kept; from under he goes through it.
sub MeetAwning ( _
    lv as Level, _
    kd as Kinds, _
    ev as Events, _
    se as Session, _
    p as Hero, _
    i as integer _
)
    if p.dead or abs(lv.things(i).at.x - p.at.x) >= AWNW _
        or not Trodden%(p, lv.things(i).at.y) then exit sub

    Toss p, lv.things(i).at.y, AWNV
    Rest lv, kd, i
    Tell ev, EVSOUND, SSPRING
end sub

' A chimney that leads on: come down into its pot, he goes
' down it for the one it leads to. A way out takes no one
' in.
sub MeetChimney (lv as Level, ev as Events, p as Hero, i as integer)
    if lv.things(i).state < 1 or p.dead _
        or abs(lv.things(i).at.x - p.at.x) >= FLUER then exit sub
    if not Trodden%(p, lv.things(i).at.y - FLUEH) then exit sub

    Descend p, lv.things(i).state
    TellAt ev, EVBURST, csng(lv.things(i).at.x), _
        csng(lv.things(i).at.y - FLUEH), PSOOT
    Tell ev, EVSOUND, SROLL
end sub

' A way out's smoke, from its pot's top at x, y on the
' screen: three puffs, each rising, drifting and growing in
' its turn.
sub DrawSmoke ( _
    art as Store, _
    se as Session, _
    x as integer, _
    y as integer _
)
    dim f as integer, t as integer, r as integer, px as integer
    dim py as integer

    for f = 0 to 2
        t = (se.ticks \ 4 + f * 11) and 31
        r = 1 + t \ 12
        px = x + t \ 4
        py = y - 3 - t
        FillBox art, px - r, py - r + 1, px + r, py + r - 1, CSOOT
        FillBox art, px - r + 1, py - r, px + r - 1, py + r, CSOOT
    next
end sub

' A pouncer out of its bin: rolled or charged into it is
' beaten, else it scratches. In its bin it is not met.
sub MeetPounce ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    lo() as Loose _
)
    dim y as integer

    if lv.things(i).state = 0 or lv.things(i).state = POUNCEGONE _
        or abs(lv.things(i).at.x - cint(p.at.x)) >= 15 then exit sub

    y = lv.things(i).at.y - Leapt%(lv, i)
    if abs(y - 6 - cint(p.at.y) + 13) >= 17 then exit sub

    Tackle lv, ev, se, g, p, i, 6 + Leapt%(lv, i), lo()
    if lv.things(i).state = 0 then Turn lv, i, POUNCEGONE
end sub

' Pouncer i at x, y on the screen: in its leap the Mob's cat
' with its back up, on its arc; landed, a cat at a prowl.
sub DrawPouncer ( _
    lv as Level, _
    art as Store, _
    se as Session, _
    i as integer, _
    x as integer, _
    y as integer _
)
    dim s as integer

    s = lv.things(i).state
    if s = 0 or s = POUNCEGONE then exit sub

    if abs(s) <= POUNCEN then s = sgn(s) * 2 else s = sgn(s)
    Blit art, x - 10, y - 13 - Leapt%(lv, i), CatPic%(se, s), 20, 14
end sub

' For the pilot at x: the nearest tipper whose pot's place
' he is under or coming to, NONE with none near.
function TipperAhead% (lv as Level, x as integer)
    dim i as integer, c as integer

    TipperAhead% = NONE
    c = (x - 64) \ TS
    if c < 0 then c = 0

    for i = lv.first(c) to lv.n_things - 1
        if lv.things(i).at.x > x + 200 then exit for
        if lv.things(i).kind <> KTIPPER _
            or lv.things(i).at.x + POTX + POTR _
            + 8 < x then continue for

        TipperAhead% = i
        exit for
    next
end function

' Charlie at tipper i: its pot on its way down hurts whom it
' comes to.
sub MeetTipper ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    lo() as Loose _
)
    dim d as integer, foot as integer

    d = PotFall%(lv, se, i)
    if d = NONE _
        or abs(lv.things(i).at.x _
        + POTX - cint(p.at.x)) >= POTR then exit sub

    foot = lv.things(i).at.y - SILLUP + d

    if foot <= p.at.y - 28 or foot - 14 >= p.at.y then exit sub

    Hurt ev, se, g, p, lv.things(i).at.x + POTX, lo(), SHURT
end sub

' Tipper i, its house's middle at x and its foot at y on the
' screen: the house; its cat at the upper window, a paw out
' the beat before its pot goes; the pot in the air, or on
' the sill from two beats after it went.
sub DrawTipper ( _
    lv as Level, _
    art as Store, _
    se as Session, _
    i as integer, _
    x as integer, _
    y as integer _
)
    dim q as long, d as integer

    q = Fired&(lv, se, i)
    d = PotFall%(lv, se, i)
    Blit art, x - 32, y - 126, ITALL, 64, 128
    Blit art, x - 13, y - 83, ITIPPER - 2 * (q >= 3& * PULSES), 17, 13
    if d = NONE and q < 2& * PULSES then exit sub

    if d = NONE then d = 0
    Blit art, x + 1, y - 85 + d, IFLOWER, 12, 16
end sub

' A mortar: at the tick it fires, who is in the air over its
' mouth is thrown up, his way kept; while its flash burns,
' who is beside it is burnt.
sub MeetMortar ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    lo() as Loose _
)
    dim q as long, dx as integer, up as integer

    q = Fired&(lv, se, i)
    if q >= FLASHP then exit sub

    dx = abs(lv.things(i).at.x - cint(p.at.x))
    up = lv.things(i).at.y - MORTARH - cint(p.at.y)

    if q < se.tempo and dx < MORTARW and up >= 0 _
        and up < MORTARREACH and not p.ground then
        Toss p, cint(p.at.y), MORTARV
        exit sub
    end if

    if dx >= BLASTW or abs(up + MORTARH) >= 12 then exit sub

    Hurt ev, se, g, p, lv.things(i).at.x, lo(), SHURT
end sub

' Mortar i at x, y on the screen: its tube; its fuse alight
' the beat before it fires; its flash; its shell a beat on
' its way up, its trail under it, and a beat burst where
' that ends, in one of two colours.
sub DrawMortar ( _
    lv as Level, _
    art as Store, _
    se as Session, _
    i as integer, _
    x as integer, _
    y as integer _
)
    dim q as long, h as integer, f as integer

    Blit art, x - 6, y - 14, IMORTAR, 13, 15
    q = Fired&(lv, se, i)

    if q < FLASHP then
        FillBox art, x - 3, y - 21, x - 2, y - 13, CWHITE
        FillBox art, x - 7, y - 18, x + 2, y - 17, CWHITE
    end if

    if q >= (MORTARBAR - 1) * PULSES and (se.ticks and 4) then
        FillBox art, x - 3, y - 16, x - 2, y - 15, CLANT
    end if

    if q < PULSES then
        h = SHELLUP * q \ PULSES
        FillBox art, x - 3, y - 18 - h, x - 2, y - 15 - h, CLANT
        FillBox art, x - 3, y - 14 - h, x - 2, y - 12 - h, CLANT + 1
    elseif q < 2 * PULSES then
        f = (q - PULSES) * 3 \ PULSES
        Blit art, x - 26, y - 40 - SHELLUP, _
            IBURST + 2 * f + 6 * (i and 1), 48, 48
    end if
end sub

' A snatcher's picture by its state: the cat's, of its own
' set.
function SnatchPic% (se as Session, s as integer)
    SnatchPic% = ISNATCH + CatPic%(se, s) - ICAT

    if abs(s) <= 1 then exit function

    SnatchPic% = ISNATCH + 8 + CatPic%(se, s) - ICATF
end function

' Rope-walker i shaken off its line: beaten, where it was.
private sub Shaken (lv as Level, ev as Events, i as integer)
    if lv.things(i).state then Beaten lv, ev, i, 6
end sub

' The lines in view: each from post to post, its washing on
' it.
sub DrawLines ( _
    lv as Level, _
    art as Store, _
    cx as integer, _
    cy as integer _
)
    dim i as integer, u as integer, x as integer, y as integer
    dim y1 as integer

    for i = 0 to lv.n_ropes - 1
        x = lv.ropes(i).at.x - cx
        if x >= SW or x + lv.ropes(i).w < 0 then continue for

        y = lv.ropes(i).at.y - cy
        DrawPost art, x, y, lv.top(lv.ropes(i).at.x \ TS) * TS - cy
        DrawPost art, x + lv.ropes(i).w, y + lv.ropes(i).fall, _
            lv.top((lv.ropes(i).at.x + lv.ropes(i).w) \ TS) * TS - cy

        if lv.ropes(i).fall then
            DrawLine art, x, y, x + lv.ropes(i).w, _
                y + lv.ropes(i).fall, CQINK
            Lanterns lv.ropes(i), art, cx, cy
            continue for
        end if

        for u = 8 to lv.ropes(i).w + 7 step 8
            if u > lv.ropes(i).w then u = lv.ropes(i).w
            y1 = LineY%(lv.ropes(i), u) - cy
            DrawLine art, x + u - 8, y, x + u, y1, CLINEN
            y = y1
        next

        Washing lv.ropes(i), art, cx, cy
    next
end sub

' Line r's paper lanterns, one every LANTGAP hung under it.
private sub Lanterns ( _
    r as Rope, _
    art as Store, _
    cx as integer, _
    cy as integer _
)
    dim u as integer

    for u = LANTGAP \ 2 to r.w - 6 step LANTGAP
        Blit art, r.at.x + u - cx - 4, LineY%(r, u) - cy, IPAPER, 9, _
            14
    next
end sub

' A line's end at x, y on the screen, the roof under it at
' y1: a post up from the roof, its crossbar under its top;
' where the roof is no lower, a hook in the wall.
private sub DrawPost ( _
    art as Store, _
    x as integer, _
    y as integer, _
    y1 as integer _
)
    if y1 < y + 6 then
        FillBox art, x - 1, y - 1, x + 1, y + 1, CQINK
        exit sub
    end if

    FillBox art, x - 2, y - 3, x + 2, y1 - 1, CQINK
    FillBox art, x - 1, y - 2, x + 1, y1 - 1, CPOLE + 1
    DrawLine art, x - 1, y - 2, x - 1, y1 - 1, CPOLE
    DrawLine art, x + 1, y - 2, x + 1, y1 - 1, CPOLE + 2
    FillBox art, x - 5, y + 1, x + 5, y + 4, CQINK
    FillBox art, x - 4, y + 2, x + 4, y + 3, CPOLE + 1
    DrawLine art, x - 4, y + 2, x + 4, y + 2, CPOLE
end sub

' Line r's washing, pegged every WASH along it and hung as
' it hangs: by where it is, a shirt, trousers, a sock, or a
' peg left bare.
private sub Washing ( _
    r as Rope, _
    art as Store, _
    cx as integer, _
    cy as integer _
)
    dim u as integer, x as integer, y as integer

    for u = 16 to r.w - 14 step WASH
        x = r.at.x + u - cx
        y = LineY%(r, u) - cy - 1

        select case (r.at.x \ TS + (u \ WASH) * 3) mod 5
        case 0
            Blit art, x - 9, y, ISHIRT, 18, 15
        case 2
            Blit art, x - 6, y, ITROUSERS, 13, 17
        case 3
            Blit art, x - 4, y, ISOCK, 8, 11
        end select
    next
end sub
