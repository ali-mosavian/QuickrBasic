' Whitecap Pass: sleds, the gale and the avalanche, goats, ice walls, loads of snow, snow-throwers.
defint A-Z
option explicit

from barkley.engine.video import Blit, SH, Store, SW
from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck
from barkley.model.images import IGOAT, IGOATR, ILOAD, ILOB, ISNOWBALL
from barkley.model.hero import Hero
from barkley.model.level import GRAV, KCAT, KGOAT, KHEAP, KTHROW, Level, NONE, TS
from barkley.model.game import Game, Loose, Part, Session
from barkley.assets.palette import CFLAKE, CICE, CSNOW, CWHITE
from barkley.assets.tunes import SBUMP, SCRACK, Sfx, SHURT, SROLL
from barkley.world.terrain.ground import Balk, Lie, Solid, Surf
from barkley.world.level import LOADFAR
from barkley.stage.fx.parts import Burst
from barkley.stage.fx.damage import Beaten, Hurt, Lose, Tackle
from barkley.stage.fx.lobs import Loft
from barkley.stage.hero.modes import Alight, GOATV, GUSTP, Lull
from barkley.stage.hero.moves import THROWC

const SLEDV = 120!

' Rolled through at SNOWV or faster, deep snow packs round him.
private const SNOWV = 100!

' A snow-thrower, a cat behind a wall of snow, its state's sign the way it faces: its turn is THROWC of the
' things' steps, ducked till THROWUP are left, then up and open; with THROWAT left it throws, if Charlie is
' within THROWR, a snowball LOBT in the air, and ducks. NLOB of them fly at once.
const THROWUP = 30, THROWAT = 10
private const THROWR = 180, LOBT = .9

' An icicle: its state 0 hung, the things' steps since it was loosed, or -1 broken. It trembles TREMBLE of
' them, then falls.
const TREMBLE = 10

' A goat, its state's sign the way it faces: 1 at graze, at home, till it sees him within GOATSEE on its ledge; then its
' head is down GOATWIND of the things' steps and it charges, GOATSTEP a step, till it is under or past him or
' GOATRUN from home, and stands dazed GOATDAZE steps; 0 while it is ridden or on its way home. Its lowered horns
' hurt. Come down on, it bolts on along the road, to the right, at GOATV, he astride GOATH up, till he is off the
' ground, stopped or GOATRIDE seconds on; then it runs home at STRAYV.
private const GOATSEE = 72, GOATWIND = 12
const GCHARGE = 100
private const GOATDAZE = 30, GOATSTEP = 8, GOATRUN = 64
private const STRAYV = 300!

private const GUSTTELL = 24

' The avalanche: let go as he passes lv.avalX, its front comes on from AVALLEAD behind at AVALV, the snow AVALH deep
' behind it, and takes him where it reaches him.
private const AVALV = 300!, AVALLEAD = 220, AVALH = 70

' An ice wall, WALLH tall, stops him WALLR from its middle: only a goat ridden breaks it.
private const WALLH = 80
const WALLR = 15

' A ledge's load of snow: its state 0 heaped, the things' steps since a bark set it off, or -1 gone. It trembles
' TREMBLE of them, slides LOADRUN along its ledge in LOADSLIDE more, gathering way, then falls at the speed it left
' with, to its heap's place LOADFAR on: there cats and snow-throwers are beaten. Its heap: its state 1 once it lies
' there, HEAPH tall and HEAPR either way at its foot; it stops him.
const LOADRUN = 40, LOADSLIDE = 20
private const HEAPH = 30
const HEAPR = 24

' Onto sled i: off on it at once, to the right, curled in nothing but the game's reckoning.
private sub Board (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    lv.ob(i).state = 1
    p.sled = -1
    p.roll = -1
    p.jumped = 0
    p.face = 0
    if p.ground then
        if p.gsp < SLEDV then p.gsp = SLEDV
    elseif p.vel.x < SLEDV then
        p.vel.x = SLEDV
    end if
    Sfx mu, se, SROLL
end sub

' An icicle: walked under, it is loosed; falling, it hurts whom it lands on, and breaks.
sub MeetIcicle (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
    dim x as integer, dx as integer, py as integer, y as integer
    x = lv.ob(i).at.x
    dx = abs(x - cint(p.at.x))
    py = cint(p.at.y)
    if lv.ob(i).state = 0 and dx < 20 and py > lv.ob(i).at.y and py < lv.ob(i).at.y + 140 then lv.ob(i).state = 1
    y = lv.ob(i).at.y + Fallen%(lv, i)
    if lv.ob(i).state <= TREMBLE or dx >= 9 or py <= y or py - 26 >= y + 16 then exit sub
    Hurt mu, se, g, p, x, lo(), SHURT
    Shatter lv, mu, se, pt(), i
end sub

' A sled left on the snow: landed on or run onto, he boards it.
sub MeetSled (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    if lv.ob(i).state <> 0 or p.sled or p.stun then exit sub
    if not p.ground and p.vel.y <= 0 then exit sub
    if abs(lv.ob(i).at.x - cint(p.at.x)) >= 14 or abs(cint(p.at.y) - lv.ob(i).at.y + 5) >= 10 then exit sub
    Board lv, mu, se, p, i
end sub

' Deep snow: it stops a sled dead and he tumbles out of it; rolled through, it packs round him.
sub MeetDrift (lv as Level, mu as Deck, se as Session, p as Hero, i as integer, pt() as Part)
    if not p.ground or abs(lv.ob(i).at.x - cint(p.at.x)) >= 22 or abs(cint(p.at.y) - lv.ob(i).at.y) >= 8 then exit sub
    if p.sled then
        Alight mu, se, p, pt(), CWHITE
        p.roll = 0
        p.gsp = p.gsp * .25
        exit sub
    end if
    if p.roll = 0 or p.snow <> 0 or abs(p.gsp) < SNOWV then exit sub
    p.snow = -1
    Burst se, pt(), p.at.x, p.at.y - 8, 8, 100, CWHITE
    Sfx mu, se, SROLL
end sub

' How far icicle i has fallen: nothing till it has trembled.
function Fallen% (lv as Level, i as integer)
    Fallen% = Plunged%(lv.ob(i).state - TREMBLE)
end function

' How far a thing has fallen, n of the things' steps into its fall.
private function Plunged% (n as integer)
    dim t as single
    Plunged% = 0
    if n <= 0 then exit function
    t = n * STEPT * 2
    Plunged% = cint(GRAV * t * t / 2)
end function

' How far along load i has come: nothing till it has trembled, LOADRUN at its ledge's end, LOADFAR at the most.
function Slid% (lv as Level, i as integer)
    dim n as integer, x as integer
    Slid% = 0
    n = lv.ob(i).state - TREMBLE
    if n <= 0 then exit function
    x = LOADRUN * n * n \ (LOADSLIDE * LOADSLIDE)
    if n > LOADSLIDE then x = LOADRUN + 2 * LOADRUN * (n - LOADSLIDE) \ LOADSLIDE
    if x > LOADFAR then x = LOADFAR
    Slid% = x
end function

' Load i's heap, the next along; NONE without one.
function HeapOf% (lv as Level, i as integer)
    dim j as integer
    HeapOf% = NONE
    for j = i + 1 to lv.nObj - 1
        if lv.ob(j).home > lv.ob(i).home + LOADFAR then exit for
        if lv.ob(j).kind <> KHEAP then continue for
        HeapOf% = j
        exit for
    next
end function

' The avalanche's step: let go as he passes its head, its front comes on, the ground shaking while it is in
' view, and takes him where it reaches him.
sub Avalanche (lv as Level, mu as Deck, se as Session, g as Game, p as Hero)
    if lv.aval.st = 0 then
        if p.at.x < lv.avalX then exit sub
        lv.aval.st = 1
        lv.aval.x = lv.avalX - AVALLEAD
        g.shake = .5
        Sfx mu, se, SCRACK
    end if
    lv.aval.x = lv.aval.x + AVALV * STEPT
    if (se.ticks and 15) = 0 and lv.aval.x > g.cam.x - 40 then g.shake = .12
    if lv.aval.x > p.at.x - 6 then Lose mu, se, p
end sub

' The avalanche over the view: its snow from the view's left to its front, AVALH deep on the ground it has
' covered, paler at its top; its face rounded, its top heaving, balls of snow rolling on both; blocks of ice
' thrown on ahead of it.
sub DrawAvalanche (lv as Level, art as Store, se as Session, cx as integer, cy as integer)
    dim x as integer, wx as integer, d as integer, h as integer, y as integer, j as integer
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
        Blit art, x - 12, y - 14 + j mod 3 * 4, ISNOWBALL + ((se.ticks \ 8 + j) and 1) * 2, 28, 28
    next
    ' and down its face, tumbling
    for j = 0 to 3
        wx = cint(lv.aval.x) - 20 + j * 5 - (se.ticks * 2 + j * 9) mod 12
        y = SnowTop%(lv, wx) - cy - 26 - j * 13 + (se.ticks + j * 7) mod 10
        Blit art, wx - cx - 14, y, ISNOWBALL + ((se.ticks \ 6 + j) and 1) * 2, 28, 28
    next
    for j = 0 to 4
        wx = cint(lv.aval.x) + 8 + j * 11 + (se.ticks * (2 + j)) mod 16
        y = SnowTop%(lv, wx) - cy - 6 - abs((se.ticks * (4 + j) + j * 13) mod 36 - 18)
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

' The blizzard over the view: flakes aslant always; a gust's streaks of snow from the right, fewer as it rises.
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
        x = SW + 40 - (se.ticks * (9 + j mod 5) + j * 71) mod (SW + 80)
        y = (j * 47 + (se.ticks \ 2) * 3) mod SH
        line (x, y)-(x + 8 + (j mod 3) * 6, y - 2 - j mod 3), CWHITE
        line (x + 4, y + 1)-(x + 10 + (j mod 3) * 6, y - 1 - j mod 3), CFLAKE
    next
end sub

' A goat's step: off its place it trots home; at graze there it faces him and, seeing him on its ledge, lowers its
' head, in a blizzard only with the calm left to charge in; then it charges till it is under or past him or its
' ledge ends, and stands dazed.
sub Graze (lv as Level, se as Session, i as integer, p as Hero)
    dim s as integer, f as integer, x as integer
    s = abs(lv.ob(i).state)
    f = sgn(lv.ob(i).state)
    select case s
    case 1
        if lv.ob(i).at.x <> lv.ob(i).home then
            f = sgn(lv.ob(i).home - lv.ob(i).at.x)
            lv.ob(i).at.x = lv.ob(i).at.x + f * GOATSTEP \ 2
        else
            f = 1 + 2 * (p.at.x < lv.ob(i).at.x)
            x = abs(p.at.x - lv.ob(i).at.x) < GOATSEE and abs(p.at.y - lv.ob(i).at.y) < 30
            if x and p.goat = 0 and (not lv.gale or Lull%(lv, se) > GUSTP \ 2) then s = GOATWIND + 1
        end if
    case 2: s = GCHARGE
    case GCHARGE
        x = lv.ob(i).at.x + f * GOATSTEP
        if (p.at.x - x) * f < 0 or abs(x - lv.ob(i).home) > GOATRUN or Surf%(lv, x + f * 12, lv.ob(i).at.y) <> lv.ob(i).at.y then
            s = GCHARGE + GOATDAZE
        else
            lv.ob(i).at.x = x
        end if
    case else
        s = s - 1
        if s = GCHARGE then s = 1
    end select
    lv.ob(i).state = s * f
end sub

' A goat met: come down on, it is ridden; its horns, lowered or at a charge, hurt.
sub MeetGoat (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose)
    dim s as integer
    s = abs(lv.ob(i).state)
    if s = 0 or p.goat or abs(lv.ob(i).at.x - p.at.x) >= 16 or p.at.y < lv.ob(i).at.y - 30 or p.at.y > lv.ob(i).at.y + 8 then exit sub
    if not p.ground and p.vel.y > 0 and p.at.y < lv.ob(i).at.y - 6 and not p.sled then
        Mount lv, mu, se, p, i
    elseif s > 1 and s <= GCHARGE and p.at.y > lv.ob(i).at.y - 18 then
        Hurt mu, se, g, p, lv.ob(i).at.x, lo(), SHURT
    end if
end sub

' Onto goat i's back: it bolts on along the road. A goat still on its way home is there.
private sub Mount (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    if lv.stray.st then lv.ob(lv.stray.i).state = 1
    lv.stray.i = i
    lv.stray.st = 1
    lv.stray.x = lv.ob(i).at.x
    lv.stray.y = lv.ob(i).at.y
    lv.ob(i).state = 0
    lv.ob(i).at.x = lv.ob(i).home
    p.goat = -1
    p.ride_t = 0
    p.at.x = lv.stray.x
    p.at.y = lv.stray.y - 1
    p.face = 0
    p.vel.x = GOATV
    p.vel.y = 60
    p.roll = 0
    p.jumped = 0
    Sfx mu, se, SBUMP
end sub

' The goat let go runs home along the ground; there, or out of sight, it is at graze again.
sub Homeward (lv as Level, g as Game)
    dim y as integer
    lv.stray.x = lv.stray.x + sgn(lv.ob(lv.stray.i).home - lv.stray.x) * STRAYV * STEPT
    y = Surf%(lv, cint(lv.stray.x), cint(lv.stray.y))
    if y <> NONE then lv.stray.y = y
    if abs(lv.stray.x - lv.ob(lv.stray.i).home) >= 6 and abs(lv.stray.x - g.cam.x - SW \ 2) <= SW then exit sub
    lv.ob(lv.stray.i).state = 1
    lv.stray.st = 0
end sub

' An ice wall across the road: he is stopped at it; a goat ridden goes through, and leaves its stump.
sub MeetWall (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
    if lv.ob(i).state = 0 or abs(lv.ob(i).at.x - p.at.x) >= WALLR then exit sub
    if p.at.y <= lv.ob(i).at.y - WALLH or p.at.y > lv.ob(i).at.y + 20 then exit sub
    if p.goat then
        lv.ob(i).state = 0
        g.score = g.score + 100
        g.shake = .2
        Burst se, pt(), csng(lv.ob(i).at.x), csng(lv.ob(i).at.y - WALLH \ 2), 16, 180, CWHITE
        Sfx mu, se, SCRACK
        exit sub
    end if
    Balk p, lv.ob(i).at.x, WALLR
end sub

' A slid load's heap across the road: he is stopped at its flank, nearer its middle the higher his feet, and
' over it goes free.
sub MeetHeap (lv as Level, p as Hero, i as integer)
    dim h as single, r as integer
    if lv.ob(i).state = 0 then exit sub
    h = lv.ob(i).at.y - p.at.y
    if h >= HEAPH or h < -20 then exit sub
    if h < 0 then h = 0
    r = cint(HEAPR * (1 - h / HEAPH))
    if abs(p.at.x - lv.ob(i).at.x) < r then Balk p, lv.ob(i).at.x, r
end sub

' The goat not ridden within 150 of x, NONE when there is none.
function GoatNear% (lv as Level, x as integer)
    dim i as integer, c as integer
    GoatNear% = NONE
    c = (x - 220) \ TS
    if c < 0 then c = 0
    i = lv.first(c)
    do while i < lv.nObj
        if lv.ob(i).home > x + 220 then exit do
        if lv.ob(i).kind = KGOAT and lv.ob(i).state <> 0 and abs(lv.ob(i).at.x - x) < 150 then GoatNear% = i
        i = i + 1
    loop
end function

' A ledge's load at x, y on the screen: shaking once set off, sliding off, falling; nothing once it is down.
sub DrawLoad (lv as Level, art as Store, se as Session, i as integer, x as integer, y as integer)
    dim s as integer, jig as integer
    s = lv.ob(i).state
    if s < 0 then exit sub
    jig = s > 0 and s <= TREMBLE and (se.ticks and 2) <> 0
    Blit art, x - 34 + jig + Slid%(lv, i), y - 16 + Plunged%(s - TREMBLE - LOADSLIDE), ILOAD, 76, 22
end sub

' Goat i at x, y on the screen: its head up at graze and dazed, down while it gathers itself and charges.
sub DrawGoat (lv as Level, art as Store, se as Session, i as integer, x as integer, y as integer)
    dim s as integer, f as integer
    s = abs(lv.ob(i).state)
    if s = 0 then exit sub
    f = -2 * (lv.ob(i).state < 0)
    if s > 1 and s <= GCHARGE then
        Blit art, x - 17, y - 40, IGOATR + (5 + (s = GCHARGE) * ((se.ticks \ 6) and 1)) * 4 + f, 34, 58
    else
        Blit art, x - 17, y - 23, IGOAT + f, 34, 24
    end if
end sub

' The goat on its way home, as the ground lies under it.
sub DrawStray (lv as Level, art as Store, se as Session, cx as integer, cy as integer)
    dim x as integer, y as integer, f as integer
    x = cint(lv.stray.x)
    y = cint(lv.stray.y)
    f = ((Lie%(lv, x, y) + 2) * 2 + ((se.ticks \ 6) and 1)) * 4 - 2 * (lv.ob(lv.stray.i).home < x)
    Blit art, x - cx - 17, y - cy - 40, IGOATR + f, 34, 58
end sub

' Icicle i breaks.
private sub Shatter (lv as Level, mu as Deck, se as Session, pt() as Part, i as integer)
    Burst se, pt(), csng(lv.ob(i).at.x), csng(lv.ob(i).at.y + Fallen%(lv, i) + 12), 8, 110, CWHITE
    lv.ob(i).state = -1
    Sfx mu, se, SCRACK
end sub

' Icicle i falling: it breaks on the cat or the snow-thrower under it, which is beaten, or on the ground.
private sub Plunge (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, i as integer)
    dim j as integer, y as integer, d as integer
    y = lv.ob(i).at.y + Fallen%(lv, i) + 16
    for j = i - 4 to i + 4
        if j < 0 or j >= lv.nObj or lv.ob(i).state <= 0 then continue for
        d = (lv.ob(j).kind = KCAT or lv.ob(j).kind = KTHROW) and lv.ob(j).state <> 0
        if d = 0 or abs(lv.ob(j).at.x - lv.ob(i).at.x) >= 14 or y <= lv.ob(j).at.y - 16 then continue for
        Beaten lv, mu, se, g, pt(), j, 8
        Shatter lv, mu, se, pt(), i
    next
    if lv.ob(i).state > 0 and Solid%(lv, lv.ob(i).at.x, y) then Shatter lv, mu, se, pt(), i
end sub

' Whether snow-thrower i is up from behind its wall: open to a roll, and about to throw.
function Risen% (lv as Level, i as integer)
    Risen% = abs(lv.ob(i).state) <= THROWUP and abs(lv.ob(i).state) > THROWAT
end function

' Thrower i's snowball, from its paw, lobbed to land where Charlie is now; none if he is far.
private sub Hurl (lv as Level, i as integer, p as Hero)
    dim x as single, y as single
    if abs(p.at.x - lv.ob(i).at.x) > THROWR or abs(p.at.y - lv.ob(i).at.y) > 60 then exit sub
    x = lv.ob(i).at.x + 6 * sgn(lv.ob(i).state)
    y = lv.ob(i).at.y - 20
    Loft lv, x, y, (p.at.x - x) / LOBT, (p.at.y - 12 - y) / LOBT - GRAV * LOBT / 2, ILOB, CWHITE
end sub

' A snow-thrower met: ducked, its wall throws him back, neither hurt; up, it is a cat, beaten by a roll.
sub MeetThrow (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
    dim s as integer
    if lv.ob(i).state = 0 or abs(lv.ob(i).at.x - p.at.x) >= 16 or p.at.y < lv.ob(i).at.y - 22 or p.at.y > lv.ob(i).at.y + 8 then exit sub
    if Risen%(lv, i) then
        Tackle lv, mu, se, g, p, i, 8, lo(), pt()
        exit sub
    end if
    s = 1 + 2 * (p.at.x < lv.ob(i).at.x)
    p.at.x = lv.ob(i).at.x + s * 16
    if p.ground then
        p.gsp = s * 150
    else
        p.vel.x = s * 150
    end if
    Sfx mu, se, SBUMP
end sub

' Icicle i, loosed: it trembles a while, then falls.
sub Totter (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, i as integer)
    if lv.ob(i).state <= 0 then exit sub
    lv.ob(i).state = lv.ob(i).state + 1
    if lv.ob(i).state > TREMBLE then Plunge lv, mu, se, g, pt(), i
end sub

' Load i of snow, set off: a step of its trembling, its slide off the ledge and its fall. Down on the road it is
' gone and its heap lies there; Charlie under it is hurt.
sub Unload (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part, i as integer)
    dim h as integer
    if lv.ob(i).state <= 0 then exit sub
    lv.ob(i).state = lv.ob(i).state + 1
    h = HeapOf%(lv, i)
    if h = NONE then exit sub
    if lv.ob(i).at.y + Plunged%(lv.ob(i).state - TREMBLE - LOADSLIDE) < lv.ob(h).at.y then exit sub
    lv.ob(i).state = -1
    lv.ob(h).state = 1
    Bury lv, mu, se, g, pt(), h
    if abs(p.at.x - lv.ob(h).at.x) >= HEAPR or abs(p.at.y - lv.ob(h).at.y) >= HEAPH then exit sub
    Hurt mu, se, g, p, lv.ob(h).at.x, lo(), SHURT
end sub

' Heap h, come down: the cats and snow-throwers under it are beaten.
private sub Bury (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, h as integer)
    dim j as integer, foe as integer
    for j = h - 6 to h + 6
        if j < 0 or j >= lv.nObj then continue for
        foe = (lv.ob(j).kind = KCAT or lv.ob(j).kind = KTHROW) and lv.ob(j).state <> 0
        if foe = 0 or abs(lv.ob(j).at.x - lv.ob(h).at.x) >= HEAPR + 10 then continue for
        if abs(lv.ob(j).at.y - lv.ob(h).at.y) < 24 then Beaten lv, mu, se, g, pt(), j, 8
    next
    g.shake = .3
    Burst se, pt(), csng(lv.ob(h).at.x), csng(lv.ob(h).at.y - 12), 16, 160, CWHITE
    Sfx mu, se, SCRACK
end sub

' Snow-thrower i: a step of its turn, facing Charlie; and its throw.
sub TakeAim (lv as Level, i as integer, p as Hero)
    dim s as integer
    if lv.ob(i).state = 0 then exit sub
    s = abs(lv.ob(i).state) - 1
    if s = 0 then s = THROWC
    lv.ob(i).state = s * (1 + 2 * (p.at.x > lv.ob(i).at.x))
    if s = THROWAT then Hurl lv, i, p
end sub
