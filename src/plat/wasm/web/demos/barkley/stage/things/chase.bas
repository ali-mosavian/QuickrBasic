' The chase's rules: who flees, by its zone, bumped and
' broken, and what it leaves.
defint A-Z
option explicit

from engine.clock import STEPT
from engine.events import Events
from barkley.model.events import EVBURST, EVCHASETUNE, EVSCORE
from barkley.model.events import EVSHAKE, EVSOUND, EVTUNE, Tell
from barkley.model.events import TellAt
from barkley.model.level import GRAV, Level, NONE, Realm, TS
from barkley.model.hero import Hero
from barkley.model.game import PULSES, Session
from barkley.assets.tunes import SBUMP, SMARK, SPOP
from barkley.world.terrain.ground import Lie, Surf
from barkley.world.terrain.track import LimbUp, LIMBW, LimbX, NLIMB
from barkley.stage.fx.parts import PBOX, PCART, PCARTBUMP, PKEEP

' The great oak, its foot at lv.oakX, lv.oakY: NLIMB limbs
' up it, left and right in turn, each a flat LIMBW long to
' stand on, LIMBRISE down its picture. The Bramble twins sit
' on a pair of them, and after each catch on the pair above:
' one holds the ball PAWUP over its limb TWINHOLD seconds,
' then lobs it to the other, TWINLOB seconds on its way and
' LOBUP over the straight line, TWINHASTE less for each
' catch. Within CATCHR of the ball on its way, he has caught
' it.
const LIMBRISE = 14
private const TWINHOLD = .6
private const TWINLOB = 1.5
private const TWINHASTE = .2
private const LOBUP = 40
const PAWUP = 24
private const CATCHR = 18

' Kipper's pedal boat: its speed, what each bump adds to it,
' its length, and its deck over the sea.
const PEDALV = 130!
const PEDALMORE = 40!
const PEDALW = 48
const PEDALUP = 8

' A chase: the cart's speed, when Charlie is far behind it,
' and away from a bump, a toboggan's less, for a sled to
' catch it again; how far ahead of the race's start it
' waits; how near touches it; the seconds a bump tells, the
' last of them bolting; the bumps that break it; its tune's
' number in CHARLIE.DAT.
const CRUISE = 330!
private const DAWDLE = 200!
private const BOLT = 560!
private const TOBOLT = 400!
const CARTLEAD = 260
const CARTR = 26
const DAZED = 1.4
const BOLTS = 1!
const NBUMP = 3

' The works' crane: its speed after Charlie, what each bump
' adds, and away from one; how near touches it; its
' gondola's foot over the floor, out of a jump's reach, and
' its rail's top; the seconds to its next crate, what each
' bump takes off them, and the seconds one is held over
' Charlie; the view's height over him, by habit and under a
' crane; the seconds its keepsake falls.
private const CRANEV = 90!
private const CRANEUP = 25!
private const CRANEBOLT = 260!
const CRANER = 22
const CRANEH = 120
const RAILUP = 164
private const RELOAD = 1.6
private const HASTE = .3
private const POISE = .6
const TOOLFALL = .4

' The quarter's relay, the guitar from cat to cat, NLEG of
' them and a bump each: bumped, a cat throws it on to the
' next, PASSV a second, and is left TUMBLE behind it. Each
' makes off its own way, LEGV about: Biscuit steadily;
' Sprocket by the beat, WOUND the quicker for three and
' still for one; Kipper bobbing, a beat BOB the quicker and
' HOPUP off the ground, a beat BOB the slower; Frost
' sliding, SLIDE the quicker down a fall; the twins lobbing
' it between them, LOBV for LOBT seconds and LOBUP2 high, as
' Charlie comes within LOBNEAR, and not again for LOBWAIT.
' The twin without it goes TWINGAP from the one with it.
const NLEG = 5
const LEGV = 250!
const LOBWAIT = 2.4
const TUMBLE = 20
const PASSV = 440!
private const WOUND = 60!
private const BOB = 110!
const HOPUP = 10
private const SLIDE = 200!
private const LOBNEAR = 70
private const LOBV = 420!
const LOBT = .4
const LOBUP2 = 20
const TWINGAP = 40

' The chase's clock, a step on.
sub Unwind (lv as Level)
    if lv.cart.t > 0 then lv.cart.t = lv.cart.t - STEPT
end sub

' Who flees afloat, h under the sea's top.
sub Float (lv as Level, h as integer)
    lv.cart.at.y = lv.tide - h
end sub

' The cat bumped off the guitar left where it was, off its
' feet.
sub Unseat (lv as Level)
    lv.box.at.x = lv.cart.at.x - TUMBLE
    lv.box.at.y = lv.cart.at.y
    lv.box.t = 0
end sub

' Who flees turned about.
sub Reverse (lv as Level)
    lv.cart.v = -lv.cart.v
end sub

' The chase as before Charlie came to it: who flees waiting
' a little way into the race, the zone's tune back. A
' keepsake taken stays taken.
sub ResetChase (lv as Level, ev as Events)
    if lv.chase_x = NONE or lv.cart.st = 3 then exit sub

    lv.cart.st = 0
    lv.cart.hits = 0
    lv.cart.v = 0
    lv.cart.t = 0
    lv.cart.at.x = lv.chase_x + CARTLEAD
    lv.cart.at.y = lv.top(cint(lv.cart.at.x) \ TS) * TS
    lv.box.st = 0
    lv.box.t = RELOAD
    Tell ev, EVTUNE, lv.tune
end sub

' The cart is off, to the chase's tune.
sub StartChase (lv as Level, ev as Events)
    lv.cart.st = 1
    Tell ev, EVCHASETUNE, 0
end sub

' How fast who flees makes off from a bump.
function MakesOff! (zn as Realm)
    MakesOff! = BOLT
    if zn.id = 3 then MakesOff! = TOBOLT
end function

' The cart along the ground at `pace`: dawdling when Charlie
' is far behind it, slow while a bump has it dazed, then
' making off at `away`; no further than the race's end.
sub MoveCart ( _
    lv as Level, _
    p as Hero, _
    pace as single, _
    away as single _
)
    dim want as single, s as integer

    want = pace
    if lv.cart.at.x - p.at.x > 240 then want = DAWDLE
    if lv.cart.t > 0 then want = away
    if lv.cart.t > BOLTS then want = 60
    lv.cart.v = lv.cart.v + (want - lv.cart.v) * 5 * STEPT
    lv.cart.at.x = lv.cart.at.x + lv.cart.v * STEPT

    if lv.cart.at.x >= lv.chase_end then
        lv.cart.at.x = lv.chase_end
        lv.cart.v = 0
    end if

    s = Surf%(lv, cint(lv.cart.at.x), cint(lv.cart.at.y))
    if s <> NONE then lv.cart.at.y = s
end sub

' The bumps that end a zone's chase: the relay's is one a
' cat.
function Bumps% (zn as Realm)
    Bumps% = NBUMP
    if zn.id = 5 then Bumps% = NLEG
end function

' A bump told, its sparks at height y: who flees is dazed.
sub Bumped (lv as Level, ev as Events, y as single)
    lv.cart.hits = lv.cart.hits + 1
    lv.cart.t = DAZED
    Tell ev, EVSCORE, 500
    Tell ev, EVSHAKE, 20
    TellAt ev, EVBURST, lv.cart.at.x + 0, y, PCARTBUMP
    Tell ev, EVSOUND, SBUMP
end sub

' Who flees is broken and the keepsake left lying; the
' zone's tune again.
sub BreakCart (lv as Level, ev as Events)
    lv.cart.st = 2
    lv.cart.t = 2
    TellAt ev, EVBURST, lv.cart.at.x + 0, lv.cart.at.y - 12, PCART
    Tell ev, EVTUNE, lv.tune
end sub

sub TakeKeep (lv as Level, ev as Events)
    lv.cart.st = 3
    Tell ev, EVSCORE, 1000
    TellAt ev, EVBURST, lv.cart.at.x + 0, lv.cart.at.y - 8, PKEEP
    Tell ev, EVSOUND, SMARK
end sub

' A lob's clock: begun as Charlie comes near who has the
' guitar, no sooner than LOBWAIT after the last, and not at
' the street's end, where there is nowhere on to lob it.
' Only the twins lob by it.
sub Pitch (lv as Level, p as Hero)
    if lv.box.t > 0 then lv.box.t = lv.box.t - STEPT
    if lv.box.t > 0 or lv.cart.t > 0 _
        or lv.cart.at.x >= lv.chase_end then exit sub

    if lv.cart.at.x - p.at.x < LOBNEAR then lv.box.t = LOBWAIT
end sub

' Whether the guitar is in the air between the twins.
function Lobbed% (lv as Level)
    Lobbed% = lv.cart.hits = NLEG - 1 and lv.box.t > LOBWAIT - LOBT
end function

' The pace of who has the guitar, by the relay's leg.
function LegPace! (lv as Level, se as Session)
    dim beat as integer

    beat = (se.pulse \ PULSES) and 3
    LegPace! = LEGV

    select case lv.cart.hits
    case 1
        LegPace! = LEGV + WOUND
        if beat = 3 then LegPace! = 0
    case 2
        LegPace! = LEGV + BOB * (1 - 2 * (beat and 1))
    case 3
        if Lie%(lv, cint(lv.cart.at.x), cint(lv.cart.at.y)) < 0 then
            LegPace! = LEGV + SLIDE
        end if
    case 4
        if Lobbed%(lv) then LegPace! = LOBV
    end select
end function

' The twins' pair of limbs: one higher for each catch, the
' highest for the last.
function Roost% (lv as Level)
    Roost% = lv.cart.hits
    if lv.cart.hits > NLIMB \ 2 - 1 then Roost% = NLIMB \ 2 - 1
end function

' Where a twin sits on limb n, the middle of its flat.
function SeatX% (lv as Level, n as integer)
    SeatX% = lv.oak.x + LimbX%(n) + LIMBW \ 2
end function

function SeatY% (lv as Level, n as integer)
    SeatY% = lv.oak.y - LimbUp%(n)
end function

' The seconds the ball is in the air between the twins:
' fewer for each catch.
function LobTime! (lv as Level)
    LobTime! = TWINLOB - TWINHASTE * lv.cart.hits
end function

' The ball where it is, lv.cart its foot: over the paws of
' the twin that holds it, lv.box.st 0 the left and 2 the
' right; odd, on its way from that one to the other,
' lv.box.t seconds still to go; a catch still telling, up
' from where he caught it, lv.box.x, lv.box.y, to who holds
' it next.
sub BallAt (lv as Level)
    dim n as integer, x as single, y as single, u as single

    n = Roost%(lv) * 2 + lv.box.st \ 2
    x = SeatX%(lv, n)
    y = SeatY%(lv, n) - PAWUP

    if lv.cart.t > 0 then
        u = lv.cart.t / DAZED
        x = x + (lv.box.at.x - x) * u
        y = y + (lv.box.at.y - y) * u
    elseif lv.box.st and 1 then
        u = 1 - lv.box.t / LobTime!(lv)
        x = x + (SeatX%(lv, n xor 1) - x) * u
        y = y + (SeatY%(lv, n xor 1) - PAWUP - y) * u
    end if

    lv.cart.at.x = x
    lv.cart.at.y = y - LOBUP * 4 * u * (1 - u)
end sub

' The twins' game, a step: the ball held, then lobbed
' across, then held again; held too while a catch tells. At
' the ball on its way he has caught it: a bump, the twins
' off to the pair above and the ball tossed up after them.
' The third catch, and the ball is his.
sub Juggle (lv as Level, ev as Events, p as Hero)
    if lv.cart.t <= 0 then lv.box.t = lv.box.t - STEPT

    if lv.box.t <= 0 then
        lv.box.st = (lv.box.st + 1) and 3
        lv.box.t = TWINHOLD
        if lv.box.st and 1 then lv.box.t = LobTime!(lv)
    end if

    BallAt lv
    if (lv.box.st and 1) = 0 or p.dead then exit sub
    if abs(p.at.x - lv.cart.at.x) > CATCHR _
        or abs(p.at.y - 12 - (lv.cart.at.y - 7)) _
        > CATCHR then exit sub

    lv.box.at.x = lv.cart.at.x
    lv.box.at.y = lv.cart.at.y
    lv.box.st = 0
    lv.box.t = TWINHOLD
    Bumped lv, ev, lv.cart.at.y - 7
end sub

' Kipper's pedal boat on the sea, from one end of its
' channel to the other and back: faster for each bump it has
' taken, and still a moment after one.
sub MovePedalo (lv as Level)
    lv.cart.at.y = lv.tide - PEDALUP
    if lv.cart.t > BOLTS then exit sub

    if lv.cart.v = 0 then lv.cart.v = 1
    lv.cart.v = sgn(lv.cart.v) * (PEDALV + lv.cart.hits * PEDALMORE)
    lv.cart.at.x = lv.cart.at.x + lv.cart.v * STEPT
    if lv.cart.at.x > lv.chase_end - PEDALW \ 2 then lv.cart.v = -1
    if lv.cart.at.x < lv.chase_x + PEDALW \ 2 then lv.cart.v = 1
end sub

' The crane along its rail, kept to the hall: after Charlie,
' to be over him, faster for each bump, and still while it
' holds a crate over him; bumped, still a while, then off
' from him at speed.
sub MoveCrane (lv as Level, p as Hero)
    dim want as single, dir as integer

    dir = sgn(p.at.x - lv.cart.at.x)
    want = dir * (CRANEV + CRANEUP * lv.cart.hits)
    if lv.box.st = 1 then want = 0
    if lv.cart.t > 0 then want = CRANEBOLT * (2 * (dir > 0) + 1)
    if lv.cart.t > BOLTS then want = 0
    lv.cart.v = lv.cart.v + (want - lv.cart.v) * 5 * STEPT
    lv.cart.at.x = lv.cart.at.x + lv.cart.v * STEPT

    if lv.cart.at.x < lv.chase_x + 40 then
        lv.cart.at.x = lv.chase_x + 40
    end if

    if lv.cart.at.x <= lv.chase_end - 40 then exit sub

    lv.cart.at.x = lv.chase_end - 40
end sub

' The crane's crate, by its state: on the hook, and held
' still once it is over Charlie; let go a moment later; then
' falling.
sub DropBox (lv as Level, p as Hero)
    if lv.box.t > 0 then lv.box.t = lv.box.t - STEPT

    select case lv.box.st
    case 0
        lv.box.at.x = lv.cart.at.x
        lv.box.at.y = lv.cart.at.y - CRANEH + 16

        if lv.box.t <= 0 and lv.cart.t <= 0 _
            and abs(p.at.x - lv.cart.at.x) < 10 then
            lv.box.st = 1
            lv.box.t = POISE
        end if
    case 1
        if lv.box.t <= 0 then
            lv.box.st = 2
            lv.box.v = 0
        end if
    case 2
        lv.box.v = lv.box.v + GRAV * STEPT
        lv.box.at.y = lv.box.at.y + lv.box.v * STEPT
    end select
end sub

' The crate in bits where it is, the next a while off:
' sooner for each bump the crane has had.
sub BreakBox (lv as Level, ev as Events)
    TellAt ev, EVBURST, lv.box.at.x + 0, lv.box.at.y - 8, PBOX
    Tell ev, EVSOUND, SPOP
    lv.box.st = 0
    lv.box.t = RELOAD - HASTE * lv.cart.hits
end sub
