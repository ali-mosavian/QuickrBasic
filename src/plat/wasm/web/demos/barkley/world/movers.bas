' The moving platforms: to and fro, up and down, lifts,
' cables.
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

' a boat's deck over the water it floats on, and over the
' floor it lies on
const FREEB = 7

' A span of the sands' old pier: the seconds it totters once
' trodden, and how far it falls.
private const SPANT = .25
private const SPANDROP = 400

' A ski lift's chair, a platform that goes up its cable
' SEATV of the way a second while he stands on it; its frame
' stands SEATH over its seat.
private const SEATV = .35

' A tram: its speed along its street while he rides its
' roof.
const TRAMV = 240!

' How far cable m's far end hangs under its near one: a
' basket's cable falls, a ski lift's climbs; a tram's street
' is level.
function Dip% (lv as Level, m as integer)
    Dip% = CABLED
    if lv.movers(m).kind = 7 then Dip% = -CABLED
    if lv.movers(m).kind = 8 then Dip% = 0
end function

' Boat m is off the floor of its pool.
function Adrift% (lv as Level, m as integer)
    Adrift% = lv.tide - FREEB < lv.movers(m).at.y
end function

' Platform m's swing now, -1 to 1, held a while at each end
' when it goes up and down; quarter: a quarter of its way
' on, which tells where it is going.
function Swing! ( _
    lv as Level, _
    se as Session, _
    m as integer, _
    quarter as integer _
)
    dim q as single, v as single

    if lv.movers(m).kind = 5 then
        ' a boat by its own clock, which stops while it is
        ' aground
        q = lv.movers(m).t + m + quarter * PI / 2
    else
        q = se.ticks * STEPT * (1.1 - .2 * lv.movers(m).kind) + m _
            + quarter * PI / 2
    end if

    v = sin(q)

    if lv.movers(m).kind = 1 then
        v = v * 1.3
        if v > 1 then v = 1
        if v < -1 then v = -1
    end if

    Swing! = v
end function

' The platforms a step on: to and fro, up and down, or a
' lift, up while Charlie stands on it and down again when
' not. Charlie goes with the one it stands on.
sub Movers ( _
    lv as Level, _
    gr as Ground, _
    mu as Deck, _
    se as Session, _
    p as Hero _
)
    dim m as integer, s as integer, o as integer, x as single
    dim y as single, dx as single, dy as single

    for m = 0 to lv.n_movers - 1
        s = lv.movers(m).sg
        x = lv.movers(m).at.x
        y = lv.movers(m).at.y

        select case lv.movers(m).kind
        case 0
            x = x + lv.movers(m).reach * Swing!(lv, se, m, 0)
        case 5
            if Adrift%(lv, m) then
                lv.movers(m).t = lv.movers(m).t + STEPT
                y = lv.tide - FREEB
            end if

            x = x + lv.movers(m).reach * Swing!(lv, se, m, 0)
        case 1
            y = y + lv.movers(m).reach * Swing!(lv, se, m, 0)
        case 6
            ' a pier's span: trodden, it totters a moment,
            ' then falls from under who is on it, and stays
            ' fallen
            if lv.movers(m).t > 0 or (p.ground and p.sg = s) then
                lv.movers(m).t = lv.movers(m).t + STEPT
            end if

            if lv.movers(m).t > SPANT + 2 then
                lv.movers(m).t = SPANT + 2
            end if

            lv.things(lv.movers(m).thing).state = lv.movers(m).t _
                > 0 and lv.movers(m).t < SPANT

            if lv.movers(m).t >= SPANT then
                if lv.movers(m).t - STEPT < SPANT then
                    Sfx mu, se, SCRACK
                end if

                if p.ground and p.sg = s then Drop gr, p
                dy = lv.movers(m).t - SPANT
                y = y + 80 * dy + GRAV * dy * dy

                if y > lv.movers(m).at.y + SPANDROP then
                    y = lv.movers(m).at.y + SPANDROP
                end if
            end if
        case 2
            if p.ground and p.sg = s then
                lv.movers(m).t = lv.movers(m).t + 90 * STEPT
            else
                lv.movers(m).t = lv.movers(m).t - 60 * STEPT
            end if

            if lv.movers(m).t < 0 then lv.movers(m).t = 0

            if lv.movers(m).t > lv.movers(m).reach then
                lv.movers(m).t = lv.movers(m).reach
            end if

            y = y - lv.movers(m).t
        case 4
            if lv.things(lv.movers(m).button).state then
                lv.movers(m).t = lv.movers(m).t + 200 * STEPT
            end if

            if lv.movers(m).t > lv.movers(m).reach then
                lv.movers(m).t = lv.movers(m).reach
            end if

            y = y - lv.movers(m).t
        case 3, 7, 8
            ' while stood on: a basket down its cable,
            ' faster as it goes, a chair up its own
            ' steadily, a tram along its street at TRAMV;
            ' back when left
            if not p.ground or p.sg <> s then
                lv.movers(m).t = lv.movers(m).t - .4 * STEPT
            elseif lv.movers(m).kind = 3 then
                lv.movers(m).t = lv.movers(m).t _
                    + (.25 + lv.movers(m).t) * .6 * STEPT
            elseif lv.movers(m).kind = 7 then
                lv.movers(m).t = lv.movers(m).t + SEATV * STEPT
            else
                lv.movers(m).t = lv.movers(m).t _
                    + TRAMV * STEPT / lv.movers(m).reach
            end if

            if lv.movers(m).t < 0 then lv.movers(m).t = 0
            if lv.movers(m).t > 1 then lv.movers(m).t = 1
            x = x + lv.movers(m).reach * lv.movers(m).t
            y = y + Dip%(lv, m) * lv.movers(m).t
        end select

        dx = x - gr.pt(s).at.x
        dy = y - gr.pt(s).at.y
        if dx = 0 and dy = 0 then continue for

        gr.pt(s).at.x = x
        gr.pt(s + 1).at.x = x + MOVEW
        gr.pt(s).at.y = y
        gr.pt(s + 1).at.y = y
        gr.cv(lv.movers(m).cv).x_lo = int(x)
        gr.cv(lv.movers(m).cv).x_hi = int(x) + MOVEW + 1
        o = lv.movers(m).thing
        lv.things(o).at.x = cint(x)
        lv.things(o).at.y = cint(y)
        if not p.ground or p.sg <> s then continue for

        p.at.x = p.at.x + dx
        p.at.y = p.at.y + dy
    next
end sub
