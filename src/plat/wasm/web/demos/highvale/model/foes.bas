' What fights and flies: foes, shots and blasts.
option explicit

from highvale.model.space import Point3

' a foe's kinds; 0 none
const FLAK = 1
const DRONE = 2
' a shot's kinds; 0 none
const ROUND = 1
const ROCKET = 2
const BURST = 3
const BOLT = 4
' a blast's kinds; 0 none
const SPARK = 1
const BLOW = 2

const MOST_FOES = 12
const MOST_SHOTS = 48
const MOST_BLASTS = 16

type Foe
    kind as integer
    place as Point3
    home as Point3
    heading as single
    hp as integer
    reload as single
end type

type Shot
    kind as integer
    place as Point3
    motion as Point3
    life as single
end type

type Blast
    kind as integer
    place as Point3
    age as single
end type
