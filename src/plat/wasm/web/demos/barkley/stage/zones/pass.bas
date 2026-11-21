' Whitecap Pass: sleds, the gale and the avalanche, goats,
' ice walls, loads of snow, snow-throwers.
defint A-Z
option explicit

from engine.video import Blit, SH, Store, SW
from engine.events import Events
from barkley.model.images import IGOAT, IGOATR, ILOAD, ISNOWBALL
from barkley.model.hero import Hero
from barkley.model.level import KGOAT, Level, NONE, TS
from barkley.model.game import Game, Loose, Session
from barkley.model.events import EVBURST, EVSCORE, EVSHAKE, EVSOUND
from barkley.model.events import Tell, TellAt
from barkley.assets.palette import CFLAKE, CICE, CSNOW, CWHITE
from barkley.assets.tunes import SBUMP, SCRACK, SHURT, SROLL
from barkley.world.terrain.ground import Balk, Lie, Surf
from barkley.world.level import LOADFAR
from barkley.stage.fx.parts import PDRIFT, PSLEDSNOW, PWALL
from barkley.stage.fx.damage import Tackle
from barkley.stage.hero.modes import Alight, Astride, Lull
from barkley.stage.hero.push import Aboard, Repel, Stall
from barkley.stage.things.react import Turn
from barkley.stage.things.pass import AVALH, Bolt, Fallen, GCHARGE
from barkley.stage.things.pass import HEAPH, HEAPR, LOADRUN, LOADSLIDE
from barkley.stage.things.pass import Plunged, Risen, Shatter, TREMBLE
from barkley.stage.hero.hurt import Hurt

const SLEDV = 120!

' Rolled through at SNOWV or faster, deep snow packs round
' him.
private const SNOWV = 100!

private const GUSTTELL = 24

' An ice wall, WALLH tall, stops him WALLR from its middle:
' only a goat ridden breaks it.
private const WALLH = 80
const WALLR = 15

' Onto sled i: off on it at once, to the right, curled in
' nothing but the game's reckoning.
private sub Board ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    p as Hero, _
    i as integer _
)
    Turn lv, i, 1
    Aboard p, SLEDV
    Tell ev, EVSOUND, SROLL
end sub

' An icicle: walked under, it is loosed; falling, it hurts
' whom it lands on, and breaks.
sub MeetIcicle ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    lo() as Loose _
)
    dim x as integer, dx as integer, py as integer, y as integer

    x = lv.things(i).at.x
    dx = abs(x - cint(p.at.x))
    py = cint(p.at.y)

    if lv.things(i).state = 0 and dx < 20 _
        and py > lv.things(i).at.y _
        and py < lv.things(i).at.y + 140 then
        Turn lv, i, 1
    end if

    y = lv.things(i).at.y + Fallen%(lv, i)
    if lv.things(i).state <= TREMBLE or dx >= 9 or py <= y _
        or py - 26 >= y + 16 then exit sub

    Hurt ev, se, g, p, x, lo(), SHURT
    Shatter lv, ev, i
end sub

' A sled left on the snow: landed on or run onto, he boards
' it.
sub MeetSled ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    p as Hero, _
    i as integer _
)
    if lv.things(i).state <> 0 or p.sled or p.stun then exit sub
    if not p.ground and p.vel.y <= 0 then exit sub
    if abs(lv.things(i).at.x - cint(p.at.x)) >= 14 _
        or abs(cint(p.at.y) - lv.things(i).at.y _
        + 5) >= 10 then exit sub

    Board lv, ev, se, p, i
end sub

' Deep snow: it stops a sled dead and he tumbles out of it;
' rolled through, it packs round him.
sub MeetDrift (lv as Level, ev as Events, p as Hero, i as integer)
    if not p.ground or abs(lv.things(i).at.x - cint(p.at.x)) >= 22 _
        or abs(cint(p.at.y) - lv.things(i).at.y) >= 8 then exit sub

    if p.sled then
        Alight ev, p, PSLEDSNOW
        Stall p
        exit sub
    end if

    if p.roll = 0 or p.snow <> 0 or abs(p.gsp) < SNOWV then exit sub

    p.snow = -1
    TellAt ev, EVBURST, p.at.x, p.at.y - 8, PDRIFT
    Tell ev, EVSOUND, SROLL
end sub

' How far along load i has come: nothing till it has
' trembled, LOADRUN at its ledge's end, LOADFAR at the most.
function Slid% (lv as Level, i as integer)
    dim n as integer, x as integer

    Slid% = 0
    n = lv.things(i).state - TREMBLE
    if n <= 0 then exit function

    x = LOADRUN * n * n \ (LOADSLIDE * LOADSLIDE)

    if n > LOADSLIDE then
        x = LOADRUN + 2 * LOADRUN * (n - LOADSLIDE) \ LOADSLIDE
    end if

    if x > LOADFAR then x = LOADFAR
    Slid% = x
end function

' The avalanche over the view: its snow from the view's left
' to its front, AVALH deep on the ground it has covered,
' paler at its top; its face rounded, its top heaving, balls
' of snow rolling on both; blocks of ice thrown on ahead of
' it.
sub DrawAvalanche ( _
    lv as Level, _
    art as Store, _
    se as Session, _
    cx as integer, _
    cy as integer _
)
    dim x as integer, wx as integer, d as integer, h as integer
    dim y as integer, j as integer

    if lv.aval.st = 0 or lv.aval.x < cx then exit sub

    for x = 0 to SW - 1 step 4
        wx = cx + x
        d = cint(lv.aval.x) - wx
        if d <= 0 then exit for

        h = AVALH
        if d < 64 then h = cint(AVALH * sqr(d / 64))
        h = h + cint(4 * sin((wx + se.ticks * 3) / 11))
        y = SnowTop%(lv, wx) - cy - h
        line (x, y + 22)-(x + 3, SH - 1), CFLAKE, BF
        line (x, y)-(x + 3, y + 21), CSNOW, BF
        line (x, y)-(x + 3, y + 1), CWHITE, BF
        ' a ball on its top every 36 along, turning
        j = wx \ 36
        if wx mod 36 >= 4 or d <= 24 then continue for

        Blit art, x - 12, y - 14 + j mod 3 * 4, _
            ISNOWBALL + ((se.ticks \ 8 + j) and 1) * 2, 28, 28
    next

    ' and down its face, tumbling
    for j = 0 to 3
        wx = cint(lv.aval.x) - 20 _
            + j * 5 - (se.ticks * 2 + j * 9) mod 12
        y = SnowTop%(lv, wx) - cy - 26 - j * 13 _
            + (se.ticks + j * 7) mod 10
        Blit art, wx - cx - 14, y, _
            ISNOWBALL + ((se.ticks \ 6 + j) and 1) * 2, 28, 28
    next

    for j = 0 to 4
        wx = cint(lv.aval.x) + 8 + j * 11 _
            + (se.ticks * (2 + j)) mod 16
        y = SnowTop%(lv, wx) - cy - 6 _
            - abs((se.ticks * (4 + j) + j * 13) mod 36 - 18)
        line (wx - cx, y)-(wx - cx + 3, y + 3), CICE, BF
        line (wx - cx, y)-(wx - cx + 3, y), CWHITE
    next
end sub

' The ground's top at x, for the snow to lie on.
private function SnowTop% (lv as Level, x as integer)
    dim y as integer

    y = Surf%(lv, x, lv.top(x \ TS) * TS + 8)
    if y = NONE then y = lv.top(x \ TS) * TS
    SnowTop% = y
end function

' The blizzard over the view: flakes aslant always; a gust's
' streaks of snow from the right, fewer as it rises.
sub DrawGale (lv as Level, se as Session)
    dim j as integer, x as integer, y as integer, n as integer

    for j = 0 to 23
        x = SW - (se.ticks * (2 + (j and 3)) + j * 61) mod (SW + 16)
        y = (se.ticks * (1 + (j and 1)) + j * 37) mod SH
        pset (x, y), CWHITE
        pset (x + 1, y - 1), CFLAKE
    next

    if Lull%(lv, se) > GUSTTELL then exit sub

    n = 17
    if Lull%(lv, se) > 0 then n = 7

    for j = 0 to n
        x = SW + 40 _
            - (se.ticks * (9 + j mod 5) + j * 71) mod (SW + 80)
        y = (j * 47 + (se.ticks \ 2) * 3) mod SH
        line (x, y)-(x + 8 + (j mod 3) * 6, y - 2 - j mod 3), CWHITE
        line (x + 4, y + 1)-(x + 10 + (j mod 3) * 6, _
            y - 1 - j mod 3), CFLAKE
    next
end sub

' A goat met: come down on, it is ridden; its horns, lowered
' or at a charge, hurt.
sub MeetGoat ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    lo() as Loose _
)
    dim s as integer

    s = abs(lv.things(i).state)
    if s = 0 or p.goat or abs(lv.things(i).at.x - p.at.x) >= 16 _
        or p.at.y < lv.things(i).at.y - 30 _
        or p.at.y > lv.things(i).at.y + 8 then exit sub

    if not p.ground and p.vel.y > 0 _
        and p.at.y < lv.things(i).at.y - 6 and not p.sled then
        Mount lv, ev, se, p, i
    elseif s > 1 and s <= GCHARGE _
        and p.at.y > lv.things(i).at.y - 18 then
        Hurt ev, se, g, p, lv.things(i).at.x, lo(), SHURT
    end if
end sub

' Onto goat i's back: it bolts on along the road, he
' astride.
private sub Mount ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    p as Hero, _
    i as integer _
)
    Bolt lv, i
    Astride p, lv.stray.at.x, lv.stray.at.y
    Tell ev, EVSOUND, SBUMP
end sub

' An ice wall across the road: he is stopped at it; a goat
' ridden goes through, and leaves its stump.
sub MeetWall (lv as Level, ev as Events, p as Hero, i as integer)
    if lv.things(i).state = 0 _
        or abs(lv.things(i).at.x - p.at.x) >= WALLR then exit sub
    if p.at.y <= lv.things(i).at.y - WALLH _
        or p.at.y > lv.things(i).at.y + 20 then exit sub

    if p.goat then
        Turn lv, i, 0
        Tell ev, EVSCORE, 100
        Tell ev, EVSHAKE, 20
        TellAt ev, EVBURST, csng(lv.things(i).at.x), _
            csng(lv.things(i).at.y - WALLH \ 2), PWALL
        Tell ev, EVSOUND, SCRACK
        exit sub
    end if

    Balk p, lv.things(i).at.x, WALLR
end sub

' A slid load's heap across the road: he is stopped at its
' flank, nearer its middle the higher his feet, and over it
' goes free.
sub MeetHeap (lv as Level, p as Hero, i as integer)
    dim h as single, r as integer

    if lv.things(i).state = 0 then exit sub

    h = lv.things(i).at.y - p.at.y
    if h >= HEAPH or h < -20 then exit sub

    if h < 0 then h = 0
    r = cint(HEAPR * (1 - h / HEAPH))

    if abs(p.at.x - lv.things(i).at.x) >= r then exit sub

    Balk p, lv.things(i).at.x, r
end sub

' The goat not ridden within 150 of x, NONE when there is
' none.
function GoatNear% (lv as Level, x as integer)
    dim i as integer, c as integer

    GoatNear% = NONE
    c = (x - 220) \ TS
    if c < 0 then c = 0
    i = lv.first(c)

    do while i < lv.n_things
        if lv.things(i).home > x + 220 then exit do

        if lv.things(i).kind = KGOAT and lv.things(i).state <> 0 _
            and abs(lv.things(i).at.x - x) < 150 then
            GoatNear% = i
        end if

        i = i + 1
    loop
end function

' A ledge's load at x, y on the screen: shaking once set
' off, sliding off, falling; nothing once it is down.
sub DrawLoad ( _
    lv as Level, _
    art as Store, _
    se as Session, _
    i as integer, _
    x as integer, _
    y as integer _
)
    dim s as integer, jig as integer

    s = lv.things(i).state
    if s < 0 then exit sub

    jig = s > 0 and s <= TREMBLE and (se.ticks and 2) <> 0
    Blit art, x - 34 + jig + Slid%(lv, i), _
        y - 16 + Plunged%(s - TREMBLE - LOADSLIDE), ILOAD, 76, 22
end sub

' Goat i at x, y on the screen: its head up at graze and
' dazed, down while it gathers itself and charges.
sub DrawGoat ( _
    lv as Level, _
    art as Store, _
    se as Session, _
    i as integer, _
    x as integer, _
    y as integer _
)
    dim s as integer, f as integer

    s = abs(lv.things(i).state)
    if s = 0 then exit sub

    f = -2 * (lv.things(i).state < 0)

    if s > 1 and s <= GCHARGE then
        Blit art, x - 17, y - 40, _
            IGOATR + (5 + (s = GCHARGE) * ((se.ticks \ 6) and 1)) _
            * 4 + f, 34, 58
    else
        Blit art, x - 17, y - 23, IGOAT + f, 34, 24
    end if
end sub

' The goat on its way home, as the ground lies under it.
sub DrawStray ( _
    lv as Level, _
    art as Store, _
    se as Session, _
    cx as integer, _
    cy as integer _
)
    dim x as integer, y as integer, f as integer

    x = cint(lv.stray.at.x)
    y = cint(lv.stray.at.y)
    f = ((Lie%(lv, x, y) + 2) * 2 + ((se.ticks \ 6) and 1)) * 4 - 2 _
        * (lv.things(lv.stray.goat).home < x)
    Blit art, x - cx - 17, y - cy - 40, IGOATR + f, 34, 58
end sub

' A snow-thrower met: ducked, its wall throws him back,
' neither hurt; up, it is a cat, beaten by a roll.
sub MeetThrow ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    lo() as Loose _
)
    dim s as integer

    if lv.things(i).state = 0 _
        or abs(lv.things(i).at.x - p.at.x) >= 16 _
        or p.at.y < lv.things(i).at.y - 22 _
        or p.at.y > lv.things(i).at.y + 8 then exit sub

    if Risen%(lv, i) then
        Tackle lv, ev, se, g, p, i, 8, lo()
        exit sub
    end if

    s = 1 + 2 * (p.at.x < lv.things(i).at.x)
    Repel p, lv.things(i).at.x + s * 16, s * 150
    Tell ev, EVSOUND, SBUMP
end sub
