' The camera: where the screen looks, and the headroom above Charlie.
defint A-Z
option explicit

from barkley.engine.video import SH, SW
from barkley.engine.clock import STEPT
from barkley.model.hero import Hero
from barkley.model.level import Level, MH, NONE, Realm, TS
from barkley.model.game import Game

const HEAD = 124, HEADUP = 172

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
        g.camx = tx
        g.camy = ty
    else
        g.camx = g.camx + (tx - g.camx) * 7 * STEPT
        g.camy = g.camy + (ty - g.camy) * 6 * STEPT
        if p.at.x - g.camx > 250 then g.camx = p.at.x - 250
        if p.at.x - g.camx < 70 then g.camx = p.at.x - 70
        if p.at.y - g.camy > 172 then g.camy = p.at.y - 172
        if p.at.y - g.camy < 48 then g.camy = p.at.y - 48
    end if
    if g.camx < 0 then g.camx = 0
    if g.camx > lv.wpx - SW then g.camx = lv.wpx - SW
    if g.camy < 0 then g.camy = 0
    if g.camy > MH * TS - SH then g.camy = MH * TS - SH
end sub

' How far under the view's top Charlie is kept: further in a crane's hall while it hangs there, to see it, and
' under the great oak while the twins have the ball.
function Headroom% (lv as Level, zn as Realm, p as Hero)
    Headroom% = HEAD
    if lv.chaseX = NONE or p.at.x < lv.chaseX or p.at.x > lv.chaseEnd then exit function
    if zn.id = 1 and (lv.cart.st = 1 or lv.cart.st = 2) then Headroom% = HEADUP
    if zn.id = 4 and lv.cart.st = 1 then Headroom% = HEADUP
end function
