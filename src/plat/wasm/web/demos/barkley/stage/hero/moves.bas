' Charlie's moves: walking, jumping, rolling, winding up, in the air; his bark.
defint A-Z
option explicit

from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck
from barkley.model.hero import Hero, HH, Pad
from barkley.model.level import GRAV, Ground, KCAT, KCRAB, KICICLE
from barkley.model.level import KLOAD, KPOUNCE, KSNATCH, KSQUIRREL
from barkley.model.level import KTHROW, KWALKER, Level, NONE, TS
from barkley.model.game import Part, Session
from barkley.assets.palette import CEARTH, CSTONE, CWHITE
from barkley.assets.tunes import SBARK, SCHARGE, Sfx, SJUMP, SLAUNCH
from barkley.assets.tunes import SPOP, SROLL
from barkley.world.terrain.ground import Along, Curve, Drop, Land
from barkley.world.terrain.ground import Slow, Solid, STALL, Twist
from barkley.world.terrain.ground import Wall
from barkley.stage.fx.parts import Burst, Emit
from barkley.stage.hero.modes import Alight, Buffet, Hang, Paddle
from barkley.stage.hero.modes import Ride, Soak, Tunnel

const JUMPV = 330!
private const JUMPCUT = 160!
const TOP = 360!
private const RUNACC = 280!
private const DEC = 1800!
private const FRIC = 280!
private const AIRACC = 500!
private const SLOPE = 380!
private const RFRIC = 60!
private const RDEC = 450!
private const RUPS = 300!
private const RDNS = 1100!
private const MAXV = 900!

' On bare ice: what is left of the ground's braking and its friction, and of a run's push.
private const ICEGRIP = .12
private const ICEPUSH = .4
private const CHARGE0 = 480!
private const CHARGEMAX = 360!

' A jump asked for this long before landing is made on it; one this long after running off an edge still is.
private const JBUFFER = .1

' The pass's own: deep snow, and a sled. Boarded, a sled goes off at SLEDV at the least; the snow's friction on
' it is SLEDGRIP of a roll's; Charlie stands SLEDH over the ground on it.
private const SLEDGRIP = .3

const THROWC = 75

' The wood's night: its fireflies' two colours, never dimmed; the steps a bark's light lasts, the last of them
' fading; the parts its light is counted in; the fireflies awake, and at rest.
const GLOWT = 300

private const SQFRIGHT = 150

' A bark: how far it carries, the seconds to the next, and the steps a cat stands startled by it.
private const BARKR = 72
const BARKT = .4
private const STARTLED = 24

' A crab: the steps it keeps to its shell once barked at, and how fast Charlie goes up off its back.
const SHELLT = 150

private const GULLHEAR = 130
const GSCARED = 4
const WETTOP = 220!

sub MoveHero (lv as Level, gr as Ground, mu as Deck, se as Session, p as Hero, k as Pad, pt() as Part)
    dim l as integer, r as integer, d as integer, hit as integer
    dim hold as integer
    dim v as single
    if p.dig then
        Tunnel lv, mu, se, p, pt()
        exit sub
    end if
    ' a snowball lasts as long as its roll; broken, by a hit or his uncurling, it flies apart
    if p.snow < 0 and not p.roll then p.snow = 1
    if p.snow > 0 then
        p.snow = 0
        Burst se, pt(), p.at.x, p.at.y - 12, 12, 140, CWHITE
        Sfx mu, se, SPOP
    end if
    ' a ride lasts as long as its roll: come to a stand, hurt or thrown, the sled is left in pieces
    if p.sled and not p.roll then Alight mu, se, p, pt(), CEARTH + 4
    ' a jump held through the hit is a jump as it ends: it was forgotten
    if p.stun and p.ground and p.inv < 1 then
        p.stun = 0
        if k.jump then k.hit = -1
    end if
    if p.stun = 0 then
        l = k.left
        r = k.right
        d = k.down
        hit = k.hit
        hold = k.jump
    end if
    if p.bark_t > 0 then p.bark_t = p.bark_t - STEPT
    if p.vine_t > 0 then p.vine_t = p.vine_t - STEPT
    if k.bark and p.stun = 0 and p.bark_t <= 0 then Bark lv, mu, se, p
    KeepJump mu, se, p, hit
    if p.coyote > 0 then p.coyote = p.coyote - STEPT
    if p.inv > 0 then p.inv = p.inv - STEPT
    if p.goat then Ride lv, p, l, r, d
    p.last_x = p.at.x
    ' slid off a slope too steep for its speed, the keys are no help a while
    if p.slip > 0 and p.ground then
        l = 0
        r = 0
    end if
    if p.slip > 0 then p.slip = p.slip - STEPT
    if lv.gale then Buffet lv, se, p
    if p.vine then
        Hang lv, mu, se, p, hit
    elseif p.swim then
        Paddle lv, gr, mu, se, p, l, r, hit, pt()
    elseif p.ground then
        Walk lv, gr, mu, se, p, l, r, d, hit, pt()
    else
        Airborne lv, gr, se, p, l, r, d, hold, pt()
    end if
    if lv.tide <> NONE then Soak lv, se, p, pt()
    if p.at.x < 8 then
        p.at.x = 8
        p.gsp = 0
        p.vel.x = 0
    end if
    if p.at.x > lv.wpx - 8 then
        p.at.x = lv.wpx - 8
        p.gsp = 0
        p.vel.x = 0
    end if
    if p.ground then
        v = abs(p.gsp)
    else
        v = abs(p.vel.x)
    end if
    if v < 5 and p.ground then
        p.idle_t = p.idle_t + STEPT
    else
        p.idle_t = 0
    end if
    if p.roll or p.charging then
        if p.charging then v = 300 + p.charge
        p.anim = p.anim + (v + 120) * STEPT / 9
    elseif v >= TOP - 5 then
        p.anim = p.anim + 20 * STEPT
    else
        p.anim = p.anim + v * STEPT / 7
    end if
end sub

' A jump pressed just before landing, or just after the ground ran out, was lost: it is kept a moment.
private sub KeepJump (mu as Deck, se as Session, p as Hero, hit as integer)
    if hit then p.want = JBUFFER
    if p.want <= 0 then exit sub
    p.want = p.want - STEPT
    if p.ground then
        hit = -1
        p.want = 0
        exit sub
    end if
    if p.coyote <= 0 then exit sub
    p.vel.y = -JUMPV
    p.jumped = -1
    p.roll = -1
    p.coyote = 0
    p.want = 0
    Sfx mu, se, SJUMP
end sub

' Whether Charlie is on bare ice: it keeps his speed and will not give it back.
private function Slick% (lv as Level, gr as Ground, p as Hero)
    Slick% = 0
    if gr.pt(p.sg).kind = 1 and p.at.x >= 0 then
        Slick% = lv.glaze(cint(p.at.x) \ TS)
    end if
end function

' On the ground: wind up, jump, roll, run; then along it, a few pixels a
' move, into walls and onto the ground's curve.
private sub Walk (lv as Level, gr as Ground, mu as Deck, se as Session, p as Hero, l as integer, r as integer, d as integer, hit as integer, pt() as Part)
    dim n as integer, s as integer, i as integer, grip as single
    dim push as single
    grip = 1
    push = 1
    if Slick%(lv, gr, p) then
        grip = ICEGRIP
        push = ICEPUSH
    end if
    if p.sled then grip = grip * SLEDGRIP
    if p.charging then
        if Winding%(mu, se, p, d, hit, pt()) then exit sub
    elseif hit then
        Leap mu, se, p
        exit sub
    end if
    p.crouch = d and abs(p.gsp) < 20 and not p.roll
    p.skid = 0
    if p.roll then
        RollOn p, l, r, grip
    else
        RunOn mu, se, p, l, r, d, grip, push, pt()
    end if
    if p.gsp > MAXV then p.gsp = MAXV
    if p.gsp < -MAXV then p.gsp = -MAXV
    n = 1 + int(abs(p.gsp) * STEPT / 8)
    for s = 1 to n
        Along gr, p, p.gsp * STEPT / n
        if not p.ground then exit sub
        ' a wall stops it on the tiles' ground; a curve is run up
        i = p.sg
        if gr.pt(i).kind = 2 or p.gsp = 0 then continue for
        if Wall%(lv, p, sgn(p.gsp)) = 0 then continue for
        p.gsp = 0
        p.at.y = gr.pt(i).at.y + (p.at.x - gr.pt(i).at.x) * gr.pt(i).dir.y / gr.pt(i).dir.x
        Along gr, p, 0
        if not p.ground then exit sub
    next
    Falter gr, p
end sub

' A dash wound up: each press winds it further; Down let go, he is away. Whether it is still held.
private function Winding% (mu as Deck, se as Session, p as Hero, d as integer, hit as integer, pt() as Part)
    dim dir as integer
    dir = 1 - 2 * p.face
    p.charge = p.charge * (1 - 2 * STEPT)
    if hit then
        p.charge = p.charge + 130
        if p.charge > CHARGEMAX then p.charge = CHARGEMAX
        Sfx mu, se, SCHARGE
        Burst se, pt(), p.at.x - dir * 10, p.at.y - 2, 3, 70, CSTONE + 3
    end if
    Winding% = d
    if d then exit function
    p.charging = 0
    p.roll = -1
    p.gsp = (CHARGE0 + p.charge) * dir
    Sfx mu, se, SLAUNCH
end function

' The jump key on the ground: from a crouch a dash is begun; else he jumps, off the ground as it lies.
private sub Leap (mu as Deck, se as Session, p as Hero)
    dim sn as single, cs as single
    if p.crouch then
        p.charging = -1
        p.charge = 0
        Sfx mu, se, SCHARGE
        exit sub
    end if
    sn = sin(p.ang)
    cs = cos(p.ang)
    p.vel.x = p.gsp * cs - JUMPV * sn
    p.vel.y = -p.gsp * sn - JUMPV * cs
    p.ground = 0
    p.roll = -1
    p.jumped = -1
    p.crouch = 0
    Sfx mu, se, SJUMP
end sub

' Rolling: the slope pulls harder down than it holds back up, and the keys only brake.
private sub RollOn (p as Hero, l as integer, r as integer, grip as single)
    dim sn as single, f as single
    sn = sin(p.ang)
    f = RDNS
    if (p.gsp > 0) = (sn > 0) then f = RUPS
    p.gsp = p.gsp - f * sn * STEPT
    ' a sled has no brake
    if l and p.gsp > 0 and not p.sled then
        p.gsp = p.gsp - RDEC * grip * STEPT
    end if
    if r and p.gsp < 0 and not p.sled then
        p.gsp = p.gsp + RDEC * grip * STEPT
    end if
    Slow p.gsp, RFRIC * grip
    if abs(p.gsp) < 30 and abs(sn) < .2 then p.roll = 0
end sub

' Running: the slope, then the keys, or with none the ground's drag. Down at speed, he curls up.
private sub RunOn (mu as Deck, se as Session, p as Hero, l as integer, r as integer, d as integer, grip as single, push as single, pt() as Part)
    dim sn as single, cs as single
    sn = sin(p.ang)
    cs = cos(p.ang)
    if p.gsp <> 0 or abs(sn) > .75 or p.slip > 0 then
        p.gsp = p.gsp - SLOPE * sn * STEPT
    end if
    if p.crouch then
        Slow p.gsp, FRIC * 4 * grip
    elseif r then
        Shove p, 1, grip, push
    elseif l then
        Shove p, -1, grip, push
    elseif p.slip <= 0 and cs >= .7 then
        ' on steep ground nothing but the slope slows it
        Slow p.gsp, FRIC * grip
    end if
    if d and abs(p.gsp) >= 60 then
        p.roll = -1
        p.crouch = 0
        Sfx mu, se, SROLL
    end if
    if p.skid and (se.ticks and 3) = 0 then
        Emit se, pt(), p.at.x, p.at.y - 1, -p.gsp * .1, -30, .4, CSTONE + 3
    end if
end sub

' A key held, way 1 right or -1 left: against his speed a brake, and a skid; with it a push, up to his most.
private sub Shove (p as Hero, way as integer, grip as single, push as single)
    dim v as single, most as single
    most = TOP
    if p.wet > 0 then most = WETTOP
    v = p.gsp * way
    if v < 0 then
        v = v + DEC * grip * STEPT
        p.skid = v < -80
    elseif v < most then
        v = v + RUNACC * push * STEPT
        if v > most then v = most
    end if
    p.gsp = v * way
    if v >= 0 then p.face = (1 - way) \ 2
end sub

' Too slow for a wall or a roof, off it; for a steep slope, down it; and off the roof of a ribbon's twist, down
' past the ribbon.
private sub Falter (gr as Ground, p as Hero)
    if abs(p.gsp) >= STALL then exit sub
    if gr.pt(p.sg).dir.x < .35 then
        Drop gr, p
    elseif gr.pt(p.sg).dir.x < .7 then
        p.slip = .5
    end if
    if Twist!(gr, p) < 0 then exit sub
    if cos(Twist!(gr, p)) >= -.5 then exit sub
    Drop gr, p
    p.at.y = p.at.y + 26
end sub

' In the air: gravity, steering, a jump cut short when let go; then a few
' pixels a move, into walls, ceilings and onto the ground.
private sub Airborne (lv as Level, gr as Ground, se as Session, p as Hero, l as integer, r as integer, d as integer, hold as integer, pt() as Part)
    dim n as integer, s as integer, m as single, landed as integer
    dim fell as single, x0 as single, y0 as single
    p.crouch = 0
    p.charging = 0
    p.skid = 0
    p.vel.y = p.vel.y + GRAV * STEPT
    if p.vel.y > 900 then p.vel.y = 900
    if r then
        Steer p, 1
    elseif l then
        Steer p, -1
    end if
    if p.jumped and not hold and p.vel.y < -JUMPCUT then
        p.vel.y = -JUMPCUT
    end if
    m = abs(p.vel.x)
    if abs(p.vel.y) > m then m = abs(p.vel.y)
    n = 1 + int(m * STEPT / 8)
    for s = 1 to n
        x0 = p.at.x
        y0 = p.at.y
        p.at.x = p.at.x + p.vel.x * STEPT / n
        if p.vel.x <> 0 then
            if Wall%(lv, p, sgn(p.vel.x)) then p.vel.x = 0
        end if
        p.at.y = p.at.y + p.vel.y * STEPT / n
        fell = p.vel.y
        landed = Curve%(gr, p, x0, y0, d)
        if landed then exit for
        if p.vel.y < 0 then
            Bonk lv, p
            continue for
        end if
        fell = p.vel.y
        landed = Land%(lv, gr, p, d)
        if landed then exit for
    next
    if landed and fell > 420 then
        Burst se, pt(), p.at.x, p.at.y - 2, 5, 60, CSTONE + 3
    end if
end sub

' In the air a key held, way 1 right or -1 left: he turns that way and gathers speed, up to his most.
private sub Steer (p as Hero, way as integer)
    dim v as single
    p.face = (1 - way) \ 2
    v = p.vel.x * way
    if v >= TOP then exit sub
    v = v + AIRACC * STEPT
    if v > TOP then v = TOP
    p.vel.x = v * way
end sub

' Rising, his head against a ceiling: stopped under it.
private sub Bonk (lv as Level, p as Hero)
    dim x as integer, y as integer
    x = cint(p.at.x)
    y = cint(p.at.y) - HH
    if Solid%(lv, x, y) = 0 and Solid%(lv, x - 4, y) = 0 and Solid%(lv, x + 4, y) = 0 then exit sub
    p.at.y = (y \ TS + 1) * TS + HH
    p.vel.y = 0
end sub

' A cat barked at from fromX stands startled STARTLED steps, then prowls away from there: its state's size is the
' steps left, its sign the way it will go.
private sub Flinch (lv as Level, i as integer, from_x as integer)
    lv.things(i).state = STARTLED
    if lv.things(i).at.x < from_x then lv.things(i).state = -STARTLED
end sub

' Charlie barks: every cat the bark reaches flinches, every crab goes into its shell, every gull is scared, every
' snow-thrower ducks, every squirrel is cowed, every pouncer leaps.
private sub Bark (lv as Level, mu as Deck, se as Session, p as Hero)
    dim i as integer, c as integer, dx as single, dy as single
    dim hung as integer, cowed as integer, early as integer
    p.bark_t = BARKT
    Sfx mu, se, SBARK
    if lv.murk then lv.glow = GLOWT
    c = (cint(p.at.x) - BARKR - 40) \ TS
    if c < 0 then c = 0
    for i = lv.first(c) to lv.n_things - 1
        if lv.things(i).home > p.at.x + BARKR + 40 then exit for
        dx = abs(lv.things(i).at.x - p.at.x)
        dy = abs(lv.things(i).at.y - p.at.y)
        ' an icicle or a ledge's load is loosed from further under it than he can jump
        hung = lv.things(i).kind = KICICLE or lv.things(i).kind = KLOAD
        if hung and lv.things(i).state = 0 and dx <= BARKR and dy < 140 then
            lv.things(i).state = 1
        end if
        ' and a squirrel on its bough is cowed
        cowed = lv.things(i).kind = KSQUIRREL and lv.things(i).state <> 0 and dx <= BARKR and dy < 140
        if cowed then
            lv.things(i).state = sgn(lv.things(i).state) * SQFRIGHT
        end if
        ' and a pouncer in its bin leaps early, at where he is
        early = lv.things(i).kind = KPOUNCE and lv.things(i).state = 0 and dx <= BARKR and dy < 40
        if early then
            lv.things(i).state = 1 + 2 * (p.at.x < lv.things(i).at.x)
        end if
        if lv.things(i).state = 0 or dx > BARKR or dy >= 40 then continue for
        select case lv.things(i).kind
        case KCAT, KWALKER, KSNATCH
            Flinch lv, i, cint(p.at.x)
        case KCRAB
            lv.things(i).state = sgn(lv.things(i).state) * SHELLT
        case KTHROW
            lv.things(i).state = sgn(lv.things(i).state) * THROWC
        end select
    next
    for i = 0 to lv.n_gulls - 1
        if lv.gulls(i).st <= 0 then continue for
        if abs(lv.gulls(i).at.x - p.at.x) >= GULLHEAR or abs(lv.gulls(i).at.y - p.at.y) >= GULLHEAR then continue for
        lv.gulls(i).st = GSCARED
    next
end sub
