' The game's own player: a pad that holds right and jumps at pits, walls and cats.
defint A-Z
option explicit

from barkley.model.hero import Hero, HW, Pad
from barkley.model.level import Ground, KBRAMBLE, KCAT, KCRAB, KJELLY, KLOG, KMOUND, KPOUNCE, KSPARK, KSWEEP, KTHORN
from barkley.model.level import KMORTAR, KSNATCH
from barkley.model.level import KTHROW
from barkley.model.level import Level, NONE, Realm, TS
from barkley.model.game import Session
from barkley.world.terrain.ground import Ribbon, Solid, Surf
from barkley.world.terrain.track import MOVEW
from barkley.world.movers import Adrift, Swing
from barkley.stage.hero.modes import Lull
from barkley.stage.hero.moves import TOP
from barkley.stage.hero.draw import LOGR
from barkley.stage.zones.works import PressAhead, PRESSP, PressT
from barkley.stage.zones.wood import MOUNDR
from barkley.stage.zones.quarter import PotClear, POTR, POTX, POUNCEGONE, TipperAhead
from barkley.stage.zones.pass import GCHARGE, GoatNear

' How near who flees is rolled into. A bump throws him back curled, and on the quarter's level street a roll only
' slows: he hops out of it once its cat is OUTRUN ahead, further than a hop gains on one that stops.
private const RAMREACH = 120, OUTRUN = 140

' The game's own player: Right held, a jump where one is wanted, still where it must wait.
sub Pilot (lv as Level, gr as Ground, zn as Realm, se as Session, p as Hero, k as Pad)
    static hold as integer
    dim want as integer, hopNow as integer
    k.r = -1
    k.l = 0
    k.d = 0
    if p.vine then
        ' hung from a vine: let go on the way up and forward, or where a small swing turns
        hopNow = p.sway_v > 0 and p.sway > 0 and (p.sway > .5 or p.sway_v < 1)
    elseif p.swim then
        hopNow = PilotSwim%(lv, zn, p, k)
    elseif p.ground and gr.pt(p.sg).k = 3 then
        want = PilotRide%(lv, gr, se, p, k)
    elseif p.ground and gr.pt(p.sg).k = 1 then
        want = PilotRun%(lv, gr, zn, se, p, k)
    elseif not p.ground then
        PilotFall lv, zn, p, k
    end if
    if lv.gale and p.ground then PilotGale lv, se, p, k, want
    ' afloat a hop is made though the last is still held
    if hopNow or (want and hold = 0) then
        k.hit = -1
        hold = 24
    end if
    k.jump = hold > 0
    if hold > 0 then hold = hold - 1
end sub

' The pilot's keys: on to the right.
private sub PressOn (k as Pad)
    k.r = -1
    k.l = 0
end sub

' The pilot's keys: no further, and back against a speed over v.
private sub HoldBack (p as Hero, k as Pad, v as integer)
    k.r = 0
    if p.gsp > v then k.l = -1
end sub

' Afloat: on to the right, a hop where a bank is in the way, or a jellyfish. Whether to hop.
private function PilotSwim% (lv as Level, zn as Realm, p as Hero, k as Pad)
    dim i as integer, d as integer, want as integer
    want = Solid%(lv, cint(p.at.x) + HW + 2, cint(p.at.y) - 11)
    for i = lv.first(cint(p.at.x) \ TS) to lv.nObj - 1
        if lv.ob(i).at.x > p.at.x + 34 then exit for
        if lv.ob(i).kind = KJELLY and lv.ob(i).at.x > p.at.x then want = -1
    next
    PilotSwim% = want
    ' Kipper's boat about: still, and a hop as it comes, to come down on its deck
    if zn.id <> 2 or lv.cart.st <> 1 then exit function
    k.r = 0
    d = cint(lv.cart.at.x - p.at.x)
    want = lv.cart.t <= 0 and abs(d) < abs(lv.cart.v) * .55 + 12 and abs(d) > abs(lv.cart.v) * .45
    PilotSwim% = want and sgn(d) <> sgn(lv.cart.v)
end function

' On a platform: still, till there is ground ahead to jump to. Whether to jump.
private function PilotRide% (lv as Level, gr as Ground, se as Session, p as Hero, k as Pad)
    dim y as integer, i as integer, m as integer, d as integer, want as integer
    y = cint(p.at.y)
    HoldBack p, k, 20
    i = cint(gr.pt(p.sg).x) + MOVEW + 10
    if Surf%(lv, i, y) <> NONE and not Solid%(lv, i, y - 12) then
        PressOn k
        want = -1
    end if
    for m = 0 to lv.nMv - 1
        if lv.mv(m).sg <> p.sg then continue for
        select case lv.mv(m).kind
        case 4, 6
            ' a bridge is run over, and a pier
            PressOn k
            want = 0
        case 8
            ' a tram is ridden to its way's end, and left by a jump
            if lv.mv(m).t >= 1 then
                PressOn k
                want = -1
            end if
        case 5
            ' a boat low in its pool: off it at the wall, once the bank is a jump up
            d = lv.top((i + 6) \ TS) * TS
            if Swing!(lv, se, m, 0) > .85 and d < y and d > y - 44 then
                PressOn k
                want = -1
            end if
        end select
    next
    PilotRide% = want
end function

' On the ground: a jump at a gap, a wall, or what would hurt; a wait at a press, a goat, a platform's gap. Whether
' to jump.
private function PilotRun% (lv as Level, gr as Ground, zn as Realm, se as Session, p as Hero, k as Pad)
    dim x as integer, y as integer, i as integer, d as integer, want as integer
    x = cint(p.at.x)
    y = cint(p.at.y)
    i = x + 22 + cint(abs(p.gsp)) \ 40
    if Surf%(lv, i, y) = NONE and Surf%(lv, i, y + 2 * TS) = NONE and not Ribbon%(gr, x) then want = -1
    ' a wall, not a ramp's foot
    if Solid%(lv, x + 13, y - 20) then want = -1
    for i = lv.first(x \ TS) to lv.nObj - 1
        if lv.ob(i).at.x > x + 130 then exit for
        if lv.ob(i).at.x <= x then continue for
        ' a pouncer is jumped in its bin and out of it; a mortar's flash is jumped, whenever it fires
        d = ((lv.ob(i).kind = KCAT or lv.ob(i).kind = KSNATCH) and lv.ob(i).state <> 0) or lv.ob(i).kind = KTHORN
        d = d or lv.ob(i).kind = KMORTAR
        d = d or (lv.ob(i).kind = KPOUNCE and lv.ob(i).state <> POUNCEGONE)
        if d and lv.ob(i).at.x <= x + 50 then want = -1
        ' what is not rolled through, a crab, a jellyfish, a sweeper, a spark: jumped from further back the faster
        ' he goes
        d = lv.ob(i).kind = KJELLY or lv.ob(i).kind = KSPARK or (lv.ob(i).kind = KCRAB and abs(lv.ob(i).state) = 1)
        d = d or ((lv.ob(i).kind = KSWEEP or lv.ob(i).kind = KTHROW) and lv.ob(i).state <> 0)
        if d and lv.ob(i).at.x <= x + 34 + abs(cint(p.gsp)) \ 8 then want = -1
    next
    PilotPress lv, se, p, k, want
    PilotTipper lv, se, p, k
    PilotGoat lv, p, k, want
    k.d = Rams%(lv, zn, p)
    if zn.id = 5 and p.roll and lv.cart.st = 1 and lv.cart.at.x - p.at.x > OUTRUN then want = -1
    if zn.id = 4 then PilotCurl lv, p, k, want
    PilotGaps lv, se, p, k, want
    if zn.id = 4 then PilotDig lv, p, k, want
    PilotRun% = want
end function

' Whether who flees is rolled into: ahead within RAMREACH, and the speed there to roll.
private function Rams% (lv as Level, zn as Realm, p as Hero)
    dim d as integer
    Rams% = 0
    if lv.cart.st <> 1 or not (zn.id = 0 or zn.id = 3 or zn.id = 5) then exit function
    d = cint(lv.cart.at.x - p.at.x)
    Rams% = d > 0 and d < RAMREACH and p.gsp > 150
end function

' Whether mound i's burrow comes out further on past a wall no jump clears: a column on its way five tiles over
' its own ground, or more.
private function Walled% (lv as Level, i as integer)
    dim c as integer, j as integer
    Walled% = 0
    j = lv.ob(i).state - 1
    if j < 0 then exit function
    for c = lv.ob(i).at.x \ TS to lv.ob(j).at.x \ TS
        if lv.top(c) > lv.ob(i).at.y \ TS - 5 then continue for
        Walled% = -1
        exit function
    next
end function

' Such a mound ahead, or just passed: braked to a stand on it, and down. The faster he comes the sooner he
' brakes; short of it he goes on at a walk.
private sub PilotDig (lv as Level, p as Hero, k as Pad, want as integer)
    dim i as integer, x as integer, c as integer, d as integer, v as integer
    x = cint(p.at.x)
    v = cint(p.gsp)
    c = (x - 48) \ TS
    if c < 0 then c = 0
    for i = lv.first(c) to lv.nObj - 1
        d = lv.ob(i).at.x - x
        if d > 10 + v \ 20 + v * v \ 3600 then exit for
        if lv.ob(i).kind <> KMOUND or d < -40 or p.dug = i + 1 or abs(lv.ob(i).at.y - p.at.y) > 8 then continue for
        if not Walled%(lv, i) then continue for
        want = 0
        k.d = abs(d) < MOUNDR - 3 and abs(v) < 20
        k.r = d >= MOUNDR - 3 and v < 40 + d * 3
        k.l = (d <= 3 - MOUNDR and v > -60) or (d > 3 - MOUNDR and v > 70 + d * 3)
        exit sub
    next
end sub

' Grown bramble or a log ahead, or about him: rolled through with the speed to roll, else jumped. Past them,
' curled still and slower than a run: out of the roll by a hop.
private sub PilotCurl (lv as Level, p as Hero, k as Pad, want as integer)
    dim i as integer, x as integer, c as integer
    x = cint(p.at.x)
    c = (x - LOGR - 8) \ TS
    if c < 0 then c = 0
    for i = lv.first(c) to lv.nObj - 1
        if lv.ob(i).at.x > x + 80 then exit for
        if lv.ob(i).at.x < x - LOGR - 4 then continue for
        if not ((lv.ob(i).kind = KBRAMBLE and lv.ob(i).state = 0) or lv.ob(i).kind = KLOG) then continue for
        if p.gsp > 150 then
            k.d = -1
        else
            want = -1
        end if
        exit sub
    next
    if p.roll and abs(p.gsp) < TOP then want = -1
end sub

' A row of presses ahead: to a stop before it, on once its first is well risen, and no jump under one.
private sub PilotPress (lv as Level, se as Session, p as Hero, k as Pad, want as integer)
    dim i as integer, d as integer
    i = PressAhead%(lv, cint(p.at.x))
    if i = NONE then exit sub
    d = lv.ob(i).at.x - 26 - cint(p.at.x)
    if d < 60 then want = 0
    if d <= 0 or d >= 40 + cint(p.gsp) \ 5 then exit sub
    if (PressT%(lv, se, i) + 22) mod PRESSP > 52 then HoldBack p, k, 20
end sub

' A tipper ahead: to a stop short of where its pot comes down, on once one has broken and the next is not soon.
private sub PilotTipper (lv as Level, se as Session, p as Hero, k as Pad)
    dim i as integer, d as integer
    i = TipperAhead%(lv, cint(p.at.x))
    if i = NONE then exit sub
    d = lv.ob(i).at.x + POTX - POTR - 8 - cint(p.at.x)
    if d <= 0 or d >= 40 + cint(p.gsp) \ 5 then exit sub
    if not PotClear%(lv, se, i) then HoldBack p, k, 20
end sub

' A goat near: to a stand short of it, up as it charges, and down on its back.
private sub PilotGoat (lv as Level, p as Hero, k as Pad, want as integer)
    dim i as integer, d as integer
    i = GoatNear%(lv, cint(p.at.x))
    if i = NONE or p.goat <> 0 then exit sub
    d = lv.ob(i).at.x - cint(p.at.x)
    if abs(lv.ob(i).state) = GCHARGE then
        k.r = 0
        want = abs(d) < 70
        exit sub
    end if
    if d <= 0 or d >= 56 + cint(p.gsp) \ 4 then exit sub
    want = 0
    HoldBack p, k, 20
end sub

' A platform's gap ahead: slowly to its edge, there till the platform is at it and staying, then on.
private sub PilotGaps (lv as Level, se as Session, p as Hero, k as Pad, want as integer)
    dim m as integer, d as integer, x as integer, y as integer
    x = cint(p.at.x)
    y = cint(p.at.y)
    for m = 0 to lv.nMv - 1
        d = lv.mv(m).edge - x
        ' a bridge and a pier are run over; a tram's street is run along
        if lv.mv(m).kind = 4 or lv.mv(m).kind = 6 or lv.mv(m).kind = 8 then d = NONE
        if d <= -4 or d >= 300 or abs(lv.mv(m).at.y + lv.mv(m).reach * (lv.mv(m).kind = 1) - y) >= 200 then continue for
        if d < 50 then want = 0
        if p.gsp > 30 + d then k.r = 0
        if p.gsp > 70 + d then k.l = -1
        if d >= 10 then continue for
        if Ready%(lv, se, m) then
            PressOn k
        else
            HoldBack p, k, 5
        end if
    next
end sub

' Whether moving platform m is at its gap's edge and staying, to be stepped onto.
private function Ready% (lv as Level, se as Session, m as integer)
    select case lv.mv(m).kind
    case 0: Ready% = Swing!(lv, se, m, 0) < -.85 and Swing!(lv, se, m, 1) < 0
    case 5
        ' a boat aground is no way over: down into its pool
        Ready% = not Adrift%(lv, m) or (Swing!(lv, se, m, 0) < -.85 and Swing!(lv, se, m, 1) < 0)
    case 1: Ready% = Swing!(lv, se, m, 0) >= 1 and Swing!(lv, se, m, 1) > .2
    case 2, 3, 7: Ready% = lv.mv(m).t = 0
    end select
end function

' In the air: straight down onto Kipper's deck or a goat's back; slowed toward a platform's gap, to come down
' before it.
private sub PilotFall (lv as Level, zn as Realm, p as Hero, k as Pad)
    dim m as integer, d as integer
    if zn.id = 2 and lv.cart.st = 1 then k.r = 0
    if GoatNear%(lv, cint(p.at.x)) <> NONE then k.r = 0
    for m = 0 to lv.nMv - 1
        d = lv.mv(m).edge - cint(p.at.x)
        if lv.mv(m).kind = 4 or lv.mv(m).kind = 6 or lv.mv(m).kind = 8 then d = NONE
        if d <= -40 or d >= 260 or p.coyote > 0 or p.vel.x <= d \ 3 or p.sg = lv.mv(m).sg then continue for
        k.r = 0
        k.l = -1
    next
end sub

' A blizzard: no jump is begun from a walk that a gust would catch, and while it blows he keeps low.
private sub PilotGale (lv as Level, se as Session, p as Hero, k as Pad, want as integer)
    if Lull%(lv, se) < 56 and want and abs(p.gsp) < 150 then
        want = 0
        HoldBack p, k, 20
    end if
    if Lull%(lv, se) = 0 then k.d = -1
    ' and from a crouch no jump: that would wind up a dash
    if p.crouch then want = 0
    ' curled and come to a stand on a slope, held there by the key: out of it by a jump
    if p.roll and abs(p.gsp) < 30 then want = -1
end sub
