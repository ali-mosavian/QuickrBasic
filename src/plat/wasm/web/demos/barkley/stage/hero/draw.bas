' Charlie drawn.
defint A-Z
option explicit

from engine.video import Blit, Store, SW
from barkley.model.images import IGOATR, IHERO, ILOG, ISLED, ISNOWBALL
from barkley.model.images import ITILT, ITW, NF
from barkley.model.hero import Hero
from barkley.model.level import Ground, KLOG, KWAVE2, Level, PI, Realm
from barkley.model.level import TS
from barkley.model.game import Session
from barkley.assets.palette import CWHITE
from barkley.world.terrain.ground import SegAt, Slant, Twist, WAVEL
from barkley.world.terrain.track import WAVEA
from barkley.stage.hero.moves import BARKT, TOP

private const FIDLE = 0
private const FBLINK = 1
private const FTAP = 2
private const FRUN = 4
private const FDASH = 10
private const FBALL = 12
private const FSKID = 16
private const FCROUCH = 17
private const FHURT = 18
private const FUP = 19
private const FWIN = 20
private const FSHIVER = 22
private const SLEDH = 5
private const GOATH = 14
const WAVEH = WAVEA + 36

' A hollow log: half its length, its height, and the speed
' he is shot through it curled.
const LOGR = 32

' Charlie as a cue has it, at x, y in the world. a: 0 stood,
' 1 running, 2 at full tilt, 3 curled, 4 crouched, 5 shaken,
' 6 glad, 7 wound up; 16 more, or west, facing left.
sub Pose ( _
    lv as Level, _
    gr as Ground, _
    se as Session, _
    p as Hero, _
    x as integer, _
    y as integer, _
    a as integer, _
    west as integer _
)
    dim fresh as Hero

    p = fresh
    p.at.x = x
    p.at.y = y
    p.ground = -1
    p.sg = SegAt%(lv, gr, x, csng(lv.start.y))
    p.anim = se.ticks / 4!
    if west or (a and 16) then p.face = 1

    select case a and 15
    case 1
        p.gsp = 200 * (1 - 2 * p.face)
    case 2
        p.gsp = 450 * (1 - 2 * p.face)
    case 3
        p.roll = -1
    case 4
        p.crouch = -1
    case 5
        p.stun = -1
    case 6
        p.win = -1
    case 7
        p.charging = -1
    end select
end sub

' Charlie as a cue has him barking, at x, y facing face, t
' of his bark left.
sub Woof ( _
    p as Hero, _
    x as integer, _
    y as integer, _
    face as integer, _
    t as single _
)
    p.at.x = x
    p.at.y = y
    p.face = face
    p.bark_t = t
end sub

' Charlie, and streaks of wind behind it at speed; running
' on a slope, the frame turned to it. What is nearer than
' Charlie: the near half of each ribbon's twist, which hides
' what runs behind it, and a hollow log.
sub DrawOver ( _
    lv as Level, _
    zn as Realm, _
    art as Store, _
    cx as integer, _
    cy as integer _
)
    dim i as integer, c as integer, x as integer

    c = (cx - WAVEL) \ TS
    if c < 0 then c = 0
    i = lv.first(c)

    do while i < lv.n_things
        x = lv.things(i).at.x - cx
        if x > SW then exit do

        if lv.things(i).kind = KWAVE2 then
            Blit art, x, lv.things(i).at.y - cy - WAVEA - 22, _
                zn.wave + 2, WAVEL, WAVEH
        end if

        if lv.things(i).kind = KLOG then
            Blit art, x - LOGR, lv.things(i).at.y - cy - 31, ILOG, _
                64, 32
        end if

        i = i + 1
    loop
end sub

' The bark, seen: two arcs before his muzzle, wider as it
' goes.
sub DrawBark (p as Hero, cx as integer, cy as integer)
    dim x as integer, y as integer, r as integer, a as single

    x = cint(p.at.x) - cx + (1 - 2 * p.face) * 12
    y = cint(p.at.y) - cy - 18
    r = 5 + cint((BARKT - p.bark_t) * 50)
    ' a quarter turn about the way he faces: to the right
    ' through 0, to the left about PI
    a = PI * (7 / 4 - p.face)
    circle (x, y), r, CWHITE, a, a + PI / 2 - 2 * PI * (1 - p.face)
    circle (x, y), r + 4, CWHITE, a, _
        a + PI / 2 - 2 * PI * (1 - p.face)
end sub

sub DrawHero ( _
    gr as Ground, _
    art as Store, _
    se as Session, _
    p as Hero, _
    cx as integer, _
    cy as integer _
)
    dim x as integer, y as integer, f as integer, t as integer
    dim tw as integer
    dim hu as single, hv as single, a as single, v as single

    if p.dig then exit sub
    if p.inv > 0 and not p.stun and (se.ticks and 4) then exit sub

    x = cint(p.at.x) - cx
    y = cint(p.at.y) - cy
    Footing gr, p, hu, hv, a, tw
    v = p.vel.x
    if p.ground then v = p.gsp

    if abs(v) > 400 and not p.dead then
        SpeedLines se, x, y - cint(14 * hu), sgn(v)
    end if

    f = HeroFrame%(se, p)

    ' on a sled: it lies as the ground does, and he stands
    ' on it
    if p.sled then
        Blit art, x - 15, y - 25, ISLED + (Slant%(p) + 2) * 2, 30, 42
        x = x + cint(SLEDH * hv)
        y = y - cint(SLEDH * hu)
    end if

    ' astride a goat: it gallops as the ground lies, and he
    ' is GOATH up its back
    if p.goat then
        Blit art, x - 17, y - 40, _
            IGOATR + ((Slant%(p) + 2) * 2 + ((se.ticks \ 6) and 1)) _
            * 4, 34, 58
        x = x + cint(GOATH * hv)
        y = y - cint(GOATH * hu)
    end if

    if tw and f >= FRUN and f < FBALL then
        DrawTwisted art, p, x, y, f, a
        exit sub
    end if

    if p.ground and f >= FRUN and f < FBALL then t = Turned%(p)

    if t = 0 then
        DrawUpright art, se, p, x, y, f, hu, hv
        exit sub
    end if

    DrawTilted art, p, x, y, f, t, hu, hv
end sub

' The ground's lie under him, hu and hv; round a ribbon's
' twist he keeps to the inside, over the line then under it:
' a the turn he is at, tw set.
private sub Footing ( _
    gr as Ground, _
    p as Hero, _
    hu as single, _
    hv as single, _
    a as single, _
    tw as integer _
)
    hu = 1
    if not p.ground then exit sub

    hu = gr.pt(p.sg).dir.x
    hv = gr.pt(p.sg).dir.y
    if Twist!(gr, p) < 0 then exit sub

    a = Twist!(gr, p)
    hu = cos(a)
    hv = 0
    tw = -1
end sub

' The air he cuts at speed: four lines behind him, d the way
' he goes, about height y.
private sub SpeedLines ( _
    se as Session, _
    x as integer, _
    y as integer, _
    d as integer _
)
    dim j as integer, sy as integer, n as integer

    for j = 0 to 3
        sy = y - 12 + (se.ticks * 7 + j * 11) mod 22
        n = 12 + (se.ticks * 5 + j * 9) mod 18
        line (x - d * 14, sy)-(x - d * (14 + n), sy), CWHITE
    next
end sub

' Running round a twist: rolled with the ribbon, a twelfth
' of a turn a picture, its paws' line 12 off its middle.
private sub DrawTwisted ( _
    art as Store, _
    p as Hero, _
    x as integer, _
    y as integer, _
    f as integer, _
    a as single _
)
    dim t as integer, j as integer

    t = cint(a * 6 / PI) mod 12
    j = (f - FRUN) \ 3
    if f >= FDASH then j = f - FDASH
    Blit art, x - 16, y - 16 - cint(12 * cos(t * PI / 6)), _
        ITW + (p.face * 24 + t * 2 + j) * 2, 32, 32
end sub

' How far he is turned to the ground he runs on: sixteenths
' of a turn, nose up.
private function Turned% (p as Hero)
    dim v as single, t as integer

    v = p.ang
    if p.face then v = -v
    t = cint(v * 8 / PI)
    if t < 0 then t = t + 16
    Turned% = t and 15
end function

' Not turned: as drawn. Curled on the ground, his middle is
' 13 off it, whichever way that is; in a snowball none of
' him shows; afraid, he shakes.
private sub DrawUpright ( _
    art as Store, _
    se as Session, _
    p as Hero, _
    x as integer, _
    y as integer, _
    f as integer, _
    hu as single, _
    hv as single _
)
    dim curled as integer, bx as integer, by as integer

    bx = x
    by = y
    curled = f >= FBALL and f < FBALL + 4

    if p.ground and curled then
        bx = bx + cint(13 * hv)
        by = by + 13 - cint(13 * hu)
    end if

    if p.snow < 0 and curled then
        Blit art, bx - 14, by - 28, _
            ISNOWBALL + (int(p.anim) and 1) * 2, 28, 28
        exit sub
    end if

    if f = FHURT or f = FSHIVER then
        bx = bx + ((se.ticks \ 2) and 1) * 2 - 1
    end if

    Blit art, bx - 16, by - 31, IHERO + (p.face * NF + f) * 2, 32, 32
end sub

' Turned about his middle, 12 off the ground, t sixteenths:
' the picture as drawn (0) or mirrored to face left (1);
' past half a turn the one half a turn back upside down,
' mirrored (3) or not (2).
private sub DrawTilted ( _
    art as Store, _
    p as Hero, _
    x as integer, _
    y as integer, _
    f as integer, _
    t as integer, _
    hu as single, _
    hv as single _
)
    dim j as integer, k as integer, q as integer, bx as integer
    dim by as integer

    k = (f - FRUN) \ 2
    if f >= FDASH then k = 3 + f - FDASH
    j = p.face
    q = t

    if q >= 8 then
        q = q - 8
        j = 3 - j
    end if

    bx = x + cint(12 * hv) - 16 + (j and 1)
    by = y - cint(12 * hu) - 18 - 5 * (j > 1)
    Blit art, bx, by, ITILT + (j * 40 + q * 5 + k) * 2, 32, 32
end sub

private function HeroFrame% (se as Session, p as Hero)
    HeroFrame% = FRUN + int(p.anim) mod 6

    if p.dead or p.stun then
        HeroFrame% = FHURT
        if (se.ticks \ 3) and 1 then HeroFrame% = FSHIVER
    elseif p.sled or p.goat then
        HeroFrame% = FDASH
    elseif p.roll or p.charging then
        HeroFrame% = FBALL + (int(p.anim) and 3)
    elseif p.swim then
        HeroFrame% = FRUN + (se.ticks \ 10) mod 6
    elseif not p.ground then
        if p.sprung or p.vine then HeroFrame% = FUP
    elseif p.skid then
        HeroFrame% = FSKID
    elseif p.crouch then
        HeroFrame% = FCROUCH
    elseif p.win then
        HeroFrame% = FWIN + ((se.ticks \ 20) and 1)
    elseif abs(p.gsp) < 5 then
        HeroFrame% = Standing%(se, p)
    elseif abs(p.gsp) >= TOP - 5 then
        HeroFrame% = FDASH + (int(p.anim) and 1)
    end if
end function

' Stood still: a blink now and then, and after a while a tap
' of the paw.
private function Standing% (se as Session, p as Hero)
    Standing% = FIDLE
    if se.ticks mod 180 < 8 then Standing% = FBLINK
    if p.idle_t > 3 then Standing% = FTAP + ((se.ticks \ 12) and 1)
end function
