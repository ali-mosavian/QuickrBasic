' The Pass's things' rules: icicles and loads of snow let
' go, goats at graze and bolting, snow-throwers' aim, the
' avalanche.
defint A-Z
option explicit

from engine.clock import STEPT
from engine.events import Events
from engine.video import SW
from barkley.model.events import EVBURST, EVSHAKE, EVSOUND, Tell
from barkley.model.events import TellAt
from barkley.model.game import Game, Loose, Session
from barkley.model.level import GRAV, KHEAP, Level, NONE
from barkley.model.hero import Hero
from barkley.model.images import ILOB
from barkley.model.kinds import Kinds
from barkley.assets.tunes import SCRACK, SHURT
from barkley.world.level import LOADFAR
from barkley.world.terrain.ground import Solid, Surf
from barkley.stage.fx.damage import Beaten, Hurt, Lose
from barkley.stage.hero.modes import GUSTP, Lull
from barkley.stage.fx.lobs import Loft
from barkley.stage.fx.parts import PBURY, PSHATTER, PSNOWBALL
from barkley.stage.hero.moves import THROWC

' A snow-thrower, a cat behind a wall of snow, its state's
' sign the way it faces: its turn is THROWC of the things'
' steps, ducked till THROWUP are left, then up and open;
' with THROWAT left it throws, if Charlie is within THROWR,
' a snowball LOBT in the air, and ducks. NLOB of them fly at
' once.
const THROWUP = 30
const THROWAT = 10
private const THROWR = 180
private const LOBT = .9

' An icicle: its state 0 hung, the things' steps since it
' was loosed, or -1 broken. It trembles TREMBLE of them,
' then falls.
const TREMBLE = 10

' A goat, its state's sign the way it faces: 1 at graze, at
' home, till it sees him within GOATSEE on its ledge; then
' its head is down GOATWIND of the things' steps and it
' charges, GOATSTEP a step, till it is under or past him or
' GOATRUN from home, and stands dazed GOATDAZE steps; 0
' while it is ridden or on its way home. Its lowered horns
' hurt. Come down on, it bolts on along the road, to the
' right, at GOATV, he astride GOATH up, till he is off the
' ground, stopped or GOATRIDE seconds on; then it runs home
' at STRAYV.
private const GOATSEE = 72
private const GOATWIND = 12
const GCHARGE = 100
private const GOATDAZE = 30
private const GOATSTEP = 8
private const GOATRUN = 64
private const STRAYV = 300!

' The avalanche: let go as he passes lv.avalX, its front
' comes on from AVALLEAD behind at AVALV, the snow AVALH
' deep behind it, and takes him where it reaches him.
private const AVALV = 300!
private const AVALLEAD = 220
const AVALH = 70

' A ledge's load of snow: its state 0 heaped, the things'
' steps since a bark set it off, or -1 gone. It trembles
' TREMBLE of them, slides LOADRUN along its ledge in
' LOADSLIDE more, gathering way, then falls at the speed it
' left with, to its heap's place LOADFAR on: there cats and
' snow-throwers are beaten. Its heap: its state 1 once it
' lies there, HEAPH tall and HEAPR either way at its foot;
' it stops him.
const LOADRUN = 40
const LOADSLIDE = 20
const HEAPH = 30
const HEAPR = 24

' The avalanche's step: let go as he passes its head, its
' front comes on, the ground shaking while it is in view,
' and takes him where it reaches him.
sub Avalanche ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero _
)
    if lv.aval.st = 0 then
        if p.at.x < lv.aval_x then exit sub

        lv.aval.st = 1
        lv.aval.x = lv.aval_x - AVALLEAD
        Tell ev, EVSHAKE, 50
        Tell ev, EVSOUND, SCRACK
    end if

    lv.aval.x = lv.aval.x + AVALV * STEPT

    if (se.ticks and 15) = 0 and lv.aval.x > g.cam.x - 40 then
        Tell ev, EVSHAKE, 12
    end if

    if lv.aval.x > p.at.x - 6 then Lose ev, se, p
end sub

' A goat's step: off its place it trots home; at graze there
' it faces him and, seeing him on its ledge, lowers its
' head, in a blizzard only with the calm left to charge in;
' then it charges till it is under or past him or its ledge
' ends, and stands dazed.
sub Graze (lv as Level, se as Session, i as integer, p as Hero)
    dim s as integer, f as integer, x as integer

    s = abs(lv.things(i).state)
    f = sgn(lv.things(i).state)

    select case s
    case 1
        if lv.things(i).at.x <> lv.things(i).home then
            f = sgn(lv.things(i).home - lv.things(i).at.x)
            lv.things(i).at.x = lv.things(i).at.x + f * GOATSTEP \ 2
        else
            f = 1 + 2 * (p.at.x < lv.things(i).at.x)
            x = abs(p.at.x - lv.things(i).at.x) < GOATSEE _
                and abs(p.at.y - lv.things(i).at.y) < 30

            if x and p.goat = 0 _
                and (not lv.gale or Lull%(lv, se) > GUSTP \ 2) then
                s = GOATWIND + 1
            end if
        end if
    case 2
        s = GCHARGE
    case GCHARGE
        x = lv.things(i).at.x + f * GOATSTEP

        if (p.at.x - x) * f < 0 _
            or abs(x - lv.things(i).home) > GOATRUN _
            or Surf%(lv, x + f * 12, _
            lv.things(i).at.y) <> lv.things(i).at.y then
            s = GCHARGE + GOATDAZE
        else
            lv.things(i).at.x = x
        end if
    case else
        s = s - 1
        if s = GCHARGE then s = 1
    end select

    lv.things(i).state = s * f
end sub

' Goat i bolts on along the road, ridden. A goat still on
' its way home is there.
sub Bolt (lv as Level, i as integer)
    if lv.stray.st then lv.things(lv.stray.goat).state = 1
    lv.stray.goat = i
    lv.stray.st = 1
    lv.stray.at.x = lv.things(i).at.x
    lv.stray.at.y = lv.things(i).at.y
    lv.things(i).state = 0
    lv.things(i).at.x = lv.things(i).home
end sub

' The goat let go runs home along the ground; there, or out
' of sight, it is at graze again.
sub Homeward (lv as Level, g as Game)
    dim y as integer

    lv.stray.at.x = lv.stray.at.x _
        + sgn(lv.things(lv.stray.goat).home - lv.stray.at.x) _
        * STRAYV * STEPT
    y = Surf%(lv, cint(lv.stray.at.x), cint(lv.stray.at.y))
    if y <> NONE then lv.stray.at.y = y
    if abs(lv.stray.at.x - lv.things(lv.stray.goat).home) >= 6 _
        and abs(lv.stray.at.x - g.cam.x - SW \ 2) <= SW then exit sub

    lv.things(lv.stray.goat).state = 1
    lv.stray.st = 0
end sub

' How far icicle i has fallen: nothing till it has trembled.
function Fallen% (lv as Level, i as integer)
    Fallen% = Plunged%(lv.things(i).state - TREMBLE)
end function

' How far a thing has fallen, n of the things' steps into
' its fall.
function Plunged% (n as integer)
    dim t as single

    Plunged% = 0
    if n <= 0 then exit function

    t = n * STEPT * 2
    Plunged% = cint(GRAV * t * t / 2)
end function

' Load i's heap, the next along; NONE without one.
function HeapOf% (lv as Level, i as integer)
    dim j as integer

    HeapOf% = NONE

    for j = i + 1 to lv.n_things - 1
        if lv.things(j).home > lv.things(i).home _
            + LOADFAR then exit for
        if lv.things(j).kind <> KHEAP then continue for

        HeapOf% = j
        exit for
    next
end function

' Whether snow-thrower i is up from behind its wall: open to
' a roll, and about to throw.
function Risen% (lv as Level, i as integer)
    Risen% = abs(lv.things(i).state) <= THROWUP _
        and abs(lv.things(i).state) > THROWAT
end function

' Icicle i breaks.
sub Shatter (lv as Level, ev as Events, i as integer)
    TellAt ev, EVBURST, csng(lv.things(i).at.x), _
        csng(lv.things(i).at.y + Fallen%(lv, i) + 12), PSHATTER
    lv.things(i).state = -1
    Tell ev, EVSOUND, SCRACK
end sub

' Icicle i falling: it breaks on the cat or the snow-thrower
' under it, which is beaten, or on the ground.
private sub Plunge ( _
    lv as Level, _
    kd as Kinds, _
    ev as Events, _
    i as integer _
)
    dim j as integer, y as integer, d as integer

    y = lv.things(i).at.y + Fallen%(lv, i) + 16

    for j = i - 4 to i + 4
        if j < 0 or j >= lv.n_things _
            or lv.things(i).state <= 0 then continue for

        d = kd.k(lv.things(j).kind).crushed _
            and lv.things(j).state <> 0
        if d = 0 _
            or abs(lv.things(j).at.x - lv.things(i).at.x) >= 14 _
            or y <= lv.things(j).at.y - 16 then continue for

        Beaten lv, ev, j, 8
        Shatter lv, ev, i
    next

    if lv.things(i).state <= 0 _
        or Solid%(lv, lv.things(i).at.x, y) = 0 then exit sub

    Shatter lv, ev, i
end sub

' Thrower i's snowball, from its paw, lobbed to land where
' Charlie is now; none if he is far.
private sub Hurl (lv as Level, i as integer, p as Hero)
    dim x as single, y as single

    if abs(p.at.x - lv.things(i).at.x) > THROWR _
        or abs(p.at.y - lv.things(i).at.y) > 60 then exit sub

    x = lv.things(i).at.x + 6 * sgn(lv.things(i).state)
    y = lv.things(i).at.y - 20
    Loft lv, x, y, (p.at.x - x) / LOBT, _
        (p.at.y - 12 - y) / LOBT - GRAV * LOBT / 2, ILOB, PSNOWBALL
end sub

' Icicle i, loosed: it trembles a while, then falls.
sub Totter (lv as Level, kd as Kinds, ev as Events, i as integer)
    if lv.things(i).state <= 0 then exit sub

    lv.things(i).state = lv.things(i).state + 1
    if lv.things(i).state > TREMBLE then Plunge lv, kd, ev, i
end sub

' Load i of snow, set off: a step of its trembling, its
' slide off the ledge and its fall. Down on the road it is
' gone and its heap lies there; Charlie under it is hurt.
sub Unload ( _
    lv as Level, _
    kd as Kinds, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    lo() as Loose, _
    i as integer _
)
    dim h as integer

    if lv.things(i).state <= 0 then exit sub

    lv.things(i).state = lv.things(i).state + 1
    h = HeapOf%(lv, i)
    if h = NONE then exit sub
    if lv.things(i).at.y _
        + Plunged%(lv.things(i).state - TREMBLE - LOADSLIDE) _
        < lv.things(h).at.y then exit sub

    lv.things(i).state = -1
    lv.things(h).state = 1
    Bury lv, kd, ev, h
    if abs(p.at.x - lv.things(h).at.x) >= HEAPR _
        or abs(p.at.y - lv.things(h).at.y) >= HEAPH then exit sub

    Hurt ev, se, g, p, lv.things(h).at.x, lo(), SHURT
end sub

' Heap h, come down: the cats and snow-throwers under it are
' beaten.
private sub Bury ( _
    lv as Level, _
    kd as Kinds, _
    ev as Events, _
    h as integer _
)
    dim j as integer, foe as integer

    for j = h - 6 to h + 6
        if j < 0 or j >= lv.n_things then continue for

        foe = kd.k(lv.things(j).kind).crushed _
            and lv.things(j).state <> 0
        if foe = 0 _
            or abs(lv.things(j).at.x - lv.things(h).at.x) >= HEAPR _
            + 10 then continue for

        if abs(lv.things(j).at.y _
            - lv.things(h).at.y) >= 24 then continue for

        Beaten lv, ev, j, 8
    next

    Tell ev, EVSHAKE, 30
    TellAt ev, EVBURST, csng(lv.things(h).at.x), _
        csng(lv.things(h).at.y - 12), PBURY
    Tell ev, EVSOUND, SCRACK
end sub

' Snow-thrower i: a step of its turn, facing Charlie; and
' its throw.
sub TakeAim (lv as Level, i as integer, p as Hero)
    dim s as integer

    if lv.things(i).state = 0 then exit sub

    s = abs(lv.things(i).state) - 1
    if s = 0 then s = THROWC
    lv.things(i).state = s * (1 + 2 * (p.at.x > lv.things(i).at.x))
    if s = THROWAT then Hurl lv, i, p
end sub
