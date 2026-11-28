' The fight: the craft's guns and rockets, the foes'
' flak and bolts, shots flown and what they hit, and the
' blasts they leave.
option explicit

from highvale.model.space import Point3
from highvale.model.land import Land
from highvale.model.craft import Craft, Controls
from highvale.model.foes import Foe, Shot, Blast
from highvale.model.foes import FLAK, DRONE
from highvale.model.foes import ROUND, ROCKET, BURST, BOLT
from highvale.model.foes import SPARK, BLOW
from highvale.land.maps import ground_at, clear_line, wrapped
from highvale.play.flight import on_map

' a shot's speed in texels a second and its life
const ROUND_SPEED = 240
const ROUND_LIFE = 1.6
const ROCKET_SPEED = 170
const ROCKET_LIFE = 2.6
const BURST_SPEED = 95
const BURST_LIFE = 4
const BOLT_SPEED = 130
const BOLT_LIFE = 2.5
' seconds between the craft's shots
const GUN_GAP = 0.09
const ROCKET_GAP = 0.7
' the guns' spread from the middle, in texels
const GUN_SIDE = 1.5
' what each shot takes off what it hits
const ROUND_HARM = 1
const ROCKET_HARM = 8
const BURST_HARM = 9
const BOLT_HARM = 6
' a rocket's blast reach, in texels
const ROCKET_REACH = 10
' how near counts as a hit, across and up
const HIT_REACH = 4
const HIT_RISE = 5
' a foe's strength
const FLAK_HP = 6
const DRONE_HP = 4
' the foes' ranges, their rates and gaps between shots
const FLAK_RANGE = 230
const FLAK_GAP = 2.4
const DRONE_SIGHT = 260
const DRONE_RANGE = 190
const DRONE_GAP = 1.7
const DRONE_SPEED = 30
const DRONE_TURN = 1.1
const DRONE_RING = 45
const DRONE_LIFT = 12
' a drone fires when its nose is this near the craft
const DRONE_CONE = 0.35
' seconds a blast lasts
const SPARK_LIFE = 0.25
const BLOW_LIFE = 0.9
const PI = 3.14159265

' A shot launched, into the first free place.
sub launch_shot ( _
    shots() as Shot, _
    byval kind as integer, _
    from_p as Point3, _
    way as Point3, _
    byval speed as single, _
    byval life as single _
)
    dim i as integer

    for i = 0 to ubound(shots)
        if shots(i).kind then continue for
        shots(i).kind = kind
        shots(i).place = from_p
        shots(i).motion.x = way.x * speed
        shots(i).motion.y = way.y * speed
        shots(i).motion.z = way.z * speed
        shots(i).life = life
        exit sub
    next
end sub

' A blast at p, into the first free place.
sub set_blast (blasts() as Blast, byval kind as integer, p as Point3)
    dim i as integer

    for i = 0 to ubound(blasts)
        if blasts(i).kind then continue for
        blasts(i).kind = kind
        blasts(i).place = p
        blasts(i).age = 0
        exit sub
    next
end sub

' The craft's guns and rocket, as the keys ask; fall is
' the aim's fall a texel ahead.
sub fire_craft ( _
    c as Craft, _
    ctl as Controls, _
    shots() as Shot, _
    byval fall as single, _
    byval dt as single _
)
    dim way as Point3
    dim muzzle as Point3

    c.gun_wait = c.gun_wait - dt
    c.rocket_wait = c.rocket_wait - dt
    way.x = cos(c.heading)
    way.y = sin(c.heading)
    way.z = -fall
    muzzle = c.place

    if ctl.guns and c.gun_wait <= 0 and c.rounds > 0 then
        c.side = -c.side
        if c.side = 0 then c.side = 1
        muzzle.x = c.place.x - way.y * GUN_SIDE * c.side
        muzzle.y = c.place.y + way.x * GUN_SIDE * c.side
        launch_shot shots(), ROUND, muzzle, way, ROUND_SPEED, _
            ROUND_LIFE
        c.rounds = c.rounds - 1
        c.gun_wait = GUN_GAP
    end if

    if ctl.launch = 0 or c.rocket_wait > 0 then exit sub
    if c.rockets = 0 then exit sub

    launch_shot shots(), ROCKET, c.place, way, ROCKET_SPEED, _
        ROCKET_LIFE
    c.rockets = c.rockets - 1
    c.rocket_wait = ROCKET_GAP
end sub

' Whether p is within a hit of q.
function near_to (p as Point3, q as Point3) as integer
    dim dx as single
    dim dy as single

    dx = wrapped(p.x - q.x)
    dy = wrapped(p.y - q.y)
    near_to = dx * dx + dy * dy < HIT_REACH * HIT_REACH and _
        abs(p.z - q.z) < HIT_RISE
end function

' Harm to foe f; a blow when it is done for.
sub harm_foe ( _
    f as Foe, _
    byval harm as integer, _
    blasts() as Blast _
)
    f.hp = f.hp - harm
    if f.hp > 0 then exit sub

    set_blast blasts(), BLOW, f.place
    if f.kind = DRONE then f.kind = 0
end sub

' A rocket's blast: harm to every live foe within reach.
sub burst_rocket (p as Point3, foes() as Foe, blasts() as Blast)
    dim i as integer
    dim dx as single
    dim dy as single

    set_blast blasts(), BLOW, p

    for i = 0 to ubound(foes)
        if foes(i).kind = 0 or foes(i).hp <= 0 then continue for
        dx = wrapped(foes(i).place.x - p.x)
        dy = wrapped(foes(i).place.y - p.y)
        if dx * dx + dy * dy > ROCKET_REACH * ROCKET_REACH then
            continue for
        end if
        harm_foe foes(i), ROCKET_HARM, blasts()
    next
end sub

' Whether the craft's shot s struck a foe; harms it.
function struck_foe ( _
    s as Shot, _
    foes() as Foe, _
    blasts() as Blast _
) as integer
    dim i as integer

    struck_foe = 0

    for i = 0 to ubound(foes)
        if foes(i).kind = 0 or foes(i).hp <= 0 then continue for
        if not near_to(s.place, foes(i).place) then continue for

        if s.kind = ROCKET then
            burst_rocket s.place, foes(), blasts()
        else
            set_blast blasts(), SPARK, s.place
            harm_foe foes(i), ROUND_HARM, blasts()
        end if

        struck_foe = -1
        exit function
    next
end function

' Whether shot s, just moved, met the ground, the craft
' or a foe; what it met struck, the shot spent.
function shot_met ( _
    s as Shot, _
    foes() as Foe, _
    blasts() as Blast, _
    c as Craft, _
    ld as Land _
) as integer
    dim hostile as integer

    shot_met = -1
    hostile = s.kind = BURST or s.kind = BOLT

    if s.place.z < ground_at(ld, s.place.x, s.place.y) then
        if s.kind = ROCKET then
            burst_rocket s.place, foes(), blasts()
        else
            set_blast blasts(), SPARK, s.place
        end if
        s.kind = 0
        exit function
    end if

    if hostile and near_to(s.place, c.place) then
        if s.kind = BURST then c.hull = c.hull - BURST_HARM
        if s.kind = BOLT then c.hull = c.hull - BOLT_HARM
        set_blast blasts(), SPARK, s.place
        s.kind = 0
        exit function
    end if

    if not hostile and struck_foe(s, foes(), blasts()) then
        s.kind = 0
        exit function
    end if

    shot_met = 0
end function

' Every shot moved dt on, in pieces no longer than a hit's
' reach so none passes through what it meets.
sub move_shots ( _
    shots() as Shot, _
    foes() as Foe, _
    blasts() as Blast, _
    c as Craft, _
    ld as Land, _
    byval dt as single _
)
    dim i as integer
    dim j as integer
    dim pieces as integer
    dim part as single
    dim speed as single

    for i = 0 to ubound(shots)
        if shots(i).kind = 0 then continue for

        shots(i).life = shots(i).life - dt

        if shots(i).life <= 0 then
            if shots(i).kind = BURST then
                set_blast blasts(), SPARK, shots(i).place
            end if
            shots(i).kind = 0
            continue for
        end if

        speed = sqr(shots(i).motion.x ^ 2 + shots(i).motion.y ^ 2)
        pieces = int(speed * dt / HIT_REACH) + 1
        part = dt / pieces

        for j = 1 to pieces
            shots(i).place.x = on_map(shots(i).place.x + _
                shots(i).motion.x * part)
            shots(i).place.y = on_map(shots(i).place.y + _
                shots(i).motion.y * part)
            shots(i).place.z = shots(i).place.z + _
                shots(i).motion.z * part
            if shot_met(shots(i), foes(), blasts(), c, ld) then
                exit for
            end if
        next
    next
end sub

' The way from p to q, made a unit long.
sub way_to (p as Point3, q as Point3, way as Point3)
    dim d as single

    way.x = wrapped(q.x - p.x)
    way.y = wrapped(q.y - p.y)
    way.z = q.z - p.z
    d = sqr(way.x * way.x + way.y * way.y + way.z * way.z)
    if d < 0.01 then d = 0.01
    way.x = way.x / d
    way.y = way.y / d
    way.z = way.z / d
end sub

' Flat distance from p to q.
function reach_of (p as Point3, q as Point3) as single
    dim dx as single
    dim dy as single

    dx = wrapped(q.x - p.x)
    dy = wrapped(q.y - p.y)
    reach_of = sqr(dx * dx + dy * dy)
end function

' a brought within half a turn of 0.
function turned (byval a as single) as single
    do while a > PI
        a = a - 2 * PI
    loop

    do while a < -PI
        a = a + 2 * PI
    loop

    turned = a
end function

' A flak site: a burst at the craft, led by its motion,
' when it is in range and in sight.
sub think_flak ( _
    f as Foe, _
    shots() as Shot, _
    c as Craft, _
    ld as Land _
)
    dim aim as Point3
    dim way as Point3
    dim lead as single

    if f.reload > 0 then exit sub
    if reach_of(f.place, c.place) > FLAK_RANGE then exit sub
    if not clear_line(ld, f.place, c.place) then exit sub

    lead = reach_of(f.place, c.place) / BURST_SPEED
    aim = c.place
    aim.x = aim.x + cos(c.heading) * c.speed * lead
    aim.y = aim.y + sin(c.heading) * c.speed * lead
    way_to f.place, aim, way
    launch_shot shots(), BURST, f.place, way, BURST_SPEED, BURST_LIFE
    f.reload = FLAK_GAP + rnd
end sub

' A drone: round its home till the craft comes near, then
' at it, firing when its nose is on it.
sub think_drone ( _
    f as Foe, _
    shots() as Shot, _
    c as Craft, _
    ld as Land, _
    byval dt as single _
)
    dim goal as Point3
    dim way as Point3
    dim bearing as single
    dim swing as single
    dim floor as single
    dim chasing as integer

    chasing = reach_of(f.place, c.place) < DRONE_SIGHT
    goal = c.place

    if not chasing then
        goal = f.home
        goal.x = goal.x + cos(f.heading + 1.2) * DRONE_RING
        goal.y = goal.y + sin(f.heading + 1.2) * DRONE_RING
    end if

    way_to f.place, goal, way
    bearing = turned(atan2_of(way.y, way.x) - f.heading)
    swing = DRONE_TURN * dt
    if bearing > swing then bearing = swing
    if bearing < -swing then bearing = -swing
    f.heading = turned(f.heading + bearing)
    f.place.x = on_map(f.place.x + cos(f.heading) * DRONE_SPEED * dt)
    f.place.y = on_map(f.place.y + sin(f.heading) * DRONE_SPEED * dt)
    f.place.z = f.place.z + sgn(goal.z + DRONE_LIFT - f.place.z) * _
        DRONE_LIFT * dt
    floor = ground_at(ld, f.place.x, f.place.y) + DRONE_LIFT
    if f.place.z < floor then f.place.z = floor

    if not chasing or f.reload > 0 then exit sub
    if reach_of(f.place, c.place) > DRONE_RANGE then exit sub
    way_to f.place, c.place, way
    bearing = turned(atan2_of(way.y, way.x) - f.heading)
    if abs(bearing) > DRONE_CONE then exit sub
    if not clear_line(ld, f.place, c.place) then exit sub

    launch_shot shots(), BOLT, f.place, way, BOLT_SPEED, BOLT_LIFE
    f.reload = DRONE_GAP
end sub

' The angle of y over x, all four quarters.
function atan2_of (byval y as single, byval x as single) as single
    dim a as single

    if x = 0 then
        a = PI / 2
        if y < 0 then a = -a
        atan2_of = a
        exit function
    end if

    a = atn(y / x)
    if x < 0 and y >= 0 then a = a + PI
    if x < 0 and y < 0 then a = a - PI
    atan2_of = a
end function

' Every live foe's turn.
sub think_foes ( _
    foes() as Foe, _
    shots() as Shot, _
    c as Craft, _
    ld as Land, _
    byval dt as single _
)
    dim i as integer

    for i = 0 to ubound(foes)
        if foes(i).kind = 0 or foes(i).hp <= 0 then continue for
        foes(i).reload = foes(i).reload - dt

        select case foes(i).kind
            case FLAK
                think_flak foes(i), shots(), c, ld
            case DRONE
                think_drone foes(i), shots(), c, ld, dt
        end select
    next
end sub

' Every blast aged dt; those past their life gone.
sub age_blasts (blasts() as Blast, byval dt as single)
    dim i as integer
    dim life as single

    for i = 0 to ubound(blasts)
        if blasts(i).kind = 0 then continue for
        blasts(i).age = blasts(i).age + dt
        life = blast_life(blasts(i).kind)
        if blasts(i).age > life then blasts(i).kind = 0
    next
end sub

' A blast's life, for its drawing.
function blast_life (byval kind as integer) as single
    blast_life = SPARK_LIFE
    if kind = BLOW then blast_life = BLOW_LIFE
end function

' A new foe's strength by kind.
function strength_of (byval kind as integer) as integer
    strength_of = FLAK_HP
    if kind = DRONE then strength_of = DRONE_HP
end function
