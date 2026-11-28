' The game's loop: the keys read, the state stepped, the
' frame drawn and shown, till Esc on the title.
option explicit

import gfx.display, gfx.surface, gfx.draw
from engine.input import Keys
from highvale.model.game import Game, TITLE, BRIEFING, FLYING
from highvale.model.game import CLEARED, DOWNED, VICTORY, MISSIONS
from highvale.model.craft import Controls
from highvale.land.palette import set_colours, AMBER, PALE, ALERT
from highvale.land.palette import FAINT, SHADE
from highvale.land.maps import allot_land
from highvale.play.flight import fly
from highvale.play.combat import fire_craft, move_shots
from highvale.play.combat import think_foes, age_blasts
from highvale.play.mission import start_mission, judge
from highvale.play.mission import mission_name, mission_task
from highvale.view.eye import Camera, aim_camera, aim_fall
from highvale.view.eye import VIEW_W, VIEW_H
from highvale.view.scene import draw_scene
from highvale.view.terrain import Terrain, make_terrain
from highvale.view.hud import draw_tape, draw_sight, draw_panel
from highvale.view.hud import write_text
from highvale.view.letters import draw_note
from highvale.view.art import Art, make_art
from highvale.control.keys import read_keys

const SCREEN_W = 320
const SCREEN_H = 200
' a step's longest, so a stall does not throw the craft
const LONGEST_STEP = 0.1
' the title's backdrop turns this fast, seen from this
' much higher than the start
const DRIFT = 0.12
const TITLE_RISE = 40
' notes flash this many times a second
const FLASH = 3

' A frame with a note while a vale is made.
sub show_charting (byval canvas as long)
    gfx.draw.clear canvas, SHADE
    draw_note canvas, "CHARTING THE VALE", 90, 2, AMBER
    gfx.display.flip
end sub

' Mission n begun, its briefing shown.
sub brief (g as Game, byval canvas as long, byval n as integer)
    show_charting canvas
    start_mission g, n
    g.state = BRIEFING
end sub

' The state stepped dt; pressed, Enter just pressed.
sub step_state ( _
    g as Game, _
    k as Camera, _
    byval canvas as long, _
    byval pressed as integer, _
    byval dt as single _
)
    select case g.state
        case TITLE
            g.heli.heading = g.heli.heading + DRIFT * dt
            if pressed then brief g, canvas, 1
        case BRIEFING
            if pressed then g.state = FLYING
        case FLYING
            fly g.heli, g.asked, g.vale, dt
            aim_camera k, g.heli
            fire_craft g.heli, g.asked, g.shots(), aim_fall(k), dt
            think_foes g.foes(), g.shots(), g.heli, g.vale, dt
            judge g
        case CLEARED
            if pressed and g.mission = MISSIONS then
                g.state = VICTORY
            elseif pressed then
                brief g, canvas, g.mission + 1
            end if
        case DOWNED
            if pressed then brief g, canvas, g.mission
        case VICTORY
            if pressed then g.state = TITLE
    end select

    move_shots g.shots(), g.foes(), g.blasts(), g.heli, g.vale, dt
    age_blasts g.blasts(), dt
end sub

' The keys' help on the title.
sub draw_help (byval dst as long)
    gfx.draw.rect dst, 0, VIEW_H, VIEW_W - 1, SCREEN_H - 1, SHADE, _
        gfx.draw.FILL
    write_text dst, 12, VIEW_H + 8, _
        "ARROWS  TURN AND TILT     A  Z  CLIMB AND SINK", PALE
    write_text dst, 12, VIEW_H + 20, _
        "SPACE  GUNS     X  ROCKETS     ESC  LEAVE", PALE
    write_text dst, 12, VIEW_H + 32, _
        "CLEAR EACH VALE OF FLAK SITES AND DRONES", FAINT
end sub

' What the state shows over the view and the panel.
sub draw_overlay (byval canvas as long, g as Game, byval t as single)
    dim lit as integer

    lit = int(t * FLASH * 2) and 1

    select case g.state
        case TITLE
            draw_note canvas, "HIGHVALE", 26, 5, AMBER
            if lit then draw_note canvas, "PRESS ENTER", 100, 2, PALE
            draw_help canvas
        case BRIEFING
            draw_panel canvas, g, t
            draw_note canvas, "MISSION " + ltrim$(str$(g.mission)), _
                30, 3, AMBER
            draw_note canvas, mission_name(g.mission), 60, 2, PALE
            draw_note canvas, mission_task(g.mission), 84, 1, PALE
            if lit then draw_note canvas, "PRESS ENTER", 110, 1, AMBER
        case FLYING
            draw_tape canvas, g.heli.heading
            draw_sight canvas
            draw_panel canvas, g, t
            if g.heli.scraping and lit then
                draw_note canvas, "PULL UP", 40, 2, ALERT
            end if
        case CLEARED, DOWNED
            draw_panel canvas, g, t
            draw_note canvas, g.note, 40, 3, AMBER
            if lit then draw_note canvas, "PRESS ENTER", 110, 1, PALE
        case VICTORY
            draw_panel canvas, g, t
            draw_note canvas, "THE VALE IS CLEAR", 40, 2, AMBER
            draw_note canvas, "WELL FLOWN", 70, 2, PALE
            if lit then draw_note canvas, "PRESS ENTER", 110, 1, PALE
    end select
end sub

sub run_game ()
    dim canvas as long
    dim cockpit as long
    dim g as Game
    dim a as Art
    dim kb as Keys
    dim k as Camera
    dim tv as Terrain
    dim last as Controls
    dim pressed as integer
    dim now as double
    dim was as double
    dim t as single
    dim dt as single

    canvas = gfx.display.open(SCREEN_W, SCREEN_H, 8, _
        gfx.display.VSYNC)
    set_colours
    cockpit = gfx.surface.new_view(canvas, 0, 0, VIEW_W, VIEW_H)
    make_art a
    gfx.draw.set_font a.font
    make_terrain tv
    allot_land g.vale
    show_charting canvas
    start_mission g, 1
    g.heli.place.z = g.heli.place.z + TITLE_RISE
    g.state = TITLE
    was = gfx.display.clock

    do
        now = gfx.display.clock
        dt = now - was
        if dt > LONGEST_STEP then dt = LONGEST_STEP
        was = now
        t = t + dt
        last = g.asked
        read_keys kb, g.asked, t
        pressed = g.asked.go and not last.go

        if g.asked.quit and not last.quit then
            if g.state = TITLE then exit do
            g.state = TITLE
        end if

        step_state g, k, canvas, pressed, dt
        aim_camera k, g.heli
        draw_scene cockpit, k, tv, g.vale, g.foes(), g.shots(), _
            g.blasts(), a, t
        draw_overlay canvas, g, t
        gfx.display.flip
    loop

    gfx.display.close
end sub
