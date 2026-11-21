' The moving platforms: to and fro, up and down, lifts, cables.
defint A-Z
option explicit

from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck
from barkley.model.hero import Hero
from barkley.model.level import GRAV, Ground, Level, PI
from barkley.model.game import Session
from barkley.assets.tunes import SCRACK, Sfx
from barkley.world.terrain.ground import Drop
from barkley.world.terrain.track import MOVEW
from barkley.world.level import CABLED

' a boat's deck over the water it floats on, and over the floor it lies on
const FREEB = 7

' A span of the sands' old pier: the seconds it totters once trodden, and how far it falls.
private const SPANT = .25, SPANDROP = 400

' A ski lift's chair, a platform that goes up its cable SEATV of the way a second while he stands on it; its
' frame stands SEATH over its seat.
private const SEATV = .35

' A tram: its speed along its street while he rides its roof.
const TRAMV = 240!

' How far cable m's far end hangs under its near one: a basket's cable falls, a ski lift's climbs; a tram's street
' is level.
function Dip% (lv as Level, m as integer)
    Dip% = CABLED
    if lv.mv(m).k = 7 then Dip% = -CABLED
    if lv.mv(m).k = 8 then Dip% = 0
end function

' Boat m is off the floor of its pool.
function Adrift% (lv as Level, m as integer)
    Adrift% = lv.tide - FREEB < lv.mv(m).y
end function

' Platform m's swing now, -1 to 1, held a while at each end when it goes up and down; quarter: a quarter of its
' way on, which tells where it is going.
function Swing! (lv as Level, se as Session, m as integer, quarter as integer)
    dim q as single, v as single
    if lv.mv(m).k = 5 then
        ' a boat by its own clock, which stops while it is aground
        q = lv.mv(m).t + m + quarter * PI / 2
    else
        q = se.ticks * STEPT * (1.1 - .2 * lv.mv(m).k) + m + quarter * PI / 2
    end if
    v = sin(q)
    if lv.mv(m).k = 1 then
        v = v * 1.3
        if v > 1 then v = 1
        if v < -1 then v = -1
    end if
    Swing! = v
end function

' The platforms a step on: to and fro, up and down, or a lift, up while Charlie stands on it and down again
' when not. Charlie goes with the one it stands on.
sub Movers (lv as Level, gr as Ground, mu as Deck, se as Session, p as Hero)
    dim m as integer, s as integer, o as integer, x as single, y as single, dx as single, dy as single
    for m = 0 to lv.nMv - 1
        s = lv.mv(m).s
        x = lv.mv(m).x
        y = lv.mv(m).y
        select case lv.mv(m).k
        case 0: x = x + lv.mv(m).a * Swing!(lv, se, m, 0)
        case 5
            if Adrift%(lv, m) then
                lv.mv(m).t = lv.mv(m).t + STEPT
                y = lv.tide - FREEB
            end if
            x = x + lv.mv(m).a * Swing!(lv, se, m, 0)
        case 1: y = y + lv.mv(m).a * Swing!(lv, se, m, 0)
        case 6
            ' a pier's span: trodden, it totters a moment, then falls from under who is on it, and stays fallen
            if lv.mv(m).t > 0 or (p.ground and p.sg = s) then lv.mv(m).t = lv.mv(m).t + STEPT
            if lv.mv(m).t > SPANT + 2 then lv.mv(m).t = SPANT + 2
            lv.ob(lv.mv(m).o).s = lv.mv(m).t > 0 and lv.mv(m).t < SPANT
            if lv.mv(m).t >= SPANT then
                if lv.mv(m).t - STEPT < SPANT then Sfx mu, se, SCRACK
                if p.ground and p.sg = s then Drop gr, p
                dy = lv.mv(m).t - SPANT
                y = y + 80 * dy + GRAV * dy * dy
                if y > lv.mv(m).y + SPANDROP then y = lv.mv(m).y + SPANDROP
            end if
        case 2
            if p.ground and p.sg = s then
                lv.mv(m).t = lv.mv(m).t + 90 * STEPT
            else
                lv.mv(m).t = lv.mv(m).t - 60 * STEPT
            end if
            if lv.mv(m).t < 0 then lv.mv(m).t = 0
            if lv.mv(m).t > lv.mv(m).a then lv.mv(m).t = lv.mv(m).a
            y = y - lv.mv(m).t
        case 4
            if lv.ob(lv.mv(m).b).s then lv.mv(m).t = lv.mv(m).t + 200 * STEPT
            if lv.mv(m).t > lv.mv(m).a then lv.mv(m).t = lv.mv(m).a
            y = y - lv.mv(m).t
        case 3, 7, 8
            ' while stood on: a basket down its cable, faster as it goes, a chair up its own steadily, a tram along
            ' its street at TRAMV; back when left
            if not p.ground or p.sg <> s then
                lv.mv(m).t = lv.mv(m).t - .4 * STEPT
            elseif lv.mv(m).k = 3 then
                lv.mv(m).t = lv.mv(m).t + (.25 + lv.mv(m).t) * .6 * STEPT
            elseif lv.mv(m).k = 7 then
                lv.mv(m).t = lv.mv(m).t + SEATV * STEPT
            else
                lv.mv(m).t = lv.mv(m).t + TRAMV * STEPT / lv.mv(m).a
            end if
            if lv.mv(m).t < 0 then lv.mv(m).t = 0
            if lv.mv(m).t > 1 then lv.mv(m).t = 1
            x = x + lv.mv(m).a * lv.mv(m).t
            y = y + Dip%(lv, m) * lv.mv(m).t
        end select
        dx = x - gr.pt(s).x
        dy = y - gr.pt(s).y
        if dx = 0 and dy = 0 then continue for
        gr.pt(s).x = x
        gr.pt(s + 1).x = x + MOVEW
        gr.pt(s).y = y
        gr.pt(s + 1).y = y
        gr.cv(lv.mv(m).z).l = int(x)
        gr.cv(lv.mv(m).z).r = int(x) + MOVEW + 1
        o = lv.mv(m).o
        lv.ob(o).x = cint(x)
        lv.ob(o).y = cint(y)
        if not p.ground or p.sg <> s then continue for
        p.x = p.x + dx
        p.y = p.y + dy
    next
end sub
