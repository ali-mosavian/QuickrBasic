' The missions: each its own vale grown from a seed, its
' foes set down by rule; and whether it is won or lost.
option explicit

from highvale.model.game import Game, CLEARED, DOWNED
from highvale.model.foes import Foe, FLAK, DRONE
from highvale.model.craft import FULL_HULL, ROUNDS, ROCKETS
from highvale.model.land import Land, MAP
from highvale.land.maps import make_land, ground_at, wrapped
from highvale.play.flight import start_craft
from highvale.play.combat import strength_of

' each mission's name, task, seed, flak sites and drones,
' a field of each list a mission
const NAMES = "COLD FORD|ASH PASS|HIGH CROWN"
const TASKS = "SILENCE THE FLAK ON THE FORD|" + _
    "CLEAR THE PASS OF FLAK AND DRONES|" + _
    "TAKE THE CROWN FROM THE LAST OF THEM"
const SEEDS = "7|19|31"
const FLAK_COUNTS = "3|4|5"
const DRONE_COUNTS = "1|3|5"
' a foe stands this far from the start at least, on land
' this far over the sea, no steeper than this
const CLEAR_OF_START = 70
const OVER_SEA = 6
const STEEPEST = 4
const TRIES = 400
' a drone's height over its ground
const DRONE_UP = 25
const PI = 3.14159265

' Field n, from 1, of a list split by bars.
function field_of (items as string, byval n as integer) as string
    dim head as integer
    dim bar as integer
    dim i as integer

    head = 1

    for i = 2 to n
        head = instr(head, items, "|") + 1
    next

    bar = instr(head, items, "|")
    if bar = 0 then bar = len(items) + 1
    field_of = mid$(items, head, bar - head)
end function

function mission_name (byval n as integer) as string
    mission_name = field_of(NAMES, n)
end function

function mission_task (byval n as integer) as string
    mission_task = field_of(TASKS, n)
end function

' A place for a foe: on land, flat enough, clear of the
' start; the last tried when none is found.
sub find_ground (ld as Land, x as single, y as single)
    dim i as integer
    dim h as integer

    for i = 1 to TRIES
        x = rnd * MAP
        y = rnd * MAP
        h = ground_at(ld, x, y)
        if h < ld.sea + OVER_SEA then continue for
        if abs(h - ground_at(ld, x + 3, y + 3)) > STEEPEST then
            continue for
        end if
        if abs(wrapped(x - MAP / 2)) + abs(wrapped(y - MAP / 2)) _
            < CLEAR_OF_START then continue for
        exit sub
    next
end sub

' f set down as kind.
sub set_foe (f as Foe, ld as Land, byval kind as integer)
    find_ground ld, f.place.x, f.place.y
    f.kind = kind
    f.place.z = ground_at(ld, f.place.x, f.place.y)
    if kind = DRONE then f.place.z = f.place.z + DRONE_UP
    f.home = f.place
    f.heading = rnd * 2 * PI
    f.hp = strength_of(kind)
    f.reload = 1 + rnd * 2
end sub

' Mission n begun: its vale made, the craft armed and set
' down in its middle, its foes placed.
sub start_mission (g as Game, byval n as integer)
    dim i as integer
    dim flaks as integer
    dim drones as integer

    g.mission = n
    make_land g.vale, val(field_of(SEEDS, n))
    g.heli.hull = FULL_HULL
    g.heli.rounds = ROUNDS
    g.heli.rockets = ROCKETS
    start_craft g.heli, g.vale, MAP / 2, MAP / 2
    flaks = val(field_of(FLAK_COUNTS, n))
    drones = val(field_of(DRONE_COUNTS, n))

    for i = 0 to ubound(g.foes)
        g.foes(i).kind = 0
        if i < flaks then set_foe g.foes(i), g.vale, FLAK
        if i >= flaks and i < flaks + drones then
            set_foe g.foes(i), g.vale, DRONE
        end if
    next

    for i = 0 to ubound(g.shots)
        g.shots(i).kind = 0
    next

    for i = 0 to ubound(g.blasts)
        g.blasts(i).kind = 0
    next
end sub

' The foes still fighting.
function foes_left (foes() as Foe) as integer
    dim i as integer
    dim n as integer

    for i = 0 to ubound(foes)
        if foes(i).kind <> 0 and foes(i).hp > 0 then n = n + 1
    next

    foes_left = n
end function

' Won when no foe is left; lost with the hull gone or
' nothing left to fire.
sub judge (g as Game)
    if g.heli.hull <= 0 then
        g.state = DOWNED
        g.note = "SHOT DOWN"
        exit sub
    end if

    if foes_left(g.foes()) = 0 then
        g.state = CLEARED
        g.note = "MISSION CLEAR"
        exit sub
    end if

    if g.heli.rounds > 0 or g.heli.rockets > 0 then exit sub
    g.state = DOWNED
    g.note = "OUT OF ROUNDS"
end sub
