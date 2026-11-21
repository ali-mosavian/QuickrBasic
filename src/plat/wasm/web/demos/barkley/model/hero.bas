' Charlie's record and size, and the pad that steers him.
defint A-Z
option explicit

from engine.vec import Vec

' Charlie. at: the feet. gsp: speed along the ground, ang
' its rise in radians, uphill to the right > 0, right round
' on a loop. sg: the stretch of the ground's line it is on,
' or left last; slip: the seconds the keys are no help, slid
' off a slope too steep for its speed. hops: the lines he
' has come down on in a row, none since the ground.
type Hero
    at as Vec
    vel as Vec
    gsp as single
    ang as single
    slip as single
    last_x as single
    charge as single
    want as single
    coyote as single
    inv as single
    anim as single
    dead_t as single
    idle_t as single
    ground as integer
    roll as integer
    jumped as integer
    crouch as integer
    charging as integer
    stun as integer
    face as integer
    skid as integer
    sprung as integer
    dead as integer
    sg as integer
    win as integer
    bark_t as single
    swim as integer
    wet as single
    sled as integer
    snow as integer
    goat as integer
    ride_t as single
    vine as integer
    vine_t as single
    sway as single
    sway_v as single
    dig as integer
    dug as integer
    hops as integer
end type

' Held keys, and jump's press since the last step.
type Pad
    left as integer
    right as integer
    down as integer
    jump as integer
    hit as integer
    was as integer
    bark as integer
    quit as integer
    start as integer
    mute as integer
end type

' The game's own player: the steps it holds jump yet.
type Autopilot
    hold as integer
end type

const HW = 7
const HH = 24
private const MIDH = 80
