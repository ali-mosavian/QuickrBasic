' The Chrome Works: presses, sweepers, sparks, jets, bumpers
' and hoops.
defint A-Z
option explicit

from engine.video import Blit, Store
from engine.audio import Deck
from barkley.model.images import IPRESS, IRAIL, ISPARK, ISRAIL
from barkley.model.hero import Hero
from barkley.model.level import KPRESS, Level, NONE, TS
from barkley.model.game import Game, Loose, Part, Session
from barkley.assets.palette import CGOLD
from barkley.assets.tunes import SBOOST, SCLANK, Sfx, SHISS, SHURT
from barkley.assets.tunes import SMARK, SSPRING
from barkley.stage.fx.parts import Burst
from barkley.stage.fx.damage import Beaten, BOUNCEV, Hurt

' The works' own: a press and a leg of its gantry; a sweeper
' and a spark's carriage. A spark goes SPARKR either way
' along its rail, alight SPARKLIT steps of every SPARKP. A
' press hangs PRESSH over the floor, its head's foot PRESSUP
' over it when up; of its turn's PRESSP steps it is up till
' PRESSFALL, down by ten on, held till PRESSRISE, then
' rising; a press a step later for every PRESSLAG along, so
' a row's turns go along it with a run.
const SPARKP = 150
const SPARKLIT = 90

private const PRESSH = 112
const PRESSUP = 68
const PRESSP = 180
const PRESSFALL = 90
const PRESSRISE = 130
const PRESSLAG = 4
private const JETV = 500!
private const BUMPV = 400!
private const HOOPV = 640!
const SPINR = 30
const SPINH = 70

' A spark on its cable: lit, it stings.
sub MeetSpark ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    lo() as Loose _
)
    dim x as integer, py as integer

    x = lv.things(i).at.x
    py = cint(p.at.y)
    if abs(x - cint(p.at.x)) >= 11 or py <= lv.things(i).at.y - 24 _
        or py - 26 >= lv.things(i).at.y then exit sub

    if Lit%(lv, se, i) then Hurt mu, se, g, p, x, lo(), SHISS
end sub

' A button in the floor: stood on, it is pressed.
sub MeetButton ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    p as Hero, _
    i as integer, _
    pt() as Part _
)
    if lv.things(i).state <> 0 or not p.ground then exit sub
    if abs(lv.things(i).at.x - cint(p.at.x)) >= 9 _
        or abs(cint(p.at.y) - lv.things(i).at.y) >= 4 then exit sub

    lv.things(i).state = 1
    Burst se, pt(), csng(lv.things(i).at.x), _
        csng(lv.things(i).at.y - 6), 8, 80, CGOLD + 4
    Sfx mu, se, SMARK
end sub

' A jet, blowing 80 ticks of 180: who is over it goes up.
sub MeetJet ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    p as Hero, _
    i as integer _
)
    dim dy as integer

    dy = cint(p.at.y) - lv.things(i).at.y
    if (se.ticks + lv.things(i).at.x) mod 180 >= 80 _
        or abs(lv.things(i).at.x - cint(p.at.x)) >= 10 then exit sub
    if dy > 2 or dy <= -64 or p.vel.y <= -JETV + 60 then exit sub

    if p.ground then
        p.vel.x = p.gsp
        Sfx mu, se, SHISS
    end if

    p.ground = 0
    p.vel.y = -JETV
    p.roll = 0
    p.jumped = 0
    p.sprung = -1
end sub

' A bumper or a buoy: touched, it throws him off the way he
' came at it.
sub MeetBump ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    pt() as Part _
)
    dim dx as integer, dy as integer, d as single

    dx = cint(p.at.x) - lv.things(i).at.x
    dy = cint(p.at.y) - 12 - lv.things(i).at.y
    if lv.things(i).state <> 0 or abs(dx) >= 20 _
        or abs(dy) >= 20 then exit sub

    d = sqr(csng(dx) * dx + csng(dy) * dy)
    if d >= 20 then exit sub

    if d < 1 then
        d = 1
        dy = -1
    end if

    p.ground = 0
    p.vel.x = dx / d * BUMPV
    p.vel.y = dy / d * BUMPV - 60
    p.jumped = 0
    p.sprung = -1
    lv.things(i).state = 10
    g.score = g.score + 10
    Burst se, pt(), csng(lv.things(i).at.x), _
        csng(lv.things(i).at.y), 6, 90, CGOLD + 4
    Sfx mu, se, SSPRING
end sub

' A ball going round its post: it hurts.
sub MeetSpin ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    lo() as Loose _
)
    dim a as single, x as integer, bx as integer, by as integer

    x = lv.things(i).at.x
    a = se.ticks * .06 + x
    bx = x + cint(SPINR * cos(a))
    by = lv.things(i).at.y - SPINH + cint(SPINR * sin(a))

    if abs(bx - cint(p.at.x)) >= 11 _
        or abs(by - cint(p.at.y) + 12) >= 16 then exit sub

    Hurt mu, se, g, p, x, lo(), SHURT
end sub

' A hoop: through it in the air he goes on, faster, the way
' he was going.
sub MeetHoop ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    p as Hero, _
    i as integer, _
    pt() as Part _
)
    if lv.things(i).state <> 0 or p.ground _
        or abs(lv.things(i).at.x - cint(p.at.x)) >= 9 then exit sub
    if abs(lv.things(i).at.y - cint(p.at.y) + 12) >= 18 then exit sub

    p.vel.x = HOOPV * (1 + 2 * (p.vel.x < 0))
    p.vel.y = -180
    p.sprung = -1
    lv.things(i).state = 20
    Burst se, pt(), csng(lv.things(i).at.x), _
        csng(lv.things(i).at.y), 8, 120, CGOLD + 4
    Sfx mu, se, SBOOST
end sub

' A sweeper met: armoured before, its hood hurts whoever
' meets it, curled or not; landed on, or rolled into from
' behind, it is beaten.
sub MeetSweep ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    lo() as Loose, _
    pt() as Part _
)
    dim y as integer, behind as integer

    y = lv.things(i).at.y
    if lv.things(i).state = 0 _
        or abs(lv.things(i).at.x - p.at.x) >= 18 or p.at.y < y - 20 _
        or p.at.y > y + 10 then exit sub

    behind = sgn(p.at.x _
        - lv.things(i).at.x) <> sgn(lv.things(i).state)

    if not p.ground and p.vel.y > 0 and p.at.y < y - 8 then
        Beaten lv, mu, se, g, pt(), i, 8
        p.vel.y = -BOUNCEV
    elseif behind and (p.roll or p.charging) then
        Beaten lv, mu, se, g, pt(), i, 8
    else
        Hurt mu, se, g, p, lv.things(i).at.x, lo(), SHURT
    end if
end sub

' Whether spark i is alight: its carriage hurts then, and is
' safe dark.
private function Lit% (lv as Level, se as Session, i as integer)
    Lit% = (se.ticks + lv.things(i).home) mod SPARKP < SPARKLIT
end function

' Where press i is in its turn, 0 to PRESSP - 1: a press
' further along a row is later.
function PressT% (lv as Level, se as Session, i as integer)
    PressT% = (se.ticks _
        + PRESSP * 100 - lv.things(i).home \ PRESSLAG) mod PRESSP
end function

' How far down press i's head is, 0 to PRESSUP.
function Stroke% (lv as Level, se as Session, i as integer)
    dim t as integer

    t = PressT%(lv, se, i)

    if t < PRESSFALL then
        Stroke% = 0
    elseif t < PRESSFALL + 10 then
        Stroke% = (t - PRESSFALL) * PRESSUP \ 10
    elseif t < PRESSRISE then
        Stroke% = PRESSUP
    else
        Stroke% = PRESSUP - (t - PRESSRISE) * PRESSUP _
            \ (PRESSP - PRESSRISE)
    end if
end function

' A press met: its head, wherever in its stroke, hurts who
' is under it or against it. It lands with a clank.
sub MeetPress ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    lo() as Loose _
)
    dim y as integer

    if PressT%(lv, se, i) = PRESSFALL + 10 then Sfx mu, se, SCLANK
    y = lv.things(i).at.y - PRESSUP + Stroke%(lv, se, i)

    if abs(lv.things(i).at.x - p.at.x) >= 26 or p.at.y - 26 >= y _
        or p.at.y <= y - 14 then exit sub

    Hurt mu, se, g, p, lv.things(i).at.x, lo(), SHURT
end sub

' For the pilot at x: the nearest press he is under or
' coming to, NONE with none near.
function PressAhead% (lv as Level, x as integer)
    dim i as integer, c as integer

    PressAhead% = NONE
    c = (x - 64) \ TS
    if c < 0 then c = 0
    i = lv.first(c)

    do while i < lv.n_things
        if lv.things(i).at.x > x + 200 then exit do

        if lv.things(i).kind = KPRESS _
            and lv.things(i).at.x + 50 >= x then
            PressAhead% = i
            exit do
        end if

        i = i + 1
    loop
end function

' Spark i, its carriage at x, y on the screen: its rail, a
' tile's length from its home either way and between; the
' carriage dark, or alight.
sub DrawSpark ( _
    lv as Level, _
    art as Store, _
    se as Session, _
    i as integer, _
    x as integer, _
    y as integer, _
    cx as integer _
)
    dim f as integer, k as integer

    for f = -3 to 2
        Blit art, lv.things(i).home + f * TS - cx, y - 4, ISRAIL, _
            TS, 4
    next

    k = ISPARK

    if Lit%(lv, se, i) then
        k = ISPARK + 2 + ((se.ticks \ 3) and 1) * 2
    end if

    Blit art, x - 11, y - 24, k, 22, 22
end sub

' Press i at x, the floor at y on the screen: its length of
' girder, its rod down to its head, its housing.
sub DrawPress ( _
    lv as Level, _
    art as Store, _
    se as Session, _
    i as integer, _
    x as integer, _
    y as integer _
)
    dim yy as integer, hy as integer

    Blit art, x - 32, y - PRESSH - 8, IRAIL, 32, 8
    Blit art, x, y - PRESSH - 8, IRAIL, 32, 8
    hy = y - PRESSUP + Stroke%(lv, se, i) - 15

    for yy = hy - 8 to y - PRESSH + 12 step -8
        Blit art, x - 4, yy, IPRESS + 2, 8, 8
    next

    Blit art, x - 18, y - PRESSH, IPRESS, 36, 21
    Blit art, x - 19, hy, IPRESS + 4, 38, 16
end sub

' A leg of a gantry, its left at x, the floor at y on the
' screen.
sub DrawLeg (art as Store, x as integer, y as integer)
    dim yy as integer

    for yy = y - 8 to y - PRESSH step -8
        Blit art, x, yy, IPRESS + 2, 8, 8
    next
end sub
