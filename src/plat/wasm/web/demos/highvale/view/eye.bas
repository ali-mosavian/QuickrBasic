' The eye: the camera set from the craft, and a point of
' the world placed on the screen as the terrain places
' it, so sprites stand where the ground is drawn.
option explicit

from highvale.model.space import Point3
from highvale.model.land import Land, SCALE
from highvale.model.craft import Craft
from highvale.land.maps import clear_line, wrapped

' the view's size on the screen, and its centre column
const VIEW_W = 320
const VIEW_H = 152
const MID_X = 159.5
' the horizon's row level, and how far the tilt moves it
const HORIZON = 54
const PITCH_ROWS = 30
' the view's width in radians, and the farthest texel
' drawn
const FOV = 1.4
const FAR = 420
' nearer than this a point is not drawn
const NEAR = 3
' the eye sits this high over the craft's place
const EYE_UP = 2

' Where the view is seen from: the eye over the map, in
' texels, its height in steps, its heading, the horizon's
' turn and its row at the middle column.
type Camera
    x as single
    y as single
    height as single
    angle as single
    roll as single
    horizon as single
end type

' Where a point fell on the screen, and its pixels a texel.
type Spot
    x as single
    y as single
    size as single
    ahead as single
end type

' Half the view's width over the tan of half the fov.
function focal () as single
    focal = (VIEW_W / 2) * cos(FOV / 2) / sin(FOV / 2)
end function

' The camera behind the craft's eyes.
sub aim_camera (k as Camera, c as Craft)
    k.x = c.place.x
    k.y = c.place.y
    k.height = c.place.z + EYE_UP
    k.angle = c.heading
    k.roll = c.bank
    k.horizon = HORIZON - c.tilt * PITCH_ROWS
end sub

' p's spot from k; false when behind or too near.
function place_spot ( _
    k as Camera, _
    p as Point3, _
    s as Spot _
) as integer
    dim dx as single
    dim dy as single
    dim side as single
    dim f as single

    dx = wrapped(p.x - k.x)
    dy = wrapped(p.y - k.y)
    s.ahead = dx * cos(k.angle) + dy * sin(k.angle)
    place_spot = 0
    if s.ahead < NEAR then exit function

    f = focal()
    side = dy * cos(k.angle) - dx * sin(k.angle)
    s.x = MID_X + side * f / s.ahead
    s.size = f / s.ahead
    s.y = k.horizon + (k.height - p.z) * SCALE * s.size
    s.y = s.y + (s.x - MID_X) * sin(k.roll) / cos(k.roll)
    place_spot = -1
end function

' Whether the ground hides nothing on the line from the
' eye to p.
function in_sight ( _
    ld as Land, _
    k as Camera, _
    p as Point3 _
) as integer
    dim eye as Point3

    eye.x = k.x
    eye.y = k.y
    eye.z = k.height
    in_sight = clear_line(ld, eye, p)
end function

' The row aimed at, the sight's: guns fire along its ray.
function aim_row () as single
    aim_row = HORIZON + 24
end function

' Height steps the aim falls a texel ahead, at that pitch.
function aim_fall (k as Camera) as single
    aim_fall = (aim_row() - k.horizon) / (SCALE * focal())
end function
