' The things' rules: each thing near the view, a step in
' two, moved by its kind's mind; the clotheslines settled
' and the loose bones' fall.
defint A-Z
option explicit

from engine.video import SW
from engine.clock import STEPT
from engine.events import Events
from barkley.model.game import Game, Loose, Session
from barkley.model.hero import Hero
from barkley.model.level import Ground, KBRAMBLE, KBUOY, KGOAT
from barkley.model.level import KICICLE, KJELLY, KLOAD, KMORTAR
from barkley.model.level import KPOUNCE, KSLED, KSNATCH, KSPARK
from barkley.model.level import KSQUIRREL, KTHROW, KTIPPER, KVINE
from barkley.model.level import Level, TS
from barkley.model.kinds import Kinds, MOWN, MPROWL, MREST
from barkley.world.movers import Respan, Totters
from barkley.stage.zones.sands import Bob
from barkley.stage.fx.parts import LooseBones
from barkley.stage.things.react import Prowl
from barkley.stage.things.wood import Regrow, Scamper
from barkley.stage.things.pass import Graze, TakeAim, Totter, Unload
from barkley.stage.things.quarter import Mortar, Pounce, Settle
from barkley.stage.things.quarter import Snatch, Tipper
from barkley.stage.things.sands import Rehome

' A spark's reach from home; a jellyfish's depth.
const SPARKR = 40
const JELLYD = 10

sub Things ( _
    lv as Level, _
    kd as Kinds, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    lo() as Loose _
)
    dim i as integer, c as integer
    ' typed: a const made from a const and left typeless is
    ' rounded to a whole number, 0 here
    const DT! = STEPT * 2

    c = (cint(g.cam.x) - 100) \ TS
    if c < 0 then c = 0

    for i = lv.first(c) to lv.n_things - 1
        if lv.things(i).home > g.cam.x + SW + 100 then exit for

        select case kd.k(lv.things(i).kind).mind
        case MPROWL
            if lv.things(i).state <> 0 then Prowl lv, i
        case MREST
            if lv.things(i).state > 0 then
                lv.things(i).state = lv.things(i).state - 1
            end if
        case MOWN
            Think lv, kd, ev, se, g, p, lo(), i
        end select
    next

    if lv.n_ropes then Settle lv
    if se.loose_n then LooseBones lv, se, lo(), DT
end sub

' Thing i's own rule, a kind of its own.
private sub Think ( _
    lv as Level, _
    kd as Kinds, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    lo() as Loose, _
    i as integer _
)
    select case lv.things(i).kind
    case KSPARK
        lv.things(i).at.x = lv.things(i).home _
            + cint(SPARKR _
            * sin((se.ticks + lv.things(i).home) * .03))
    case KICICLE
        Totter lv, kd, ev, i
    case KLOAD
        Unload lv, kd, ev, se, g, p, lo(), i
    case KGOAT
        if lv.things(i).state <> 0 then Graze lv, se, i, p
    case KTHROW
        TakeAim lv, i, p
    case KSQUIRREL
        if lv.things(i).state <> 0 then Scamper lv, i, p
    case KPOUNCE
        Pounce lv, i, p
    case KSNATCH
        if lv.things(i).state <> 0 then
            Snatch lv, ev, se, i, lo()
        end if
    case KMORTAR
        Mortar lv, ev, se, i
    case KTIPPER
        Tipper lv, ev, se, i
    case KBRAMBLE
        Regrow lv, ev, i
    case KVINE
        if p.vine <> i + 1 then
            lv.things(i).state = lv.things(i).state * 7 \ 8
        end if
    case KBUOY
        lv.things(i).at.y = Bob%(lv, lv.things(i).at.x, -4, 9)

        if lv.things(i).state > 0 then
            lv.things(i).state = lv.things(i).state - 1
        end if
    case KJELLY
        lv.things(i).at.y = Bob%(lv, lv.things(i).at.x, JELLYD, 0)

        if lv.things(i).state > 1 then
            lv.things(i).state = lv.things(i).state - 1
        end if
    end select
end sub

' The level's things as it starts: each at home, as its kind
' starts unless it keeps how it was left; the gulls too.
sub Rewind (lv as Level, kd as Kinds)
    dim i as integer, k as integer

    for i = 0 to lv.n_things - 1
        lv.things(i).at.x = lv.things(i).home
        k = lv.things(i).kind
        if kd.k(k).keeps then continue for

        lv.things(i).state = kd.k(k).start
    next

    Rehome lv
end sub

' The pier's fallen spans back in their places, the swarms
' at home: Charlie starts again before them.
sub Mend (lv as Level)
    dim m as integer

    for m = 0 to lv.n_swarms - 1
        lv.swarms(m).at.x = lv.swarms(m).home.x
        lv.swarms(m).at.y = lv.swarms(m).home.y
        lv.swarms(m).st = 0
    next

    Respan lv

    for m = 0 to lv.n_things - 1
        if lv.things(m).kind = KSLED then lv.things(m).state = 0
        if lv.things(m).kind <> KGOAT then continue for

        lv.things(m).state = 1
        lv.things(m).at.x = lv.things(m).home
    next

    lv.stray.st = 0
    lv.aval.st = 0
end sub

' Each mover's thing where its mover went; a pier's span
' tottering as it does.
sub Borne (lv as Level, gr as Ground)
    dim m as integer, o as integer, s as integer

    for m = 0 to lv.n_movers - 1
        o = lv.movers(m).thing
        if lv.movers(m).kind = 6 then
            lv.things(o).state = Totters%(lv, m)
        end if

        if lv.movers(m).shift.x = 0 and lv.movers(m).shift.y = 0 then
            continue for
        end if

        s = lv.movers(m).sg
        lv.things(o).at.x = cint(gr.pt(s).at.x)
        lv.things(o).at.y = cint(gr.pt(s).at.y)
    next
end sub
