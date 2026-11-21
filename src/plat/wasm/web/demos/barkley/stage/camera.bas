' The camera: where the screen looks, and the headroom above Charlie.
defint A-Z
option explicit

from barkley.engine.video import SH, SW
from barkley.engine.clock import STEPT
from barkley.model.hero import Hero
from barkley.model.level import Level, MH, NONE, Realm, TS
from barkley.model.game import Game

const HEAD = 124
const HEADUP = 172

' The view ahead of Charlie's run, kept in the level; snap puts it there.
sub Camera (lv as Level, zn as Realm, g as Game, p as Hero, snap as integer)
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
        g.cam.x = tx
        g.cam.y = ty
    else
        g.cam.x = g.cam.x + (tx - g.cam.x) * 7 * STEPT
        g.cam.y = g.cam.y + (ty - g.cam.y) * 6 * STEPT
        if p.at.x - g.cam.x > 250 then g.cam.x = p.at.x - 250
        if p.at.x - g.cam.x < 70 then g.cam.x = p.at.x - 70
        if p.at.y - g.cam.y > 172 then g.cam.y = p.at.y - 172
        if p.at.y - g.cam.y < 48 then g.cam.y = p.at.y - 48
    end if
    if g.cam.x < 0 then g.cam.x = 0
    if g.cam.x > lv.wpx - SW then g.cam.x = lv.wpx - SW
    if g.cam.y < 0 then g.cam.y = 0
    if g.cam.y > MH * TS - SH then g.cam.y = MH * TS - SH
end sub

' How far under the view's top Charlie is kept: further in a crane's hall while it hangs there, to see it, and
' under the great oak while the twins have the ball.
function Headroom% (lv as Level, zn as Realm, p as Hero)
    Headroom% = HEAD
    if lv.chase_x = NONE or p.at.x < lv.chase_x or p.at.x > lv.chase_end then exit function
    if zn.id = 1 and (lv.cart.st = 1 or lv.cart.st = 2) then
        Headroom% = HEADUP
    end if
    if zn.id = 4 and lv.cart.st = 1 then Headroom% = HEADUP
end function
