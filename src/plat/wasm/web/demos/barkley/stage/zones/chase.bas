' The chases: who flees with a zone's keepsake, and the
' bumps that break it.
defint A-Z
option explicit

from engine.video import Blit, Store, SW
from engine.events import Events
from barkley.model.images import IBRACKET, ICART, ICARTW, ICAT, ICRANE
from barkley.model.images import IDUNKED, IFROST, IKNOT, ILIMB, IOAK
from barkley.model.images import IOAKFOOT, IPEDAL, IRAIL, IRELAY
from barkley.model.images import ITWIN, IWBOX
from barkley.model.hero import Hero
from barkley.model.level import Level, NONE, Realm
from barkley.model.game import Game, Loose, PULSES, Session
from barkley.assets.palette import CYELLOW
from barkley.assets.cast import DrawKeep
from barkley.assets.tunes import SHISS
from barkley.world.terrain.ground import Lie, Surf
from barkley.world.terrain.track import LimbUp, NLIMB
from barkley.stage.fx.damage import BOUNCEV, Hurt
from barkley.stage.zones.pass import SLEDV
from barkley.stage.hero.push import Bat, Check, Knock, Rebound
from barkley.stage.things.chase import BallAt, BOLTS, BreakBox
from barkley.stage.things.chase import BreakCart, Bumped, Bumps, CARTR
from barkley.stage.things.chase import CRANEH, CRANER, CRUISE, DAZED
from barkley.stage.things.chase import DropBox, Float, HOPUP, Juggle
from barkley.stage.things.chase import LegPace, LIMBRISE, Lobbed, LOBT
from barkley.stage.things.chase import LobTime, LOBUP2, LOBWAIT
from barkley.stage.things.chase import MakesOff, MoveCart, MoveCrane
from barkley.stage.things.chase import MovePedalo, NBUMP, NLEG, PASSV
from barkley.stage.things.chase import PEDALUP, PEDALW, Pitch, RAILUP
from barkley.stage.things.chase import Reverse, Roost, SeatX, SeatY
from barkley.stage.things.chase import StartChase, TakeKeep, TOOLFALL
from barkley.stage.things.chase import TWINGAP, Unseat, Unwind

' Biscuit's honey cart less its wheels, as it goes and
' bumped: level, then leaning half a gentle slope's fall and
' all of one's, CARTLEAN higher over and under each time;
' then its wheel at two turns. Its wheels are WHEELS either
' side of its middle.
private const CARTW = 56
private const CARTH = 34
private const CARTLEAN = 7
private const WHEELS = 13

' A step of the chase, by its state: begun as Charlie comes
' to the race; then who flees does, by its zone, and he may
' meet it; broken, its keepsake is his when he reaches it.
sub Flee ( _
    lv as Level, _
    zn as Realm, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    lo() as Loose _
)
    dim fell as integer

    if lv.chase_x = NONE then exit sub

    Unwind lv

    select case lv.cart.st
    case 0
        if zn.id = 2 then Float lv, PEDALUP
        if zn.id = 4 then BallAt lv
        if p.at.x >= lv.chase_x then StartChase lv, ev
    case 1
        select case zn.id
        case 0, 3
            ' a cart and a toboggan go alike
            MoveCart lv, p, CRUISE, MakesOff!(zn)
            MeetCart lv, ev, se, g, p, lo()
        case 1
            MoveCrane lv, p
            MeetCrane lv, ev, p
            fell = lv.box.st = 2
            DropBox lv, p
            if fell then MeetBox lv, ev, se, g, p, lo()
        case 2
            MovePedalo lv
            MeetPedalo lv, ev, p
        case 4
            Juggle lv, ev, p
        case 5
            Pitch lv, p
            MoveCart lv, p, LegPace!(lv, se), PASSV
            MeetRelay lv, ev, se, g, p, lo()
        end select

        ' its last bump breaks it; the twins' ball is his
        ' with the catch
        if lv.cart.hits >= Bumps%(zn) then BreakCart lv, ev

        if zn.id = 4 and lv.cart.st = 2 then
            TakeKeep lv, ev
        end if
    case 2
        if zn.id = 1 and lv.cart.t > 2 - TOOLFALL then exit sub

        ' the sands' keepsake floats
        if zn.id = 2 then Float lv, 0

        ' over it will do: he comes down off the last bump
        ' past it
        if abs(p.at.x - lv.cart.at.x) < 16 _
            and p.at.y > lv.cart.at.y - 90 _
            and p.at.y < lv.cart.at.y + 8 then
            TakeKeep lv, ev
        end if
    end select
end sub

' Charlie at the cart, once it is over its last bump:
' curled, he bumps it and is thrown back off it, or on a
' sled checked; else it hurts him. Its last bump breaks it.
private sub MeetCart ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    lo() as Loose _
)
    if lv.cart.t > BOLTS or p.dead then exit sub
    if abs(p.at.x - lv.cart.at.x) > CARTR _
        or p.at.y < lv.cart.at.y - 46 _
        or p.at.y > lv.cart.at.y + 6 then exit sub

    if not (p.roll or p.charging) then
        Hurt ev, se, g, p, cint(lv.cart.at.x), lo(), SHISS
        exit sub
    end if

    Bumped lv, ev, lv.cart.at.y - 20

    ' sled against sled he is checked, not thrown: a sled
    ' thrown back has no brake to stop it
    if p.sled then
        Check p, SLEDV
        exit sub
    end if

    Knock p, -120, -BOUNCEV
end sub

' Who flees, by its zone; nothing once its keepsake is taken
' back, but the wood's oak, which stands, and its twins, who
' stay to sulk.
sub DrawChase ( _
    lv as Level, _
    zn as Realm, _
    art as Store, _
    se as Session, _
    cx as integer, _
    cy as integer _
)
    if lv.chase_x = NONE then exit sub

    if zn.id = 4 then
        DrawOak lv, art, cx, cy
        DrawTwins lv, zn, art, cx, cy
    end if

    if lv.cart.st = 3 then exit sub

    select case zn.id
    case 0
        DrawCart lv, zn, art, se, cx, cy
    case 1
        DrawCrane lv, zn, art, se, cx, cy
    case 2
        DrawPedalo lv, zn, art, se, cx, cy
    case 3
        DrawFrost lv, zn, art, se, cx, cy
    case 5
        DrawRelay lv, zn, art, se, cx, cy
    end select
end sub

' Charlie at who has the guitar, as at a cart; not while the
' twins have it in the air. A bump is marked where it was
' made, for the cat left there off its feet.
private sub MeetRelay ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    lo() as Loose _
)
    dim was as integer

    if Lobbed%(lv) then exit sub

    was = lv.cart.hits
    MeetCart lv, ev, se, g, p, lo()
    if lv.cart.hits = was then exit sub

    Unseat lv
end sub

' The twins' places from the guitar's, a and b: with it in
' the air, who threw it falling behind and who catches it
' ahead; else one under it, the other round from behind to
' TWINGAP ahead by the next lob.
private sub TwinsAt (lv as Level, a as integer, b as integer)
    dim w as single

    a = 0
    b = TWINGAP
    if lv.box.t <= 0 then exit sub

    w = (LOBWAIT - lv.box.t) / LOBT

    if w < 1 then
        a = -cint(TWINGAP * w)
        b = cint(TWINGAP * (1 - w))
    else
        b = cint(TWINGAP * (1 - 2 * lv.box.t / (LOBWAIT - LOBT)))
    end if
end sub

' How high the guitar is over its cat's back: thrown on from
' a bump, coming down to the next cat; lobbed, over the
' twins.
private function Aloft% (lv as Level)
    dim w as single

    Aloft% = 0
    if lv.cart.t > BOLTS then Aloft% = cint((lv.cart.t - BOLTS) * 60)
    if not Lobbed%(lv) then exit function

    w = (LOBWAIT - lv.box.t) / LOBT
    Aloft% = cint(LOBUP2 * 4 * w * (1 - w))
end function

' The relay: the cat last bumped off its feet where it was,
' a while; who has the guitar striding, it on its back,
' Kipper off the ground on his quick beat, the other twin
' by; a pip over it for each cat still to bump. Broken, the
' guitar where it fell.
private sub DrawRelay ( _
    lv as Level, _
    zn as Realm, _
    art as Store, _
    se as Session, _
    cx as integer, _
    cy as integer _
)
    dim x as integer, y as integer, leg as integer, k as integer
    dim i as integer, a as integer, b as integer
    dim up as integer, q as single

    x = cint(lv.cart.at.x) - cx
    y = cint(lv.cart.at.y) - cy
    leg = lv.cart.hits

    if leg > 0 and lv.cart.t > 0 then
        Blit art, cint(lv.box.at.x) - cx - 10, _
            cint(lv.box.at.y) - cy - 17, IRELAY + (leg * 3 - 1) * 2, _
            20, 18
    end if

    if lv.cart.st = 2 then
        DrawKeep zn, art, x, y - ((se.ticks \ 16) and 1)
        exit sub
    end if

    q = (se.pulse mod PULSES) / PULSES

    if leg = 2 and ((se.pulse \ PULSES) and 1) = 0 then
        up = cint(HOPUP * 4 * q * (1 - q))
    end if

    DrawKeep zn, art, x - 9, y - 10 - up - Aloft%(lv)
    k = IRELAY + (leg * 3 + ((se.ticks \ 4) and 1)) * 2

    if leg = NLEG - 1 then
        TwinsAt lv, a, b
        Blit art, x + b - 10, y - 17, k, 20, 18
    end if

    Blit art, x + a - 10, y - 17 - up, k, 20, 18

    for i = leg to NLEG - 1
        line (x - 18 + i * 8, y - 40)-(x - 14 + i * 8, y - 36), _
            CYELLOW, BF
    next
end sub

' The picture of the twin on side sd, 0 the left: 0
' grinning, 1 reaching, 2 a paw out, 3 shocked, 4 sulking.
' The ball his, they sulk; a catch shocks them, then they
' reach for the pair above; who holds the ball reaches up
' with it while the other grins; thrown, the thrower's paw
' is out a while, and the other reaches for it.
function TwinPose% (lv as Level, sd as integer)
    dim mine as integer

    TwinPose% = 4
    if lv.cart.st = 3 then exit function

    TwinPose% = 3
    if lv.cart.t > DAZED * .6 then exit function

    TwinPose% = 1
    if lv.cart.t > 0 then exit function

    mine = lv.box.st \ 2 = sd

    if (lv.box.st and 1) = 0 then
        TwinPose% = -mine
        exit function
    end if

    if not mine then exit function

    TwinPose% = 0
    if lv.box.t > LobTime!(lv) - .35 then TwinPose% = 2
end function

' The great oak: its limbs, out from behind it; its foot,
' and its trunk up from there out of sight; its hollow and
' its brackets.
private sub DrawOak ( _
    lv as Level, _
    art as Store, _
    cx as integer, _
    cy as integer _
)
    dim n as integer, x as integer, y as integer, k as integer
    dim w as integer, h as integer

    x = lv.oak.x - cx
    if x < -170 or x > SW + 170 then exit sub

    for n = 0 to NLIMB - 1
        y = lv.oak.y - LimbUp%(n) - LIMBRISE - cy
        ' the middle pair's are the far ones, longer and
        ' deeper
        k = ILIMB
        w = 94
        h = 50

        if n \ 2 = 1 then
            k = ILIMB + 2
            w = 142
            h = 58
        end if

        if n and 1 then
            Blit art, x + 22, y, k, w, h
        else
            Blit art, x - 22 - w, y, k + 4, w, h
        end if
    next

    y = lv.oak.y - 45 - cy
    Blit art, x - 76, y, IOAKFOOT, 152, 48

    do while y > 0
        y = y - 64
        Blit art, x - 33, y, IOAK, 66, 64
    loop

    y = lv.oak.y - cy
    Blit art, x - 2, y - 84, IKNOT, 17, 20
    Blit art, x + 23, y - 34, IBRACKET, 12, 5
    Blit art, x + 26, y - 26, IBRACKET, 12, 5
    Blit art, x - 35, y - 60, IBRACKET + 2, 12, 5
end sub

' The twins on their limbs, each in the picture of what it
' does, and the ball with a pip over it for each catch still
' to make. A catch telling, they are shocked where they sat,
' then leap to the pair above.
private sub DrawTwins ( _
    lv as Level, _
    zn as Realm, _
    art as Store, _
    cx as integer, _
    cy as integer _
)
    dim sd as integer, n as integer, x as single, y as single
    dim u as single, i as integer, bx as integer
    dim by as integer

    for sd = 0 to 1
        n = Roost%(lv) * 2 + sd
        x = SeatX%(lv, n)
        y = SeatY%(lv, n)
        u = 0

        if lv.cart.st = 1 and lv.cart.t > 0 then
            u = lv.cart.t / (DAZED * .6)
        end if

        if u > 1 then u = 1

        if u > 0 then
            x = x + (SeatX%(lv, n - 2) - x) * u
            y = y + (SeatY%(lv, n - 2) - y) * u - 24 * 4 * u * (1 - u)
        end if

        Blit art, cint(x) - 15 - cx, cint(y) - 27 - cy, _
            ITWIN + (sd * 5 + TwinPose%(lv, sd)) * 2, 31, 28
    next

    if lv.cart.st = 3 then exit sub

    bx = cint(lv.cart.at.x) - cx
    by = cint(lv.cart.at.y) - cy
    DrawKeep zn, art, bx, by

    for i = lv.cart.hits to NBUMP - 1
        line (bx - 10 + i * 8, by - 22)-(bx - 6 + i * 8, by - 18), _
            CYELLOW, BF
    next
end sub

' Frost on her toboggan as the ground lies, shaken while a
' bump has her dazed, the bumps she can yet take over her;
' knocked off it, she sits in deep snow, the camera lying by
' her.
private sub DrawFrost ( _
    lv as Level, _
    zn as Realm, _
    art as Store, _
    se as Session, _
    cx as integer, _
    cy as integer _
)
    dim x as integer, y as integer, i as integer

    x = cint(lv.cart.at.x) - cx
    y = cint(lv.cart.at.y) - cy

    if lv.cart.st = 2 then
        Blit art, x - 62, y - 21, IDUNKED, 56, 22
        DrawKeep zn, art, x, y - ((se.ticks \ 16) and 1)
        exit sub
    end if

    if lv.cart.t > BOLTS then x = x + ((se.ticks \ 2) and 1) * 2 - 1
    Blit art, x - 17, y - 46, _
        IFROST + (Lie%(lv, cint(lv.cart.at.x), cint(lv.cart.at.y)) _
        + 2) * 2, 34, 65

    for i = lv.cart.hits to NBUMP - 1
        line (x - 10 + i * 8, y - 40)-(x - 6 + i * 8, y - 36), _
            CYELLOW, BF
    next
end sub

' Charlie down onto the pedal boat's deck, out of the air: a
' bump, it turns round and he goes up off it.
private sub MeetPedalo (lv as Level, ev as Events, p as Hero)
    if lv.cart.t > BOLTS or p.dead or p.ground or p.swim _
        or p.vel.y <= 0 then exit sub
    if abs(p.at.x - lv.cart.at.x) > PEDALW \ 2 _
        or p.at.y < lv.cart.at.y - 6 _
        or p.at.y > lv.cart.at.y + 14 then exit sub

    Bumped lv, ev, lv.cart.at.y - 8
    Reverse lv
    Rebound p, BOUNCEV, 0
end sub

' The pedal boat, a pip over it for each bump it can still
' take; broken, the teddy afloat where it sank, and the cat
' paddling off a while.
private sub DrawPedalo ( _
    lv as Level, _
    zn as Realm, _
    art as Store, _
    se as Session, _
    cx as integer, _
    cy as integer _
)
    dim x as integer, y as integer, i as integer

    x = cint(lv.cart.at.x) - cx
    y = cint(lv.cart.at.y) - cy

    if lv.cart.st = 2 then
        DrawKeep zn, art, x, y + 3 - ((se.ticks \ 16) and 1)
        DrawFled lv, art, se, x, y - 9, 120
        exit sub
    end if

    Blit art, x - 26, y - 22, IPEDAL - 2 * (lv.cart.t > BOLTS), 52, 34

    for i = lv.cart.hits to NBUMP - 1
        line (x - 10 + i * 8, y - 34)-(x - 6 + i * 8, y - 30), _
            CYELLOW, BF
    next
end sub

' The cat making off from (x, y), v a second, a while after
' its ride broke.
private sub DrawFled ( _
    lv as Level, _
    art as Store, _
    se as Session, _
    x as integer, _
    y as integer, _
    v as integer _
)
    if lv.cart.t <= 0 then exit sub

    Blit art, x + cint((2 - lv.cart.t) * v), y, _
        ICAT + ((se.ticks \ 4) and 1) * 2, 20, 14
end sub

' The cart, a pip over it for each bump it can still take;
' broken, the keepsake where it fell, and the cat making off
' a while.
private sub DrawCart ( _
    lv as Level, _
    zn as Realm, _
    art as Store, _
    se as Session, _
    cx as integer, _
    cy as integer _
)
    dim x as integer, y as integer, i as integer

    x = cint(lv.cart.at.x) - cx
    y = cint(lv.cart.at.y) - cy

    if lv.cart.st = 2 then
        DrawKeep zn, art, x, y - ((se.ticks \ 16) and 1)
        DrawFled lv, art, se, x, y - 13, 260
        exit sub
    end if

    DrawWheels lv, art, x, cy

    for i = lv.cart.hits to NBUMP - 1
        line (x - 10 + i * 8, y - 50)-(x - 6 + i * 8, y - 46), _
            CYELLOW, BF
    next
end sub

' The cart on its ground, its middle at x on the screen:
' each wheel on the ground under it, turning as it goes, and
' the planks between them, leaning by what the wheels'
' grounds differ: not at all, half a gentle slope's fall, or
' all of one's.
private sub DrawWheels ( _
    lv as Level, _
    art as Store, _
    x as integer, _
    cy as integer _
)
    dim ya as integer, yb as integer, lean as integer, k as integer

    ya = Surf%(lv, cint(lv.cart.at.x) - WHEELS, cint(lv.cart.at.y))
    yb = Surf%(lv, cint(lv.cart.at.x) + WHEELS, cint(lv.cart.at.y))
    if ya = NONE then ya = cint(lv.cart.at.y)
    if yb = NONE then yb = cint(lv.cart.at.y)
    lean = (yb - ya + 3) \ 7
    if lean < 0 then lean = 0
    if lean > 2 then lean = 2
    k = ICART + lean * 4
    if lv.cart.t > BOLTS then k = k + 2
    Blit art, x - CARTW \ 2, _
        (ya + yb) \ 2 - cy - 41 - lean * CARTLEAN, k, CARTW, _
        CARTH + lean * CARTLEAN * 2
    k = ICARTW + ((cint(lv.cart.at.x) \ 8) and 1) * 2
    Blit art, x - WHEELS - 6, ya - cy - 12, k, 13, 13
    Blit art, x + WHEELS - 6, yb - cy - 12, k, 13, 13
end sub

' Charlie thrown up to the crane: in the air at its gondola
' he bumps it and is knocked aside, out of a jet's way, and
' down, or up off it if he came down on it; its crate is
' lost.
private sub MeetCrane (lv as Level, ev as Events, p as Hero)
    dim foot as single, vy as single

    if lv.cart.t > BOLTS or p.dead or p.ground then exit sub

    foot = lv.cart.at.y - CRANEH
    if abs(p.at.x - lv.cart.at.x) > CRANER or p.at.y - 26 > foot _
        or p.at.y < foot - 24 then exit sub

    if lv.box.st > 0 or lv.box.t <= 0 then BreakBox lv, ev
    Bumped lv, ev, foot - 12
    vy = 160
    if p.at.y < foot - 14 then vy = -200
    Bat p, 140 * (2 * (p.at.x <= lv.cart.at.x) + 1), vy
end sub

' The crate falling: it hurts him if it meets him, and
' breaks on him or on the floor.
private sub MeetBox ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    lo() as Loose _
)
    if abs(p.at.x - lv.box.at.x) < 14 _
        and p.at.y > lv.box.at.y - 16 _
        and p.at.y - 26 < lv.box.at.y and not p.dead then
        Hurt ev, se, g, p, cint(lv.box.at.x), lo(), SHISS
        BreakBox lv, ev
    elseif lv.box.at.y >= lv.cart.at.y then
        BreakBox lv, ev
    end if
end sub

' The rail over the hall and the crane on it, a pip over it
' for each bump it can still take, its crate on the hook or
' falling; broken, its lamp out and the toolbox fallen from
' it.
private sub DrawCrane ( _
    lv as Level, _
    zn as Realm, _
    art as Store, _
    se as Session, _
    cx as integer, _
    cy as integer _
)
    dim x as integer, y as integer, i as integer, fy as integer
    dim a as integer, b as integer, jolt as integer

    x = cint(lv.cart.at.x) - cx
    fy = cint(lv.cart.at.y) - cy
    y = fy - RAILUP
    a = (cx - lv.chase_x) \ 32
    if a < 0 then a = 0
    b = (cx + SW - lv.chase_x) \ 32

    if b > (lv.chase_end - lv.chase_x) \ 32 - 1 then
        b = (lv.chase_end - lv.chase_x) \ 32 - 1
    end if

    for i = a to b
        Blit art, lv.chase_x + i * 32 - cx, y, IRAIL, 32, 8
    next

    Blit art, x - 22, y - 2, _
        ICRANE - 2 * (lv.cart.st = 2 or lv.cart.t > BOLTS), 44, 46

    if lv.cart.st = 2 then
        i = cint((lv.cart.t - 2 + TOOLFALL) * CRANEH / TOOLFALL)
        if i < 0 then i = 0
        DrawKeep zn, art, x, fy - i - ((se.ticks \ 16) and 1)
        exit sub
    end if

    for i = lv.cart.hits to NBUMP - 1
        line (x - 10 + i * 8, y - 7)-(x - 6 + i * 8, y - 3), _
            CYELLOW, BF
    next

    if lv.box.st = 0 and lv.box.t > 0 then exit sub

    jolt = (lv.box.st = 1) * ((se.ticks \ 3) and 1)
    Blit art, cint(lv.box.at.x) - cx - 8 + jolt, _
        cint(lv.box.at.y) - cy - 16, IWBOX, 16, 16
end sub
