' The helicopter's flight: the keys turn, tilt and lift
' it, its speed follows the tilt, and the ground holds it
' up, scraping the hull when it is flown into.
option explicit

from highvale.model.craft import Craft, Controls
from highvale.model.land import Land, MAP
from highvale.land.maps import ground_at

' radians a second at full turn
const TURN_RATE = 1.3
' the tilt's travel a second
const TILT_RATE = 2.2
' texels a second, nose down and nose up
const TOP_SPEED = 72
const BACK_SPEED = 22
' the share of the speed's gap closed a second
const PICKUP = 1.4
' height steps a second
const CLIMB_RATE = 40
const CEILING = 250
const CLEARANCE = 3
' the horizon's most tilt in a turn, and its rate
const BANK_MOST = 0.28
const BANK_RATE = 1.4
' hull lost a second scraping, and the speed it takes
const SCRAPE_HARM = 14
const SCRAPE_SPEED = 12
' the craft starts facing the lowest of these headings,
' the ground read this far out at these points
const HEADINGS = 8
const LOOK_POINTS = 4
const LOOK_GAP = 20
const PI = 3.14159265

' v moved toward goal by step at most.
function toward ( _
    byval v as single, _
    byval goal as single, _
    byval step_by as single _
) as single
    if v < goal then
        v = v + step_by
        if v > goal then v = goal
    else
        v = v - step_by
        if v < goal then v = goal
    end if

    toward = v
end function

' x brought within the map, which wraps.
function on_map (byval x as single) as single
    if x < 0 then x = x + MAP
    if x >= MAP then x = x - MAP
    on_map = x
end function

' The heading from x, y whose ground ahead is lowest.
function open_heading ( _
    ld as Land, _
    byval x as single, _
    byval y as single _
) as single
    dim i as integer
    dim j as integer
    dim a as single
    dim sum as single
    dim least as single

    least = 1e9

    for i = 0 to HEADINGS - 1
        a = i * 2 * PI / HEADINGS
        sum = 0

        for j = 1 to LOOK_POINTS
            sum = sum + ground_at(ld, x + cos(a) * j * LOOK_GAP, _
                y + sin(a) * j * LOOK_GAP)
        next

        if sum >= least then continue for
        least = sum
        open_heading = a
    next
end function

' The craft set down at x, y, over the ground there,
' facing the open way.
sub start_craft ( _
    c as Craft, _
    ld as Land, _
    byval x as single, _
    byval y as single _
)
    c.place.x = x
    c.place.y = y
    c.place.z = ground_at(ld, x, y) + 30
    c.heading = open_heading(ld, x, y)
    c.speed = 0
    c.tilt = 0
    c.bank = 0
    c.scraping = 0
end sub

' One step of flight of dt seconds.
sub fly ( _
    c as Craft, _
    ctl as Controls, _
    ld as Land, _
    byval dt as single _
)
    dim goal as single
    dim floor as single

    c.tilt = toward(c.tilt, ctl.pitch, TILT_RATE * dt)
    goal = c.tilt * TOP_SPEED
    if c.tilt < 0 then goal = c.tilt * BACK_SPEED
    c.speed = c.speed + (goal - c.speed) * PICKUP * dt
    c.heading = c.heading + ctl.turn * TURN_RATE * dt
    c.bank = toward(c.bank, -ctl.turn * BANK_MOST, BANK_RATE * dt)
    c.place.x = on_map(c.place.x + cos(c.heading) * c.speed * dt)
    c.place.y = on_map(c.place.y + sin(c.heading) * c.speed * dt)
    c.place.z = c.place.z + ctl.climb * CLIMB_RATE * dt
    if c.place.z > CEILING then c.place.z = CEILING

    floor = ground_at(ld, c.place.x, c.place.y) + CLEARANCE
    c.scraping = c.place.z < floor and abs(c.speed) > SCRAPE_SPEED
    if c.place.z < floor then c.place.z = floor
    if c.scraping then c.hull = c.hull - SCRAPE_HARM * dt
end sub
