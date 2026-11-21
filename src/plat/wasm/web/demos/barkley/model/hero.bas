' Charlie's record and size, and the pad that steers him.
defint A-Z
option explicit

' Charlie. x, y: the feet. gsp: speed along the ground, ang its rise in
' radians, uphill to the right > 0, right round on a loop. sg: the stretch
' of the ground's line it is on, or left last; slip: the seconds the keys
' are no help, slid off a slope too steep for its speed. hops: the lines he has
' come down on in a row, none since the ground.
type Hero
    x as single
    y as single
    vx as single
    vy as single
    gsp as single
    ang as single
    slip as single
    lastx as single
    charge as single
    want as single
    coyote as single
    inv as single
    anim as single
    deadT as single
    idleT as single
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
    barkT as single
    swim as integer
    wet as single
    sled as integer
    snow as integer
    goat as integer
    rideT as single
    vine as integer
    vineT as single
    sway as single
    swayV as single
    dig as integer
    dug as integer
    hops as integer
end type

' Held keys, and jump's press since the last step.
type Pad
    l as integer
    r as integer
    d as integer
    jump as integer
    hit as integer
    was as integer
    bark as integer
    quit as integer
    start as integer
    mute as integer
end type

const HW = 7, HH = 24
private const MIDH = 80
