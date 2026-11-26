' The player's helicopter and what the keys ask of it.
option explicit

from highvale.model.space import Point3

' the hull's whole, the guns' rounds and the rockets
const FULL_HULL = 100
const ROUNDS = 600
const ROCKETS = 8

type Craft
    place as Point3
    heading as single
    speed as single
    tilt as single
    bank as single
    hull as single
    rounds as integer
    rockets as integer
    gun_wait as single
    rocket_wait as single
    side as integer
    scraping as integer
end type

' Each -1, 0 or 1: turn right, nose down, climb; the rest
' true while held.
type Controls
    turn as integer
    pitch as integer
    climb as integer
    guns as integer
    launch as integer
    go as integer
    quit as integer
end type
