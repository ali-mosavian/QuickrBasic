' Russet Wood: saplings, vines, bramble, logs and burrows, bees, squirrels and fireflies.
defint A-Z
option explicit

from barkley.engine.video import SW
from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck
from barkley.model.images import IACORN
from barkley.model.hero import Hero
from barkley.model.level import Level, Realm, Swarm
from barkley.model.game import Game, Loose, Part, Session, Tones
from barkley.assets.palette import CBEE, CFLY, CINK, CLEAF, CLOAM, CNUT, CVINE, CVINED, CWING, Tint
from barkley.assets.tunes import Sfx, SHURT, SPOP, SROLL, SSPRING
from barkley.world.terrain.ground import Balk
from barkley.world.level import GLOWN
from barkley.stage.fx.parts import Burst
from barkley.stage.fx.damage import Hurt
from barkley.stage.fx.lobs import Loft
from barkley.stage.hero.modes import VINEGRIP, VINELEN
from barkley.stage.hero.draw import LOGR

const GLOWFADE = 60
private const NFLY = 18
const FLYREST = 5

' A sapling: its crown that far from its foot; its throw, what each of his speed adds to it, and at the most;
' the things' steps it stands sprung.
const SAPREACH = 38
private const SAPV = 280!, SAPGAIN = .8, SAPMAX = 700!
const SAPT = 30

' Bramble: how near hurts; the things' steps till it has grown back; its leaves' colour.
private const BRAMBLER = 16
const BRAMBLET = 90

const LOGH = 30, LOGV = 520!

' A burrow's mound: how near he digs in; his speed under the ground, and up out of the other end; the loam's
' colour.
const MOUNDR = 12

' A squirrel on a bough: how far it runs either side of its home, and how far past that it follows him with
' its eye; its run a things' step; the steps between its acorns, the last of them stood with one held out,
' and how near under it he must be for it to fall; the steps a bark keeps it cowed; an acorn's colour.
const SQRUN = 48
private const SQSEE = 100
const SQV = 3, SQWAIT = 60, SQAIM = 15, SQNEAR = 24

const NBEE = 9
private const BEESEE = 60, BEEV = 170!, BEER = 12, BEELOSE = 160

' The wood's night: a bark's light counted down, and the zone's own colours as bright as it leaves them, the
' fireflies' always.
sub Nightfall (lv as Level, zn as Realm, tn as Tones)
    dim i as integer, n as integer, k as integer, c as long
    if lv.glow > 0 then lv.glow = lv.glow - 1
    n = GLOWN
    if lv.glow < GLOWFADE then n = lv.glow * GLOWN \ GLOWFADE
    if n = lv.glowAt then exit sub
    lv.glowAt = n
    for i = 0 to tn.n(zn.stage) - 1
        k = tn.which(zn.stage, i)
        if k = CFLY or k = CFLY + 1 then continue for
        c = tn.own(zn.stage, i)
        Tint tn, k, Dimmed%(c and 63, 2, n) + 256& * Dimmed%(c \ 256 and 63, 2, n) + 65536 * Dimmed%(c \ 65536, 6, n)
    next
end sub

' v of a colour's 63 with n of GLOWN parts of its light: with none, three tenths of it over f.
private function Dimmed% (v as integer, f as integer, n as integer)
    dim d as integer
    d = f + v * 3 \ 10
    Dimmed% = d + (v - d) * n \ GLOWN
end function

' A sapling bowed to the ground: crossing its crown he is thrown up, the higher the faster he came, his way kept;
' it stands sprung a moment before it bows again.
sub MeetSapling (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    dim py as integer, v as single
    py = cint(p.at.y)
    if lv.ob(i).s > 0 or abs(lv.ob(i).x + SAPREACH - cint(p.at.x)) >= 10 then exit sub
    if py < lv.ob(i).y - 12 or py > lv.ob(i).y + 2 or (not p.ground and p.vel.y < 0) then exit sub
    if p.ground then p.vel.x = p.gsp
    v = SAPV + SAPGAIN * abs(p.vel.x)
    if v > SAPMAX then v = SAPMAX
    p.ground = 0
    p.vel.y = -v
    p.roll = 0
    p.jumped = 0
    p.sprung = -1
    lv.ob(i).s = SAPT
    Sfx mu, se, SSPRING
end sub

' A vine's end reached in the air, and no vine just let go: he hangs from it, his way become its swing.
sub MeetVine (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    if p.vine or p.vine_t > 0 or p.ground or p.swim or p.stun or p.dead then exit sub
    if abs(p.at.x - lv.ob(i).x) >= 12 or abs(p.at.y - VINEGRIP - lv.ob(i).y - VINELEN) >= 12 then exit sub
    p.vine = i + 1
    p.sway = (p.at.x - lv.ob(i).x) / VINELEN
    p.sway_v = p.vel.x / VINELEN
    p.roll = 0
    p.jumped = 0
    p.sprung = 0
    Sfx mu, se, SSPRING
end sub

' Bramble: it hurts him, but curled he cuts through it.
sub MeetBramble (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose)
    if lv.ob(i).s > 0 or abs(lv.ob(i).x - p.at.x) >= BRAMBLER then exit sub
    if p.at.y < lv.ob(i).y - 24 or p.at.y > lv.ob(i).y + 8 then exit sub
    if not p.roll then
        Hurt mu, se, g, p, lv.ob(i).x, lo(), SHURT
        exit sub
    end if
    lv.ob(i).s = BRAMBLET
    Sfx mu, se, SPOP
end sub

' A burrow's mound: crouched on it he digs in, for its other end; the mound he comes out of will not take him
' back till he has stepped off it.
sub MeetMound (lv as Level, mu as Deck, se as Session, p as Hero, i as integer, pt() as Part)
    dim onIt as integer
    onIt = abs(lv.ob(i).x - p.at.x) < MOUNDR
    if p.dug = i + 1 then
        if not onIt then p.dug = 0
        exit sub
    end if
    if not (onIt and p.crouch and p.ground) or abs(lv.ob(i).y - p.at.y) >= 8 or lv.ob(i).s < 1 then exit sub
    p.dig = lv.ob(i).s
    p.ground = 0
    p.crouch = 0
    p.gsp = 0
    p.vel.y = 0
    Burst se, pt(), p.at.x, p.at.y - 4, 10, 140, CLOAM
    Sfx mu, se, SROLL
end sub

' A hollow log across the way: on foot he is stopped at its end; curled he is shot through it, out of sight, and
' leaves it at LOGV. Its state: he is in it.
sub MeetLog (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    dim s as integer
    if abs(lv.ob(i).x - p.at.x) >= LOGR or p.at.y <= lv.ob(i).y - LOGH or p.at.y > lv.ob(i).y + 4 then
        lv.ob(i).s = 0
        exit sub
    end if
    if not p.roll then
        Balk p, lv.ob(i).x, LOGR
        exit sub
    end if
    if lv.ob(i).s = 0 then Sfx mu, se, SSPRING
    lv.ob(i).s = 1
    s = 1 + 2 * (p.gsp < 0)
    if p.ground and p.gsp * s < LOGV then p.gsp = s * LOGV
end sub

' Bramble's step: just cut, its leaves fly; cut, it grows back BRAMBLET steps on.
sub Regrow (lv as Level, se as Session, pt() as Part, i as integer)
    if lv.ob(i).s = 0 then exit sub
    if lv.ob(i).s = BRAMBLET then Burst se, pt(), csng(lv.ob(i).x), csng(lv.ob(i).y - 12), 10, 140, CLEAF
    lv.ob(i).s = lv.ob(i).s - 1
end sub

sub Swarms (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose)
    dim i as integer
    for i = 0 to lv.nSwarm - 1
        Buzz mu, se, lv.swarms(i), g, p, lo()
    next
end sub

' A swarm's step: he comes within BEESEE of it and it is after his head, at BEEV; on him, it stings; left
' BEELOSE behind, it goes home.
private sub Buzz (mu as Deck, se as Session, b as Swarm, g as Game, p as Hero, lo() as Loose)
    dim dx as single, dy as single, d as single
    dx = p.at.x - b.at.x
    dy = p.at.y - 13 - b.at.y
    d = sqr(dx * dx + dy * dy)
    if d < BEESEE then b.st = 1
    if b.st = 1 and d > BEELOSE then b.st = 2
    if b.st = 0 then exit sub
    if b.st = 1 and d < BEER + BEEV * STEPT then Hurt mu, se, g, p, cint(b.at.x), lo(), SHURT
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

' The swarms: each bee round its swarm's middle on a loop of its own, its wings over it.
sub DrawBees (lv as Level, se as Session, cx as integer, cy as integer)
    dim i as integer, j as integer, x as integer, y as integer
    for i = 0 to lv.nSwarm - 1
        if abs(lv.swarms(i).at.x - cx - SW \ 2) > SW \ 2 + 24 then continue for
        for j = 0 to NBEE - 1
            x = cint(lv.swarms(i).at.x) - cx + cint(13 * sin(se.ticks * .21 + j * 2.4)) - 2
            y = cint(lv.swarms(i).at.y) - cy + cint(9 * cos(se.ticks * .17 + j * 1.7))
            line (x, y)-(x + 3, y + 1), CBEE, BF
            line (x + 1, y)-(x + 1, y + 1), CINK
            line (x + 3, y)-(x + 3, y + 1), CINK
            line (x + 1, y - 2)-(x + 2, y - 1), CWING, BF
        next
    next
end sub

' A vine hung from x, y on the screen, swung a radians from straight down: two strands, a leaf to each side turn
' about, a loop at its end.
sub DrawVine (x as integer, y as integer, a as single)
    dim j as integer, u as integer, v as integer
    for j = 0 to VINELEN
        u = x + cint(j * sin(a))
        v = y + cint(j * cos(a))
        pset (u, v), CVINE
        pset (u + 1, v), CVINED
        if j mod 7 = 3 then line (u + 2 - 5 * ((j \ 7) and 1), v)-(u + 3 - 5 * ((j \ 7) and 1), v + 1), CVINE, bf
    next
    circle (u, v + 3), 3, CVINE
end sub

' The wood's fireflies over the view, drifting, each dark a moment in turn: a few at rest, and all of them, haloed,
' while a bark's light lasts.
sub DrawFlies (lv as Level, se as Session, cx as integer)
    dim j as integer, x as integer, y as integer
    for j = 0 to FLYREST + (NFLY - FLYREST) * lv.glowAt \ GLOWN - 1
        if ((se.ticks \ 16 + j * 3) and 7) = 0 then continue for
        x = (j * 73 + 31 + SW - (cx \ 2) mod SW) mod SW + cint(6 * sin(se.ticks * .02 + j))
        y = 22 + (j * 47) mod 136 + cint(4 * sin(se.ticks * .031 + j * 2))
        if lv.glowAt = 0 then
            pset (x, y), CFLY
            continue for
        end if
        pset (x, y), CFLY + 1
        pset (x - 1, y), CFLY
        pset (x + 1, y), CFLY
        pset (x, y - 1), CFLY
        pset (x, y + 1), CFLY
    next
end sub

' Squirrel i on its bough: while Charlie is under its run it keeps over him, else it goes home; every SQWAIT
' steps it stops, an acorn held out the last SQAIM of them, and lets it fall once he is near under it. Cowed,
' it keeps still. Its state: the steps till the acorn falls, signed the way it faces.
sub Scamper (lv as Level, i as integer, p as Hero)
    dim n as integer, d as integer, perch as integer, quarry as integer
    n = abs(lv.ob(i).s) - 1
    d = sgn(lv.ob(i).s)
    quarry = abs(p.at.x - lv.ob(i).a) < SQRUN + SQSEE and p.at.y > lv.ob(i).y
    if n = 0 and quarry and abs(p.at.x - lv.ob(i).x) >= SQNEAR then n = 1
    if n = 0 then
        if quarry then Loft lv, lv.ob(i).x + 5 * d, lv.ob(i).y + 1, 0, 0, IACORN, CNUT
        n = SQWAIT
    end if
    lv.ob(i).s = d * n
    if n <= SQAIM or n > SQWAIT then exit sub
    perch = lv.ob(i).a
    if quarry then perch = cint(p.at.x)
    if perch < lv.ob(i).a - SQRUN then perch = lv.ob(i).a - SQRUN
    if perch > lv.ob(i).a + SQRUN then perch = lv.ob(i).a + SQRUN
    if abs(perch - lv.ob(i).x) < SQV then exit sub
    d = sgn(perch - lv.ob(i).x)
    lv.ob(i).x = lv.ob(i).x + d * SQV
    lv.ob(i).s = d * n
end sub
