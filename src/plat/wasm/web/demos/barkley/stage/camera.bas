' The camera: where the screen looks, and the headroom above
' Charlie.
defint A-Z
option explicit

from engine.video import SH, SW
from engine.clock import STEPT
from barkley.model.hero import Hero
from barkley.model.level import Level, MH, NONE, Realm, TS
from barkley.model.game import Camera

const HEAD = 124
const HEADUP = 172

' The view ahead of Charlie's run, kept in the level; snap
' puts it there.
sub Follow ( _
    lv as Level, _
    zn as Realm, _
    cam as Camera, _
    p as Hero, _
    snap as integer _
)
    dim tx as single, ty as single, v as single

    if p.ground then
        v = p.gsp
    else
        v = p.vel.x
    end if

    v = v * .2
    if v > 70 then v = 70
    if v < -70 then v = -70
    tx = p.at.x - SW \ 2 + v
    ty = p.at.y - Headroom%(lv, zn, p)

    if snap then
        cam.x = tx
        cam.y = ty
    else
        cam.x = cam.x + (tx - cam.x) * 7 * STEPT
        cam.y = cam.y + (ty - cam.y) * 6 * STEPT
        if p.at.x - cam.x > 250 then cam.x = p.at.x - 250
        if p.at.x - cam.x < 70 then cam.x = p.at.x - 70
        if p.at.y - cam.y > 172 then cam.y = p.at.y - 172
        if p.at.y - cam.y < 48 then cam.y = p.at.y - 48
    end if

    if cam.x < 0 then cam.x = 0
    if cam.x > lv.wpx - SW then cam.x = lv.wpx - SW
    if cam.y < 0 then cam.y = 0
    if cam.y > MH * TS - SH then cam.y = MH * TS - SH
end sub

' How far under the view's top Charlie is kept: further in a
' crane's hall while it hangs there, to see it, and under
' the great oak while the twins have the ball.
function Headroom% (lv as Level, zn as Realm, p as Hero)
    Headroom% = HEAD
    if lv.chase_x = NONE or p.at.x < lv.chase_x _
        or p.at.x > lv.chase_end then exit function

    if zn.id = 1 and (lv.cart.st = 1 or lv.cart.st = 2) then
        Headroom% = HEADUP
    end if

    if zn.id = 4 and lv.cart.st = 1 then Headroom% = HEADUP
end function

' The view's top left put at x, y.
sub Look (cam as Camera, x as single, y as single)
    cam.x = x
    cam.y = y
end sub
