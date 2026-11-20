' Charlie on what a zone gives him: water, vines, goats, sleds, burrows; the gale and the tide on him.
defint A-Z
option explicit

from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck
from barkley.model.hero import Hero
from barkley.model.level import GRAV, Ground, Level, NONE
from barkley.model.game import Part, Session
from barkley.assets.palette import CLOAM, CSEA
from barkley.assets.tunes import Sfx, SJUMP, SPOP
from barkley.world.terrain.ground import Land, Slow, WADE, Wall
from barkley.stage.fx.parts import Burst, Emit

const GOATV = 240!
private const GOATRIDE = 2!

' A blizzard: of every GUSTP ticks the wind blows the last GUSTON, from the right, the snow streaming GUSTTELL
' before it does. On his feet it bears him back, WINDG a second each second on the ground and WINDA in the air,
' curled or not, till he goes with it at WINDV; curled or crouched on the ground, or mounted, it passes over him.
const GUSTP = 192
private const GUSTON = 72, WINDG = 600!, WINDA = 900!, WINDV = 200!

' A vine: its length; how far under its end his feet hang; the hop up as he lets go, and the seconds before he
' may catch one again; its two colours.
const VINELEN = 56, VINEGRIP = 14
private const VINEHOP = 150!, VINEWAIT = .4

const DIGV = 400!
private const DIGPOP = 260!
const AFLOAT = 6
private const PADV = 80!, PADACC = 260!, HOPV = 250!
const DRYT = .35

' Off the sled, which is no more: a puff of colour c where it was.
sub Alight (mu as Deck, se as Session, p as Hero, pt() as Part, c as integer)
    p.sled = 0
    Burst se, pt(), p.x, p.y - 4, 10, 120, c
    Sfx mu, se, SPOP
end sub

' Hung from a vine: he swings as a weight on it, the vine with him; a jump lets go, with the swing's speed and a
' hop up, and a hurt lets go without one.
sub Hang (lv as Level, mu as Deck, se as Session, p as Hero, hit as integer)
    dim i as integer
    i = p.vine - 1
    p.swayV = p.swayV - GRAV / VINELEN * sin(p.sway) * STEPT
    p.sway = p.sway + p.swayV * STEPT
    p.x = lv.ob(i).x + VINELEN * sin(p.sway)
    p.y = lv.ob(i).y + VINELEN * cos(p.sway) + VINEGRIP
    p.vx = VINELEN * p.swayV * cos(p.sway)
    p.vy = -VINELEN * p.swayV * sin(p.sway)
    lv.ob(i).s = cint(p.sway * 100)
    if not (hit or p.stun or p.dead) then exit sub
    p.vine = 0
    p.vineT = VINEWAIT
    p.want = 0
    if not hit then exit sub
    p.vy = p.vy - VINEHOP
    p.roll = -1
    Sfx mu, se, SJUMP
end sub

' Under the ground to the mound he digs for: straight there at DIGV, unseen, the loam stirred over him; there,
' thrown up out of it.
sub Tunnel (lv as Level, mu as Deck, se as Session, p as Hero, pt() as Part)
    dim i as integer, dx as single
    i = p.dig - 1
    dx = lv.ob(i).x - p.x
    if abs(dx) > DIGV * STEPT then
        p.vx = sgn(dx) * DIGV
        p.x = p.x + p.vx * STEPT
        p.y = p.y + (lv.ob(i).y - p.y) * DIGV * STEPT / abs(dx)
        if (se.ticks and 3) = 0 then Burst se, pt(), p.x, p.y, 2, 50, CLOAM
        exit sub
    end if
    p.x = lv.ob(i).x
    p.y = lv.ob(i).y
    p.vx = 0
    p.vy = -DIGPOP
    p.dig = 0
    p.dug = i + 1
    p.roll = 0
    p.jumped = 0
    p.sprung = -1
    Burst se, pt(), p.x, p.y - 4, 10, 140, CLOAM
    Sfx mu, se, SPOP
end sub

' The ticks of calm left before the blizzard's next gust: 0 while it blows. The wind drops while the avalanche
' comes: nothing may stop him then, and a gust would.
function Lull% (lv as Level, se as Session)
    dim t as integer
    t = GUSTP - GUSTON - se.ticks mod GUSTP
    if t < 0 then t = 0
    if lv.avalX <> NONE and lv.aval.st = 1 then t = GUSTP
    Lull% = t
end function

' A gust on him: borne back on his feet and in the air, spared curled or crouched on the ground, or mounted.
sub Buffet (lv as Level, se as Session, p as Hero)
    if Lull%(lv, se) > 0 or p.swim or p.sled or p.goat then exit sub
    if not p.ground then
        if p.vx > -WINDV then p.vx = p.vx - WINDA * STEPT
    elseif not (p.roll or p.crouch or p.charging) then
        if p.gsp > -WINDV then p.gsp = p.gsp - WINDG * STEPT
    end if
end sub

' Astride a goat: it runs on along the road and takes no rein. Off the ground, stopped or GOATRIDE on, he is off
' it, and it turns for home from where it last stood.
sub Ride (lv as Level, p as Hero, l as integer, r as integer, d as integer)
    p.rideT = p.rideT + STEPT
    l = 0
    r = 0
    d = 0
    if p.rideT > .1 and (not p.ground or p.rideT > GOATRIDE or abs(p.x - p.lastx) < 1) then
        p.goat = 0
        lv.stray.st = 2
    elseif p.ground then
        p.gsp = GOATV
        p.face = 0
        p.roll = 0
        lv.stray.x = p.x
        lv.stray.y = p.y
    end if
end sub

' Charlie and the sea. He is afloat once his feet are under it, falling or deeper than he can wade. Out of it he
' is wet, slow and dripping, till a roll shakes him dry.
sub Soak (lv as Level, se as Session, p as Hero, pt() as Part)
    if p.swim then exit sub
    if (p.ground and p.y > lv.tide + WADE) or (not p.ground and p.vy >= 0 and p.y > lv.tide + AFLOAT) then
        Dunk lv, se, p, pt()
        exit sub
    end if
    if p.wet <= 0 then exit sub
    if p.roll and p.ground then
        p.wet = p.wet - STEPT
        if p.wet <= 0 then Burst se, pt(), p.x, p.y - 12, 10, 130, CSEA + 1
    elseif (se.ticks and 7) = 0 then
        Emit se, pt(), p.x, p.y - 14, 0, 30, .3, CSEA + 2
    end if
end sub

' Into the sea: what he ran at he swims at, and a fall splashes. A hurt ends on landing: the sea is a landing too.
private sub Dunk (lv as Level, se as Session, p as Hero, pt() as Part)
    if p.ground then
        p.vx = p.gsp * cos(p.ang)
    elseif p.vy > 200 then
        Burst se, pt(), p.x, lv.tide + 0!, 8, 110, CSEA
    end if
    p.swim = -1
    p.ground = 0
    p.stun = 0
end sub

' Afloat: at the water's surface as it rises and falls, paddled left and right, slowly; a jump is a hop out of
' it, and ground come to under the surface is waded onto.
sub Paddle (lv as Level, gr as Ground, mu as Deck, se as Session, p as Hero, l as integer, r as integer, hit as integer, pt() as Part)
    p.roll = 0
    p.jumped = 0
    p.sprung = 0
    p.crouch = 0
    p.charging = 0
    p.skid = 0
    p.wet = DRYT
    if hit then
        p.swim = 0
        p.vy = -HOPV
        p.jumped = -1
        Burst se, pt(), p.x, lv.tide + 0!, 5, 80, CSEA
        Sfx mu, se, SJUMP
        exit sub
    end if
    if r then
        if p.vx < PADV then p.vx = p.vx + PADACC * STEPT
        p.face = 0
    elseif l then
        if p.vx > -PADV then p.vx = p.vx - PADACC * STEPT
        p.face = 1
    else
        Slow p.vx, PADACC
    end if
    ' the speed he came in with is soon lost
    if abs(p.vx) > PADV then Slow p.vx, PADACC * 4
    p.x = p.x + p.vx * STEPT
    if p.vx <> 0 then
        if Wall%(lv, p, sgn(p.vx)) then p.vx = 0
    end if
    p.y = lv.tide + AFLOAT
    p.vy = 0
    if Land%(lv, gr, p, 0) then p.swim = 0
end sub
