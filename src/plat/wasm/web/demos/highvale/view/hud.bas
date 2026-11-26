' The instruments: the heading tape over the view, the
' sight, and the panel under it: height, heading, speed,
' rounds, rockets, hull, foes left and the scope.
option explicit

import gfx.draw
from highvale.model.game import Game
from highvale.model.foes import Foe, FLAK, DRONE
from highvale.model.craft import Craft, FULL_HULL
from highvale.land.palette import PANEL, PANEL_EDGE, SHADE, AMBER
from highvale.land.palette import FAINT, HULL_BAR, ALERT, SCOPE
from highvale.land.palette import SCOPE_GRID, PALE, TAPE, TICK
from highvale.land.maps import ground_at, wrapped
from highvale.view.eye import VIEW_W, VIEW_H, MID_X, aim_row
from highvale.play.mission import foes_left, mission_name

' the tape's place and its degrees a pixel
const TAPE_X = 96
const TAPE_W = 128
const TAPE_Y = 3
const TAPE_H = 11
const DEGREES_PX = 0.625
' the scope's place, and its reach in texels
const SCOPE_X = 258
const SCOPE_Y = 156
const SCOPE_W = 58
const SCOPE_H = 40
const SCOPE_REACH = 320
const PI = 3.14159265

' Text in the panel's font.
sub write_text ( _
    byval dst as long, _
    byval x as integer, _
    byval y as integer, _
    s as string, _
    byval colour as integer _
)
    gfx.draw.text dst, x, y, s, colour
end sub

' A whole number as text with no sign's room.
function number (byval v as single) as string
    number = ltrim$(str$(int(v)))
end function

' A heading as compass degrees, north up the map.
function compass (byval heading as single) as integer
    dim d as integer

    d = int(heading * 180 / PI + 90) mod 360
    if d < 0 then d = d + 360
    compass = d
end function

' The heading tape: ticks each 10 degrees, letters at the
' quarters, the mark at the middle.
sub draw_tape (byval dst as long, byval heading as single)
    dim d as integer
    dim first as integer
    dim x as single
    dim at_d as single
    dim mark as string

    gfx.draw.rect dst, TAPE_X, TAPE_Y, TAPE_X + TAPE_W - 1, _
        TAPE_Y + TAPE_H - 1, TAPE, gfx.draw.FILL
    at_d = compass(heading)
    first = (int(at_d - TAPE_W * DEGREES_PX / 2) \ 10) * 10

    for d = first to first + TAPE_W * DEGREES_PX + 10 step 10
        x = MID_X + (d - at_d) / DEGREES_PX
        if x < TAPE_X + 2 or x > TAPE_X + TAPE_W - 3 then continue for
        gfx.draw.line dst, x, TAPE_Y + TAPE_H - 3, x, _
            TAPE_Y + TAPE_H - 2, TICK
        if ((d + 360) mod 90) <> 0 then continue for
        mark = mid$("NESW", ((d + 360) mod 360) \ 90 + 1, 1)
        write_text dst, x - 1, TAPE_Y + 1, mark, TICK
    next

    gfx.draw.line dst, MID_X, TAPE_Y, MID_X, TAPE_Y + 2, AMBER
end sub

' The sight where the guns aim.
sub draw_sight (byval dst as long)
    dim y as single
    dim d as integer

    y = aim_row()

    for d = 4 to 6
        gfx.draw.pset dst, MID_X - d, y, PALE
        gfx.draw.pset dst, MID_X + d, y, PALE
        gfx.draw.pset dst, MID_X, y - d, PALE
        gfx.draw.pset dst, MID_X, y + d, PALE
    next
end sub

' The scope: the foes about the craft, its nose up.
sub draw_scope (byval dst as long, c as Craft, foes() as Foe)
    dim i as integer
    dim dx as single
    dim dy as single
    dim ahead as single
    dim side as single
    dim x as single
    dim y as single
    dim cx as single
    dim cy as single
    dim tone as integer

    cx = SCOPE_X + SCOPE_W / 2
    cy = SCOPE_Y + SCOPE_H / 2
    gfx.draw.rect dst, SCOPE_X - 1, SCOPE_Y - 1, SCOPE_X + SCOPE_W, _
        SCOPE_Y + SCOPE_H, SHADE, gfx.draw.FILL
    gfx.draw.rect dst, SCOPE_X, SCOPE_Y, SCOPE_X + SCOPE_W - 1, _
        SCOPE_Y + SCOPE_H - 1, SCOPE, gfx.draw.FILL

    for y = SCOPE_Y + 4 to SCOPE_Y + SCOPE_H - 1 step 8
        gfx.draw.line dst, SCOPE_X, y, SCOPE_X + SCOPE_W - 1, y, _
            SCOPE_GRID
    next

    gfx.draw.line dst, cx, SCOPE_Y, cx, SCOPE_Y + SCOPE_H - 1, _
        SCOPE_GRID

    for i = 0 to ubound(foes)
        if foes(i).kind = 0 or foes(i).hp <= 0 then continue for
        dx = wrapped(foes(i).place.x - c.place.x)
        dy = wrapped(foes(i).place.y - c.place.y)
        ahead = dx * cos(c.heading) + dy * sin(c.heading)
        side = dy * cos(c.heading) - dx * sin(c.heading)
        x = cx + side * SCOPE_W / SCOPE_REACH
        y = cy - ahead * SCOPE_W / SCOPE_REACH
        if x < SCOPE_X or x > SCOPE_X + SCOPE_W - 1 then continue for
        if y < SCOPE_Y or y > SCOPE_Y + SCOPE_H - 1 then continue for
        tone = ALERT
        if foes(i).kind = DRONE then tone = AMBER
        gfx.draw.pset dst, x, y, tone
    next

    gfx.draw.pset dst, cx, cy - 1, PALE
    gfx.draw.line dst, cx - 1, cy, cx + 1, cy, PALE
end sub

' A label in faint tone over its value in amber.
sub gauge ( _
    byval dst as long, _
    byval x as integer, _
    byval y as integer, _
    label as string, _
    value as string _
)
    write_text dst, x, y, label, FAINT
    write_text dst, x, y + 8, value, AMBER
end sub

' The panel under the view.
sub draw_panel (byval dst as long, g as Game, byval t as single)
    dim c as Craft
    dim top as integer
    dim above as single
    dim bar as integer
    dim tone as integer

    c = g.heli
    top = VIEW_H
    gfx.draw.rect dst, 0, top, VIEW_W - 1, 199, PANEL, gfx.draw.FILL
    gfx.draw.line dst, 0, top, VIEW_W - 1, top, PANEL_EDGE
    gfx.draw.line dst, 0, top + 1, VIEW_W - 1, top + 1, SHADE
    above = c.place.z - ground_at(g.vale, c.place.x, c.place.y)
    gauge dst, 8, top + 6, "ALT", number(above)
    gauge dst, 40, top + 6, "HDG", number(compass(c.heading))
    gauge dst, 8, top + 26, "GUN", number(c.rounds)
    gauge dst, 40, top + 26, "RKT", number(c.rockets)
    gauge dst, 216, top + 6, "SPD", number(abs(c.speed))
    write_text dst, 80, top + 6, "HULL", FAINT
    gfx.draw.rect dst, 80, top + 14, 141, top + 19, SHADE, _
        gfx.draw.FILL
    bar = int(60 * c.hull / FULL_HULL)
    tone = HULL_BAR
    if c.hull < FULL_HULL / 3 then tone = ALERT
    if bar > 0 then
        gfx.draw.rect dst, 81, top + 15, 80 + bar, top + 18, tone, _
            gfx.draw.FILL
    end if
    gauge dst, 80, top + 26, "TARGETS", _
        number(foes_left(g.foes())) + " LEFT"
    gfx.draw.rect dst, 150, top + 4, 206, top + 44, SHADE, _
        gfx.draw.FILL
    write_text dst, 154, top + 8, "MISSION " + number(g.mission), _
        FAINT
    write_text dst, 154, top + 20, mission_name(g.mission), AMBER
    draw_scope dst, c, g.foes()
end sub
