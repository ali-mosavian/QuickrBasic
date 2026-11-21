' The wood's things' rules: a bramble grown back, a
' squirrel's acorns, the swarms after Charlie.
defint A-Z
option explicit

from engine.clock import STEPT
from engine.events import Events
from barkley.model.events import EVBURST, TellAt
from barkley.model.game import Game, Loose, Session
from barkley.model.hero import Hero
from barkley.model.images import IACORN
from barkley.model.level import Level, Swarm
from barkley.assets.tunes import SHURT
from barkley.stage.fx.damage import Hurt
from barkley.stage.fx.lobs import Loft
from barkley.stage.fx.parts import PACORN, PLEAF

' A bramble: the things' steps till it has grown back.
const BRAMBLET = 90

' A squirrel on a bough: how far it runs either side of its
' home, and how far past that it follows him with its eye;
' its run a things' step; the steps between its acorns, the
' last of them stood with one held out, and how near under
' it he must be for it to fall; the steps a bark keeps it
' cowed; an acorn's colour.
const SQRUN = 48
private const SQSEE = 100
const SQV = 3
const SQWAIT = 60
const SQAIM = 15
const SQNEAR = 24

' A swarm: how near it sees him, its speed, how near it
' stings and how far behind it gives up.
private const BEESEE = 60
private const BEEV = 170!
private const BEER = 12
private const BEELOSE = 160

' Bramble's step: just cut, its leaves fly; cut, it grows
' back BRAMBLET steps on.
sub Regrow (lv as Level, ev as Events, i as integer)
    if lv.things(i).state = 0 then exit sub

    if lv.things(i).state = BRAMBLET then
        TellAt ev, EVBURST, csng(lv.things(i).at.x), _
            csng(lv.things(i).at.y - 12), PLEAF
    end if

    lv.things(i).state = lv.things(i).state - 1
end sub

sub Swarms ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    lo() as Loose _
)
    dim i as integer

    for i = 0 to lv.n_swarms - 1
        Buzz ev, se, lv.swarms(i), g, p, lo()
    next
end sub

' A swarm's step: he comes within BEESEE of it and it is
' after his head, at BEEV; on him, it stings; left BEELOSE
' behind, it goes home.
private sub Buzz ( _
    ev as Events, _
    se as Session, _
    b as Swarm, _
    g as Game, _
    p as Hero, _
    lo() as Loose _
)
    dim dx as single, dy as single, d as single

    dx = p.at.x - b.at.x
    dy = p.at.y - 13 - b.at.y
    d = sqr(dx * dx + dy * dy)
    if d < BEESEE then b.st = 1
    if b.st = 1 and d > BEELOSE then b.st = 2
    if b.st = 0 then exit sub

    if b.st = 1 and d < BEER + BEEV * STEPT then
        Hurt ev, se, g, p, cint(b.at.x), lo(), SHURT
    end if

    if b.st = 2 then
        dx = b.home.x - b.at.x
        dy = b.home.y - b.at.y
        d = sqr(dx * dx + dy * dy)
    end if

    if d > BEEV * STEPT then
        b.at.x = b.at.x + dx * BEEV * STEPT / d
        b.at.y = b.at.y + dy * BEEV * STEPT / d
        exit sub
    end if

    b.at.x = b.at.x + dx
    b.at.y = b.at.y + dy
    if b.st = 2 then b.st = 0
end sub

' Squirrel i on its bough: while Charlie is under its run it
' keeps over him, else it goes home; every SQWAIT steps it
' stops, an acorn held out the last SQAIM of them, and lets
' it fall once he is near under it. Cowed, it keeps still.
' Its state: the steps till the acorn falls, signed the way
' it faces.
sub Scamper (lv as Level, i as integer, p as Hero)
    dim n as integer, d as integer, perch as integer
    dim quarry as integer

    n = abs(lv.things(i).state) - 1
    d = sgn(lv.things(i).state)
    quarry = abs(p.at.x - lv.things(i).home) < SQRUN + SQSEE _
        and p.at.y > lv.things(i).at.y

    if n = 0 and quarry _
        and abs(p.at.x - lv.things(i).at.x) >= SQNEAR then
        n = 1
    end if

    if n = 0 then
        if quarry then
            Loft lv, lv.things(i).at.x + 5 * d, _
                lv.things(i).at.y + 1, 0, 0, IACORN, PACORN
        end if

        n = SQWAIT
    end if

    lv.things(i).state = d * n
    if n <= SQAIM or n > SQWAIT then exit sub

    perch = lv.things(i).home
    if quarry then perch = cint(p.at.x)

    if perch < lv.things(i).home - SQRUN then
        perch = lv.things(i).home - SQRUN
    end if

    if perch > lv.things(i).home + SQRUN then
        perch = lv.things(i).home + SQRUN
    end if

    if abs(perch - lv.things(i).at.x) < SQV then exit sub

    d = sgn(perch - lv.things(i).at.x)
    lv.things(i).at.x = lv.things(i).at.x + d * SQV
    lv.things(i).state = d * n
end sub
