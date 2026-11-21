' The chases: who flees with a zone's keepsake, and the bumps that break it.
defint A-Z
option explicit

from barkley.engine.video import Blit, Store, SW
from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck
from barkley.model.images import IBRACKET, ICART, ICARTW, ICAT, ICRANE, IDUNKED, IFROST, IKNOT, ILIMB, IOAK, IOAKFOOT
from barkley.model.images import IPEDAL, IRAIL, IRELAY, ITWIN, IWBOX
from barkley.model.hero import Hero
from barkley.model.level import GRAV, Level, NONE, Realm, TS
from barkley.model.game import Game, Loose, Part, PULSES, Session
from barkley.assets.palette import CEARTH, CYELLOW
from barkley.assets.cast import DrawKeep
from barkley.assets.tunes import CAUGHT, SBUMP, Sfx, SHISS, SMARK, SPOP, ZoneTune, ZTUNE
from barkley.world.terrain.ground import Lie, Surf
from barkley.world.terrain.track import LimbUp, LIMBW, LimbX, NLIMB
from barkley.stage.fx.parts import Burst
from barkley.stage.fx.damage import BOUNCEV, Hurt
from barkley.stage.zones.pass import SLEDV

' Biscuit's honey cart less its wheels, as it goes and bumped: level, then leaning half a gentle slope's fall and
' all of one's, CARTLEAN higher over and under each time; then its wheel at two turns. Its wheels are WHEELS
' either side of its middle.
private const CARTW = 56, CARTH = 34, CARTLEAN = 7, WHEELS = 13

' The great oak, its foot at lv.oakX, lv.oakY: NLIMB limbs up it, left and right in turn, each a flat LIMBW long
' to stand on, LIMBRISE down its picture. The Bramble twins sit on a pair of them, and after each catch on the
' pair above: one holds the ball PAWUP over its limb TWINHOLD seconds, then lobs it to the other, TWINLOB seconds
' on its way and LOBUP over the straight line, TWINHASTE less for each catch. Within CATCHR of the ball on its
' way, he has caught it.
private const LIMBRISE = 14, TWINHOLD = .6, TWINLOB = 1.5, TWINHASTE = .2, LOBUP = 40
const PAWUP = 24
private const CATCHR = 18

' Kipper's pedal boat: its speed, what each bump adds to it, its length, and its deck over the sea.
const PEDALV = 130!, PEDALMORE = 40!, PEDALW = 48, PEDALUP = 8

' A chase: the cart's speed, when Charlie is far behind it, and away from a bump, a toboggan's less, for a sled to
' catch it again; how far ahead of the race's start it waits; how near touches it; the seconds a bump tells, the
' last of them bolting; the bumps that break it; its tune's number in CHARLIE.DAT.
private const CRUISE = 330!, DAWDLE = 200!, BOLT = 560!, TOBOLT = 400!
const CARTLEAD = 260
private const CARTR = 26
private const DAZED = 1.4
const BOLTS = 1!, NBUMP = 3, CHASETUNE = 5

' The works' crane: its speed after Charlie, what each bump adds, and away from one; how near touches it; its
' gondola's foot over the floor, out of a jump's reach, and its rail's top; the seconds to its next crate, what
' each bump takes off them, and the seconds one is held over Charlie; the view's height over him, by habit and
' under a crane; the seconds its keepsake falls.
private const CRANEV = 90!, CRANEUP = 25!, CRANEBOLT = 260!, CRANER = 22, CRANEH = 120
const RAILUP = 164
private const RELOAD = 1.6, HASTE = .3, POISE = .6, TOOLFALL = .4

' The quarter's relay, the guitar from cat to cat, NLEG of them and a bump each: bumped, a cat throws it on to the
' next, PASSV a second, and is left TUMBLE behind it. Each makes off its own way, LEGV about: Biscuit steadily;
' Sprocket by the beat, WOUND the quicker for three and still for one; Kipper bobbing, a beat BOB the quicker and
' HOPUP off the ground, a beat BOB the slower; Frost sliding, SLIDE the quicker down a fall; the twins lobbing it
' between them, LOBV for LOBT seconds and LOBUP2 high, as Charlie comes within LOBNEAR, and not again for
' LOBWAIT. The twin without it goes TWINGAP from the one with it.
const NLEG = 5, LEGV = 250!, LOBWAIT = 2.4
const TUMBLE = 20
private const PASSV = 440!, WOUND = 60!, BOB = 110!, HOPUP = 10, SLIDE = 200!
private const LOBNEAR = 70, LOBV = 420!, LOBT = .4, LOBUP2 = 20, TWINGAP = 40

' The chase as before Charlie came to it: who flees waiting a little way into the race, the zone's tune back. A
' keepsake taken stays taken.
sub ResetChase (lv as Level, mu as Deck)
    if lv.chaseX = NONE or lv.cart.st = 3 then exit sub
    lv.cart.st = 0
    lv.cart.hits = 0
    lv.cart.v = 0
    lv.cart.t = 0
    lv.cart.x = lv.chaseX + CARTLEAD
    lv.cart.y = lv.top(cint(lv.cart.x) \ TS) * TS
    lv.box.st = 0
    lv.box.t = RELOAD
    ZoneTune mu, mu.stageTune
end sub

' A step of the chase, by its state: begun as Charlie comes to the race; then who flees does, by its zone, and he
' may meet it; broken, its keepsake is his when he reaches it.
sub Flee (lv as Level, zn as Realm, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    if lv.chaseX = NONE then exit sub
    if lv.cart.t > 0 then lv.cart.t = lv.cart.t - STEPT
    select case lv.cart.st
    case 0
        if zn.id = 2 then lv.cart.y = lv.tide - PEDALUP
        if zn.id = 4 then BallAt lv
        if p.x >= lv.chaseX then StartChase lv, mu
    case 1
        select case zn.id
        case 0, 3
            ' a cart and a toboggan go alike
            MoveCart lv, p, CRUISE, MakesOff!(zn)
            MeetCart lv, mu, se, g, p, lo(), pt()
        case 1
            MoveCrane lv, p
            MeetCrane lv, mu, se, g, p, pt()
            DropBox lv, mu, se, g, p, lo(), pt()
        case 2
            MovePedalo lv
            MeetPedalo lv, mu, se, g, p, pt()
        case 4: Juggle lv, mu, se, g, p, pt()
        case 5
            Lob lv, p
            MoveCart lv, p, LegPace!(lv, se), PASSV
            MeetRelay lv, mu, se, g, p, lo(), pt()
        end select
        ' its last bump breaks it; the twins' ball is his with the catch
        if lv.cart.hits >= Bumps%(zn) then BreakCart lv, mu, se, pt()
        if zn.id = 4 and lv.cart.st = 2 then TakeKeep lv, mu, se, g, pt()
    case 2
        if zn.id = 1 and lv.cart.t > 2 - TOOLFALL then exit sub
        ' the sands' keepsake floats
        if zn.id = 2 then lv.cart.y = lv.tide
        ' over it will do: he comes down off the last bump past it
        if abs(p.x - lv.cart.x) < 16 and p.y > lv.cart.y - 90 and p.y < lv.cart.y + 8 then TakeKeep lv, mu, se, g, pt()
    end select
end sub

' The cart is off, to the chase's tune in the zone's slot: the zone's own lead plays it, and the level's end still.
private sub StartChase (lv as Level, mu as Deck)
    dim zlead as string
    lv.cart.st = 1
    zlead = mu.song(CAUGHT).leadP
    ZoneTune mu, CHASETUNE
    mu.song(ZTUNE).leadP = zlead
    mu.song(CAUGHT).leadP = zlead
end sub

' How fast who flees makes off from a bump.
private function MakesOff! (zn as Realm)
    MakesOff! = BOLT
    if zn.id = 3 then MakesOff! = TOBOLT
end function

' The cart along the ground at `pace`: dawdling when Charlie is far behind it, slow while a bump has it dazed,
' then making off at `away`; no further than the race's end.
private sub MoveCart (lv as Level, p as Hero, pace as single, away as single)
    dim want as single, s as integer
    want = pace
    if lv.cart.x - p.x > 240 then want = DAWDLE
    if lv.cart.t > 0 then want = away
    if lv.cart.t > BOLTS then want = 60
    lv.cart.v = lv.cart.v + (want - lv.cart.v) * 5 * STEPT
    lv.cart.x = lv.cart.x + lv.cart.v * STEPT
    if lv.cart.x >= lv.chaseEnd then
        lv.cart.x = lv.chaseEnd
        lv.cart.v = 0
    end if
    s = Surf%(lv, cint(lv.cart.x), cint(lv.cart.y))
    if s <> NONE then lv.cart.y = s
end sub

' Charlie at the cart, once it is over its last bump: curled, he bumps it and is thrown back off it, or on a sled
' checked; else it hurts him. Its last bump breaks it.
private sub MeetCart (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    if lv.cart.t > BOLTS or p.dead then exit sub
    if abs(p.x - lv.cart.x) > CARTR or p.y < lv.cart.y - 46 or p.y > lv.cart.y + 6 then exit sub
    if not (p.roll or p.charging) then
        Hurt mu, se, g, p, cint(lv.cart.x), lo(), SHISS
        exit sub
    end if
    Bumped lv, mu, se, g, pt(), lv.cart.y - 20
    ' sled against sled he is checked, not thrown: a sled thrown back has no brake to stop it
    if p.sled and p.ground then p.gsp = SLEDV
    if p.sled and not p.ground then p.vx = SLEDV
    if p.sled then exit sub
    p.ground = 0
    p.jumped = 0
    p.vx = -120
    p.vy = -BOUNCEV
end sub

' The bumps that end a zone's chase: the relay's is one a cat.
private function Bumps% (zn as Realm)
    Bumps% = NBUMP
    if zn.id = 5 then Bumps% = NLEG
end function

' A bump told, its sparks at height y: who flees is dazed.
private sub Bumped (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, y as single)
    lv.cart.hits = lv.cart.hits + 1
    lv.cart.t = DAZED
    g.score = g.score + 500
    g.shake = .2
    Burst se, pt(), lv.cart.x + 0, y, 12, 150, CYELLOW
    Sfx mu, se, SBUMP
end sub

' Who flees is broken and the keepsake left lying; the zone's tune again.
private sub BreakCart (lv as Level, mu as Deck, se as Session, pt() as Part)
    lv.cart.st = 2
    lv.cart.t = 2
    Burst se, pt(), lv.cart.x + 0, lv.cart.y - 12, 16, 200, CEARTH + 4
    ZoneTune mu, mu.stageTune
end sub

private sub TakeKeep (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part)
    lv.cart.st = 3
    g.score = g.score + 1000
    Burst se, pt(), lv.cart.x + 0, lv.cart.y - 8, 12, 120, CYELLOW
    Sfx mu, se, SMARK
end sub

' Who flees, by its zone; nothing once its keepsake is taken back, but the wood's oak, which stands, and its
' twins, who stay to sulk.
sub DrawChase (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
    if lv.chaseX = NONE then exit sub
    if zn.id = 4 then
        DrawOak lv, art, cx, cy
        DrawTwins lv, zn, art, cx, cy
    end if
    if lv.cart.st = 3 then exit sub
    select case zn.id
    case 0: DrawCart lv, zn, art, se, cx, cy
    case 1: DrawCrane lv, zn, art, se, cx, cy
    case 2: DrawPedalo lv, zn, art, se, cx, cy
    case 3: DrawFrost lv, zn, art, se, cx, cy
    case 5: DrawRelay lv, zn, art, se, cx, cy
    end select
end sub

' A lob's clock: begun as Charlie comes near who has the guitar, no sooner than LOBWAIT after the last. Only the
' twins lob by it.
private sub Lob (lv as Level, p as Hero)
    if lv.box.t > 0 then lv.box.t = lv.box.t - STEPT
    if lv.box.t > 0 or lv.cart.t > 0 then exit sub
    if lv.cart.x - p.x < LOBNEAR then lv.box.t = LOBWAIT
end sub

' Whether the guitar is in the air between the twins.
function Lobbed% (lv as Level)
    Lobbed% = lv.cart.hits = NLEG - 1 and lv.box.t > LOBWAIT - LOBT
end function

' The pace of who has the guitar, by the relay's leg.
private function LegPace! (lv as Level, se as Session)
    dim beat as integer
    beat = (se.pulse \ PULSES) and 3
    LegPace! = LEGV
    select case lv.cart.hits
    case 1
        LegPace! = LEGV + WOUND
        if beat = 3 then LegPace! = 0
    case 2: LegPace! = LEGV + BOB * (1 - 2 * (beat and 1))
    case 3: if Lie%(lv, cint(lv.cart.x), cint(lv.cart.y)) < 0 then LegPace! = LEGV + SLIDE
    case 4: if Lobbed%(lv) then LegPace! = LOBV
    end select
end function

' Charlie at who has the guitar, as at a cart; not while the twins have it in the air. A bump is marked where it
' was made, for the cat left there off its feet.
private sub MeetRelay (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim was as integer
    if Lobbed%(lv) then exit sub
    was = lv.cart.hits
    MeetCart lv, mu, se, g, p, lo(), pt()
    if lv.cart.hits = was then exit sub
    lv.box.x = lv.cart.x - TUMBLE
    lv.box.y = lv.cart.y
    lv.box.t = 0
end sub

' The twins' places from the guitar's, a and b: with it in the air, who threw it falling behind and who catches it
' ahead; else one under it, the other round from behind to TWINGAP ahead by the next lob.
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

' How high the guitar is over its cat's back: thrown on from a bump, coming down to the next cat; lobbed, over
' the twins.
private function Aloft% (lv as Level)
    dim w as single
    Aloft% = 0
    if lv.cart.t > BOLTS then Aloft% = cint((lv.cart.t - BOLTS) * 60)
    if not Lobbed%(lv) then exit function
    w = (LOBWAIT - lv.box.t) / LOBT
    Aloft% = cint(LOBUP2 * 4 * w * (1 - w))
end function

' The relay: the cat last bumped off its feet where it was, a while; who has the guitar striding, it on its back,
' Kipper off the ground on his quick beat, the other twin by; a pip over it for each cat still to bump. Broken,
' the guitar where it fell.
private sub DrawRelay (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
    dim x as integer, y as integer, leg as integer, k as integer, i as integer, a as integer, b as integer
    dim up as integer, q as single
    x = cint(lv.cart.x) - cx
    y = cint(lv.cart.y) - cy
    leg = lv.cart.hits
    if leg > 0 and lv.cart.t > 0 then
        Blit art, cint(lv.box.x) - cx - 10, cint(lv.box.y) - cy - 17, IRELAY + (leg * 3 - 1) * 2, 20, 18
    end if
    if lv.cart.st = 2 then
        DrawKeep zn, art, x, y - ((se.ticks \ 16) and 1)
        exit sub
    end if
    q = (se.pulse mod PULSES) / PULSES
    if leg = 2 and ((se.pulse \ PULSES) and 1) = 0 then up = cint(HOPUP * 4 * q * (1 - q))
    DrawKeep zn, art, x - 9, y - 10 - up - Aloft%(lv)
    k = IRELAY + (leg * 3 + ((se.ticks \ 4) and 1)) * 2
    if leg = NLEG - 1 then
        TwinsAt lv, a, b
        Blit art, x + b - 10, y - 17, k, 20, 18
    end if
    Blit art, x + a - 10, y - 17 - up, k, 20, 18
    for i = leg to NLEG - 1
        line (x - 18 + i * 8, y - 40)-(x - 14 + i * 8, y - 36), CYELLOW, BF
    next
end sub

' The twins' pair of limbs: one higher for each catch, the highest for the last.
private function Roost% (lv as Level)
    Roost% = lv.cart.hits
    if lv.cart.hits > NLIMB \ 2 - 1 then Roost% = NLIMB \ 2 - 1
end function

' Where a twin sits on limb n, the middle of its flat.
private function SeatX% (lv as Level, n as integer)
    SeatX% = lv.oakX + LimbX%(n) + LIMBW \ 2
end function

private function SeatY% (lv as Level, n as integer)
    SeatY% = lv.oakY - LimbUp%(n)
end function

' The seconds the ball is in the air between the twins: fewer for each catch.
private function LobTime! (lv as Level)
    LobTime! = TWINLOB - TWINHASTE * lv.cart.hits
end function

' The ball where it is, lv.cart its foot: over the paws of the twin that holds it, lv.box.st 0 the left and 2 the
' right; odd, on its way from that one to the other, lv.box.t seconds still to go; a catch still telling, up from
' where he caught it, lv.box.x, lv.box.y, to who holds it next.
private sub BallAt (lv as Level)
    dim n as integer, x as single, y as single, u as single
    n = Roost%(lv) * 2 + lv.box.st \ 2
    x = SeatX%(lv, n)
    y = SeatY%(lv, n) - PAWUP
    if lv.cart.t > 0 then
        u = lv.cart.t / DAZED
        x = x + (lv.box.x - x) * u
        y = y + (lv.box.y - y) * u
    elseif lv.box.st and 1 then
        u = 1 - lv.box.t / LobTime!(lv)
        x = x + (SeatX%(lv, n xor 1) - x) * u
        y = y + (SeatY%(lv, n xor 1) - PAWUP - y) * u
    end if
    lv.cart.x = x
    lv.cart.y = y - LOBUP * 4 * u * (1 - u)
end sub

' The twins' game, a step: the ball held, then lobbed across, then held again; held too while a catch tells. At
' the ball on its way he has caught it: a bump, the twins off to the pair above and the ball tossed up after
' them. The third catch, and the ball is his.
private sub Juggle (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, pt() as Part)
    if lv.cart.t <= 0 then lv.box.t = lv.box.t - STEPT
    if lv.box.t <= 0 then
        lv.box.st = (lv.box.st + 1) and 3
        lv.box.t = TWINHOLD
        if lv.box.st and 1 then lv.box.t = LobTime!(lv)
    end if
    BallAt lv
    if (lv.box.st and 1) = 0 or p.dead then exit sub
    if abs(p.x - lv.cart.x) > CATCHR or abs(p.y - 12 - (lv.cart.y - 7)) > CATCHR then exit sub
    lv.box.x = lv.cart.x
    lv.box.y = lv.cart.y
    lv.box.st = 0
    lv.box.t = TWINHOLD
    Bumped lv, mu, se, g, pt(), lv.cart.y - 7
end sub

' The picture of the twin on side sd, 0 the left: 0 grinning, 1 reaching, 2 a paw out, 3 shocked, 4 sulking. The
' ball his, they sulk; a catch shocks them, then they reach for the pair above; who holds the ball reaches up with
' it while the other grins; thrown, the thrower's paw is out a while, and the other reaches for it.
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

' The great oak: its limbs, out from behind it; its foot, and its trunk up from there out of sight; its hollow
' and its brackets.
private sub DrawOak (lv as Level, art as Store, cx as integer, cy as integer)
    dim n as integer, x as integer, y as integer, k as integer, w as integer, h as integer
    x = lv.oakX - cx
    if x < -170 or x > SW + 170 then exit sub
    for n = 0 to NLIMB - 1
        y = lv.oakY - LimbUp%(n) - LIMBRISE - cy
        ' the middle pair's are the far ones, longer and deeper
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
    y = lv.oakY - 45 - cy
    Blit art, x - 76, y, IOAKFOOT, 152, 48
    do while y > 0
        y = y - 64
        Blit art, x - 33, y, IOAK, 66, 64
    loop
    y = lv.oakY - cy
    Blit art, x - 2, y - 84, IKNOT, 17, 20
    Blit art, x + 23, y - 34, IBRACKET, 12, 5
    Blit art, x + 26, y - 26, IBRACKET, 12, 5
    Blit art, x - 35, y - 60, IBRACKET + 2, 12, 5
end sub

' The twins on their limbs, each in the picture of what it does, and the ball with a pip over it for each catch
' still to make. A catch telling, they are shocked where they sat, then leap to the pair above.
private sub DrawTwins (lv as Level, zn as Realm, art as Store, cx as integer, cy as integer)
    dim sd as integer, n as integer, x as single, y as single, u as single, i as integer, bx as integer
    dim by as integer
    for sd = 0 to 1
        n = Roost%(lv) * 2 + sd
        x = SeatX%(lv, n)
        y = SeatY%(lv, n)
        u = 0
        if lv.cart.st = 1 and lv.cart.t > 0 then u = lv.cart.t / (DAZED * .6)
        if u > 1 then u = 1
        if u > 0 then
            x = x + (SeatX%(lv, n - 2) - x) * u
            y = y + (SeatY%(lv, n - 2) - y) * u - 24 * 4 * u * (1 - u)
        end if
        Blit art, cint(x) - 15 - cx, cint(y) - 27 - cy, ITWIN + (sd * 5 + TwinPose%(lv, sd)) * 2, 31, 28
    next
    if lv.cart.st = 3 then exit sub
    bx = cint(lv.cart.x) - cx
    by = cint(lv.cart.y) - cy
    DrawKeep zn, art, bx, by
    for i = lv.cart.hits to NBUMP - 1
        line (bx - 10 + i * 8, by - 22)-(bx - 6 + i * 8, by - 18), CYELLOW, BF
    next
end sub

' Frost on her toboggan as the ground lies, shaken while a bump has her dazed, the bumps she can yet take over
' her; knocked off it, she sits in deep snow, the camera lying by her.
private sub DrawFrost (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
    dim x as integer, y as integer, i as integer
    x = cint(lv.cart.x) - cx
    y = cint(lv.cart.y) - cy
    if lv.cart.st = 2 then
        Blit art, x - 62, y - 21, IDUNKED, 56, 22
        DrawKeep zn, art, x, y - ((se.ticks \ 16) and 1)
        exit sub
    end if
    if lv.cart.t > BOLTS then x = x + ((se.ticks \ 2) and 1) * 2 - 1
    Blit art, x - 17, y - 46, IFROST + (Lie%(lv, cint(lv.cart.x), cint(lv.cart.y)) + 2) * 2, 34, 65
    for i = lv.cart.hits to NBUMP - 1
        line (x - 10 + i * 8, y - 40)-(x - 6 + i * 8, y - 36), CYELLOW, BF
    next
end sub

' Kipper's pedal boat on the sea, from one end of its channel to the other and back: faster for each bump it has
' taken, and still a moment after one.
private sub MovePedalo (lv as Level)
    lv.cart.y = lv.tide - PEDALUP
    if lv.cart.t > BOLTS then exit sub
    if lv.cart.v = 0 then lv.cart.v = 1
    lv.cart.v = sgn(lv.cart.v) * (PEDALV + lv.cart.hits * PEDALMORE)
    lv.cart.x = lv.cart.x + lv.cart.v * STEPT
    if lv.cart.x > lv.chaseEnd - PEDALW \ 2 then lv.cart.v = -1
    if lv.cart.x < lv.chaseX + PEDALW \ 2 then lv.cart.v = 1
end sub

' Charlie down onto the pedal boat's deck, out of the air: a bump, it turns round and he goes up off it.
private sub MeetPedalo (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, pt() as Part)
    if lv.cart.t > BOLTS or p.dead or p.ground or p.swim or p.vy <= 0 then exit sub
    if abs(p.x - lv.cart.x) > PEDALW \ 2 or p.y < lv.cart.y - 6 or p.y > lv.cart.y + 14 then exit sub
    Bumped lv, mu, se, g, pt(), lv.cart.y - 8
    lv.cart.v = -lv.cart.v
    p.vy = -BOUNCEV
    p.jumped = 0
end sub

' The pedal boat, a pip over it for each bump it can still take; broken, the teddy afloat where it sank, and the
' cat paddling off a while.
private sub DrawPedalo (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
    dim x as integer, y as integer, i as integer
    x = cint(lv.cart.x) - cx
    y = cint(lv.cart.y) - cy
    if lv.cart.st = 2 then
        DrawKeep zn, art, x, y + 3 - ((se.ticks \ 16) and 1)
        DrawFled lv, art, se, x, y - 9, 120
        exit sub
    end if
    Blit art, x - 26, y - 22, IPEDAL - 2 * (lv.cart.t > BOLTS), 52, 34
    for i = lv.cart.hits to NBUMP - 1
        line (x - 10 + i * 8, y - 34)-(x - 6 + i * 8, y - 30), CYELLOW, BF
    next
end sub

' The cat making off from (x, y), v a second, a while after its ride broke.
private sub DrawFled (lv as Level, art as Store, se as Session, x as integer, y as integer, v as integer)
    if lv.cart.t <= 0 then exit sub
    Blit art, x + cint((2 - lv.cart.t) * v), y, ICAT + ((se.ticks \ 4) and 1) * 2, 20, 14
end sub

' The cart, a pip over it for each bump it can still take; broken, the keepsake where it fell, and the cat making
' off a while.
private sub DrawCart (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
    dim x as integer, y as integer, i as integer
    x = cint(lv.cart.x) - cx
    y = cint(lv.cart.y) - cy
    if lv.cart.st = 2 then
        DrawKeep zn, art, x, y - ((se.ticks \ 16) and 1)
        DrawFled lv, art, se, x, y - 13, 260
        exit sub
    end if
    DrawWheels lv, art, x, cy
    for i = lv.cart.hits to NBUMP - 1
        line (x - 10 + i * 8, y - 50)-(x - 6 + i * 8, y - 46), CYELLOW, BF
    next
end sub

' The cart on its ground, its middle at x on the screen: each wheel on the ground under it, turning as it goes,
' and the planks between them, leaning by what the wheels' grounds differ: not at all, half a gentle slope's fall,
' or all of one's.
private sub DrawWheels (lv as Level, art as Store, x as integer, cy as integer)
    dim ya as integer, yb as integer, lean as integer, k as integer
    ya = Surf%(lv, cint(lv.cart.x) - WHEELS, cint(lv.cart.y))
    yb = Surf%(lv, cint(lv.cart.x) + WHEELS, cint(lv.cart.y))
    if ya = NONE then ya = cint(lv.cart.y)
    if yb = NONE then yb = cint(lv.cart.y)
    lean = (yb - ya + 3) \ 7
    if lean < 0 then lean = 0
    if lean > 2 then lean = 2
    k = ICART + lean * 4
    if lv.cart.t > BOLTS then k = k + 2
    Blit art, x - CARTW \ 2, (ya + yb) \ 2 - cy - 41 - lean * CARTLEAN, k, CARTW, CARTH + lean * CARTLEAN * 2
    k = ICARTW + ((cint(lv.cart.x) \ 8) and 1) * 2
    Blit art, x - WHEELS - 6, ya - cy - 12, k, 13, 13
    Blit art, x + WHEELS - 6, yb - cy - 12, k, 13, 13
end sub

' The crane along its rail, kept to the hall: after Charlie, to be over him, faster for each bump, and still while
' it holds a crate over him; bumped, still a while, then off from him at speed.
private sub MoveCrane (lv as Level, p as Hero)
    dim want as single, dir as integer
    dir = sgn(p.x - lv.cart.x)
    want = dir * (CRANEV + CRANEUP * lv.cart.hits)
    if lv.box.st = 1 then want = 0
    if lv.cart.t > 0 then want = CRANEBOLT * (2 * (dir > 0) + 1)
    if lv.cart.t > BOLTS then want = 0
    lv.cart.v = lv.cart.v + (want - lv.cart.v) * 5 * STEPT
    lv.cart.x = lv.cart.x + lv.cart.v * STEPT
    if lv.cart.x < lv.chaseX + 40 then lv.cart.x = lv.chaseX + 40
    if lv.cart.x > lv.chaseEnd - 40 then lv.cart.x = lv.chaseEnd - 40
end sub

' Charlie thrown up to the crane: in the air at its gondola he bumps it and is knocked aside, out of a jet's way,
' and down, or up off it if he came down on it; its crate is lost.
private sub MeetCrane (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, pt() as Part)
    dim foot as single
    if lv.cart.t > BOLTS or p.dead or p.ground then exit sub
    foot = lv.cart.y - CRANEH
    if abs(p.x - lv.cart.x) > CRANER or p.y - 26 > foot or p.y < foot - 24 then exit sub
    if lv.box.st > 0 or lv.box.t <= 0 then BreakBox lv, mu, se, pt()
    Bumped lv, mu, se, g, pt(), foot - 12
    p.vx = 140 * (2 * (p.x <= lv.cart.x) + 1)
    p.vy = 160
    if p.y < foot - 14 then p.vy = -200
end sub

' The crane's crate, by its state: on the hook, and held still once it is over Charlie; let go a moment later; then
' falling, to hurt him if it meets him and to break on the floor.
private sub DropBox (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    if lv.box.t > 0 then lv.box.t = lv.box.t - STEPT
    select case lv.box.st
    case 0
        lv.box.x = lv.cart.x
        lv.box.y = lv.cart.y - CRANEH + 16
        if lv.box.t <= 0 and lv.cart.t <= 0 and abs(p.x - lv.cart.x) < 10 then
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
        lv.box.y = lv.box.y + lv.box.v * STEPT
        if abs(p.x - lv.box.x) < 14 and p.y > lv.box.y - 16 and p.y - 26 < lv.box.y and not p.dead then
            Hurt mu, se, g, p, cint(lv.box.x), lo(), SHISS
            BreakBox lv, mu, se, pt()
        elseif lv.box.y >= lv.cart.y then
            BreakBox lv, mu, se, pt()
        end if
    end select
end sub

' The crate in bits where it is, the next a while off: sooner for each bump the crane has had.
private sub BreakBox (lv as Level, mu as Deck, se as Session, pt() as Part)
    Burst se, pt(), lv.box.x + 0, lv.box.y - 8, 10, 140, CEARTH + 4
    Sfx mu, se, SPOP
    lv.box.st = 0
    lv.box.t = RELOAD - HASTE * lv.cart.hits
end sub

' The rail over the hall and the crane on it, a pip over it for each bump it can still take, its crate on the hook
' or falling; broken, its lamp out and the toolbox fallen from it.
private sub DrawCrane (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
    dim x as integer, y as integer, i as integer, fy as integer, a as integer, b as integer, jolt as integer
    x = cint(lv.cart.x) - cx
    fy = cint(lv.cart.y) - cy
    y = fy - RAILUP
    a = (cx - lv.chaseX) \ 32
    if a < 0 then a = 0
    b = (cx + SW - lv.chaseX) \ 32
    if b > (lv.chaseEnd - lv.chaseX) \ 32 - 1 then b = (lv.chaseEnd - lv.chaseX) \ 32 - 1
    for i = a to b
        Blit art, lv.chaseX + i * 32 - cx, y, IRAIL, 32, 8
    next
    Blit art, x - 22, y - 2, ICRANE - 2 * (lv.cart.st = 2 or lv.cart.t > BOLTS), 44, 46
    if lv.cart.st = 2 then
        i = cint((lv.cart.t - 2 + TOOLFALL) * CRANEH / TOOLFALL)
        if i < 0 then i = 0
        DrawKeep zn, art, x, fy - i - ((se.ticks \ 16) and 1)
        exit sub
    end if
    for i = lv.cart.hits to NBUMP - 1
        line (x - 10 + i * 8, y - 7)-(x - 6 + i * 8, y - 3), CYELLOW, BF
    next
    if lv.box.st = 0 and lv.box.t > 0 then exit sub
    jolt = (lv.box.st = 1) * ((se.ticks \ 3) and 1)
    Blit art, cint(lv.box.x) - cx - 8 + jolt, cint(lv.box.y) - cy - 16, IWBOX, 16, 16
end sub
