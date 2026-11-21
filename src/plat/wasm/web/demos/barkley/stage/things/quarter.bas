' The Quarter's things' rules: the clotheslines and their
' rope-walkers, pouncers, snatchers, tippers and mortars.
defint A-Z
option explicit

from engine.events import Events
from barkley.model.events import EVBURST, EVSOUND, Tell, TellAt
from barkley.model.level import GRAV, Level, NONE, Rope
from barkley.model.hero import Hero
from barkley.model.game import Loose, NLOOSE, PULSES, Session, TOONEW
from barkley.assets.tunes import SMORTAR, SPOP, SSNATCH
from barkley.stage.things.react import BEAT, Paced, Prowl
from barkley.stage.fx.parts import Pick, PPOT, PSNATCH

' A clothesline: how far its middle hangs under its ends;
' his speed up off the first he comes down on, what each
' more in a row adds, and the row that adds no more; how far
' it gives under him, and the things' steps it swings; how
' far apart its washing is pegged.
const LINESAG = 5
const LINEV = 300!
const LINEMORE = 100!
const LINEROW = 3
private const LINEGIVE = 8
const LINET = 30
const WASH = 22

' A rope-walker: how near its line's ends it walks.
const WALKIN = 14

' A snatcher: the pixels a things' step it runs to a loose
' bone, and how near one it takes it.
const SNATCHV = 3
const SNATCHR = 8

' A tipper's pot leaves its sill on its own beat of every
' MORTARBAR, the one its state names, as a mortar fires. It
' falls SILLUP, POTX from the house's middle, and breaks on
' the paving; on its way it hurts whom it meets within POTR.
' The sill is bare for two beats, and the beat before the
' next the cat's paw is out. For the pilot the way under is
' clear once a pot has broken, while there are CROSST
' seconds till the next.
const SILLUP = 70
const POTX = 7
const POTR = 10
const CROSST = .8

' A mortar fires on its own beat of every MORTARBAR, the one
' its state names; its flash burns FLASHP pulses, a quarter
' beat. Its mouth is MORTARH over the roof. Who is within
' MORTARW of its middle and less than MORTARREACH over its
' mouth as it fires goes up at MORTARV; who is within BLASTW
' beside it burns. Its shell bursts SHELLUP up.
const MORTARBAR = 4
const FLASHP = 900
const MORTARH = 14
const MORTARW = 12
const MORTARREACH = 48
const MORTARV = 620!
const BLASTW = 18
const SHELLUP = 96

' A pouncer: how near its bin he comes before it leaps; its
' leap's reach, a cat's beat's, its height and things'
' steps; its state landed, and beaten.
const POUNCER = 56
const POUNCEFAR = 40
const POUNCEN = 20
private const POUNCEUP = 26
const POUNCEOUT = 100
const POUNCEGONE = 30000

' Line r's height u along it: hung, and once trodden given
' where it was, swinging less each step. A lantern line is
' taut from end to end.
function LineY% (r as Rope, u as integer)
    dim y as single, d as single

    if r.fall then
        LineY% = r.at.y + (clng(r.fall) * u + r.w \ 2) \ r.w
        exit function
    end if

    y = r.at.y + LINESAG * 4! * u * (r.w - u) / r.w / r.w

    if r.t > 0 then
        d = u / r.trod
        if u > r.trod then d = (r.w - u) / (r.w - r.trod)
        y = y + LINEGIVE * d * r.t / LINET * cos((LINET - r.t) * .7)
    end if

    LineY% = cint(y)
end function

' Line r trodden u along it: it swings.
sub Twang (r as Rope, u as integer)
    r.trod = u
    r.t = LINET
end sub

' How high a pouncer is over the roof n steps into its leap.
private function Arc% (n as integer)
    Arc% = POUNCEUP * 4 * n * (POUNCEN - n) \ (POUNCEN * POUNCEN)
end function

' How high pouncer i is over its roof now: in its leap, on
' its arc.
function Leapt% (lv as Level, i as integer)
    Leapt% = 0

    if abs(lv.things(i).state) > POUNCEN then exit function

    Leapt% = Arc%(abs(lv.things(i).state) - 1)
end function

' A pouncer's step. In its bin, state 0, till he comes
' within POUNCER of it; then it leaps at him, its state one
' more than the steps of its leap made, signed the way it
' goes; landed, POUNCEOUT that way, it prowls as a cat of
' the Mob does; beaten, POUNCEGONE.
sub Pounce (lv as Level, i as integer, p as Hero)
    dim n as integer, d as integer

    if lv.things(i).state = POUNCEGONE then exit sub

    if lv.things(i).state = 0 then
        if abs(p.at.x - lv.things(i).home) < POUNCER _
            and abs(p.at.y - lv.things(i).at.y) < 40 then
            Spring lv, i, p
        end if

        exit sub
    end if

    n = abs(lv.things(i).state)
    d = sgn(lv.things(i).state)

    if n < POUNCEN then
        lv.things(i).at.x = lv.things(i).home _
            + d * POUNCEFAR * n \ POUNCEN
        lv.things(i).state = d * (n + 1)
        exit sub
    end if

    lv.things(i).state = d
    Prowl lv, i
    lv.things(i).state = lv.things(i).state * POUNCEOUT
end sub

' Pouncer i out of its bin, at him.
sub Spring (lv as Level, i as integer, p as Hero)
    lv.things(i).state = 1
    if p.at.x < lv.things(i).home then lv.things(i).state = -1
end sub

' The pulses since mortar i last fired.
function Fired& (lv as Level, se as Session, i as integer)
    dim q as long

    q = (se.pulse - clng(lv.things(i).state) _
        * PULSES) mod (clng(MORTARBAR) * PULSES)
    if q < 0 then q = q + clng(MORTARBAR) * PULSES
    Fired& = q
end function

' How far tipper i's pot has fallen from its sill; NONE with
' none in the air.
function PotFall% (lv as Level, se as Session, i as integer)
    dim t as single, d as single

    PotFall% = NONE
    t = Fired&(lv, se, i) / (se.tempo * 60!)
    d = GRAV / 2 * t * t
    if d < SILLUP then PotFall% = cint(d)
end function

' Whether the way under tipper i is clear to set off
' through.
function PotClear% (lv as Level, se as Session, i as integer)
    dim q as long

    q = Fired&(lv, se, i)
    PotClear% = PotFall%(lv, se, i) = NONE _
        and q < clng(MORTARBAR) * PULSES _
        - clng(CROSST * 60 * se.tempo)
end function

' A tipper's step: its pot breaks at the step it lands.
sub Tipper (lv as Level, ev as Events, se as Session, i as integer)
    dim q as long, land as long

    q = Fired&(lv, se, i)
    land = clng(sqr(2! * SILLUP / GRAV) * 60 * se.tempo)
    if q < land or q >= land + se.tempo then exit sub

    TellAt ev, EVBURST, lv.things(i).at.x + POTX, _
        lv.things(i).at.y - 3, PPOT
    Tell ev, EVSOUND, SPOP
end sub

' A mortar's step: its thump at the step it fires.
sub Mortar (lv as Level, ev as Events, se as Session, i as integer)
    if Fired&(lv, se, i) < 2 * se.tempo then Tell ev, EVSOUND, SMORTAR
end sub

' The loose bone nearest snatcher i of those to be taken
' that are within its beat and by its height; NONE with
' none.
private function Spied% (lv as Level, i as integer, lo() as Loose)
    dim j as integer, d as integer, best as integer

    Spied% = NONE
    best = 2 * BEAT + 1

    for j = 0 to NLOOSE - 1
        if lo(j).t <= 0 or lo(j).t >= TOONEW _
            or abs(lo(j).at.x - lv.things(i).home) _
            > BEAT then continue for

        d = abs(lo(j).at.x - lv.things(i).at.x)
        if d >= best _
            or abs(lo(j).at.y - lv.things(i).at.y _
            + 6) >= 14 then continue for

        best = d
        Spied% = j
    next
end function

' Snatcher i's step: a loose bone within its beat it runs
' to, SNATCHV a cat's pace, and takes for good; with none,
' or startled, it is a cat.
sub Snatch ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    i as integer, _
    lo() as Loose _
)
    dim b as integer, n as integer

    b = NONE

    if se.loose_n > 0 and abs(lv.things(i).state) = 1 then
        b = Spied%(lv, i, lo())
    end if

    if b = NONE then
        Prowl lv, i
        exit sub
    end if

    if abs(lo(b).at.x - lv.things(i).at.x) >= 1 then
        lv.things(i).state = sgn(lo(b).at.x - lv.things(i).at.x)
    end if

    for n = 1 to SNATCHV
        if abs(lo(b).at.x - lv.things(i).at.x) < SNATCHR then exit for
        if not Paced%(lv, i) then exit for
    next

    if abs(lo(b).at.x - lv.things(i).at.x) >= SNATCHR then exit sub

    Pick lo(), se, b
    TellAt ev, EVBURST, lo(b).at.x, lo(b).at.y, PSNATCH
    Tell ev, EVSOUND, SSNATCH
end sub

' The lines' step: one that swings, a step nearer rest; the
' cat on one, a step along it.
sub Settle (lv as Level)
    dim i as integer

    for i = 0 to lv.n_ropes - 1
        if lv.ropes(i).t > 0 then lv.ropes(i).t = lv.ropes(i).t - 1
        if lv.ropes(i).cat then Tread lv, lv.ropes(i)
    next
end sub

' The cat walking line r: a pixel a step along its beat,
' turned at the beat's end and WALKIN from the line's;
' startled, it stands and counts its state down to a walk's,
' as a cat does. At the line's height, as it swings.
private sub Tread (lv as Level, r as Rope)
    dim i as integer, x as integer

    i = r.cat - 1
    if lv.things(i).state = 0 then exit sub

    x = lv.things(i).at.x + lv.things(i).state

    if abs(lv.things(i).state) > 1 then
        lv.things(i).state = lv.things(i).state _
            - sgn(lv.things(i).state)
    elseif abs(x - lv.things(i).home) > BEAT or x < r.at.x + WALKIN _
        or x > r.at.x + r.w - WALKIN then
        lv.things(i).state = -lv.things(i).state
    else
        lv.things(i).at.x = x
    end if

    lv.things(i).at.y = LineY%(r, lv.things(i).at.x - r.at.x)
end sub
