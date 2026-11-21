' The gulls' rule: hover, dive at Charlie's bones, climb
' home with them.
defint A-Z
option explicit

from engine.clock import STEPT
from engine.events import Events
from barkley.model.level import Bird, Level
from barkley.model.events import EVBURST, EVSCORE, EVSOUND, Tell
from barkley.model.events import TellAt
from barkley.model.game import Game, Loose, Session
from barkley.model.hero import Hero
from barkley.assets.tunes import SPOP, SSQUAWK
from barkley.stage.fx.parts import PBEATEN, Strew
from barkley.stage.things.react import GSCARED

private const GULLSEE = 100
private const DIVEV = 210!
private const CLIMBV = 90!
private const GULLREST = 2.5
private const GULLTAKE = 5
const GHOVER = 1
const GDIVE = 2
const GCLIMB = 3

' Gull b toward x, y at v: there, or nearer. Whether it is
' there.
private function Glide% ( _
    b as Bird, _
    x as single, _
    y as single, _
    v as single _
)
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

' Gull b, the n'th, for a step. It hovers about its home
' till Charlie, with bones, passes under: then dives at
' where he will be, takes bones if it reaches him, and
' climbs home with them, to dive no more. A bark scares it
' home and makes it drop what it took; a rolling Charlie
' knocks it out of the air.
private sub Gull ( _
    ev as Events, _
    se as Session, _
    b as Bird, _
    n as integer, _
    g as Game, _
    p as Hero, _
    lo() as Loose _
)
    dim x as single, y as single, lead as single

    x = b.home.x + 24 * sin(se.ticks * .02 + n * 2)
    y = b.home.y + 4 * sin(se.ticks * .07 + n)

    select case b.st
    case GHOVER
        b.face = cos(se.ticks * .02 + n * 2) < 0
        b.at.x = x
        b.at.y = y
        if b.rest > 0 then b.rest = b.rest - STEPT

        if b.rest <= 0 and b.loot = 0 and g.bones > 0 _
            and abs(p.at.x - x) < GULLSEE and p.at.y > y + 30 then
            lead = (p.at.y - 13 - y) / DIVEV * 60
            b.aim.x = p.at.x + (p.at.x - p.last_x) * lead
            b.aim.y = p.at.y - 13
            b.st = GDIVE
            Tell ev, EVSOUND, SSQUAWK
        end if
    case GDIVE
        if Glide%(b, b.aim.x, b.aim.y, DIVEV) then b.st = GCLIMB
    case GCLIMB
        if Glide%(b, x, y, CLIMBV) then
            b.st = GHOVER
            b.rest = GULLREST
        end if
    case GSCARED
        Strew lo(), se, b.at.x, b.at.y, b.loot
        b.loot = 0
        b.st = GCLIMB
    end select

    if abs(b.at.x - p.at.x) >= 13 _
        or abs(b.at.y - p.at.y + 13) >= 15 then exit sub

    if p.roll or p.charging then
        Strew lo(), se, b.at.x, b.at.y, b.loot
        b.loot = 0
        b.st = 0
        Tell ev, EVSCORE, 100
        TellAt ev, EVBURST, b.at.x, b.at.y, PBEATEN
        Tell ev, EVSOUND, SPOP
    elseif b.st = GDIVE then
        b.loot = g.bones
        if b.loot > GULLTAKE then b.loot = GULLTAKE
        g.bones = g.bones - b.loot
        b.st = GCLIMB
        Tell ev, EVSOUND, SSQUAWK
    end if
end sub

sub Flock ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    lo() as Loose _
)
    dim i as integer

    for i = 0 to lv.n_gulls - 1
        if lv.gulls(i).st <= 0 then continue for

        Gull ev, se, lv.gulls(i), i, g, p, lo()
    next
end sub

' The gulls home, hovering, with nothing.
sub Rehome (lv as Level)
    dim i as integer

    for i = 0 to lv.n_gulls - 1
        lv.gulls(i).at.x = lv.gulls(i).home.x
        lv.gulls(i).at.y = lv.gulls(i).home.y
        lv.gulls(i).rest = 0
        lv.gulls(i).st = GHOVER
        lv.gulls(i).loot = 0
    next
end sub
