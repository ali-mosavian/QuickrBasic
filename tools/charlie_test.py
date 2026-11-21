#!/usr/bin/env python3
"""charlie_test.py [EXE]: CHARLIE.BAS's level played through by an autopilot on the host build.

The main loop is swapped for one that plays with the game's own pilot, which holds Right and jumps
at pits, walls and cats ahead. The level must be cleared on the first life, round all five of
its loops, run upside down: a gap too wide, a wall too high, a loop entered too slowly or a landing that
misses the ground fails it.

Every half bar of both tunes and every sound effect is played: a PLAY string the game builds wrong is an
Illegal function call mid-game. Jump held through a hit must jump as the hit's hold ends: it was forgotten.
The level cleared, Charlie must go on to its family past the goal and stop by them; then the second zone, the
Chrome Works, is played through the same way, its moving platforms and lifts ridden.
First a cat is walked to the level's left end: its look ahead,
left of column 0, was a negative tile column, and the game stopped with Subscript out of range.

Before it runs, the fields assigned outside their record's owner are counted (docs/engine-design.md,
Boundaries): the count may only fall.
"""

import sys
import shutil
import tempfile
import subprocess
from pathlib import Path

import basic_src
import charlieart

ROOT = Path(__file__).resolve().parents[1]
DEMO = ROOT / "src/plat/wasm/web/demos/CHARLIE.BAS"

# Each record's owner, the package or module that alone assigns its fields; a record not named
# here is owned with the record it is a field of.
OWNERS = {
    "clock": "engine/clock",
    "datafile": "engine/datafile",
    "store": "engine/video",
    "keys": "engine/input",
    "deck": "engine/audio",
    "song": "engine/audio",
    "half": "engine/audio",
    "hero": "stage/hero",
    "pad": "control",
    "autopilot": "control/pilot",
    "thing": "stage/things",
    "swarm": "stage/things",
    "bird": "stage/things",
    "runner": "stage/things",
    "rover": "stage/things",
    "front": "stage/things",
    "rope": "stage/things",
    "level": "world",
    "ground": "world",
    "realm": "world",
    "mover": "world",
    "game": "flow",
    "session": "flow",
    "cue": "flow/intro",
    "tones": "assets/palette",
    "part": "stage/fx",
    "loose": "stage/fx",
    "lob": "stage/fx",
    "world": "model/world",
}
# The fields assigned outside their owner today: the count may only fall.
OUTSIDE = 556

DRIVER = """from engine.video import Blit, OpenScreen, SH, SW
from engine.clock import Clock, Pace, Steps, STEPT
from engine.audio import Beat, Half, NBAR, NextHalf, NSLOT, NVOICE, Song, StartSong
from barkley.model.images import FAMF, IBARON, ICISTERN, IFAM, ITANK, ITREE, NFAM
from barkley.model.hero import Hero, Pad
from barkley.model.level import GRAV, Ground, KBONE, KBRAMBLE, KBUOY, KCAT, KCISTERN, KCRAB, KDRIFT, KGOAT, KHOOP
from barkley.model.level import KICICLE, KJELLY, KJET, KLEG, KLOAD, KLOG, KLOOP, KMOUND, KPIPE, KPRESS, KSAPLING, KSEAT
from barkley.model.level import KSLED, KSPAN, KSPARK, KSPRING, KSQUIRREL, KSWEEP, KTHROW, KVINE, KWALL, KWAVE, Level, MH
from barkley.model.level import KAWNING, KBIN, KCHIMNEY, KGOLD, KPOUNCE, KWALKER, NGOLD, NONE, NSTAGE, TS, ZONES
from barkley.model.level import KMORTAR, KSNATCH, KTIPPER
from barkley.model.game import CLEARED, Cue, Game, INTRO, Loose, NCUE, NLOOSE, NPART, OVER, Part, PLAYING, Session
from barkley.model.game import Found, Gild, PULSES, TITLE
from barkley.assets.palette import CBEE, CFLY, CGOLD, CINK, CNUT, CSEA, CVINE, CVINED, CWHITE, CYELLOW
from barkley.assets.palette import CCLAY, CLINEN, CPOLE, CQINK, CSOOT
from barkley.assets.store import BuildArt
from barkley.assets.cast import BaronPic, FamPic
from barkley.assets.tunes import CAUGHT, CAUGHTUP, EndBeat, LEFTBEHIND, LoadSongs, PACKING, PickSong, SCLANK, Sfx, SHURT
from barkley.assets.tunes import KeepBeat, SJUMP, SMORTAR, SongFor, Sudden, WHISTLED, ZoneTune, ZTUNE
from barkley.world.terrain.ground import LoadTables, LOOPR, SegAt, Solid, Surf, WADE, WAVEL
from barkley.world.terrain.track import MOVEW, WAVEA
from barkley.world.layout import PITD
from barkley.world.level import CABLED, GoldNo, LANTFALL, LANTH, LoadLevel, Mend, SINTRO, ZoneName
from barkley.world.movers import FREEB
from barkley.stage.fx.parts import Burst
from barkley.stage.fx.damage import BOUNCEV, Hurt, Lose
from barkley.stage.hero.modes import AFLOAT, DIGV, DRYT, FLUEH, Lull, VINEGRIP, VINELEN
from barkley.stage.hero.moves import GLOWT, JUMPV, MoveHero, SHELLT, THROWC, TOP, WETTOP
from barkley.stage.hero.draw import LOGR
from barkley.stage.zones.works import PRESSFALL, PRESSLAG, PRESSP, PRESSRISE, PressT, PRESSUP, SPARKLIT, SPARKP, Stroke
from barkley.stage.zones.sands import Flow, FLUMEV, GCLIMB, GDIVE, GHOVER, JELLYV, STEPV, TIDEP
from barkley.stage.zones.wood import BRAMBLET, FLYREST, GLOWFADE, LOGH, LOGV, MOUNDR, NBEE, SAPREACH, SAPT, SQNEAR
from barkley.stage.zones.wood import SQRUN, SQV, SQWAIT
from barkley.stage.zones.pass import Fallen, GCHARGE, HeapOf, HEAPR, LOADRUN, LOADSLIDE, SLEDV, Slid, THROWAT, THROWUP
from barkley.stage.zones.pass import TREMBLE, WALLR
from barkley.stage.zones.quarter import AWNT, AWNW, Leapt, LINESAG, LINET, LineY, POUNCEFAR, POUNCEGONE, POUNCEN
from barkley.stage.zones.quarter import PotClear, PotFall, POTR, POTX, POUNCEOUT, POUNCER, SILLUP
from barkley.stage.zones.chase import BOLTS, CARTLEAD, CHASETUNE, NBUMP, PAWUP, PEDALMORE, PEDALUP, PEDALV, PEDALW
from barkley.stage.zones.chase import LEGV, Lobbed, LOBWAIT, NLEG, RAILUP, TUMBLE, TwinPose
from barkley.stage.camera import Camera, HEAD, Headroom, HEADUP
from barkley.stage.scene import Render
from barkley.stage.play import JELLYD, SPARKR, Spawn, Things
from barkley.control.pilot import Pilot
from barkley.flow.save import Keep, LoadSave, Reach
from barkley.stage.hud import PIPX, PIPY
from barkley.stage.zones.downs import GOLDPTS
from barkley.flow.intro import BEATS, BETWEEN, BUMP0, CueAt, HORIZON, ILEN, IntroDraw, QHERO, QSND, QVAN, StartIntro
from barkley.flow.states import GoStage, ResetWorld, StartGame, TALLYN, Tick
from barkley.main import Advance
declare function GroundAt% (lv as Level, x as integer)
declare function HalfLen% (x as string)
declare function Tripled% (x as string)
declare function Moves% (x as string, wide as integer)
declare function SongLasts% (sg as Song)
declare sub Stand (lv as Level, gr as Ground, p as Hero, x as integer, v as single)
declare sub StandUnder (lv as Level, gr as Ground, p as Hero, x as integer, x0 as integer)
declare sub AtBall (lv as Level, p as Hero)
declare function FlatRun% (lv as Level, ice as integer)
declare function Fall% (wd as World, g as Game, p as Hero, lo() as Loose, pt() as Part, hue%, a%, b%)
declare function Fur% (x as integer, w as integer, y0 as integer, y1 as integer)
declare function Pix& (x as integer, w as integer, y0 as integer, y1 as integer)
declare function OnCue% (cu() as Cue, kind as integer, bt as single)
declare sub KeepCol (x as integer, a() as integer)
declare function Lowest% (x as integer, a() as integer)
declare function Uneven% (hz as single, grain as integer, per as integer, most as integer, want as integer)
' the game's steps against a screen's frames, timed by a clock of 128 or 256 moves a second: stepped by that clock
' some frames showed two steps and some none, and the picture juddered. At a screen's 59.94 or 60.1 frames a second
' each shows a step; at 119.88 every other; at 75 four of five, never three; and with no screen, looked at 20000
' times a second, the steps still come
if Uneven%(59.94, 128, 1, 1, 600) + Uneven%(60.1, 256, 1, 1, 600) = 0 then print "paced 60 ok"
if Uneven%(119.88, 128, 2, 1, 300) = 0 then print "paced 120 ok"
if Uneven%(75, 256, 0, 2, 480) + Uneven%(20000, 128, 0, 1, 2) = 0 then print "paced by time ok"
OpenScreen
LoadTables wd.lv
BuildArt wd.art, wd.tn
LoadLevel wd, 0
dim g as Game, p as Hero, k as Pad, i as long, j as integer, loops as integer, was as integer
dim c as integer, v as integer, m as integer, zp as integer, seen as string, brim as integer, ebb as integer
dim hid as long, shown as long, bt as single, colA(SH - 1) as integer, colB(SH - 1) as integer, act1 as string
dim lo(NLOOSE - 1) as Loose, pt(NPART - 1) as Part, cu(NCUE) as Cue, spans as integer, fs as Session
wd.mu.muted = -1
StartGame wd, g, p, lo(), pt()
for j = 0 to wd.lv.n_things - 1
    if wd.lv.things(j).kind = KCAT then
        wd.lv.things(j).at.x = 24
        wd.lv.things(j).home = 24
        wd.lv.things(j).at.y = wd.lv.start.y
        g.cam.x = 0
        exit for
    end if
next
for i = 1 to 300
    Things wd.lv, wd.mu, wd.se, g, p, lo(), pt()
next
print "edge ok"
wd.mu.muted = 0
LoadSongs wd.mu
for j = 0 to NSLOT - 1
    wd.mu.coming = j
    StartSong wd.mu, wd.se.ticks
    play Beat$(wd.mu, j, wd.mu.song(j).half(0).kit, 3, 0)
    for i = 0 to NVOICE - 1
        play , , wd.mu.song(j).vo_one(i) + "V1", , , wd.mu.song(j).vo_two(i) + "V1"
    next
next
play "T255", "T255", "T255", , "T255", "T255", "T255", "T255", "T255"
dim hf as Half
for j = 0 to NSLOT - 1
    for i = 0 to NBAR * 2 - 1
        hf = wd.mu.song(j).half(i)
        play hf.drums, hf.bass, hf.one, , hf.lead, hf.two, hf.under, hf.third, hf.strike
    next
next
for j = SJUMP to SMORTAR
    Sfx wd.mu, wd.se, j
next
print "sounds ok"
' the level's end has a tune of its own, played once through its eight bars: it was Charlie's theme from its
' hits, round and round
g.mode = CLEARED
PickSong wd.mu, SongFor%(g), -1
for i = 1 to 40
    NextHalf wd.mu, wd.se.ticks
    if wd.mu.ended then exit for
next
if wd.mu.slot = CAUGHT and i = 16 then print "caught ok"
' the intro has its own tune in the zone's slot, 28 bars that end: its kit, its voicings and its first half bar played
StartIntro wd, g, cu()
PickSong wd.mu, SongFor%(g), 0
NextHalf wd.mu, wd.se.ticks
for i = 0 to NVOICE - 1
    play , , wd.mu.song(ZTUNE).vo_one(i) + "V1", , , wd.mu.song(ZTUNE).vo_two(i) + "V1"
next
was = wd.mu.slot = ZTUNE and wd.mu.z_held = 4
if was and len(wd.mu.song(ZTUNE).order) = 28 and wd.mu.song(ZTUNE).ends then print "intro tune ok"
' the intro, played through and drawn: the view where the script has it, on the house, the hedge, the bump and
' where Charlie lands; a cue of every kind; Charlie stood by the road, then away at full tilt; at its end the
' title, on the Amber Downs in full colour with their own tune
wd.mu.muted = -1
seen = string$(QSND, "0")
for i = 0 to NCUE
    if cu(i).kind = 0 then exit for
    mid$(seen, cu(i).kind, 1) = "1"
next
was = seen = string$(QSND, "1")
for i = 1 to 4000
    Advance wd, g, p, k, lo(), pt(), cu()
    if g.mode <> INTRO then exit for
    if i mod 15 = 0 then IntroDraw wd, g, p, cu()
    select case i
    case 60: if point(0, 0) <> 0 or point(30, 100) < 130 or point(30, 100) > 136 then was = 0
    case 300: if g.cam.x < 0 or g.cam.x > 60 or p.at.x < 70 or p.at.x > 250 then was = 0
    case 1200: if g.cam.x <> 1400 then was = 0
    case 1680: if g.cam.x <> 2140 then was = 0
    case 2700: if g.cam.x <> 3900 or p.at.x <> 4060 or p.gsp <> 0 then was = 0
    case 3300: if p.gsp < 400 or p.at.x - g.cam.x < 150 or p.at.x - g.cam.x > 300 then was = 0
    end select
next
was = was and abs(i - ILEN * 30) < 3 and g.mode = TITLE
if was and wd.zn.stage = 0 and wd.mu.z_held = 1 and wd.tn.dusk = 16 then print "intro ok"
was = 0
' a key skips it
StartIntro wd, g, cu()
for i = 1 to 10
    Advance wd, g, p, k, lo(), pt(), cu()
next
was = g.mode = INTRO
k.hit = -1
Advance wd, g, p, k, lo(), pt(), cu()
if was and g.mode = TITLE and wd.zn.stage = 0 and k.hit = 0 then print "skip ok"
was = 0
' before he jumps from the van Charlie is at its door, behind it: the van's side hides him and he shows past the
' door. He was drawn over them both. The picture with him and without, where the van's side is and past its door
StartIntro wd, g, cu()
for i = 1 to 2145
    Advance wd, g, p, k, lo(), pt(), cu()
next
bt = g.mode_t * BEATS
m = OnCue%(cu(), QHERO, bt)
j = OnCue%(cu(), QVAN, bt)
CueAt wd.lv, cu(), j, bt, c, v
c = c - cint(g.cam.x)
IntroDraw wd, g, p, cu()
hid = Pix&(c - 60, 8, HORIZON - 45, HORIZON - 14)
shown = Pix&(c - 92, 16, HORIZON - 45, HORIZON - 14)
cu(m).start.y = cu(m).start.y - 300
cu(m).finish.y = cu(m).finish.y - 300
IntroDraw wd, g, p, cu()
hid = hid - Pix&(c - 60, 8, HORIZON - 45, HORIZON - 14)
shown = shown - Pix&(c - 92, 16, HORIZON - 45, HORIZON - 14)
if hid = 0 and shown <> 0 then print "door ok"
ResetWorld wd, g, p, lo(), pt()
wd.mu.muted = 0
g.mode = TITLE
wd.mu.muted = -1
StartGame wd, g, p, lo(), pt()
g.bones = 3
k.jump = -1
k.was = -1
Hurt wd.mu, wd.se, g, p, cint(p.at.x) + 10, lo(), SHURT
for i = 1 to 150
    Tick wd, g, p, k, lo(), pt()
    if p.stun = 0 and p.jumped then was = -1
next
if was then print "held ok"
was = 0
k.jump = 0
k.was = 0
' hit, his bones fly out and are gone in their four seconds, and a burst's dust dies: their step read 0, so both
' hung in the air where they were made for ever, the bones never to be picked up
dim bx as single, by as single
StartGame wd, g, p, lo(), pt()
g.bones = 3
Hurt wd.mu, wd.se, g, p, cint(p.at.x) + 10, lo(), SHURT
Burst wd.se, pt(), p.at.x, p.at.y - 40, 8, 100, CWHITE
bx = lo(0).at.x
by = lo(0).at.y
was = wd.se.loose_n = 3 and wd.se.part_n >= 8
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = was and lo(0).at.x <> bx and lo(0).at.y <> by
for i = 1 to 300
    Tick wd, g, p, k, lo(), pt()
next
if was and wd.se.loose_n = 0 and wd.se.part_n = 0 then print "spill ok"
was = 0
' the first 45 degree ramp after flat ground, c its first column
for c = 8 to wd.lv.w - 8
    j = GroundAt%(wd.lv, c * TS - 1)
    if j <> NONE and GroundAt%(wd.lv, c * TS - 40) = j and GroundAt%(wd.lv, c * TS + 15) = j - TS then exit for
next
' run onto it at every speed: at some, its rise ahead was taken for a wall and Charlie stopped dead at its foot
StartGame wd, g, p, lo(), pt()
k.right = -1
for v = 120 to 840 step 60
    Stand wd.lv, wd.gr, p, c * TS - 30, csng(v)
    for i = 1 to 12
        MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
        if p.at.x > c * TS + 24 then exit for
        if p.ground and p.gsp < v / 2 then was = -1
    next
next
' and stood on it, no key down, it stays: it slid back to the foot
k.right = 0
Stand wd.lv, wd.gr, p, c * TS + 8, 0
for i = 1 to 90
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
next
if was = 0 and abs(p.at.x - (c * TS + 8)) < 2 then print "slope ok"
' a jump pressed a moment before landing is made on landing: it was lost
StartGame wd, g, p, lo(), pt()
k.hit = -1
k.jump = -1
was = 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if was = 0 and not p.ground and p.vel.y > 0 and GroundAt%(wd.lv, cint(p.at.x)) - p.at.y < 10 then
        k.hit = -1
        was = 1
    end if
    if was = 1 and p.ground then was = 2
    if was = 2 and p.jumped and p.vel.y < -200 then was = 3
next
if was = 3 then print "buffer ok"
' and one pressed a moment after running off an edge is still made: it was lost too
StartGame wd, g, p, lo(), pt()
for c = 8 to wd.lv.w - 8
    if GroundAt%(wd.lv, c * TS) = NONE and GroundAt%(wd.lv, c * TS - 20) <> NONE then exit for
next
Stand wd.lv, wd.gr, p, c * TS - 20, 200
k.right = -1
k.jump = 0
k.hit = 0
was = 0
for i = 1 to 60
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    k.hit = 0
    if not p.ground then was = was + 1
    if was = 3 then
        k.hit = -1
        k.jump = -1
    end if
    if was = 4 then exit for
next
if p.jumped and p.vel.y < -250 then print "edge jump ok"
' walked slowly off an edge, Charlie falls at once: a foot still over the ground put it back on the edge, and it
' hung there half a second, till it was fast enough to clear it in a step
Stand wd.lv, wd.gr, p, c * TS - 20, 60
k.right = -1
k.jump = 0
k.hit = 0
v = cint(p.at.y)
for i = 1 to 45
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
next
if p.at.y > v + 20 then print "ledge ok"
was = 0
k.right = 0
k.jump = 0
k.hit = 0
k.was = 0
' dropped anywhere over the ground, Charlie lands: past the first loop it fell through the level
StartGame wd, g, p, lo(), pt()
for c = 40 to wd.lv.wpx - 40 step 7
    p.at.x = c
    p.at.y = 40
    p.vel.x = 0
    p.vel.y = 700
    p.ground = 0
    p.sg = 0
    for i = 1 to 200
        MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
        if p.ground or p.at.y > MH * TS then exit for
    next
    if p.ground = 0 and wd.lv.top(c \\ TS) < MH then was = was + 1
next
if was = 0 then print "drop ok"
' a loop is ground: met fast, no key held, it is run round, over its top and out the far side; met slowly Charlie
' stalls on its side and slides back to its foot
for j = 0 to wd.lv.n_things - 1
    if wd.lv.things(j).kind = KLOOP then exit for
next
c = wd.lv.things(j).at.x
Stand wd.lv, wd.gr, p, c - 8, 460
was = 0
for i = 1 to 300
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if p.ground then if wd.gr.pt(p.sg).dir.x < -.9 then was = -1
    if p.at.x > c + LOOPR + 12 then exit for
next
v = was and p.ground and p.at.x > c + LOOPR + 12
Stand wd.lv, wd.gr, p, c - 60, 200
was = 0
for i = 1 to 300
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if p.ground then if wd.gr.pt(p.sg).dir.x < 0 then was = -1
next
if v and was = 0 and p.ground and abs(p.at.x - c) < LOOPR and p.at.y > wd.lv.things(j).at.y - 8 then print "loop ok"
' too slow at its top, Charlie falls from it and lands inside
Stand wd.lv, wd.gr, p, c - 8, 340
was = 0
for i = 1 to 200
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if not p.ground and p.at.y < wd.lv.things(j).at.y - LOOPR then was = -1
next
if was and p.ground and p.at.x < c + LOOPR then print "stall ok"
' a jump before a loop comes down on the ground under it, not on the far half of the ring
Stand wd.lv, wd.gr, p, c - 30, 0
k.hit = -1
k.jump = -1
was = 0
for i = 1 to 90
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    k.hit = 0
    if p.ground then if wd.gr.pt(p.sg).kind = 2 then was = -1
next
k.jump = 0
if was = 0 and p.ground and abs(p.at.y - wd.lv.things(j).at.y) < 2 then print "ghost ok"
' a quarter pipe is run up, and its wall: off the top and onto the ground above
for j = 0 to wd.lv.n_things - 1
    if wd.lv.things(j).kind = KPIPE then exit for
next
c = wd.lv.things(j).at.x
Stand wd.lv, wd.gr, p, c - 100, 360
k.right = -1
was = 0
for i = 1 to 300
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if p.ground then if wd.gr.pt(p.sg).dir.x < .1 then was = -1
    if p.ground and p.at.x > c + 8 then exit for
next
if was and p.ground and p.at.x > c + 8 and abs(p.at.y - wd.lv.top(c \\ TS) * TS) < 2 then print "pipe ok"
' a ribbon is ground: run onto at speed it is ridden over each hump and off its far end onto the ground
for j = 0 to wd.lv.n_things - 1
    if wd.lv.things(j).kind = KWAVE then exit for
next
c = wd.lv.things(j).at.x
Stand wd.lv, wd.gr, p, c - 40, 420
k.right = -1
was = 0
v = 0
for i = 1 to 400
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if p.ground then if wd.gr.pt(p.sg).kind = 2 and p.at.y < wd.lv.things(j).at.y - WAVEA + 8 then was = was + 1
    ' down its far sides too: with only the rising halves there it was flown from crest to crest
    if p.ground then if wd.gr.pt(p.sg).kind = 2 and wd.gr.pt(p.sg).dir.y > .3 then v = v + 1
    if p.ground and wd.gr.pt(p.sg).kind = 1 and p.at.x > c + 384 then exit for
next
if was > 3 and v > 6 and p.ground and p.at.x > c + 384 and abs(p.at.y - wd.lv.things(j).at.y) < 2 then print "wave ok"
' a ribbon is a twist, run on its inside: at its crest Charlie hangs under its line, not stood on it as on a hill
Stand wd.lv, wd.gr, p, c - 40, 420
was = 0
for i = 1 to 200
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if p.ground and abs(p.at.x - c - WAVEL) < 6 then
        g.cam.x = p.at.x - 160
        g.cam.y = p.at.y - 100
        Render wd, g, p, lo(), pt()
        if Fur%(160, 8, 104, 124) > 40 and Fur%(160, 8, 76, 96) = 0 then was = -1
        exit for
    end if
next
if was then print "twist ok"
' a quarter round he is seen from over his back, across the line and none of him high over it as when stood
' on the rise; three quarters round the ribbon is before him
Stand wd.lv, wd.gr, p, c - 40, 420
was = 0
for i = 1 to 200
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if p.ground and (was and 1) = 0 and abs(p.at.x - c - WAVEL / 2) < 6 then
        g.cam.x = p.at.x - 160
        g.cam.y = p.at.y - 100
        Render wd, g, p, lo(), pt()
        if Fur%(160, 2, 92, 99) > 20 and Fur%(160, 2, 103, 107) > 12 and Fur%(160, 8, 70, 87) = 0 then print "turn ok"
        was = was or 1
    end if
    if p.ground and abs(p.at.x - c - WAVEL * 1.5) < 6 then
        g.cam.x = p.at.x - 160
        g.cam.y = p.at.y - 100
        Render wd, g, p, lo(), pt()
        if Fur%(160, 16, 60, 140) = 0 then print "hid ok"
        exit for
    end if
next
' and too slow for it, drops from under the crest to the pit's floor
Stand wd.lv, wd.gr, p, c - 40, 420
for i = 1 to 300
    if p.ground and p.at.x >= c + 150 and p.at.x < c + 230 then
        p.gsp = 100
        k.right = 0
    end if
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if p.ground and p.at.y > wd.lv.things(j).at.y + 40 then exit for
next
if p.ground and wd.gr.pt(p.sg).kind = 1 and p.at.y > wd.lv.things(j).at.y + 40 and p.at.x < c + 384 then print "slow ok"
' a bark stops a prowler where it is a while, and it goes on away from him: it walked on as if deaf
k.right = 0
StartGame wd, g, p, lo(), pt()
j = -1
for i = 0 to wd.lv.n_things - 1
    c = wd.lv.things(i).home - 40
    if wd.lv.things(i).kind = KCAT and j < 0 and abs(GroundAt%(wd.lv, c) - wd.lv.things(i).at.y) < 8 then
        if SegAt%(wd.lv, wd.gr, c, csng(GroundAt%(wd.lv, c))) >= 0 then j = i
    end if
next
wd.lv.things(j).at.x = wd.lv.things(j).home
wd.lv.things(j).state = -1
Stand wd.lv, wd.gr, p, wd.lv.things(j).home - 40, 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
was = 0
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.things(j).at.x <> wd.lv.things(j).home then was = was + 1
next
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
if was = 0 and wd.lv.things(j).at.x > wd.lv.things(j).home then print "bark ok"
was = 0
StartGame wd, g, p, lo(), pt()
for i = 1 to 9000
    Pilot wd.lv, wd.gr, wd.zn, wd.se, p, k, wd.pilot
    Tick wd, g, p, k, lo(), pt()
    j = 0
    if p.ground then j = wd.gr.pt(p.sg).dir.x < -.9
    if j and was = 0 then loops = loops + 1
    was = j
    if i mod 30 = 0 then Render wd, g, p, lo(), pt()
    if g.mode <> PLAYING then exit for
next
print "result"; g.mode; g.lives; g.bones; loops; cint(p.at.x); cint(p.at.y)
k.left = 0
k.right = 0
k.down = 0
k.jump = 0
k.hit = 0
for i = 1 to 400
    Tick wd, g, p, k, lo(), pt()
next
Render wd, g, p, lo(), pt()
was = p.win and abs(p.at.x - (wd.lv.fam_x - 106)) < 3 and wd.se.cheer
if was and abs(g.cam.x - (wd.lv.fam_x - 166)) < 3 then print "met ok"
' They were one picture waiting and one cheering, and the Baron two lumps on a back: each of them waits in three
' pictures and cheers in six, he sits in four and points in two, and each one's last is the art tool's, its size
' read from the picture
was = 0
for c = 0 to NFAM - 1
    v = 0
    m = 0
    for i = 0 to 400
        fs.ticks = i
        j = (FamPic%(fs, c, 0) - IFAM) \\ 2 - c * FAMF
        if j < 0 or j > 2 then was = -99 else v = v or 2 ^ j
        j = (FamPic%(fs, c, -1) - IFAM) \\ 2 - c * FAMF
        if j < 3 or j >= FAMF then was = -99 else m = m or 2 ^ j
    next
    if v = 7 and m = 504 then was = was + 1
    j = IFAM + (c * FAMF + FAMF - 1) * 2
    line (0, 0)-(99, 99), 0, BF
    Blit wd.art, 0, 0, j, (wd.art.img(wd.art.at(j)) and 65535) \\ 8, wd.art.img(wd.art.at(j)) \\ 65536
    print "fam"; Pix&(0, 100, 0, 99)
next
v = 0
m = 0
for i = 0 to 400
    fs.ticks = i
    v = v or 2 ^ (((BaronPic%(fs, 0) - IBARON) \\ 2) and 7)
    m = m or 2 ^ (((BaronPic%(fs, 1) - IBARON) \\ 2) and 7)
next
line (0, 0)-(99, 99), 0, BF
Blit wd.art, 0, 0, IBARON + 10, 40, 44
print "baron"; Pix&(0, 100, 0, 99)
if was = NFAM and v = 15 and m = 48 then print "cast ok"
' it stays a while, fades, and the Downs' evening begins in full colour, the score and lives kept
was = 0
j = g.lives
for i = 1 to 1200
    Tick wd, g, p, k, lo(), pt()
    if wd.tn.dusk < 8 then was = i
    if wd.zn.stage = 1 then exit for
next
was = was > 60 and wd.zn.stage = 1 and wd.zn.id = 0
if was and g.mode = PLAYING and wd.tn.dusk = 16 and g.score > 0 and g.lives = j then print "faded ok"
' and in its own colours, over the shared ones
zp = wd.tn.n(1) > 0 and wd.tn.now(wd.tn.which(1, 0)) = wd.tn.own(1, 0)
zp = zp and wd.tn.own(1, 0) <> wd.tn.day(wd.tn.which(1, 0))
' the works have their own tune in the zone's slot, at 132: its kit, its voicings and its first half bar played. They
' had the Downs'
GoStage wd, 2, g, p, lo(), pt()
PickSong wd.mu, SongFor%(g), 0
NextHalf wd.mu, wd.se.ticks
for i = 0 to NVOICE - 1
    play , , wd.mu.song(ZTUNE).vo_one(i) + "V1", , , wd.mu.song(ZTUNE).vo_two(i) + "V1"
next
if wd.mu.slot = ZTUNE and wd.mu.z_held = 5 and wd.mu.song(ZTUNE).tempo = 132 then print "works tune ok"
' a platform carries who stands on it, to and fro
for m = 0 to wd.lv.n_movers - 1
    if wd.lv.movers(m).kind = 0 then exit for
next
c = wd.lv.movers(m).sg
p.at.x = wd.gr.pt(c).at.x + 24
p.at.y = wd.gr.pt(c).at.y
p.sg = c
p.ground = -1
p.gsp = 0
was = cint(p.at.x)
v = was
for i = 1 to 240
    Tick wd, g, p, k, lo(), pt()
    if p.at.x < was then was = cint(p.at.x)
    if p.at.x > v then v = cint(p.at.x)
next
if p.ground and p.sg = c and abs(p.at.x - wd.gr.pt(c).at.x - 24) < 2 and v - was > 40 then print "carried ok"
' who flees in a chase, met at the race's end: run into, its cart hurts Charlie and takes no bump; rolled into, it
' takes one and is dazed; dazed, it takes no other
GoStage wd, 1, g, p, lo(), pt()
wd.lv.cart.st = 1
wd.lv.cart.at.x = wd.lv.chase_end
wd.lv.cart.at.y = GroundAt%(wd.lv, wd.lv.chase_end)
g.bones = 3
k.right = -1
Stand wd.lv, wd.gr, p, wd.lv.chase_end - 60, 200
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
next
was = p.inv > 0 and g.bones = 0 and wd.lv.cart.hits = 0
g.bones = 3
p.inv = 0
p.stun = 0
Stand wd.lv, wd.gr, p, wd.lv.chase_end - 60, 300
p.roll = -1
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.hits then exit for
next
c = wd.lv.cart.hits = 1 and wd.lv.cart.t > BOLTS and g.bones = 3 and not p.ground
Stand wd.lv, wd.gr, p, wd.lv.chase_end - 10, 0
p.roll = -1
for i = 1 to 10
    Tick wd, g, p, k, lo(), pt()
next
if was and c and wd.lv.cart.hits = 1 and g.bones = 3 then print "cart ok"
was = 0
k.right = 0
' on a slope the cart follows it, each wheel on the ground under it: it was drawn level, a wheel sunk in the slope
' and a wheel in the air. The picture without the cart and with it, down its wheels' columns: halfway down a slope
' of the race, then at the slope's foot, its front wheel on the level
GoStage wd, 1, g, p, lo(), pt()
g.mode_t = 5
g.shake = 0
was = -1
c = wd.lv.chase_x + 104
for j = 0 to 1
    do while c < wd.lv.chase_end - 104
        v = GroundAt%(wd.lv, c + 13) - GroundAt%(wd.lv, c - 13)
        if j = 0 and v = 13 and GroundAt%(wd.lv, c + 30) - GroundAt%(wd.lv, c - 30) = 30 then exit do
        if j = 1 and v >= 5 and v <= 8 and GroundAt%(wd.lv, c + 30) = GroundAt%(wd.lv, c + 13) then exit do
        c = c + 8 - 6 * j
    loop
    wd.lv.cart.st = 3
    wd.lv.cart.at.x = c
    wd.lv.cart.at.y = GroundAt%(wd.lv, c)
    Stand wd.lv, wd.gr, p, c - 200, 0
    g.cam.x = c - 160
    g.cam.y = wd.lv.cart.at.y - 124
    Render wd, g, p, lo(), pt()
    KeepCol 147, colA()
    KeepCol 173, colB()
    wd.lv.cart.st = 1
    Render wd, g, p, lo(), pt()
    v = Lowest%(147, colA()) - (GroundAt%(wd.lv, c - 13) - cint(g.cam.y))
    m = Lowest%(173, colB()) - (GroundAt%(wd.lv, c + 13) - cint(g.cam.y))
    if c >= wd.lv.chase_end - 104 or abs(v) > 1 or abs(m) > 1 then was = 0
next
if was then print "tilt ok"
was = 0
' the works' night ends under Sprocket's crane, on a rail over a hall. The view holds the rail while it is fought; a
' jump does not reach it; a jet's throw does and bumps it: it is dazed and Charlie knocked back down
GoStage wd, 3, g, p, lo(), pt()
for j = 0 to wd.lv.n_things - 1
    if wd.lv.things(j).kind = KJET and wd.lv.things(j).at.x > wd.lv.chase_x then exit for
next
c = wd.lv.things(j).at.x
Stand wd.lv, wd.gr, p, c - 40, 0
Tick wd, g, p, k, lo(), pt()
wd.lv.box.t = 9
k.hit = -1
k.jump = -1
m = 0
for i = 1 to 70
    wd.lv.cart.at.x = p.at.x
    Tick wd, g, p, k, lo(), pt()
    if not p.ground then m = -1
next
k.jump = 0
v = cint(wd.lv.cart.at.y) - RAILUP - cint(g.cam.y)
was = wd.lv.cart.st = 1 and wd.mu.z_held = CHASETUNE + 1 and m and p.ground and wd.lv.cart.hits = 0 _
    and v >= 0 and v < 40
wd.se.ticks = 180 - (c mod 180)
Stand wd.lv, wd.gr, p, c, 0
wd.lv.cart.at.x = c
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.hits then exit for
next
was = was and wd.lv.cart.hits = 1 and wd.lv.cart.t > BOLTS and p.vel.y > 0 and not p.ground
' and a hoop's throw does, from the side
for j = 0 to wd.lv.n_things - 1
    if wd.lv.things(j).kind = KHOOP and wd.lv.things(j).at.x > wd.lv.chase_x then exit for
next
wd.lv.cart.t = 0
wd.lv.cart.at.x = wd.lv.things(j).at.x + 200
wd.lv.box.t = 9
p.at.x = wd.lv.things(j).at.x - 12
p.at.y = wd.lv.things(j).at.y + 12
p.ground = 0
p.vel.x = 300
p.vel.y = 0
m = 0
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
    if p.vel.x > 500 then m = -1
    if wd.lv.cart.hits > 1 then exit for
next
if was and m and wd.lv.cart.hits = 2 then print "crane ok"
' its crate is held over Charlie stood still, let go, and hurts him; he aside, it breaks on the floor and the next
' is a while off, his bones his own still, with any of the first hit's he has picked up
GoStage wd, 3, g, p, lo(), pt()
Stand wd.lv, wd.gr, p, wd.lv.chase_x + 300, 0
Tick wd, g, p, k, lo(), pt()
wd.lv.cart.at.x = p.at.x
wd.lv.box.t = 0
g.bones = 3
m = 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.box.st = 1 then m = -1
    if p.inv > 0 then exit for
next
was = m and p.inv > 0 and g.bones = 0 and wd.lv.box.st = 0
p.inv = 0
p.stun = 0
g.bones = 3
Stand wd.lv, wd.gr, p, wd.lv.chase_x + 300, 0
wd.lv.cart.at.x = p.at.x
wd.lv.cart.v = 0
wd.lv.box.t = 0
m = 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.box.st = 2 and m = 0 then
        Stand wd.lv, wd.gr, p, wd.lv.chase_x + 240, 0
        m = -1
    end if
    if m and wd.lv.box.st = 0 then exit for
next
if was and m and p.inv = 0 and g.bones >= 3 and wd.lv.box.st = 0 and wd.lv.box.t > 1 then print "crate ok"
' its third bump breaks it: the toolbox falls, is not Charlie's while it does, and is once it lies; the works' own
' tune is back
wd.lv.cart.hits = NBUMP - 1
wd.lv.cart.t = 0
wd.lv.box.t = 9
wd.se.ticks = 180 - (c mod 180)
Stand wd.lv, wd.gr, p, c, 0
wd.lv.cart.at.x = c
hid = g.score
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.st = 2 then exit for
next
was = wd.lv.cart.st = 2 and wd.lv.cart.hits = NBUMP and wd.mu.z_held = 9
for i = 1 to 10
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.cart.st = 2
for i = 1 to 60
    Stand wd.lv, wd.gr, p, cint(wd.lv.cart.at.x), 0
    Tick wd, g, p, k, lo(), pt()
next
if was and wd.lv.cart.st = 3 and g.score = hid + 1500 then print "toolbox ok"
was = 0
' a zone's own pictures are in the store only while it is played, where the last zone's were: after the Downs the
' works' tank is as the art tool drew it, after the works the Downs' tree, and the store no fuller for the change
GoStage wd, 0, g, p, lo(), pt()
hid = wd.art.next_at
GoStage wd, 2, g, p, lo(), pt()
line (0, 0)-(63, 79), 0, BF
Blit wd.art, 0, 0, ITANK, 32, 40
print "zonetank"; Pix&(0, 32, 0, 39)
GoStage wd, 0, g, p, lo(), pt()
line (0, 0)-(63, 79), 0, BF
Blit wd.art, 0, 0, ITREE, 64, 80
print "zonetree"; Pix&(0, 64, 0, 79); wd.art.next_at - hid; wd.art.zone_at > 0
' the sands have their own tune in the zone's slot, at 120: its kit, its voicings and its first half bar played.
' They had the Downs'
GoStage wd, 4, g, p, lo(), pt()
PickSong wd.mu, SongFor%(g), 0
NextHalf wd.mu, wd.se.ticks
for i = 0 to NVOICE - 1
    play , , wd.mu.song(ZTUNE).vo_one(i) + "V1", , , wd.mu.song(ZTUNE).vo_two(i) + "V1"
next
was = wd.mu.slot = ZTUNE and wd.mu.z_held = 7
if was and wd.mu.song(ZTUNE).tempo = 120 and len(wd.mu.song(ZTUNE).order) = 36 then print "sands tune ok"
' a zone's second act plays its tune arranged again, a song of its own over the first act's bars, its first half
' bar and voicings played: the sands' sunset at 108 with a second pan under the first, silent at noon; the works'
' night with its lead an octave down; the Downs' evening on the organs from its first bar. Each had its first act's
was = wd.mu.song(ZTUNE).half(8).third = "P2" and wd.mu.song(ZTUNE).lvl(5) = 0
GoStage wd, 5, g, p, lo(), pt()
was = was and wd.mu.z_held = 10 and wd.mu.song(ZTUNE).tempo = 108 and len(wd.mu.song(ZTUNE).order) = 36
was = was and instr(wd.mu.song(ZTUNE).half(8).third, "N49") = 3
GoStage wd, 2, g, p, lo(), pt()
act1 = wd.mu.song(ZTUNE).half(8).lead
GoStage wd, 3, g, p, lo(), pt()
was = was and wd.mu.z_held = 9 and wd.mu.song(ZTUNE).tempo = 132 and instr(act1, "N53") = 3
was = was and instr(wd.mu.song(ZTUNE).half(8).lead, "N41") = 3
GoStage wd, 0, g, p, lo(), pt()
was = was and wd.mu.z_held = 1 and left$(wd.mu.song(ZTUNE).voicing, 1) = "h"
GoStage wd, 1, g, p, lo(), pt()
was = was and wd.mu.z_held = 8 and wd.mu.song(ZTUNE).voicing = string$(28, "o") and len(wd.mu.song(ZTUNE).order) = 76
' the pass's blizzard in F sharp minor, its third voice the wind: it played the morning's tune, in A
GoStage wd, 6, g, p, lo(), pt()
was = was and wd.mu.z_held = 14 and instr(wd.mu.song(ZTUNE).half(8).bass, "N22") > 0
GoStage wd, 7, g, p, lo(), pt()
was = was and wd.mu.z_held = 15 and wd.mu.song(ZTUNE).tempo = 150 and len(wd.mu.song(ZTUNE).order) = 36
was = was and instr(wd.mu.song(ZTUNE).half(8).bass, "N19") > 0 and instr(wd.mu.song(ZTUNE).mid_p, "W3") > 0
' the wood's reel in D minor at 126, a fiddle's saw leading; its night at 112, the flute's triangle leading: both
' played the Downs' tunes
GoStage wd, 8, g, p, lo(), pt()
was = was and wd.mu.z_held = 16 and wd.mu.song(ZTUNE).tempo = 126 and len(wd.mu.song(ZTUNE).order) = 36
was = was and instr(wd.mu.song(ZTUNE).half(8).bass, "N15") > 0 and instr(wd.mu.song(ZTUNE).lead_p, "W2") > 0
GoStage wd, 9, g, p, lo(), pt()
was = was and wd.mu.z_held = 17 and wd.mu.song(ZTUNE).tempo = 112 and instr(wd.mu.song(ZTUNE).lead_p, "W1") > 0
for m = 1 to 9 step 2
    GoStage wd, m, g, p, lo(), pt()
    PickSong wd.mu, SongFor%(g), 0
    NextHalf wd.mu, wd.se.ticks
    for i = 0 to NVOICE - 1
        play , , wd.mu.song(ZTUNE).vo_one(i) + "V1", , , wd.mu.song(ZTUNE).vo_two(i) + "V1"
    next
    was = was and wd.mu.slot = ZTUNE
next
GoStage wd, 0, g, p, lo(), pt()
if was then print "arranged ok"
' a swung song's off-beat eighth comes on the triplet's last: each voice of each of its half bars still lasts half
' a bar. The lead's eighths are a sixth and a twelfth, turn about; the string on two a quarter's rest, a sixth and a
' twelfth's rest; the brushes on the beat a twelfth, then the beat's rests. A straight song has no such lengths.
' Swing was not played at all: the quarter's tune would have been straight
dim swingOk as integer
swingOk = wd.mu.song(ZTUNE).swing = 0 and SongLasts%(wd.mu.song(ZTUNE))
for i = 0 to 7
    swingOk = swingOk and not Tripled%(wd.mu.song(ZTUNE).half(i).lead + wd.mu.song(ZTUNE).half(i).bass)
next
ZoneTune wd.mu, 17
swingOk = swingOk and wd.mu.song(ZTUNE).swing = 1 and wd.mu.song(ZTUNE).tempo = 132 and SongLasts%(wd.mu.song(ZTUNE))
swingOk = swingOk and wd.mu.song(ZTUNE).half(8).lead = "L6N58L12N57L6N58L12N60"
swingOk = swingOk and wd.mu.song(ZTUNE).half(2).one = "P4L6N41P12"
swingOk = swingOk and left$(wd.mu.song(ZTUNE).half(0).drums, 3) = "L12"
swingOk = swingOk and right$(wd.mu.song(ZTUNE).half(0).drums, 9) = "P12P24P24"
for i = 0 to 39
    swingOk = swingOk and HalfLen%(wd.mu.song(ZTUNE).half(i).drums) = 24
next
if swingOk then print "swing ok"
' the quarter's tune is sung: none of its lead's notes is a twenty-fourth, in its half bars more of the lead's moves
' are steps than leaps three times over, and where the lead plays the trumpet rests and the clavinet has a chord's
' note at most. It was pecked, its off-beat eighths 66 ms long, all leaps from chord note to chord note, the Mob's
' riff and the trumpet over it: it sounded random
dim sungOk as integer, near as integer, far as integer
sungOk = -1
for i = 0 to 39
    hf = wd.mu.song(ZTUNE).half(i)
    sungOk = sungOk and instr(hf.lead, "L24N") = 0
    near = near + Moves%(hf.lead, 0)
    far = far + Moves%(hf.lead, -1)
    if instr(hf.lead, "N") = 0 then continue for
    sungOk = sungOk and instr(hf.third, "N") = 0 and Moves%(hf.two, 0) + Moves%(hf.two, -1) = 0
next
' the quarter's kit is no FM: its lead, bass, trumpet and both chord voices are saws and pulses, each filtered and
' in the room, none a pulse through a low-pass, which lets the synthesizer's pulse's DC by. All but the lead were
' FM, near dry: it sounded as an AdLib card. No drum of its two songs is noise: the brushes, hats, snare and claps
' were, a hiss
dim kit as string, kitOk as integer, zt as integer
kitOk = -1
for i = 0 to 4
    select case i
    case 0: kit = wd.mu.song(ZTUNE).lead_p
    case 1: kit = wd.mu.song(ZTUNE).bass_p
    case 2: kit = wd.mu.song(ZTUNE).mid_p
    case 3: kit = wd.mu.song(ZTUNE).vo_one(0)
    case 4: kit = wd.mu.song(ZTUNE).vo_two(0)
    end select
    kitOk = kitOk and instr(kit, "W4") = 0 and instr(kit, "Y") > 0 and instr(kit, "R") > 0 and instr(kit, "R0") = 0
    kitOk = kitOk and not (instr(kit, "W0") > 0 and instr(kit, "Y1") > 0)
next
' The quarter's drums are FM, each fitted to a recorded
' drum and chosen by ear. They were drawn by hand: a sine
' for a kick, a triangle for a hat, a square for a stick.
dim drums_ok as integer
drums_ok = -1
for zt = 17 to 18
    ZoneTune wd.mu, zt
    for i = 0 to len(wd.mu.song(ZTUNE).dr_let) - 1
        kit = wd.mu.song(ZTUNE).dr_patch(i)
        kitOk = kitOk and instr(kit, "W3") = 0
        drums_ok = drums_ok and instr(kit, "W4") > 0
        drums_ok = drums_ok and instr(kit, "Z1,0H") = 0
    next
next
ZoneTune wd.mu, 0
if sungOk and near > 3 * far then print "sung ok"
if kitOk then print "kit ok"
if drums_ok then print "drums ok"
' The quarter's snare is two notes at once on two voices,
' a thump under its hiss. One voice gave the thump or the
' hiss's swell, never both: a snare with no crack.
dim layers_ok as integer
ZoneTune wd.mu, 17
hf = wd.mu.song(ZTUNE).half(23)
layers_ok = instr(hf.strike, "O") > 0
layers_ok = layers_ok and HalfLen%(hf.strike) = 24
hf = wd.mu.song(ZTUNE).half(16)
layers_ok = layers_ok and instr(hf.strike, "O") = 0
layers_ok = layers_ok and HalfLen%(hf.strike) = 24
ZoneTune wd.mu, 0
if layers_ok then print "layers ok"
' the Downs' first act has three golden bones, none found. The first is over the first tree, five tiles up, drawn
' in gold, and a full jump from the ground under it finds it: a thousand points, and it is in the save a step on,
' twelve bytes of it. Found, it is not drawn, and not found twice; its pip on the HUD is lit and the next dark.
' The second found, the save has both, and read back gives both and not the third; a save of four bytes, of
' before there were any, gives none. A stage past the tenth keeps its own in the save's last part
dim goldOk as integer, goldAt as integer, gold2 as integer, goldScore as long, saved as long
GoStage wd, 0, g, p, lo(), pt()
' the zone's card is across the view's middle as a stage begins, gold on red: past it
g.mode_t = 20
wd.se.gold_lo = 0
wd.se.gold_hi = 0
goldAt = -1
for i = wd.lv.n_things - 1 to 0 step -1
    if wd.lv.things(i).kind <> KGOLD then continue for
    gold2 = goldAt
    goldAt = i
next
goldOk = wd.lv.n_gold = NGOLD and goldAt >= 0 and GoldNo%(wd.lv, goldAt) = 0 and GoldNo%(wd.lv, gold2) = 1
Stand wd.lv, wd.gr, p, wd.lv.things(goldAt).at.x, 0
goldOk = goldOk and wd.lv.things(goldAt).at.y = p.at.y - 5 * TS - 14 and not Found%(wd.se, 0, 0)
g.shake = 0
g.cam.x = wd.lv.things(goldAt).at.x - 160
g.cam.y = wd.lv.things(goldAt).at.y - 100
Render wd, g, p, lo(), pt()
goldOk = goldOk and point(160, 100) >= CGOLD and point(160, 100) <= CGOLD + 4
goldOk = goldOk and point(6 + PIPX + 3, 5 + PIPY + 3) = CINK
goldScore = g.score
k.jump = -1
k.hit = -1
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
    k.hit = 0
    if Found%(wd.se, 0, 0) then exit for
next
k.jump = 0
goldOk = goldOk and i < 40 and g.score = goldScore + GOLDPTS and wd.se.unkept
Tick wd, g, p, k, lo(), pt()
open "CHARLIE.SAV" for binary as #2
goldOk = goldOk and lof(2) = 12 and wd.se.unkept = 0
get #2, 5, saved
close #2
goldOk = goldOk and saved = 1
for j = 0 to 1
    p.at.x = wd.lv.things(goldAt).at.x
    p.at.y = wd.lv.things(goldAt).at.y + 13
    p.ground = 0
    p.vel.y = 0
    Tick wd, g, p, k, lo(), pt()
next
goldOk = goldOk and g.score = goldScore + GOLDPTS
' he stands clear of where it was: over it he hid it, found or not
Stand wd.lv, wd.gr, p, wd.lv.things(goldAt).at.x - 60, 0
g.shake = 0
g.cam.x = wd.lv.things(goldAt).at.x - 160
g.cam.y = wd.lv.things(goldAt).at.y - 100
Render wd, g, p, lo(), pt()
goldOk = goldOk and (point(160, 100) < CGOLD or point(160, 100) > CGOLD + 4)
goldOk = goldOk and point(6 + PIPX + 3, 5 + PIPY + 3) = CYELLOW and point(6 + PIPX + 11, 5 + PIPY + 3) = CINK
p.at.x = wd.lv.things(gold2).at.x
p.at.y = wd.lv.things(gold2).at.y + 13
p.ground = 0
p.vel.y = 0
Tick wd, g, p, k, lo(), pt()
Tick wd, g, p, k, lo(), pt()
goldOk = goldOk and Found%(wd.se, 0, 1) and not Found%(wd.se, 0, 2) and g.score = goldScore + 2 * GOLDPTS
wd.se.gold_lo = 0
LoadSave wd.se
goldOk = goldOk and Found%(wd.se, 0, 0) and Found%(wd.se, 0, 1) and not Found%(wd.se, 0, 2) and wd.se.gold_lo = 3
Gild wd.se, 12, 2
goldOk = goldOk and Found%(wd.se, 12, 2) and not Found%(wd.se, 12, 1) and wd.se.gold_hi = 256 and wd.se.gold_lo = 3
Keep wd.se
wd.se.gold_hi = 0
LoadSave wd.se
goldOk = goldOk and wd.se.gold_hi = 256
open "CHARLIE.SAV" for binary as #2
get #2, 1, saved
close #2
kill "CHARLIE.SAV"
open "CHARLIE.SAV" for binary as #2
put #2, 1, saved
close #2
LoadSave wd.se
goldOk = goldOk and wd.se.gold_lo = 0 and wd.se.gold_hi = 0 and not Found%(wd.se, 0, 0)
GoStage wd, 0, g, p, lo(), pt()
if goldOk then print "gold ok"
' after a chase the level's end is the longer one, twelve bars that end, the cat sent packing first, its first half
' bar played; where there is no chase it is the eight. It was the eight everywhere
GoStage wd, 1, g, p, lo(), pt()
was = wd.mu.c_held = PACKING + 1 and len(wd.mu.song(CAUGHT).order) = 12 and wd.mu.song(CAUGHT).ends
g.mode = CLEARED
PickSong wd.mu, SongFor%(g), -1
NextHalf wd.mu, wd.se.ticks
was = was and wd.mu.slot = CAUGHT and wd.mu.bar = 0 and wd.mu.half = 1
GoStage wd, 0, g, p, lo(), pt()
if was and wd.mu.c_held = CAUGHTUP + 1 and len(wd.mu.song(CAUGHT).order) = 8 then print "packing ok"
' in the quarter the guitar is gone till its chase: the blue hour's end is the eight bars whistled, a flute where
' her string was; the festival night's, after the chase, the twelve with her guitar. It was the guitar's eight
GoStage wd, 10, g, p, lo(), pt()
was = wd.mu.c_held = WHISTLED + 1 and len(wd.mu.song(CAUGHT).order) = 8 and wd.mu.song(CAUGHT).voicing = "wwwwwwww"
GoStage wd, 11, g, p, lo(), pt()
was = was and wd.mu.c_held = PACKING + 1 and wd.mu.song(CAUGHT).voicing <> "wwwwwwww"
GoStage wd, 0, g, p, lo(), pt()
if was and wd.mu.c_held = CAUGHTUP + 1 then print "whistled ok"
' the last life lost, game over has its own tune in that slot, four bars at 80 that end, its first half bar played,
' and stays while it plays; then the title, the level's end back in its slot. Game over was five seconds of the
' zone's tune
k.hit = 0
k.start = 0
k.quit = 0
g.lives = 1
Lose wd.mu, wd.se, p
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if g.mode = OVER then exit for
next
was = g.mode = OVER and wd.mu.c_held = LEFTBEHIND + 1 and SongFor%(g) = CAUGHT and wd.mu.song(CAUGHT).tempo = 80
PickSong wd.mu, SongFor%(g), -1
NextHalf wd.mu, wd.se.ticks
was = was and wd.mu.slot = CAUGHT and len(wd.mu.song(CAUGHT).order) = 4 and wd.mu.song(CAUGHT).ends
for i = 1 to 600
    Tick wd, g, p, k, lo(), pt()
next
was = was and g.mode = OVER
for i = 1 to 400
    Tick wd, g, p, k, lo(), pt()
    if g.mode = TITLE then exit for
next
if was and g.mode = TITLE and i < 400 and wd.mu.c_held = CAUGHTUP + 1 then print "over ok"
GoStage wd, 0, g, p, lo(), pt()
' the level's end counts the bonus into the score in time with its tune: none before the bars its lead plays, a
' step on each of their beats, the last of it on their last; a key takes it all at once. The beat is the tune's
' own, counted from where it began, and the scene's with no music. It ran out in a second, off the beat
g.mode = CLEARED
g.mode_t = 0
g.tallied = 0
g.bonus = 1600
hid = g.score
for i = 1 to 230
    Tick wd, g, p, k, lo(), pt()
next
was = g.bonus = 1600
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
was = was and g.bonus = 1500 and g.score = hid + 100
for i = 1 to 150
    Tick wd, g, p, k, lo(), pt()
next
was = was and g.bonus = 1000
for i = 1 to 150
    Tick wd, g, p, k, lo(), pt()
next
was = was and g.bonus = 500
for i = 1 to 180
    Tick wd, g, p, k, lo(), pt()
next
was = was and g.bonus = 0 and g.score = hid + 1600 and g.tallied = TALLYN
GoStage wd, 0, g, p, lo(), pt()
g.mode = CLEARED
g.mode_t = 0
g.tallied = 0
g.bonus = 1230
hid = g.score
k.hit = -1
Tick wd, g, p, k, lo(), pt()
k.hit = 0
was = was and g.bonus = 0 and g.score = hid + 1230
wd.mu.muted = 0
wd.mu.slot = CAUGHT
wd.mu.song_at = wd.se.ticks - 150
was = was and abs(EndBeat!(wd.mu, wd.se, g) - 5) < .01
wd.mu.slot = ZTUNE
was = was and EndBeat!(wd.mu, wd.se, g) < 0
wd.mu.slot = -1
wd.mu.muted = -1
GoStage wd, 0, g, p, lo(), pt()
if was then print "tally ok"
' between zones the van's bump again, on the next zone's road: after the Downs' evening's fade the works' road in
' their dusk's colours, the van, the toolbox and a cat drawn, to four bars of their own that come in at once; then
' the works' first act, the score and lives kept. A key skips it. The works began at once
GoStage wd, 1, g, p, lo(), pt()
g.score = 1234
g.mode = CLEARED
g.bonus = 0
wd.se.met_t = 7
for i = 1 to 200
    Advance wd, g, p, k, lo(), pt(), cu()
    if g.mode = INTRO then exit for
next
was = g.mode = INTRO and wd.zn.stage = SINTRO and wd.zn.id = 1 and wd.se.scene_next = 2
was = was and wd.mu.z_held = BETWEEN + 1 and wd.tn.dusk = 16
was = was and Sudden%(wd.se, g) and abs(g.mode_t * BEATS - BUMP0) < 1
was = was and wd.tn.n(2) > 0 and wd.tn.now(wd.tn.which(2, 0)) = wd.tn.own(2, 0)
for i = 1 to 12 * 30
    Advance wd, g, p, k, lo(), pt(), cu()
    if (i mod 60) = 0 then IntroDraw wd, g, p, cu()
next
was = was and g.mode = INTRO and abs(g.cam.x - 2140) < 1
for i = 1 to 200
    Advance wd, g, p, k, lo(), pt(), cu()
    if g.mode <> INTRO then exit for
next
was = was and i < 200 and g.mode = PLAYING and wd.zn.stage = 2
was = was and wd.mu.z_held = 5 and wd.tn.dusk = 16 and g.score = 1234
GoStage wd, 3, g, p, lo(), pt()
g.mode = CLEARED
g.bonus = 0
wd.se.met_t = 7
for i = 1 to 200
    Advance wd, g, p, k, lo(), pt(), cu()
    if g.mode = INTRO then exit for
next
was = was and g.mode = INTRO and wd.zn.id = 2 and wd.se.scene_next = 4
IntroDraw wd, g, p, cu()
k.hit = -1
Advance wd, g, p, k, lo(), pt(), cu()
k.hit = 0
was = was and g.mode = PLAYING and wd.zn.stage = 4 and wd.mu.z_held = 7
GoStage wd, 0, g, p, lo(), pt()
if was then print "bump ok"
' Saltwick Sands' sea rises and falls by the clock, from low water to high and back, and from low water at the
' level's start whatever the clock then: it began where the game's clock had it, a sunset's high water by chance
GoStage wd, 4, g, p, lo(), pt()
g.mode_t = 5
hid = wd.se.ticks
Flow wd.lv, wd.se
was = wd.lv.tide = wd.lv.tide_lo and wd.lv.tide_lo > wd.lv.tide_hi and wd.se.ticks > 0
wd.se.ticks = wd.se.ticks + TIDEP \\ 2
Flow wd.lv, wd.se
was = was and wd.lv.tide = wd.lv.tide_hi
wd.se.ticks = hid
' Charlie over a gap comes down into the sea and floats, his feet under its surface, and paddles slowly: before
' the sea he fell out of the level. c the gap's first column, m the far bank's, v the bank's top: the sea is held
' 24 under it
for c = 8 to wd.lv.w - 1
    if wd.lv.top(c) = MH then exit for
next
for m = c to wd.lv.w - 1
    if wd.lv.top(m) < MH then exit for
next
v = wd.lv.top(m) * TS
wd.lv.tide_lo = v + 24
wd.lv.tide_hi = wd.lv.tide_lo
p.at.x = c * TS + 24
p.at.y = v - 20
p.ground = 0
p.vel.x = 0
p.vel.y = 0
j = g.lives
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.swim and not p.dead and g.lives = j and abs(p.at.y - (wd.lv.tide + AFLOAT)) < 1
j = cint(p.at.x)
k.right = -1
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
next
if was and p.at.x - j > 20 and p.at.x - j < 50 then print "tide ok"
' the sea is drawn in the gap up to its surface and not over it; ground the sea is over is under it too, and of
' the wet pictures: its top tile was sand
g.cam.x = c * TS - 100
g.cam.y = wd.lv.tide - 100
Render wd, g, p, lo(), pt()
was = point(104, 104) = CSEA + 2 and (point(104, 94) < CSEA or point(104, 94) > CSEA + 6)
wd.lv.tide_lo = wd.lv.top(c - 2) * TS - 20
wd.lv.tide_hi = wd.lv.tide_lo
Tick wd, g, p, k, lo(), pt()
g.cam.x = c * TS - 100
g.cam.y = wd.lv.tide - 100
Render wd, g, p, lo(), pt()
j = point(72, 130)
if was and point(72, 104) = CSEA + 2 and j >= CSEA and j <= CSEA + 6 then print "sea ok"
' a hop from the water gets him onto a bank two tiles over its surface, which the levels count on, and not onto
' one far over it: the tide opens and closes the way
was = 0
for j = 0 to 1
    wd.lv.tide_lo = v + 2 * TS + 28 * j
    wd.lv.tide_hi = wd.lv.tide_lo
    Flow wd.lv, wd.se
    p.at.x = m * TS - 12
    p.at.y = wd.lv.tide + AFLOAT
    p.swim = -1
    p.ground = 0
    p.vel.x = 0
    k.right = -1
    k.jump = -1
    for i = 1 to 90
        if i = 20 then k.hit = -1
        Tick wd, g, p, k, lo(), pt()
        if p.ground then exit for
    next
    if j = 0 and p.ground and p.at.x > m * TS - 8 then was = -1
    if j = 1 and p.ground then was = 0
next
k.jump = 0
if was and p.swim then print "bank ok"
' out of the water he is wet and slow, till a roll shakes him dry
wd.lv.tide_lo = v + 24
wd.lv.tide_hi = wd.lv.tide_lo
Stand wd.lv, wd.gr, p, m * TS + 24, WETTOP - 20
p.swim = 0
p.wet = DRYT
k.right = -1
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = p.wet > 0 and p.gsp > WETTOP - 20 and p.gsp <= WETTOP + 1
Stand wd.lv, wd.gr, p, m * TS + 24, WETTOP - 20
p.roll = -1
k.right = 0
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.wet <= 0
Stand wd.lv, wd.gr, p, m * TS + 24, WETTOP - 20
k.right = -1
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
k.right = 0
if was and p.gsp > WETTOP + 60 then print "wet ok"
' no water is a trap: the bank past each stretch swum at high water is within a hop of it, two tiles, or the
' stretch is waded at low water, to its spring. A sea gap's far bank was seven tiles up, Charlie afloat for good.
GoStage wd, 0, g, p, lo(), pt()
was = -1
zp = -1
for m = 4 to 5
    GoStage wd, m, g, p, lo(), pt()
    brim = wd.lv.tide_lo
    ebb = wd.lv.tide_hi
    ' the level's own tide, not one a check before this held it at: the sands' noon was read at such a one
    if brim = ebb then was = 0
    if wd.lv.tide_hi < brim then swap brim, ebb
    for c = 1 to wd.lv.w - 2
        v = wd.lv.top(c) * TS
        j = wd.lv.top(c + 1) * TS
        if v > brim + WADE and j <= brim + WADE and brim - j > 2 * TS and v > ebb + WADE then was = 0
    next
    ' and no bone nor spring lies where the sea never leaves: the sunset's pools had bones on their floors
    for i = 0 to wd.lv.n_things - 1
        if wd.lv.things(i).kind = KBONE and wd.lv.things(i).at.y > ebb + 9 then zp = 0
        if wd.lv.things(i).kind = KSPRING and wd.lv.things(i).at.y > ebb + WADE then zp = 0
    next
next
if was and brim < ebb then print "shore ok"
if zp then print "reach ok"
' a boat lies on the floor of its pool at low water, and still; afloat it drifts, its deck just over the sea, and
' Charlie aboard goes up with it
GoStage wd, 4, g, p, lo(), pt()
for m = 0 to wd.lv.n_movers - 1
    if wd.lv.movers(m).kind = 5 then exit for
next
c = wd.lv.movers(m).sg
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = m < wd.lv.n_movers and wd.gr.pt(c).at.y = wd.lv.movers(m).at.y and wd.gr.pt(c).at.x = wd.lv.movers(m).at.x
was = was and wd.lv.tide - FREEB > wd.lv.movers(m).at.y
wd.lv.tide_lo = wd.lv.movers(m).at.y - 20
wd.lv.tide_hi = wd.lv.tide_lo
p.at.x = wd.gr.pt(c).at.x + 20
p.at.y = wd.gr.pt(c).at.y
p.sg = c
p.ground = -1
p.gsp = 0
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.gr.pt(c).at.y = wd.lv.tide - FREEB and wd.gr.pt(c).at.x <> wd.lv.movers(m).at.x
was = was and p.ground and p.sg = c and abs(p.at.y - wd.gr.pt(c).at.y) < 1
if was then print "boat ok"
' the sands' old pier: a span stays while no one is on it, totters under Charlie stood on it, then falls from under
' him into its pool, where he is not lost, and is back when he starts again; run over at speed every span holds
' till he is past, he is on the far bank dry, and all of them have fallen behind him. At the level's own low
' water, its pool dry: the check before held the tide high
GoStage wd, 0, g, p, lo(), pt()
GoStage wd, 4, g, p, lo(), pt()
for m = 0 to wd.lv.n_movers - 1
    if wd.lv.movers(m).kind = 6 then exit for
next
c = wd.lv.movers(m).sg
spans = 0
for i = m to wd.lv.n_movers - 1
    if wd.lv.movers(i).kind = 6 then spans = spans + 1
next
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
was = m < wd.lv.n_movers and spans > 5 and wd.gr.pt(c).at.y = wd.lv.movers(m).at.y
was = was and wd.lv.movers(m).t = 0 and wd.lv.things(wd.lv.movers(m).thing).kind = KSPAN
p.at.x = wd.gr.pt(c).at.x + 24
p.at.y = wd.gr.pt(c).at.y
p.sg = c
p.ground = -1
p.gsp = 0
for i = 1 to 12
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.gr.pt(c).at.y = wd.lv.movers(m).at.y and p.ground and p.sg = c _
    and wd.lv.things(wd.lv.movers(m).thing).state <> 0
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.gr.pt(c).at.y > wd.lv.movers(m).at.y + 200 _
    and wd.lv.things(wd.lv.movers(m).thing).at.y > wd.lv.movers(m).at.y + 200
was = was and not (p.ground and p.sg = c)
' down on the pool's dry floor, not through it
was = was and not p.dead and p.ground and abs(p.at.y - (wd.lv.movers(m).at.y + PITD * TS)) < 2
Spawn wd.lv, wd.gr, wd.zn, wd.mu, g, p
Tick wd, g, p, k, lo(), pt()
was = was and wd.gr.pt(c).at.y = wd.lv.movers(m).at.y and wd.lv.movers(m).t = 0 _
    and wd.lv.things(wd.lv.movers(m).thing).state = 0
Stand wd.lv, wd.gr, p, wd.lv.movers(m).edge - 40, 300
k.right = -1
v = 0
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if p.swim or p.at.y > wd.lv.movers(m).at.y + 8 then v = -1
    if p.at.x > wd.lv.movers(m).edge + spans * MOVEW + 30 then exit for
next
k.right = 0
was = was and i < 200 and v = 0 and p.ground and abs(p.at.y - wd.lv.movers(m).at.y) < 2
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
next
j = 0
for i = m to wd.lv.n_movers - 1
    if wd.lv.movers(i).kind = 6 and wd.gr.pt(wd.lv.movers(i).sg).at.y > wd.lv.movers(i).at.y + 200 then j = j + 1
next
if was and j = spans then print "pier ok"
' the sands' flume: walked onto, Charlie is swept down it curled, no slower than its water runs, and thrown from
' its end over the pool; its trough is drawn over the ground, the planks under the ground's line as dark as its
' end's cap over it. A slope took him at his own pace
GoStage wd, 4, g, p, lo(), pt()
for j = 0 to wd.lv.n_things - 1
    if wd.lv.things(j).kind = KCISTERN then exit for
next
was = j < wd.lv.n_things
Stand wd.lv, wd.gr, p, wd.lv.things(j).at.x - 30, 80
k.right = -1
v = 0
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if p.ground and p.at.x > wd.lv.things(j).at.x + 30 and p.at.x < wd.lv.things(j).at.x + 70 then
        v = v + 1
        if p.gsp < FLUMEV or not p.roll then was = 0
    end if
    if not p.ground and p.at.x > wd.lv.things(j).at.x + 96 then exit for
next
k.right = 0
was = was and v > 2 and i < 60 and p.vel.x >= FLUMEV
g.cam.x = wd.lv.things(j).at.x - 100
g.shake = 0
Render wd, g, p, lo(), pt()
c = 100
m = wd.lv.things(j).at.y - int(g.cam.y)
if was and point(c + 40, m + 42) = point(c + 95, m + 77) and wd.art.at(ICISTERN) > 0 then print "flume ok"
' a works' sweeper hurts whoever meets its hood, a rolling Charlie too; rolled into from behind it is beaten, and
' landed on, with a bounce. Jumped at 50 pixels like a cat, its hood caught the pilot rising
GoStage wd, 2, g, p, lo(), pt()
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KSWEEP then exit for
next
was = c < wd.lv.n_things
for j = 0 to 1
    g.bones = 5
    p.inv = 0
    p.stun = 0
    wd.lv.things(c).state = -1
    Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 14, 200
    if j then p.roll = -1
    Tick wd, g, p, k, lo(), pt()
    was = was and p.inv > 0 and wd.lv.things(c).state <> 0
next
hid = g.score
p.inv = 0
p.stun = 0
wd.lv.things(c).state = -1
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x + 14, -200
p.roll = -1
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.things(c).state = 0 and g.score = hid + 100 and p.inv <= 0
' rising past its hood, his feet over its top, he is clear of it
wd.lv.things(c).state = -1
p.at.x = wd.lv.things(c).at.x - 15
p.at.y = wd.lv.things(c).at.y - 22
p.ground = 0
p.roll = 0
p.vel.x = 0
p.vel.y = -200
Tick wd, g, p, k, lo(), pt()
was = was and p.inv <= 0 and wd.lv.things(c).state <> 0
p.at.x = wd.lv.things(c).at.x
p.at.y = wd.lv.things(c).at.y - 30
p.ground = 0
p.roll = 0
p.vel.x = 0
p.vel.y = 100
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
    if p.vel.y < 0 then exit for
next
if was and wd.lv.things(c).state = 0 and p.inv <= 0 and abs(p.vel.y + BOUNCEV) < 30 then print "sweeper ok"
' a works' spark goes SPARKR either way along its rail; its carriage hurts for the SPARKLIT steps of SPARKP it is
' alight, and is safe dark
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KSPARK then exit for
next
was = c < wd.lv.n_things
Stand wd.lv, wd.gr, p, wd.lv.things(c).home - 100, 0
m = wd.lv.things(c).at.x
v = wd.lv.things(c).at.x
for i = 1 to 240
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.things(c).at.x < m then m = wd.lv.things(c).at.x
    if wd.lv.things(c).at.x > v then v = wd.lv.things(c).at.x
next
was = was and m >= wd.lv.things(c).home - SPARKR and m < wd.lv.things(c).home - SPARKR + 4
was = was and v <= wd.lv.things(c).home + SPARKR and v > wd.lv.things(c).home + SPARKR - 4
for j = 0 to 1
    do until (wd.se.ticks + wd.lv.things(c).home) mod SPARKP = 10 + j * SPARKLIT
        wd.se.ticks = wd.se.ticks + 1
    loop
    g.bones = 5
    p.inv = 0
    p.stun = 0
    Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 4, 0
    Tick wd, g, p, k, lo(), pt()
    was = was and (p.inv > 0) = (j = 0)
next
if was then print "spark ok"
' the works' presses: a head comes down in its turn, a press along a row after the one before it by the pixels
' between them over PRESSLAG; it hurts who is under it once it is down to him, and who jumps into it while it is
' up; from a stop, a run begun as the first has risen goes under the whole row; the head is drawn where it is
GoStage wd, 3, g, p, lo(), pt()
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KPRESS then exit for
next
for m = c + 1 to wd.lv.n_things - 1
    if wd.lv.things(m).kind = KPRESS then exit for
next
was = m < wd.lv.n_things
do until Stroke%(wd.lv, wd.se, c) = 0 and PressT%(wd.lv, wd.se, c) < 20
    wd.se.ticks = wd.se.ticks + 1
loop
g.bones = 5
p.inv = 0
p.stun = 0
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x, 0
v = -1
j = -1
for i = 1 to PRESSP
    Tick wd, g, p, k, lo(), pt()
    if p.inv > 0 and v < 0 then v = Stroke%(wd.lv, wd.se, c)
    if Stroke%(wd.lv, wd.se, c) = PRESSUP and j < 0 then j = wd.se.ticks
next
was = was and v > PRESSUP - 28
do until Stroke%(wd.lv, wd.se, m) = PRESSUP
    wd.se.ticks = wd.se.ticks + 1
loop
do while Stroke%(wd.lv, wd.se, m) = PRESSUP
    wd.se.ticks = wd.se.ticks - 1
loop
was = was and (wd.se.ticks + 1 - j) mod PRESSP = (wd.lv.things(m).at.x - wd.lv.things(c).at.x) \\ PRESSLAG
do until Stroke%(wd.lv, wd.se, c) = 0 and PressT%(wd.lv, wd.se, c) < 20
    wd.se.ticks = wd.se.ticks + 1
loop
g.bones = 5
p.inv = 0
p.stun = 0
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x, 0
k.hit = -1
k.jump = -1
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
    if p.inv > 0 then exit for
next
k.hit = 0
k.jump = 0
was = was and i < 20 and Stroke%(wd.lv, wd.se, c) = 0
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
next
do until PressT%(wd.lv, wd.se, c) = PRESSRISE + 28
    wd.se.ticks = wd.se.ticks + 1
loop
g.bones = 5
p.inv = 0
p.stun = 0
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 60, 0
for j = m to wd.lv.n_things - 1
    if wd.lv.things(j).kind = KLEG then exit for
next
k.right = -1
for i = 1 to 400
    Tick wd, g, p, k, lo(), pt()
    if p.inv > 0 then was = 0
    if p.at.x > wd.lv.things(j).at.x + 20 then exit for
next
k.right = 0
was = was and i < 400
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 60, 0
g.cam.x = wd.lv.things(c).at.x - 160
g.shake = 0
for j = 0 to 1
    do until PressT%(wd.lv, wd.se, c) = 20 + j * (PRESSFALL - 4)
        wd.se.ticks = wd.se.ticks + 1
    loop
    Render wd, g, p, lo(), pt()
    colA(j) = point(160, wd.lv.things(c).at.y - int(g.cam.y) - 4)
    colB(j) = point(160, wd.lv.things(c).at.y - int(g.cam.y) - PRESSUP - 4)
next
if was and colA(0) <> colA(1) and colB(0) <> colB(1) then print "press ok"
' Whitecap Pass is the fourth zone, by name and in its own colours: its snow's top the same picture at both hours,
' white on a clear morning and not at dusk
was = ZoneName$(3) = "WHITECAP PASS"
for j = 0 to 1
    GoStage wd, 6 + j, g, p, lo(), pt()
    Tick wd, g, p, k, lo(), pt()
    g.cam.x = 150
    g.shake = 0
    Render wd, g, p, lo(), pt()
    colA(j) = point(100, GroundAt%(wd.lv, 250) - int(g.cam.y))
    if j = 0 then hid = wd.tn.now(colA(0))
    was = was and wd.zn.id = 3
next
if was and colA(0) = colA(1) and hid = &H3F3F3F and wd.tn.now(colA(1)) <> hid then print "pass ok"
' Lantern Quarter is the sixth zone, by name and in its own colours: its roof's ridge the same picture at both
' hours, lit indigo at the blue hour and not on the festival night; its tune swung, the festival's the faster
was = ZoneName$(5) = "LANTERN QUARTER"
for j = 0 to 1
    GoStage wd, 10 + j, g, p, lo(), pt()
    Tick wd, g, p, k, lo(), pt()
    g.cam.x = 150
    g.shake = 0
    Render wd, g, p, lo(), pt()
    colA(j) = point(100, GroundAt%(wd.lv, 250) - int(g.cam.y))
    if j = 0 then hid = wd.tn.now(colA(0))
    was = was and wd.zn.id = 5 and wd.mu.song(ZTUNE).swing = 1 and wd.mu.song(ZTUNE).tempo = 132 + 12 * j
next
if was and colA(0) = colA(1) and hid = &H3D2522 and wd.tn.now(colA(1)) <> hid then print "quarter ok"
' a clothesline throws him who comes down on it, his way kept: 60 up off the first, 300 squared over twice GRAV,
' 107 off the second in a row at 400 and 167 off the third at 500, the most; the ground ends the row; from under he
' goes through it, and under it falls on; trodden it gives, and is still again LINET things' steps on; drawn
' post to post
GoStage wd, 10, g, p, lo(), pt()
g.mode_t = 20
was = wd.lv.n_ropes > 0
c = wd.lv.ropes(0).at.x + wd.lv.ropes(0).w \\ 2
m = wd.lv.ropes(0).at.y + LINESAG
for j = 0 to 4
    if j = 4 then
        ' a row ended on the roof before it, then begun again
        Stand wd.lv, wd.gr, p, wd.lv.ropes(0).at.x - 30, 0
        Tick wd, g, p, k, lo(), pt()
        was = was and p.hops = 0
    end if
    if j = 0 or j = 4 then
        p.ground = 0
        p.at.x = c
        p.at.y = m - 30
        p.vel.x = 0
        p.vel.y = 0
        p.jumped = -1
    end if
    for i = 1 to 200
        Tick wd, g, p, k, lo(), pt()
        if p.vel.y < 0 then exit for
    next
    was = was and abs(p.at.y - m) < 4 and wd.lv.ropes(0).t >= LINET - 1 and p.sprung
    if j = 0 then hid = LineY%(wd.lv.ropes(0), wd.lv.ropes(0).w \\ 2)
    for i = 1 to 200
        Tick wd, g, p, k, lo(), pt()
        if p.vel.y >= 0 then exit for
    next
    colA(j) = m - cint(p.at.y)
next
was = was and abs(colA(0) - 60) < 8 and abs(colA(1) - 107) < 8 and abs(colA(2) - 167) < 8
was = was and abs(colA(3) - colA(2)) < 6 and abs(colA(4) - colA(0)) < 6 and hid > m + 3
' from under: risen through it, the row not begun
p.at.x = c
p.at.y = m + 20
p.vel.y = -300
p.hops = 0
for i = 1 to 12
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.at.y < m - 10 and p.vel.y < 0 and p.hops = 0
' and falling from under it he falls on
p.at.y = m + 20
p.vel.y = 50
Tick wd, g, p, k, lo(), pt()
was = was and p.at.y > m + 20 and p.vel.y > 50 and p.hops = 0
' at rest it hangs LINESAG at its middle, drawn there, its post under its end
Stand wd.lv, wd.gr, p, wd.lv.ropes(0).at.x - 60, 0
for i = 1 to LINET * 2 + 4
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.ropes(0).t = 0 and LineY%(wd.lv.ropes(0), wd.lv.ropes(0).w \\ 2) = m
g.cam.x = c - 160
g.shake = 0
Render wd, g, p, lo(), pt()
was = was and point(160, m - int(g.cam.y)) = CLINEN and point(160, m - 6 - int(g.cam.y)) <> CLINEN
was = was and point(wd.lv.ropes(0).at.x - int(g.cam.x), m + 6 - int(g.cam.y)) = CPOLE + 1
if was then print "line ok"
' an awning throws him who comes down on it 135 up, 450 squared over twice GRAV, however often; it stays given
' AWNT things' steps, drawn so; from under he goes through it, and past its edge he falls by it
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KAWNING then exit for
next
was = c < wd.lv.n_things
m = wd.lv.things(c).at.y
for j = 0 to 2
    if j < 2 then
        p.ground = 0
        ' near its outer edge, then at its middle
        p.at.x = wd.lv.things(c).at.x - (AWNW - 2) * (1 - j)
        p.at.y = m - 30
        p.vel.x = 0
        p.vel.y = 0
        p.jumped = -1
    end if
    for i = 1 to 200
        Tick wd, g, p, k, lo(), pt()
        if p.vel.y < 0 then exit for
    next
    was = was and abs(p.at.y - m) < 4 and wd.lv.things(c).state >= AWNT - 1 and p.sprung
    for i = 1 to 200
        Tick wd, g, p, k, lo(), pt()
        if p.vel.y >= 0 then exit for
    next
    colA(j) = m - cint(p.at.y)
next
was = was and abs(colA(0) - 135) < 8 and abs(colA(1) - colA(0)) < 3 and abs(colA(2) - colA(0)) < 3
' its canvas's top row at rest, and given
p.at.x = wd.lv.things(c).at.x - 100
g.cam.x = wd.lv.things(c).at.x - 160
g.cam.y = m - 100
for j = 0 to 1
    wd.lv.things(c).state = j * AWNT
    g.shake = 0
    Render wd, g, p, lo(), pt()
    colB(j) = point(160, 101)
next
was = was and colB(0) <> colB(1) and colB(0) >= 134 and colB(0) <= 136 and colB(1) >= 134 and colB(1) <= 136
for i = 1 to AWNT * 2 + 2
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.things(c).state = 0
' from under: risen through it; and a step past its edge, fallen by
p.ground = 0
p.at.x = wd.lv.things(c).at.x
p.at.y = m + 20
p.vel.y = -300
for i = 1 to 12
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.at.y < m - 10 and p.vel.y < 0 and wd.lv.things(c).state = 0
p.at.x = wd.lv.things(c).at.x - AWNW - 1
p.at.y = m - 10
p.vel.x = 0
p.vel.y = 100
for i = 1 to 12
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.at.y > m + 10 and wd.lv.things(c).state = 0
if was then print "awning ok"
' a chimney that leads on takes him who comes down into its pot: unseen, through the houses to its other at DIGV,
' and up out of that one's pot 118 over it, 420 squared over twice GRAV; the way out takes no one in, and it
' alone smokes; past a pot's edge he falls by it, and on the roof he walks by it
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KCHIMNEY then exit for
next
was = c < wd.lv.n_things
m = wd.lv.things(c).state - 1
was = was and m > c and wd.lv.things(m).kind = KCHIMNEY and wd.lv.things(m).state = 0
for j = 0 to 2
    ' into the way in; onto the way out; a step past the way in's edge
    v = c
    if j = 1 then v = m
    p.ground = 0
    p.at.x = wd.lv.things(v).at.x - 9 * (j = 2)
    p.at.y = wd.lv.things(v).at.y - FLUEH - 20
    p.vel.x = 0
    p.vel.y = 0
    for i = 1 to 40
        Tick wd, g, p, k, lo(), pt()
        if p.dig or p.ground then exit for
    next
    colA(j) = p.dig
next
was = was and colA(0) = m + 1 and colA(1) = 0 and colA(2) = 0
' nor is it gone into from the roof it stands on
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x, 0
for i = 1 to 3
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.dig = 0 and p.ground
p.ground = 0
p.at.x = wd.lv.things(c).at.x
p.at.y = wd.lv.things(c).at.y - FLUEH - 20
p.vel.y = 0
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
    if p.dig then exit for
next
bx = p.at.x
for i = 1 to 15
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.dig = m + 1 and abs(p.at.x - bx - 15 * DIGV / 60) < 1
g.shake = 0
Render wd, g, p, lo(), pt()
was = was and Fur%(cint(p.at.x) - int(g.cam.x), 14, cint(p.at.y) - 26 - int(g.cam.y), cint(p.at.y) - int(g.cam.y)) = 0
for i = 1 to 600
    Tick wd, g, p, k, lo(), pt()
    if p.dig = 0 then exit for
next
was = was and abs(p.at.x - wd.lv.things(m).at.x) < 1 and abs(p.at.y - (wd.lv.things(m).at.y - FLUEH)) < 8 _
    and p.vel.y < 0
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if p.vel.y >= 0 then exit for
next
was = was and abs(wd.lv.things(m).at.y - FLUEH - p.at.y - 118) < 8
' the stack's brick at each, and smoke over the way out's pot only, counted over a puff's rise
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 60, 0
for j = 0 to 1
    v = c
    if j = 1 then v = m
    colB(j) = 0
    g.cam.x = wd.lv.things(v).at.x - 160
    g.cam.y = wd.lv.things(v).at.y - 150
    for i = 0 to 7
        wd.se.ticks = wd.se.ticks + 16
        g.shake = 0
        Render wd, g, p, lo(), pt()
        for hid = 90 to 116
            if point(162, hid) = CSOOT or point(166, hid) = CSOOT then colB(j) = colB(j) + 1
        next
    next
    was = was and point(160, 140) >= 122 and point(160, 140) <= 125
next
if was and colB(0) = 0 and colB(1) > 8 then print "chimney ok"
' a tram waits at its street's near end while no one rides it; ridden, it goes along at 240 a second, he with it,
' to its way's end and no further, level all the way; left, it comes back; drawn in its red
for m = 0 to wd.lv.n_movers - 1
    if wd.lv.movers(m).kind = 8 then exit for
next
was = m < wd.lv.n_movers
c = wd.lv.movers(m).thing
Stand wd.lv, wd.gr, p, wd.lv.movers(m).edge - 40, 0
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
bx = wd.lv.things(c).at.x
was = was and bx = wd.lv.movers(m).at.x and wd.lv.movers(m).t = 0
p.at.x = bx + MOVEW \\ 2
p.at.y = wd.lv.movers(m).at.y
p.sg = wd.lv.movers(m).sg
p.ang = 0
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
was = was and abs(wd.lv.things(c).at.x - bx - 240) < 6 _
    and abs(p.at.x - wd.lv.things(c).at.x - MOVEW \\ 2) < 2 and p.ground
was = was and p.sg = wd.lv.movers(m).sg
g.shake = 0
Render wd, g, p, lo(), pt()
was = was and point(wd.lv.things(c).at.x + 20 - int(g.cam.x), wd.lv.things(c).at.y + 23 - int(g.cam.y)) = 187
for i = 1 to 600
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.movers(m).t = 1 and wd.lv.things(c).at.x = bx + wd.lv.movers(m).reach and p.ground _
    and p.sg = wd.lv.movers(m).sg
was = was and wd.lv.things(c).at.y = wd.lv.movers(m).at.y
Stand wd.lv, wd.gr, p, wd.lv.movers(m).edge - 40, 0
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
next
if was and wd.lv.movers(m).t = 0 and wd.lv.things(c).at.x = bx then print "tram ok"
' a lantern line is ground: come down on, he stands on it and it throws no one; left to it he slides down it,
' faster as he goes, off its end and onto the roof past it, LANTFALL lower; from under he goes through it; drawn
' taut in ink, a lantern's light under it
for c = 0 to wd.lv.n_ropes - 1
    if wd.lv.ropes(c).fall then exit for
next
was = c < wd.lv.n_ropes
bx = wd.lv.ropes(c).at.x + 30
m = LineY%(wd.lv.ropes(c), 30)
p.ground = 0
p.at.x = bx
p.at.y = m - 20
p.vel.x = 0
p.vel.y = 0
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
    if p.ground then exit for
next
was = was and p.ground and p.sg = wd.lv.ropes(c).sg and p.hops = 0 and wd.lv.ropes(c).t = 0
was = was and abs(p.at.y - LineY%(wd.lv.ropes(c), cint(p.at.x) - wd.lv.ropes(c).at.x)) < 3
for j = 0 to 1
    for i = 1 to 12
        Tick wd, g, p, k, lo(), pt()
    next
    colA(j) = p.gsp
    was = was and p.ground and p.sg = wd.lv.ropes(c).sg
next
was = was and colA(0) > 60 and colA(1) > colA(0) + 60
for i = 1 to 600
    Tick wd, g, p, k, lo(), pt()
    if p.ground and p.sg <> wd.lv.ropes(c).sg then exit for
next
was = was and p.at.x > wd.lv.ropes(c).at.x + wd.lv.ropes(c).w
was = was and abs(p.at.y - (wd.lv.ropes(c).at.y + LANTH + LANTFALL)) < 2
' from under: risen through it
p.ground = 0
p.at.x = bx
p.at.y = m + 24
p.vel.x = 0
p.vel.y = -330
for i = 1 to 8
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.at.y < m - 4 and not p.ground
Stand wd.lv, wd.gr, p, wd.lv.ropes(c).at.x - 60, 0
g.cam.x = bx - 160
g.cam.y = m - 100
g.shake = 0
Render wd, g, p, lo(), pt()
was = was and point(160, 100) = CQINK and point(160 - 30 + 9, LineY%(wd.lv.ropes(c), 9) - int(g.cam.y) + 6) = 142
if was then print "slide ok"
' a pouncer waits in its bin, eyes under the lid, while he is far; within POUNCER of it it leaps at him, POUNCEFAR
' along in POUNCEN things' steps, and scratches on its way; landed it prowls as a cat of the Mob does
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KPOUNCE then exit for
next
was = c < wd.lv.n_things and wd.lv.things(c - 1).kind = KBIN and wd.lv.things(c).state = 0
bx = wd.lv.things(c).at.x
m = wd.lv.things(c).at.y
Stand wd.lv, wd.gr, p, bx - POUNCER - 10, 0
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.things(c).state = 0
' its eyes show in the bin, and not once it is out
g.cam.x = bx - 160
g.cam.y = m - 100
g.shake = 0
Render wd, g, p, lo(), pt()
colB(0) = point(160 - 4, 100 - 19)
' near: out at him, to his left; half its leap on, half its reach along and up over the roof
Stand wd.lv, wd.gr, p, bx - POUNCER + 4, 0
g.bones = 5
p.inv = 0
for i = 1 to POUNCEN
    Tick wd, g, p, k, lo(), pt()
    p.inv = 9
next
was = was and wd.lv.things(c).state < 0 and wd.lv.things(c).state >= -POUNCEN _
    and abs(wd.lv.things(c).at.x - (bx - POUNCEFAR \\ 2)) < 5
was = was and Leapt%(wd.lv, c) > 15 and wd.lv.things(c).at.y = m
Render wd, g, p, lo(), pt()
colB(1) = point(160 - 4, 100 - 19)
was = was and colB(0) = 141 and colB(1) <> 141
' it scratches in the air
p.inv = 0
p.at.x = wd.lv.things(c).at.x
p.at.y = wd.lv.things(c).at.y - Leapt%(wd.lv, c) + 7
p.ground = 0
p.roll = 0
Tick wd, g, p, k, lo(), pt()
was = was and p.stun and g.bones = 0
for i = 1 to POUNCEN * 2
    Tick wd, g, p, k, lo(), pt()
    p.inv = 9
next
was = was and abs(wd.lv.things(c).state) = POUNCEOUT and Leapt%(wd.lv, c) = 0 _
    and abs(wd.lv.things(c).at.x - bx) <= POUNCEFAR + 1
v = wd.lv.things(c).at.x
for i = 1 to 8
    Tick wd, g, p, k, lo(), pt()
    p.inv = 9
next
' a turn at its beat's end, then a pixel a things' step
was = was and abs(wd.lv.things(c).at.x - v) >= 2 and abs(wd.lv.things(c).at.x - v) <= 4
if was then print "pounce ok"
' a stage begun again has it back in its bin; a bark from past its reach brings it out; rolled into in the air it
' is beaten, and no cat lands
GoStage wd, 10, g, p, lo(), pt()
g.mode_t = 20
was = was and wd.lv.things(c).state = 0 and wd.lv.things(c).at.x = bx
Stand wd.lv, wd.gr, p, bx - POUNCER - 10, 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
was = was and wd.lv.things(c).state < 0 and wd.lv.things(c).state >= -2
for i = 1 to 10
    Tick wd, g, p, k, lo(), pt()
next
p.inv = 0
p.at.x = wd.lv.things(c).at.x
p.at.y = wd.lv.things(c).at.y - Leapt%(wd.lv, c) + 7
p.ground = 0
p.roll = -1
v = g.score
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.things(c).state = POUNCEGONE and g.score = v + 100 and not p.stun
for i = 1 to POUNCEN * 2 + 4
    Tick wd, g, p, k, lo(), pt()
next
g.cam.x = bx - 160
g.cam.y = m - 100
g.shake = 0
Render wd, g, p, lo(), pt()
if was and wd.lv.things(c).state = POUNCEGONE and point(160 - 4, 100 - 19) <> 141 then print "pouncer ok"
' a rope-walker is a cat on a clothesline, its home two columns along it: it walks the line a pixel a things'
' step, from 14 short of the line's end to 40 past its home, at the line's height; a bark from near stands it
' startled, then it walks on away; who comes down on its line shakes it off, beaten, once; a stage begun again has
' it back, and it scratches him who rises into it
GoStage wd, 10, g, p, lo(), pt()
g.mode_t = 20
c = 0
j = 0
was = 0
for i = 0 to wd.lv.n_things - 1
    if wd.lv.things(i).kind = KWALKER then c = i
next
for i = 0 to wd.lv.n_ropes - 1
    if wd.lv.ropes(i).cat <> c + 1 then continue for
    j = i
    was = wd.lv.things(c).kind = KWALKER
next
bx = wd.lv.ropes(j).at.x
m = wd.lv.ropes(j).at.y
was = was and wd.lv.ropes(j).w = 124 and wd.lv.things(c).at.x = bx + 46 and wd.lv.things(c).state = -1
Stand wd.lv, wd.gr, p, bx - 30, 0
colA(0) = 9999
colA(1) = 0
for i = 1 to 240
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.things(c).at.x < colA(0) then colA(0) = wd.lv.things(c).at.x
    if wd.lv.things(c).at.x > colA(1) then colA(1) = wd.lv.things(c).at.x
    if wd.lv.things(c).at.x = bx + 14 then was = was and wd.lv.things(c).at.y = m + 2
    if wd.lv.things(c).at.x = bx + 86 then was = was and wd.lv.things(c).at.y = m + 4
next
was = was and colA(0) = bx + 14 and colA(1) = bx + 86 and wd.lv.things(c).state = -1
' barked at from the roof past it
Stand wd.lv, wd.gr, p, bx + 130, 0
v = wd.lv.things(c).at.x
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
was = was and wd.lv.things(c).state < -1
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.things(c).state < -1 and abs(wd.lv.things(c).at.x - v) <= 1
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.things(c).state = -1 and wd.lv.things(c).at.x < v - 2
' drawn on its line: its back's fur
colA(2) = wd.lv.things(c).at.x
colA(3) = wd.lv.things(c).at.y
g.cam.x = colA(2) - 160
g.cam.y = colA(3) - 100
g.shake = 0
Render wd, g, p, lo(), pt()
colB(0) = point(160 - 2, 100 - 5)
' he comes down on the line 44 or more from it, twice
hid = g.score
g.bones = 2
p.ground = 0
p.at.x = bx + 20
p.at.y = m - 20
p.vel.x = 0
p.vel.y = 0
p.roll = 0
for j = 0 to 1
    for i = 1 to 200
        Tick wd, g, p, k, lo(), pt()
        if p.vel.y < 0 then exit for
    next
    was = was and p.vel.y < 0 and wd.lv.things(c).state = 0 and g.score = hid + 100 and g.bones = 2 and not p.stun
    for i = 1 to 200
        Tick wd, g, p, k, lo(), pt()
        if p.vel.y >= 0 then exit for
    next
next
Stand wd.lv, wd.gr, p, bx + 130, 0
for i = 1 to 70
    Tick wd, g, p, k, lo(), pt()
next
g.cam.x = colA(2) - 160
g.cam.y = colA(3) - 100
g.shake = 0
Render wd, g, p, lo(), pt()
colB(1) = point(160 - 2, 100 - 5)
was = was and colB(0) >= 32 and colB(0) <= 37 and (colB(1) < 32 or colB(1) > 37)
GoStage wd, 10, g, p, lo(), pt()
g.mode_t = 20
was = was and wd.lv.things(c).state = -1 and wd.lv.things(c).at.x = bx + 46
g.bones = 2
p.inv = 0
p.ground = 0
p.roll = 0
p.at.x = wd.lv.things(c).at.x
p.at.y = wd.lv.things(c).at.y + 10
p.vel.y = -100
Tick wd, g, p, k, lo(), pt()
was = was and p.stun and g.bones = 0
if was then print "walker ok"
' a snatcher is a cat that runs off with loose bones. One to be taken, 30 along its beat: it runs to it three
' pixels a things' step and takes it for good 23 along, 7 short of it. One past its beat, or over its head, it
' leaves; one too new it leaves till it is not; startled by a bark it takes none; rolled into it is beaten as a cat;
' a stage begun again has it back
GoStage wd, 10, g, p, lo(), pt()
g.mode_t = 20
c = 0
for i = 0 to wd.lv.n_things - 1
    if wd.lv.things(i).kind = KSNATCH then c = i
next
bx = wd.lv.things(c).at.x
m = wd.lv.things(c).at.y
was = wd.lv.things(c).kind = KSNATCH and wd.lv.things(c).state = -1
' the bones that lie by it taken away: his count is to tell only what he gets of the loose ones
for i = 0 to wd.lv.n_things - 1
    if wd.lv.things(i).kind = KBONE and abs(wd.lv.things(i).at.x - bx) < 200 then wd.lv.things(i).state = 0
next
Stand wd.lv, wd.gr, p, bx - 100, 0
hid = g.bones
lo(0).at.x = bx + 30
lo(0).at.y = m - 4
lo(0).vel.x = 0
lo(0).vel.y = 0
lo(0).t = 3
wd.se.loose_n = 1
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
    if lo(0).t <= 0 then exit for
next
was = was and i <= 17 and wd.lv.things(c).at.x = bx + 23 and wd.se.loose_n = 0 and g.bones = hid
' past its beat, and over its head
lo(0).at.x = bx + 60
lo(0).at.y = m - 4
lo(0).vel.y = 0
lo(0).t = 3
lo(1).at.x = bx + 30
lo(1).at.y = m - 80
lo(1).vel.x = 0
lo(1).vel.y = 0
lo(1).t = 3
wd.se.loose_n = 2
v = wd.lv.things(c).at.x
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
was = was and lo(0).t > 0 and lo(1).t > 0 and wd.se.loose_n = 2 and abs(wd.lv.things(c).at.x - v) <= 10
' too new, behind it: left while it is, taken after
lo(1).t = 0
lo(0).at.x = wd.lv.things(c).at.x - 20
lo(0).at.y = m - 4
lo(0).vel.y = 0
lo(0).t = 3.75
wd.se.loose_n = 1
v = wd.lv.things(c).at.x
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
was = was and lo(0).t > 0 and wd.lv.things(c).at.x >= v
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = was and lo(0).t <= 0 and wd.se.loose_n = 0
' barked at from 50 off, a bone 15 past it: it stands, and takes it when it has done
v = wd.lv.things(c).at.x
Stand wd.lv, wd.gr, p, v - 50, 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
lo(0).at.x = v + 15
lo(0).at.y = m - 4
lo(0).vel.y = 0
lo(0).t = 3
wd.se.loose_n = 1
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
was = was and lo(0).t > 0 and wd.lv.things(c).state > 1 and abs(wd.lv.things(c).at.x - v) <= 1
g.cam.x = wd.lv.things(c).at.x - 160
g.cam.y = wd.lv.things(c).at.y - 100
g.shake = 0
Render wd, g, p, lo(), pt()
colB(1) = point(160 - 2, 100 - 5)
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
was = was and lo(0).t <= 0 and wd.se.loose_n = 0 and g.bones = hid
' drawn in its own grey, startled and at a prowl; rolled into, beaten
g.cam.x = wd.lv.things(c).at.x - 160
g.cam.y = wd.lv.things(c).at.y - 100
g.shake = 0
Render wd, g, p, lo(), pt()
colB(0) = point(160 - 2, 100 - 5)
was = was and colB(0) >= 164 and colB(0) <= 167 and colB(1) >= 164 and colB(1) <= 167
shown = g.score
p.inv = 0
p.ground = 0
p.roll = -1
p.at.x = wd.lv.things(c).at.x
p.at.y = wd.lv.things(c).at.y - 3
p.vel.y = 0
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.things(c).state = 0 and g.score = shown + 100 and not p.stun
GoStage wd, 10, g, p, lo(), pt()
g.mode_t = 20
was = was and wd.lv.things(c).state = -1 and wd.lv.things(c).at.x = bx
if was then print "snatcher ok"
' a mortar fires on its own beat of every four, by the zone tune's clock. The festival's is 144 beats a minute: a
' beat is 25 ticks, and the mortar on beat 1 fires at each tick 25 past a hundred. Who stands beside it then is
' burnt, and for 6 ticks more, not the 7th, and at no other. Who is in the air over its mouth at that tick goes up
' at 620, his way kept, 256 high, 620 squared over twice GRAV; 3 ticks on he falls on. Its shell is 46 up half a
' beat on, 12 ticks; a beat and a half on its burst's white middle is 96 up. A stage begun again keeps its beat
GoStage wd, 11, g, p, lo(), pt()
g.mode_t = 20
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KMORTAR then exit for
next
was = c < wd.lv.n_things and wd.lv.things(c).state = 1 and wd.mu.song(ZTUNE).tempo = 144
bx = wd.lv.things(c).at.x
m = wd.lv.things(c).at.y
colA(0) = wd.mu.muted
wd.mu.muted = -1
' beside it from 15 ticks after it fired till it fires again, unhurt; then burnt
Stand wd.lv, wd.gr, p, bx - 10, 0
g.bones = 3
p.inv = 9
do until wd.se.ticks mod 100 = 40
    Tick wd, g, p, k, lo(), pt()
loop
p.inv = 0
do until wd.se.ticks mod 100 = 24
    Tick wd, g, p, k, lo(), pt()
loop
was = was and not p.stun and g.bones = 3
Tick wd, g, p, k, lo(), pt()
was = was and p.stun and g.bones = 0
' the 7th tick on, not
do until wd.se.ticks mod 100 = 31
    Tick wd, g, p, k, lo(), pt()
loop
Stand wd.lv, wd.gr, p, bx - 10, 0
p.stun = 0
p.inv = 0
g.bones = 3
Tick wd, g, p, k, lo(), pt()
was = was and not p.stun and g.bones = 3
' the 6th, burnt
p.inv = 9
do until wd.se.ticks mod 100 = 30
    Tick wd, g, p, k, lo(), pt()
loop
p.inv = 0
Tick wd, g, p, k, lo(), pt()
was = was and p.stun and g.bones = 0
' 10 over its mouth as it fires
p.inv = 9
do until wd.se.ticks mod 100 = 24
    Tick wd, g, p, k, lo(), pt()
loop
p.stun = 0
p.inv = 0
p.ground = 0
p.roll = 0
p.at.x = bx
p.at.y = m - 24
p.vel.x = 80
p.vel.y = 0
Tick wd, g, p, k, lo(), pt()
was = was and p.vel.y = -620 and p.vel.x > 60 and not p.stun
v = cint(p.at.y)
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if p.vel.y >= 0 then exit for
next
was = was and abs(v - cint(p.at.y) - 256) < 8
' there 3 ticks after it fires
do until wd.se.ticks mod 100 = 27
    Tick wd, g, p, k, lo(), pt()
loop
p.ground = 0
p.at.x = bx
p.at.y = m - 24
p.vel.x = 0
p.vel.y = 0
Tick wd, g, p, k, lo(), pt()
was = was and p.vel.y > 0 and not p.stun
' drawn: its tube and its shell, its burst, and neither two beats on
Stand wd.lv, wd.gr, p, bx - 60, 0
p.inv = 9
for j = 0 to 2
    do until wd.se.ticks mod 100 = 37 + j * 25
        Tick wd, g, p, k, lo(), pt()
    loop
    g.cam.x = bx - 160
    g.cam.y = m - 150
    g.shake = 0
    Render wd, g, p, lo(), pt()
    colA(1 + j) = point(158, 87)
    colB(1 + j) = point(158, 38)
    was = was and point(160, 144) >= 164 and point(160, 144) <= 166
next
was = was and colA(1) = 141 and colA(2) <> 141 and colA(3) <> 141
was = was and colB(1) <> 147 and colB(2) = 147 and colB(3) <> 147
' the clock: the game's ticks, each the tune's beats a minute of pulses; the tune's own ticks while it plays
hid = wd.se.ticks
shown = wd.mu.song_at
colA(4) = wd.mu.tune
colA(5) = wd.mu.slot
wd.se.ticks = 1000
KeepBeat wd.mu, wd.se
was = was and wd.se.pulse = 144000 and wd.se.tempo = 144
wd.mu.muted = 0
wd.mu.tune = -1
wd.mu.slot = ZTUNE
wd.mu.song_at = 400
KeepBeat wd.mu, wd.se
was = was and wd.se.pulse = 86400
wd.mu.muted = -1
KeepBeat wd.mu, wd.se
was = was and wd.se.pulse = 144000
wd.mu.muted = 0
wd.mu.tune = 0
KeepBeat wd.mu, wd.se
was = was and wd.se.pulse = 144000
wd.mu.tune = -1
wd.mu.slot = CAUGHT
KeepBeat wd.mu, wd.se
was = was and wd.se.pulse = 144000
wd.se.ticks = hid
wd.mu.song_at = shown
wd.mu.tune = colA(4)
wd.mu.slot = colA(5)
wd.mu.muted = colA(0)
if was then print "mortar ok"
' the pilot walked into a mortar's flash, burnt in 9 runs of 25: it jumps one now, whenever it fires. Run past the
' festival night's two from ten moments of the bar, it is never hurt. From the stage's start it would come to
' them at one moment only, the one a platform before them lets it through at
GoStage wd, 11, g, p, lo(), pt()
g.mode_t = 20
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KMORTAR then exit for
next
bx = wd.lv.things(c).at.x
colA(0) = wd.mu.muted
wd.mu.muted = -1
was = c < wd.lv.n_things
for j = 0 to 9
    Stand wd.lv, wd.gr, p, bx - 150, 0
    p.stun = 0
    p.inv = 0
    wd.se.ticks = j * 10
    for i = 1 to 600
        Pilot wd.lv, wd.gr, wd.zn, wd.se, p, k, wd.pilot
        Tick wd, g, p, k, lo(), pt()
        if p.stun then was = 0
        if p.at.x > bx + 250 then exit for
    next
    was = was and p.at.x > bx + 250
next
wd.mu.muted = colA(0)
if was then print "mortar pilot ok"
' a tipper sits at a high window of a house behind the way and drops a flowerpot on its beat of every four. The
' pot falls 70 to the paving in 26 steps and breaks there. It hurts who stands under it, once it is down to his
' head, and no one a step aside; under a bare sill no one is hurt. The way is clear for the pilot from the pot's
' breaking till 0.8 seconds before the next
GoStage wd, 11, g, p, lo(), pt()
g.mode_t = 20
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KTIPPER then exit for
next
was = c < wd.lv.n_things
for i = 0 to wd.lv.n_things - 1
    if wd.lv.things(i).kind = KPOUNCE then wd.lv.things(i).state = POUNCEGONE
next
bx = wd.lv.things(c).at.x + POTX
m = wd.lv.things(c).at.y
colA(0) = wd.mu.muted
wd.mu.muted = -1
k.right = 0
k.left = 0
k.down = 0
k.jump = 0
k.hit = 0
Stand wd.lv, wd.gr, p, bx - 100, 0
do until wd.se.ticks mod 100 = 50
    Tick wd, g, p, k, lo(), pt()
loop
was = was and PotFall%(wd.lv, wd.se, c) = 0 and not PotClear%(wd.lv, wd.se, c)
Stand wd.lv, wd.gr, p, cint(bx), 0
g.bones = 3
do until wd.se.ticks mod 100 = 62
    Tick wd, g, p, k, lo(), pt()
loop
was = was and PotFall%(wd.lv, wd.se, c) = 15
' drawn: the house's wall; the pot in the air, its sill bare
g.shake = 0
g.cam.x = wd.lv.things(c).at.x - 160
g.cam.y = m - 150
Render wd, g, p, lo(), pt()
was = was and point(140, 70) >= 130 and point(140, 70) <= 132
was = was and point(167, 92) = CCLAY and point(167, 77) <> CCLAY and point(161, 77) <> 140
do until wd.se.ticks mod 100 = 68
    Tick wd, g, p, k, lo(), pt()
loop
was = was and g.bones = 3 and p.inv <= 0
do until wd.se.ticks mod 100 = 75
    Tick wd, g, p, k, lo(), pt()
loop
was = was and g.bones = 0 and p.inv > 0 and PotFall%(wd.lv, wd.se, c) = 65
v = 0
for i = 0 to NPART - 1
    if pt(i).t > 0 and pt(i).c = CCLAY then v = v + 1
next
Tick wd, g, p, k, lo(), pt()
was = was and v = 0 and PotFall%(wd.lv, wd.se, c) = NONE
for i = 0 to NPART - 1
    if pt(i).t > 0 and pt(i).c = CCLAY then v = v + 1
next
was = was and v = 8
' broken once: no more of it three steps on
for i = 1 to 3
    Tick wd, g, p, k, lo(), pt()
next
v = 0
for i = 0 to NPART - 1
    if pt(i).t > 0 and pt(i).c = CCLAY then v = v + 1
next
was = was and v <= 8
' under the bare sill, then under the pot stood on it, the cat's paw out for the last beat: not hurt
p.stun = 0
p.inv = 0
g.bones = 3
Stand wd.lv, wd.gr, p, cint(bx), 0
do until wd.se.ticks mod 100 = 49
    Tick wd, g, p, k, lo(), pt()
    j = wd.se.ticks mod 100
    if j = 80 or j = 0 or j = 30 then was = was and PotClear%(wd.lv, wd.se, c) = (j = 80 or j = 0)
    if j <> 90 and j <> 20 and j <> 40 then continue do
    g.cam.x = wd.lv.things(c).at.x - 160
    g.cam.y = m - 150
    Render wd, g, p, lo(), pt()
    was = was and (point(167, 77) = CCLAY) = (j <> 90) and (point(161, 77) = 140) = (j = 40)
loop
was = was and g.bones = 3 and p.inv <= 0
' and a step aside, a whole bar
Stand wd.lv, wd.gr, p, bx - POTR - 2, 0
for i = 1 to 100
    Tick wd, g, p, k, lo(), pt()
next
if was and g.bones = 3 and p.inv <= 0 and SILLUP = 70 then print "tipper ok"
' the pilot waits short of a tipper till its pot has broken, and does not go under with the next too near: come
' at it at a run from 200 before, at each step of the bar, it is never hurt, and is past it
was = c < wd.lv.n_things
for j = 0 to 99
    Stand wd.lv, wd.gr, p, bx - 200, 340
    g.bones = 3
    p.inv = 0
    wd.se.ticks = j
    for i = 1 to 600
        Pilot wd.lv, wd.gr, wd.zn, wd.se, p, k, wd.pilot
        Tick wd, g, p, k, lo(), pt()
        if p.inv > 0 then was = 0
        if p.at.x > bx + 60 then exit for
    next
    was = was and p.at.x > bx + 60
next
k.right = 0
k.left = 0
k.jump = 0
k.hit = 0
wd.mu.muted = colA(0)
if was then print "tipper pilot ok"
' the quarter's golden bones can each be got. The festival night's second stood a tile too high over its balcony:
' no way of playing at it got it. Blue hour: a jump up; dropped on the long line, three bounces, then right; from
' the last balcony over the tram's way. Festival night: a jump beside the mortar as it fires; the awning to the
' balcony, back to its end and up; a jump among the four mortars
hid = wd.se.gold_lo
shown = wd.se.gold_hi
colA(0) = wd.mu.muted
wd.mu.muted = -1
was = -1
for c = 0 to 5
    GoStage wd, 10 + c \\ 3, g, p, lo(), pt()
    g.mode_t = 20
    wd.se.gold_lo = 0
    wd.se.gold_hi = 0
    j = 0
    for i = 0 to wd.lv.n_things - 1
        if wd.lv.things(i).kind <> KGOLD then continue for
        colB(j) = i
        j = j + 1
    next
    m = colB(c mod 3)
    bx = wd.lv.things(m).at.x
    p.inv = 9
    ' v: the tick he jumps at, or on the line holds right from
    select case c
    case 0
        Stand wd.lv, wd.gr, p, bx - 90, 200
        v = 2
    case 1, 2
        p.ground = 0
        p.roll = 0
        p.vel.x = 0
        p.vel.y = 0
        p.at.x = wd.lv.ropes(0).at.x + 40
        p.at.y = wd.lv.ropes(0).at.y - 30
        v = 110
        if c = 2 then p.at.x = bx - 50
        if c = 2 then p.at.y = wd.lv.things(m).at.y + 14 + 7 * TS - 82
        if c = 2 then v = 18
    case 3
        Stand wd.lv, wd.gr, p, bx - 10, 0
        wd.se.ticks = 1000
        v = 9
    case 4
        Stand wd.lv, wd.gr, p, bx - 200, 100
        v = 2
    case 5
        Stand wd.lv, wd.gr, p, bx - 60, 150
        v = 8
    end select
    for i = 1 to 260
        k.right = c <> 3 and (c <> 1 or i >= v)
        k.left = 0
        k.jump = c <> 1 and i >= v and i < v + 25
        k.hit = c <> 1 and i = v
        ' on the balcony: back to its end, and up
        if c = 4 and p.ground and p.at.y < wd.lv.things(m).at.y + 14 + 6 * TS - 70 then
            k.right = 0
            k.left = p.at.x > bx + 10
            k.jump = not k.left
            k.hit = not k.left
        end if
        Tick wd, g, p, k, lo(), pt()
        if Found%(wd.se, 10 + c \\ 3, c mod 3) then exit for
    next
    was = was and Found%(wd.se, 10 + c \\ 3, c mod 3)
next
k.right = 0
k.left = 0
k.jump = 0
k.hit = 0
wd.se.gold_lo = hid
wd.se.gold_hi = shown
wd.se.unkept = -1
Tick wd, g, p, k, lo(), pt()
wd.mu.muted = colA(0)
if was then print "quarter gold ok"
' the first ten acts' golden bones can each be got: by a run at it, with a jump on the way or none, or steered to
' its column once in the air. ways: for each, three digits how far back he starts, two the tick he jumps at, 99 for
' never, one whether he steers, one his speed in hundreds. Each from the same tick of the game's clock: a geyser
' and a gust keep time by it. Russet Wood's third at noon stood two tiles over the sapling's throw: no way of
' playing at it got it
dim ways as string, tickWas as long
ways = "050030009099100501200" + "050030009099100501200" + "050030013003020909900" + "050030009009000909900"
ways = ways + "050030009099100501200" + "050030009099100501200" + "050030005099100501200"
ways = ways + "050030041099000909900" + "050030029099003300300" + "050030029099000500300"
hid = wd.se.gold_lo
shown = wd.se.gold_hi
tickWas = wd.se.ticks
was = -1
for c = 0 to 29
    wd.se.ticks = 1000
    GoStage wd, c \\ 3, g, p, lo(), pt()
    g.mode_t = 20
    wd.se.gold_lo = 0
    wd.se.gold_hi = 0
    j = 0
    for i = 0 to wd.lv.n_things - 1
        if wd.lv.things(i).kind <> KGOLD then continue for
        colB(j) = i
        j = j + 1
    next
    was = was and j = NGOLD
    bx = wd.lv.things(colB(c mod 3)).at.x
    Stand wd.lv, wd.gr, p, cint(bx - val(mid$(ways, c * 7 + 1, 3))), 100 * val(mid$(ways, c * 7 + 7, 1))
    p.inv = 9
    v = val(mid$(ways, c * 7 + 4, 2))
    m = val(mid$(ways, c * 7 + 6, 1))
    for i = 1 to 300
        k.right = m = 0 or p.at.x < bx - 3
        k.left = m <> 0 and p.at.x > bx + 3
        k.jump = i >= v and i < v + 25
        k.hit = i = v
        Tick wd, g, p, k, lo(), pt()
        if Found%(wd.se, c \\ 3, c mod 3) then exit for
    next
    was = was and Found%(wd.se, c \\ 3, c mod 3)
next
k.right = 0
k.left = 0
k.jump = 0
k.hit = 0
wd.se.gold_lo = hid
wd.se.gold_hi = shown
wd.se.unkept = -1
Tick wd, g, p, k, lo(), pt()
wd.se.ticks = tickWas
if was then print "golds ok"
' the festival night ends in a relay for the big one's guitar: five cats, a bump each, and the third bump no end
' of it. Each goes its own way, Charlie kept behind it: Biscuit steadily; Sprocket by the beat, with a rest;
' Kipper quick and slow in turn; Frost the quicker down a fall and never slower; the twins steadily till he
' nears, then a lob, but none at the street's end: quick, out of a roll's reach while it flies, and no other
' till LOBWAIT is out. Rolled into,
' each takes its bump, throws the guitar on well ahead and is left there off its feet; the fifth leaves the
' guitar lying, the zone's tune back, and it is his to take
dim relayOk as integer, slowest as single, quickest as single, lobs as integer, bar as integer
GoStage wd, 11, g, p, lo(), pt()
g.mode_t = 20
c = wd.mu.z_held
Stand wd.lv, wd.gr, p, wd.lv.chase_x + 4, 0
Tick wd, g, p, k, lo(), pt()
relayOk = wd.lv.cart.st = 1 and wd.lv.cart.hits = 0 and wd.mu.z_held = CHASETUNE + 1 and c <> CHASETUNE + 1
bar = 4 * PULSES \\ wd.se.tempo + 1
for m = 0 to NLEG - 1
    ' a bar of its pace, the street's second fall on its way
    wd.lv.cart.at.x = wd.lv.chase_x + 70 * TS - 350
    wd.lv.cart.at.y = GroundAt%(wd.lv, cint(wd.lv.cart.at.x))
    slowest = 1000
    quickest = 0
    for i = 1 to 60 + bar
        Stand wd.lv, wd.gr, p, cint(wd.lv.cart.at.x) - 150, 0
        Tick wd, g, p, k, lo(), pt()
        if i <= 60 then continue for
        if wd.lv.cart.v < slowest then slowest = wd.lv.cart.v
        if wd.lv.cart.v > quickest then quickest = wd.lv.cart.v
    next
    select case m
    case 0, 4: relayOk = relayOk and abs(slowest - LEGV) < 5 and abs(quickest - LEGV) < 5
    case 1: relayOk = relayOk and slowest < LEGV - 150 and quickest > LEGV + 30
    case 2: relayOk = relayOk and slowest < LEGV - 50 and slowest > LEGV - 150 and quickest > LEGV + 50
    case 3: relayOk = relayOk and slowest > LEGV - 5 and quickest > LEGV + 80
    end select
    if m = 1 then
        ' drawn: the guitar's amber over Sprocket's back, his hard hat, a pip for each cat still to bump
        g.shake = 0
        g.cam.x = wd.lv.cart.at.x - 160
        g.cam.y = wd.lv.cart.at.y - 150
        Render wd, g, p, lo(), pt()
        relayOk = relayOk and point(160 - 6, 150 - 15) = CGOLD + 2 and point(160 + 5, 150 - 11) = CGOLD + 3
        relayOk = relayOk and point(160 - 16, 150 - 38) <> CYELLOW and point(160 - 8, 150 - 38) = CYELLOW
        relayOk = relayOk and point(160 + 16, 150 - 38) = CYELLOW and point(160 + 24, 150 - 38) <> CYELLOW
    end if
    if m = NLEG - 1 then
        ' at the street's end there is nowhere on to lob it
        bx = wd.lv.cart.at.x
        slowest = wd.lv.cart.v
        wd.lv.cart.at.x = wd.lv.chase_end
        Stand wd.lv, wd.gr, p, wd.lv.chase_end - 60, 0
        Tick wd, g, p, k, lo(), pt()
        relayOk = relayOk and not Lobbed%(wd.lv)
        wd.lv.cart.at.x = bx
        wd.lv.cart.v = slowest
        Stand wd.lv, wd.gr, p, cint(wd.lv.cart.at.x) - 60, 0
        Tick wd, g, p, k, lo(), pt()
        relayOk = relayOk and Lobbed%(wd.lv)
        quickest = 0
        for i = 1 to 20
            Stand wd.lv, wd.gr, p, cint(wd.lv.cart.at.x) - 10, 300
            p.roll = -1
            Tick wd, g, p, k, lo(), pt()
            if wd.lv.cart.v > quickest then quickest = wd.lv.cart.v
        next
        relayOk = relayOk and Lobbed%(wd.lv) and wd.lv.cart.hits = m and p.inv <= 0 and quickest > LEGV + 80
        lobs = 0
        v = cint(LOBWAIT * 60)
        for i = 22 to v + 10
            bt = wd.lv.box.t
            Stand wd.lv, wd.gr, p, cint(wd.lv.cart.at.x) - 60, 0
            Tick wd, g, p, k, lo(), pt()
            if wd.lv.box.t > bt then lobs = lobs + 1
            if i = v - 10 then relayOk = relayOk and lobs = 0 and not Lobbed%(wd.lv)
        next
        relayOk = relayOk and lobs = 1
        for i = 1 to 30
            Stand wd.lv, wd.gr, p, cint(wd.lv.cart.at.x) - 150, 0
            Tick wd, g, p, k, lo(), pt()
        next
    end if
    hid = g.score
    Stand wd.lv, wd.gr, p, cint(wd.lv.cart.at.x) - 30, 450
    p.roll = -1
    for i = 1 to 40
        Tick wd, g, p, k, lo(), pt()
        if wd.lv.cart.hits > m then exit for
    next
    relayOk = relayOk and wd.lv.cart.hits = m + 1 and g.score = hid + 500 and wd.lv.cart.t > BOLTS
    relayOk = relayOk and abs(wd.lv.box.at.x - (wd.lv.cart.at.x - TUMBLE)) < 4
    if m < NLEG - 1 then relayOk = relayOk and wd.lv.cart.st = 1
    if m = 1 then
        ' drawn, Charlie thrown back clear of him: Sprocket left there, his hat off his head
        for i = 1 to 20
            Tick wd, g, p, k, lo(), pt()
        next
        g.cam.x = wd.lv.box.at.x - 160
        g.cam.y = wd.lv.box.at.y - 150
        Render wd, g, p, lo(), pt()
        relayOk = relayOk and point(160 + 5, 150 - 15) = CGOLD + 3
    end if
    bx = wd.lv.cart.at.x
    for i = 1 to 90
        Stand wd.lv, wd.gr, p, cint(wd.lv.cart.at.x) - 150, 0
        Tick wd, g, p, k, lo(), pt()
    next
    if m < NLEG - 1 then relayOk = relayOk and wd.lv.cart.at.x - bx > 350
next
relayOk = relayOk and wd.lv.cart.st = 2 and wd.mu.z_held = c
g.cam.x = wd.lv.cart.at.x - 160
g.cam.y = wd.lv.cart.at.y - 150
Render wd, g, p, lo(), pt()
relayOk = relayOk and point(160 + 3, 150 - 6) = CGOLD + 2
hid = g.score
Stand wd.lv, wd.gr, p, cint(wd.lv.cart.at.x), 0
Tick wd, g, p, k, lo(), pt()
relayOk = relayOk and wd.lv.cart.st = 3 and g.score = hid + 1000
if relayOk then print "relay ok"
' Russet Wood is the fifth zone, by name and in its own colours: its litter's top the same picture at both hours,
' russet gold at a misty noon and not on the firefly night
was = ZoneName$(4) = "RUSSET WOOD"
for j = 0 to 1
    GoStage wd, 8 + j, g, p, lo(), pt()
    Tick wd, g, p, k, lo(), pt()
    g.cam.x = 150
    g.shake = 0
    Render wd, g, p, lo(), pt()
    colA(j) = point(100, GroundAt%(wd.lv, 250) - int(g.cam.y))
    if j = 0 then hid = wd.tn.now(colA(0))
    was = was and wd.zn.id = 4
next
if was and colA(0) = colA(1) and hid = &H0B2338 and wd.tn.now(colA(1)) <> hid then print "wood ok"
' the wood's night is dark: its own colours sunk, its fireflies' not, a few of them about; a bark wakes the rest,
' haloed, and lights it; the light fades and GLOWT steps on is gone; a restart in the light finds it dark; its
' noon is never dark. The mist's first tone, 6 7 16 of 63, is 3 4 10 dark and 4 5 13 half lit
GoStage wd, 9, g, p, lo(), pt()
for i = 1 to 5
    Tick wd, g, p, k, lo(), pt()
next
was = wd.lv.murk and wd.tn.which(9, 0) = 96 and wd.tn.now(96) = &H0A0403 and wd.tn.now(CFLY) = &H13392D
for m = 0 to 1
    g.shake = 0
    Render wd, g, p, lo(), pt()
    colA(m) = 0
    ' counted over the lamps' heads: a lamp's jar is of the fireflies' colours, and one in view read as forty
    for i = 0 to 79
        for j = 0 to SW - 1
            if point(j, i) = CFLY or point(j, i) = CFLY + 1 then colA(m) = colA(m) + 1
        next
    next
    if m = 1 then exit for
    p.bark_t = 0
    k.bark = -1
    Tick wd, g, p, k, lo(), pt()
    k.bark = 0
    Tick wd, g, p, k, lo(), pt()
    was = was and wd.tn.now(96) = &H100706
next
was = was and colA(0) >= 1 and colA(0) <= FLYREST and colA(1) > 4 * FLYREST
for i = 2 to GLOWT - GLOWFADE \\ 2
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.tn.now(96) = &H0D0504
for i = 1 to GLOWFADE \\ 2
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.tn.now(96) = &H0A0403
p.bark_t = 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
Tick wd, g, p, k, lo(), pt()
was = was and wd.tn.now(96) = &H100706
GoStage wd, 9, g, p, lo(), pt()
Tick wd, g, p, k, lo(), pt()
was = was and wd.tn.now(96) = &H0A0403
' the pilot stops on a mound whose burrow comes out past a wall it could not jump, digs, and goes on from the
' other end, six tiles up; the next mound, whose burrow leads back, it runs over
dim dugOut as integer
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KMOUND then exit for
next
v = wd.lv.things(c).state - 1
dugOut = c < wd.lv.n_things and v > c
if v < 0 then v = c
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 150, 300
for i = 1 to 200
    Pilot wd.lv, wd.gr, wd.zn, wd.se, p, k, wd.pilot
    Tick wd, g, p, k, lo(), pt()
    if p.ground and p.at.x > wd.lv.things(v).at.x + 40 then exit for
next
dugOut = dugOut and i < 200 and p.ground and abs(p.at.y - wd.lv.things(c).at.y + 6 * TS) < 2
for i = 1 to 400
    Pilot wd.lv, wd.gr, wd.zn, wd.se, p, k, wd.pilot
    Tick wd, g, p, k, lo(), pt()
    dugOut = dugOut and p.dig = 0
    if p.at.x > wd.lv.things(v).at.x + 600 then exit for
next
dugOut = dugOut and i < 400
k.left = 0
k.right = 0
k.down = 0
k.jump = 0
k.hit = 0
if dugOut then print "dig ok"
' the wood's night ends at the great oak, where the Bramble twins lob the Pomchi's ball across a pair of its
' limbs. Come to, the game is on to the chase's tune and the view is higher over him; the left twin holds the
' ball over its limb, then lobs it: halfway it is LOBUP over the straight line, and TWINLOB on in the right
' twin's paws. At the ball held he has not caught it; at it on its way he has: a bump, and none again while it
' tells; then the twins sit on the pair above, the ball quicker across. He stands on a limb, and there catches
' it as it comes by. The third catch and the ball is his, the zone's tune back, the twins sulking
dim twinsOk as integer, twScore as long, twTune as integer
GoStage wd, 9, g, p, lo(), pt()
twTune = wd.mu.z_held
Stand wd.lv, wd.gr, p, wd.lv.chase_x - 40, 0
Tick wd, g, p, k, lo(), pt()
twinsOk = wd.lv.oak.x <> NONE and wd.lv.cart.st = 0 and Headroom%(wd.lv, wd.zn, p) = HEAD
twinsOk = twinsOk and TwinPose%(wd.lv, 0) = 1 and TwinPose%(wd.lv, 1) = 0
Stand wd.lv, wd.gr, p, wd.lv.oak.x - 20, 0
Tick wd, g, p, k, lo(), pt()
twinsOk = twinsOk and wd.lv.cart.st = 1 and wd.lv.box.st = 0 and Headroom%(wd.lv, wd.zn, p) = HEADUP
twinsOk = twinsOk and wd.mu.z_held = CHASETUNE + 1 and twTune <> CHASETUNE + 1
twinsOk = twinsOk and wd.lv.cart.at.x = wd.lv.oak.x - 64 and wd.lv.cart.at.y = wd.lv.oak.y - 3 * TS - PAWUP
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.box.st = 1 then exit for
next
twinsOk = twinsOk and i < 120
for i = 1 to 45
    Tick wd, g, p, k, lo(), pt()
next
twinsOk = twinsOk and abs(wd.lv.cart.at.x - wd.lv.oak.x) < 3 and abs(wd.lv.cart.at.y - (wd.lv.oak.y - 120)) < 2
twinsOk = twinsOk and TwinPose%(wd.lv, 0) = 0 and TwinPose%(wd.lv, 1) = 1
for i = 1 to 47
    Tick wd, g, p, k, lo(), pt()
next
twinsOk = twinsOk and wd.lv.box.st = 2 and wd.lv.cart.at.x = wd.lv.oak.x + 64
twinsOk = twinsOk and wd.lv.cart.at.y = wd.lv.oak.y - 4 * TS - PAWUP
AtBall wd.lv, p
Tick wd, g, p, k, lo(), pt()
twinsOk = twinsOk and wd.lv.cart.hits = 0 and wd.lv.box.st = 2
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.box.st = 3 then exit for
next
for j = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
twScore = g.score
AtBall wd.lv, p
Tick wd, g, p, k, lo(), pt()
twinsOk = twinsOk and i < 60 and wd.lv.cart.hits = 1 and g.score = twScore + 500 and wd.lv.cart.t > 0
twinsOk = twinsOk and TwinPose%(wd.lv, 0) = 3 and TwinPose%(wd.lv, 1) = 3
for j = 0 to 1
    AtBall wd.lv, p
    for i = 1 to 30
        Tick wd, g, p, k, lo(), pt()
    next
    twinsOk = twinsOk and wd.lv.cart.hits = 1 and wd.lv.cart.t > 0
next
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.t <= 0 then exit for
next
twinsOk = twinsOk and i < 60 and wd.lv.box.st = 0 and wd.lv.cart.at.x = wd.lv.oak.x - 112
twinsOk = twinsOk and wd.lv.cart.at.y = wd.lv.oak.y - 6 * TS - PAWUP
Stand wd.lv, wd.gr, p, wd.lv.oak.x - 42, 0
twinsOk = twinsOk and p.at.y = wd.lv.oak.y - 9 * TS and p.sg >= wd.gr.plat_a
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.hits = 2 then exit for
next
twinsOk = twinsOk and i < 200 and p.ground and p.at.y = wd.lv.oak.y - 9 * TS and wd.lv.box.at.x > wd.lv.oak.x - 62
' the third pair's lob is TWINHASTE * 2 the quicker: 66 steps
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.t <= 0 and wd.lv.box.st = 1 then exit for
next
for j = 1 to 100
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.box.st = 2 then exit for
next
twinsOk = twinsOk and i < 200 and abs(j - 66) < 2 and wd.lv.cart.hits = 2 and wd.lv.cart.at.x = wd.lv.oak.x + 64
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.box.st = 3 then exit for
next
for j = 1 to 10
    Tick wd, g, p, k, lo(), pt()
next
AtBall wd.lv, p
Tick wd, g, p, k, lo(), pt()
twinsOk = twinsOk and i < 60 and wd.lv.cart.st = 3 and wd.lv.cart.hits = NBUMP and g.score = twScore + 2500
twinsOk = twinsOk and wd.mu.z_held = twTune and TwinPose%(wd.lv, 0) = 4 and TwinPose%(wd.lv, 1) = 4
' drawn: the oak's bark at its trunk; each twin's mask where it sits on the top pair; the wood's spring a
' toadstool, its cap the wood's berry. The oak is looked at 40 lower on the screen: the HUD's golden-bone pips
' lie over the left twin's place else
g.shake = 0
g.cam.x = wd.lv.oak.x - 160
g.cam.y = wd.lv.oak.y - 230
Render wd, g, p, lo(), pt()
twinsOk = twinsOk and point(160, 130) >= 129 and point(160, 130) <= 133
twinsOk = twinsOk and point(160 - 64 - 6, 230 - 9 * TS - 19) = 79 and point(160 + 64 + 6, 230 - 10 * TS - 19) = 79
g.cam.x = wd.lv.oak.x + 16
g.cam.y = wd.lv.oak.y - 190
Render wd, g, p, lo(), pt()
twinsOk = twinsOk and point(157, 180) = 157
if twinsOk then print "twins ok"
GoStage wd, 8, g, p, lo(), pt()
Tick wd, g, p, k, lo(), pt()
if was and wd.lv.murk = 0 and wd.tn.now(96) = wd.tn.own(8, 0) then print "firefly ok"
' a sapling bowed to the ground throws him who crosses its crown, the higher the faster he came: from a walk over a
' jump's height, from a run 60 higher yet; sprung it stands upright and throws no one, then bows again
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KSAPLING then exit for
next
was = c < wd.lv.n_things
k.right = -1
for m = 0 to 1
    wd.lv.things(c).state = 0
    Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x, 0
    p.gsp = 100 + m * 250
    for i = 1 to 60
        Tick wd, g, p, k, lo(), pt()
        if not p.ground then exit for
    next
    was = was and p.sprung and wd.lv.things(c).state > 0 and abs(p.at.x - wd.lv.things(c).at.x - SAPREACH) < 20
    ' the vine in his way is not for this throw
    p.vine_t = 9
    for i = 1 to 200
        Tick wd, g, p, k, lo(), pt()
        if p.vel.y >= 0 then exit for
    next
    colA(m) = wd.lv.things(c).at.y - cint(p.at.y)
next
k.right = 0
was = was and colA(0) > JUMPV * JUMPV / (2 * GRAV) and colA(1) > colA(0) + 60
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 60, 0
for i = 1 to 2 * SAPT + 4
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.things(c).state = 0
wd.lv.things(c).state = SAPT
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x + SAPREACH, 0
for i = 1 to 4
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.ground and wd.lv.things(c).state > 0
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 60, 0
' its leaves are at its crown's place bowed, and not sprung
for m = 0 to 1
    g.shake = 0
    Render wd, g, p, lo(), pt()
    colB(m) = point(wd.lv.things(c).at.x + SAPREACH + 5 - int(g.cam.x), wd.lv.things(c).at.y - 6 - int(g.cam.y))
    wd.lv.things(c).state = 0
next
if was and (colB(0) < 134 or colB(0) > 137) and colB(1) >= 134 and colB(1) <= 137 then print "sapling ok"
' a vine's end caught in the air: he hangs from it and swings as a weight would, 200 a second at its foot to 58
' degrees, 1.02 of a radian, and back as far, the vine drawn with him; a jump at its foot lets go with the swing's
' speed and a hop; the vine is not caught again at once, and hangs straight again
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KVINE then exit for
next
was = c < wd.lv.n_things
p.vine_t = 0
p.ground = 0
p.at.x = wd.lv.things(c).at.x
p.at.y = wd.lv.things(c).at.y + VINELEN + VINEGRIP
p.vel.x = 200
p.vel.y = 0
colA(0) = 0
colA(1) = 0
colB(0) = 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if p.sway * 100 > colA(0) then colA(0) = p.sway * 100
    if p.sway * 100 < colA(1) then colA(1) = p.sway * 100
    if colB(0) <> 0 or p.sway < .8 then continue for
    g.shake = 0
    Render wd, g, p, lo(), pt()
    m = wd.lv.things(c).at.x + cint(28 * sin(p.sway)) - int(g.cam.x)
    colB(0) = point(m, wd.lv.things(c).at.y + cint(28 * cos(p.sway)) - int(g.cam.y))
    colB(1) = point(wd.lv.things(c).at.x - int(g.cam.x), wd.lv.things(c).at.y + 28 - int(g.cam.y))
next
was = was and p.vine = c + 1 and colA(0) > 97 and colA(0) < 107 and colA(1) < -97 and colA(1) > -107
was = was and colB(0) = CVINE and colB(1) <> CVINE and colB(1) <> CVINED
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if p.sway_v > 0 and abs(p.sway) < .05 then exit for
next
k.hit = -1
Tick wd, g, p, k, lo(), pt()
k.hit = 0
was = was and p.vine = 0 and abs(p.vel.x - 200) < 15 and p.vel.y < -100
for i = 1 to 3
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.vine = 0
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 60, 0
for i = 1 to 80
    Tick wd, g, p, k, lo(), pt()
next
g.shake = 0
g.cam.y = wd.lv.things(c).at.y - 40
Render wd, g, p, lo(), pt()
j = point(wd.lv.things(c).at.x - int(g.cam.x), 68)
if was and wd.lv.things(c).state = 0 and j = CVINE then print "vine ok"
' bramble hurts him who walks into it, his bones lost; curled he goes through it, his bones kept, and it is drawn
' cut, its leaves in the air; BRAMBLET steps on it is grown again and hurts
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KBRAMBLE then exit for
next
was = c < wd.lv.n_things
for m = 0 to 2
    p.inv = 0
    p.stun = 0
    g.bones = 5
    wd.se.part_n = 0
    Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 40, 300
    p.roll = m = 1
    k.right = m <> 1
    for i = 1 to 60
        Tick wd, g, p, k, lo(), pt()
        if g.bones = 0 or p.at.x > wd.lv.things(c).at.x + 24 then exit for
    next
    k.right = 0
    if m <> 1 then was = was and g.bones = 0 and wd.lv.things(c).state = 0
    if m <> 1 then continue for
    was = was and g.bones = 5 and wd.lv.things(c).state > 0 and p.at.x > wd.lv.things(c).at.x and wd.se.part_n > 0
    Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 60, 0
    for j = 0 to 1
        for i = 1 to 4 + j * 2 * BRAMBLET
            Tick wd, g, p, k, lo(), pt()
        next
        g.shake = 0
        Render wd, g, p, lo(), pt()
        colB(j) = point(wd.lv.things(c).at.x - 19 + 22 - int(g.cam.x), wd.lv.things(c).at.y - 25 + 7 - int(g.cam.y))
    next
    ' a thorn's pale point is there grown, and not cut
    was = was and wd.lv.things(c).state = 0 and colB(0) <> 125 and colB(1) = 125
next
if was then print "bramble ok"
' a hollow log stops him on foot at its end, and a jump clears it, as does a throw over it; curled he is shot
' through it and leaves it at LOGV, none of his fur seen while he is in it
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KLOG then exit for
next
was = c < wd.lv.n_things
p.inv = 0
p.stun = 0
' the bones the bramble knocked loose are white as he is: gone before his fur is looked for
for i = 0 to NLOOSE - 1
    lo(i).t = 0
next
wd.se.loose_n = 0
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 60, 0
k.right = -1
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
next
was = was and abs(p.at.x - (wd.lv.things(c).at.x - LOGR)) < 2 and p.ground
k.hit = -1
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
    k.hit = 0
    if p.ground and p.at.x > wd.lv.things(c).at.x + LOGR then exit for
next
was = was and p.ground and p.at.x > wd.lv.things(c).at.x + LOGR
k.right = 0
' thrown over it uncurled he is not stopped in the air
p.ground = 0
p.roll = 0
p.at.x = wd.lv.things(c).at.x - LOGR - 20
p.at.y = wd.lv.things(c).at.y - LOGH - 40
p.vel.x = 200
p.vel.y = -150
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.at.x > wd.lv.things(c).at.x
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 60, 200
p.roll = -1
colA(0) = 0
colA(1) = 0
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
    if p.at.x > wd.lv.things(c).at.x + LOGR + 2 then exit for
    if abs(p.at.x - wd.lv.things(c).at.x) >= 8 then continue for
    g.shake = 0
    Render wd, g, p, lo(), pt()
    colA(0) = Fur%(cint(p.at.x) - int(g.cam.x), 14, cint(p.at.y) - 26 - int(g.cam.y), cint(p.at.y) - int(g.cam.y))
    colA(1) = 1
next
was = was and colA(1) = 1 and colA(0) = 0 and p.at.x > wd.lv.things(c).at.x + LOGR and p.roll
if was and abs(p.gsp - LOGV) < 30 then print "log ok"
' a burrow's mound leaves him be who runs over it; crouched on it he digs in, and goes DIGV for its other mound
' unseen, the bones and the bramble over his way and a hit nothing to him; there he is thrown up DIGPOP, and it
' does not take him back while he stays on it; stepped off and on again it does, to the first
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KMOUND then exit for
next
was = c < wd.lv.n_things
v = wd.lv.things(c).state - 1
if v < 0 then v = c
was = was and v > c and wd.lv.things(v).kind = KMOUND and wd.lv.things(v).state = c + 1 _
    and wd.lv.things(v).at.x = wd.lv.things(c).at.x + 224
p.inv = 0
p.stun = 0
p.roll = 0
p.dug = 0
k.down = 0
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 40, 0
k.right = -1
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
    was = was and p.dig = 0
    if p.at.x > wd.lv.things(c).at.x + MOUNDR + 2 then exit for
next
k.right = 0
was = was and p.at.x > wd.lv.things(c).at.x + MOUNDR and p.at.x < wd.lv.things(c).at.x + 20
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 40, 0
g.shake = 0
Render wd, g, p, lo(), pt()
j = wd.lv.things(c).at.x - int(g.cam.x) - 1
m = wd.lv.things(c).at.y - int(g.cam.y)
was = was and point(j, m - 3) = 137 and point(j, m - 9) = 123
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x + 3, 0
k.down = -1
Tick wd, g, p, k, lo(), pt()
was = was and p.dig = v + 1
bx = p.at.x
m = g.bones
for i = 1 to 15
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.dig = v + 1 and abs(p.at.x - bx - 15 * DIGV / 60) < 1
Hurt wd.mu, wd.se, g, p, 0, lo(), SHURT
g.shake = 0
Render wd, g, p, lo(), pt()
was = was and Fur%(cint(p.at.x) - int(g.cam.x), 14, cint(p.at.y) - 26 - int(g.cam.y), cint(p.at.y) - int(g.cam.y)) = 0
for i = 16 to 60
    Tick wd, g, p, k, lo(), pt()
    if p.dig = 0 then exit for
next
' 224 pixels at 400 a second: 33 steps, and out on the 34th
was = was and i = 34 and p.at.x = wd.lv.things(v).at.x and g.bones = m and p.stun = 0 and not p.dead
by = p.at.y
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if p.at.y < by then by = p.at.y
    was = was and p.dig = 0
next
' thrown up at 260 against 750: 45 pixels
was = was and abs(wd.lv.things(v).at.y - by - 45) < 4 and p.ground and abs(p.at.x - wd.lv.things(v).at.x) < 2
k.down = 0
Stand wd.lv, wd.gr, p, wd.lv.things(v).at.x + MOUNDR + 4, 0
Tick wd, g, p, k, lo(), pt()
Stand wd.lv, wd.gr, p, wd.lv.things(v).at.x - 2, 0
k.down = -1
for i = 1 to 80
    Tick wd, g, p, k, lo(), pt()
    if p.dug = c + 1 then exit for
next
k.down = 0
if was and p.dug = c + 1 and p.at.x = wd.lv.things(c).at.x then print "burrow ok"
' a squirrel stands on its bough, and keeps over him who is under it, turned his way; SQWAIT of the things' steps
' on, the last SQAIM of them with an acorn held out, it lets one fall: 52 pixels to his head, 22 steps, and it
' hurts; run under, he is gone before it lands; a bark cows it, still and without acorns; on its bough he is
' scratched by it on foot, and beats it curled
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KSQUIRREL then exit for
next
v = wd.lv.things(c).home
was = c < wd.lv.n_things and wd.lv.things(c).state <> 0 and GroundAt%(wd.lv, v) = wd.lv.things(c).at.y
was = was and GroundAt%(wd.lv, v + 100) = wd.lv.things(c).at.y + 80
p.inv = 0
p.stun = 0
p.roll = 0
k.down = 0
StandUnder wd.lv, wd.gr, p, v + 30, v + 100
Camera wd.lv, wd.zn, g, p, -1
if wd.se.ticks and 1 then Tick wd, g, p, k, lo(), pt()
wd.lv.things(c).at.x = v
wd.lv.things(c).state = SQWAIT
g.bones = 5
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
next
g.shake = 0
Render wd, g, p, lo(), pt()
j = wd.lv.things(c).at.x - int(g.cam.x)
m = wd.lv.things(c).at.y - int(g.cam.y)
was = was and abs(wd.lv.things(c).at.x - v - 30) < SQV and wd.lv.things(c).state = SQWAIT - 20
was = was and point(j - 4, m - 13) = 160 and point(j + 6, m - 5) <> 116
for i = 41 to 119
    Tick wd, g, p, k, lo(), pt()
next
g.shake = 0
Render wd, g, p, lo(), pt()
was = was and point(j + 6, m - 5) = 116 and wd.lv.n_lobs = 0 and p.stun = 0
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.n_lobs = 1 and wd.lv.things(c).state = SQWAIT
for i = 1 to 21
    Tick wd, g, p, k, lo(), pt()
next
g.shake = 0
Render wd, g, p, lo(), pt()
j = cint(wd.lv.lobs(0).at.x) - int(g.cam.x) - 1
was = was and wd.lv.n_lobs = 1 and p.stun = 0 and g.bones = 5
was = was and point(j, cint(wd.lv.lobs(0).at.y) - int(g.cam.y) - 2) = 161
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.n_lobs = 0 and p.stun <> 0 and g.bones = 0 and not p.dead
' it bursts in its own colour
m = 0
for i = 0 to NPART - 1
    if pt(i).t > 0 and pt(i).c = CNUT then m = m + 1
next
was = was and m = 6
' run under it as it holds one out, it falls once he is near, and he is past before it lands
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
p.inv = 0
p.stun = 0
g.bones = 5
StandUnder wd.lv, wd.gr, p, v - 130, v + 100
p.gsp = TOP
wd.lv.things(c).at.x = v - SQRUN
wd.lv.things(c).state = -2
k.right = -1
bx = NONE
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.n_lobs > 0 and bx = NONE then bx = p.at.x - wd.lv.things(c).at.x
next
k.right = 0
was = was and abs(bx) < SQNEAR and wd.lv.n_lobs = 0 and p.stun = 0 and g.bones >= 5 and _
    p.at.x > v + 60 and g.mode = PLAYING
' a bark under it: it keeps still, and nothing falls
StandUnder wd.lv, wd.gr, p, v + 30, v + 100
p.gsp = 0
p.bark_t = 0
wd.lv.things(c).at.x = v
wd.lv.things(c).state = SQWAIT
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
was = was and abs(wd.lv.things(c).state) > SQWAIT
for i = 1 to 100
    Tick wd, g, p, k, lo(), pt()
    was = was and wd.lv.things(c).at.x = v and wd.lv.n_lobs = 0
next
' on its bough: on foot it scratches him, curled he beats it
for j = 0 to 1
    wd.lv.things(c).at.x = v
    wd.lv.things(c).state = SQWAIT
    p.inv = 0
    p.stun = 0
    g.bones = 5
    hid = g.score
    Stand wd.lv, wd.gr, p, v - 40, 200
    p.roll = -j
    for i = 1 to 20
        Tick wd, g, p, k, lo(), pt()
        if p.stun or wd.lv.things(c).state = 0 then exit for
    next
    colA(j) = (p.stun <> 0) + 2 * (wd.lv.things(c).state = 0) + 4 * (g.score = hid + 100)
next
if was and colA(0) = -1 and colA(1) = -6 then print "squirrel ok"
' a swarm of bees keeps at home while he is far, drawn there; near it he wakes it, and it is on him and stings
' on the 15th step, 53 pixels at BEEV; a walk does not lose it, a run has by the 50th step, BEELOSE behind, and it
' goes home; so it does when he starts again
was = wd.lv.n_swarms > 0
v = wd.lv.swarms(0).home.x
m = wd.lv.swarms(0).home.y
was = was and GroundAt%(wd.lv, v) = m + 48
p.inv = 0
p.stun = 0
p.roll = 0
Stand wd.lv, wd.gr, p, v - 100, 0
Camera wd.lv, wd.zn, g, p, -1
Mend wd.lv
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.swarms(0).st = 0 and wd.lv.swarms(0).at.x = v and wd.lv.swarms(0).at.y = m
g.shake = 0
Render wd, g, p, lo(), pt()
c = 0
for j = m - int(g.cam.y) - 12 to m - int(g.cam.y) + 12
    for i = v - int(g.cam.x) - 18 to v - int(g.cam.x) + 18
        if point(i, j) = CBEE then c = c + 1
    next
next
was = was and c >= NBEE * 2
g.bones = 5
Stand wd.lv, wd.gr, p, v - 40, 0
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
    if p.stun then exit for
next
was = was and i = 15 and g.bones = 0 and wd.lv.swarms(0).st = 1
' walked from, it keeps up
p.stun = 0
Stand wd.lv, wd.gr, p, v - 40, 0
wd.lv.swarms(0).at.x = p.at.x
wd.lv.swarms(0).at.y = p.at.y - 13
for i = 1 to 60
    p.inv = 9
    p.gsp = 100
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.swarms(0).st = 1 and abs(wd.lv.swarms(0).at.x - p.at.x) < 8 and p.at.x > v
' run from, it is lost
wd.lv.swarms(0).at.x = p.at.x
k.right = -1
for i = 1 to 70
    p.inv = 9
    p.gsp = TOP
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.swarms(0).st = 2 then exit for
next
k.right = 0
was = was and i = 50
bx = wd.lv.swarms(0).at.x
Stand wd.lv, wd.gr, p, cint(p.at.x), 0
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
next
was = was and bx > v + 100 and wd.lv.swarms(0).st = 0 and wd.lv.swarms(0).at.x = v and wd.lv.swarms(0).at.y = m
wd.lv.swarms(0).st = 1
wd.lv.swarms(0).at.x = v + 50
Spawn wd.lv, wd.gr, wd.zn, wd.mu, g, p
if was and wd.lv.swarms(0).st = 0 and wd.lv.swarms(0).at.x = v then print "bees ok"
' the pilot, curled still past what he rolled through and slower than a run, hops out of the roll: left in it
' he was a crawl by the bees, and stung to death
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KSQUIRREL then exit for
next
p.inv = 0
p.stun = 0
Stand wd.lv, wd.gr, p, wd.lv.things(c).home + 80, 250
p.roll = -1
for i = 1 to 65
    Pilot wd.lv, wd.gr, wd.zn, wd.se, p, k, wd.pilot
    Tick wd, g, p, k, lo(), pt()
next
k.left = 0
k.right = 0
k.down = 0
k.jump = 0
k.hit = 0
if p.roll = 0 and p.ground and p.gsp >= 250 then print "uncurl ok"
' the pass's bare ice keeps his speed and will not give it back: let go at a run he slides on where snow stops
' him, held back he is long in stopping, and from a stand he is slow away; it is drawn bare, the snow's white gone
GoStage wd, 6, g, p, lo(), pt()
was = FlatRun%(wd.lv, -1) <> NONE and FlatRun%(wd.lv, 0) <> NONE
for j = 0 to 1
    c = FlatRun%(wd.lv, -j)
    Stand wd.lv, wd.gr, p, c * TS + 8, 200
    for i = 1 to 40
        Tick wd, g, p, k, lo(), pt()
    next
    colA(j) = cint(p.gsp)
    Stand wd.lv, wd.gr, p, c * TS + 8, 200
    k.left = -1
    for i = 1 to 10
        Tick wd, g, p, k, lo(), pt()
    next
    k.left = 0
    colB(j) = cint(p.gsp)
    Stand wd.lv, wd.gr, p, c * TS + 8, 0
    k.right = -1
    for i = 1 to 30
        Tick wd, g, p, k, lo(), pt()
    next
    k.right = 0
    colA(2 + j) = cint(p.gsp)
    g.cam.x = c * TS - 32
    g.shake = 0
    Render wd, g, p, lo(), pt()
    v = 0
    for i = 32 to 287
        if wd.tn.now(point(i, wd.lv.top(c) * TS - int(g.cam.y))) = &H3F3F3F then v = v + 1
    next
    colB(2 + j) = v
next
was = was and colA(0) < 40 and colA(1) > 160 and colB(0) < 20 and colB(1) > 140
was = was and colA(2) > 120 and colA(3) > 30 and colA(3) < 80
if was and colB(2) > 180 and colB(3) < 100 then print "ice ok"
' the pass's sled: walked onto, he is off on it at SLEDV; it lies as the ground does, level on the flat and its
' nose down a fall, where it gathers speed with no key held; a key against it is no brake; a hop keeps him on it;
' deep snow ends the ride, he unhurt; a restart puts the sled back; hurt, he is off it
GoStage wd, 6, g, p, lo(), pt()
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KSLED then exit for
next
for m = c to wd.lv.n_things - 1
    if wd.lv.things(m).kind = KDRIFT then exit for
next
was = c < wd.lv.n_things and m < wd.lv.n_things
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 16, 40
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
    if p.sled then exit for
next
was = was and p.sled and wd.lv.things(c).state = 1 and p.gsp >= SLEDV and Fall%(wd, g, p, lo(), pt(), 59, -10, 8) = 0
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if p.ground and p.ang < -.3 then exit for
next
v = cint(p.gsp)
for i = 1 to 12
    Tick wd, g, p, k, lo(), pt()
next
j = Fall%(wd, g, p, lo(), pt(), 59, -10, 8)
was = was and p.ground and p.ang < -.3 and p.gsp > v + 60 and j > 6 and j < 12
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if p.ground and abs(p.ang) < .05 then exit for
next
v = cint(p.gsp)
k.left = -1
for i = 1 to 15
    Tick wd, g, p, k, lo(), pt()
next
k.left = 0
was = was and p.sled and p.gsp > v - 15
k.hit = -1
k.jump = -1
Tick wd, g, p, k, lo(), pt()
k.hit = 0
was = was and not p.ground
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if p.ground then exit for
next
k.jump = 0
was = was and p.sled and p.roll
for i = 1 to 600
    Tick wd, g, p, k, lo(), pt()
    if not p.sled then exit for
next
was = was and i < 600 and abs(p.at.x - wd.lv.things(m).at.x) < 40 and p.inv <= 0 and p.ground and _
    not p.roll and wd.lv.things(c).state = 1
Spawn wd.lv, wd.gr, wd.zn, wd.mu, g, p
was = was and wd.lv.things(c).state = 0
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 16, 40
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
g.bones = 5
was = was and p.sled
Hurt wd.mu, wd.se, g, p, cint(p.at.x) + 10, lo(), SHURT
Tick wd, g, p, k, lo(), pt()
if was and not p.sled and p.inv > 0 then print "sled ok"
' in the pass a jet is a geyser: its column the pass's water and not the works' steam; who is over it while it
' blows goes up, and stays down while it sleeps
GoStage wd, 6, g, p, lo(), pt()
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KJET then exit for
next
was = c < wd.lv.n_things
for j = 0 to 1
    do until (wd.se.ticks + wd.lv.things(c).at.x) mod 180 = 10 + j * 90
        wd.se.ticks = wd.se.ticks + 1
    loop
    Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x, 0
    p.vel.y = 0
    Tick wd, g, p, k, lo(), pt()
    was = was and (not p.ground) = (j = 0)
    g.cam.x = wd.lv.things(c).at.x - 100
    g.shake = 0
    Render wd, g, p, lo(), pt()
    colA(j) = point(100, wd.lv.things(c).at.y - 34 - int(g.cam.y))
next
if was and colA(0) >= 116 and colA(0) <= 126 and colA(1) <> colA(0) then print "geyser ok"
' deep snow rolled through at speed packs round him, and not walked through nor rolled through slowly: a snowball,
' none of him seen in it; it takes a hit for him, his bones kept, and flies apart; it lasts no longer than his roll
GoStage wd, 7, g, p, lo(), pt()
for m = 0 to wd.lv.n_things - 1
    if wd.lv.things(m).kind = KDRIFT then exit for
next
was = m < wd.lv.n_things
' till the act's card is off the screen
for i = 1 to 240
    Tick wd, g, p, k, lo(), pt()
next
for j = 0 to 2
    Stand wd.lv, wd.gr, p, wd.lv.things(m).at.x - 40, 200 - 140 * (j and 1)
    if j then p.roll = -1
    for i = 1 to 30
        Tick wd, g, p, k, lo(), pt()
        if p.snow then exit for
    next
    was = was and (p.snow <> 0) = (j = 2)
next
g.shake = 0
Render wd, g, p, lo(), pt()
c = cint(p.at.x) - int(g.cam.x)
v = cint(p.at.y) - int(g.cam.y)
was = was and Fur%(c, 14, v - 28, v) = 0 and point(c, v - 14) >= 116 and point(c, v - 14) <= 120
' rolled on out of the snow, it is his still
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.snow < 0 and p.at.x > wd.lv.things(m).at.x + 30
g.bones = 5
p.inv = 0
Hurt wd.mu, wd.se, g, p, cint(p.at.x) + 10, lo(), SHURT
was = was and g.bones = 5 and p.inv > 0 and p.roll
Tick wd, g, p, k, lo(), pt()
was = was and p.snow = 0
p.inv = 0
Stand wd.lv, wd.gr, p, wd.lv.things(m).at.x - 40, 200
p.roll = -1
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
    if p.snow then exit for
next
was = was and p.snow <> 0
p.roll = 0
Tick wd, g, p, k, lo(), pt()
if was and p.snow = 0 then print "snowball ok"
' the pass's snow-thrower: ducked, its wall throws a roll back, neither hurt; its snowball comes down where
' Charlie stood as it was thrown, hurts him there still and misses him stepped aside; up, it is drawn up and a roll
' beats it; a bark ducks it
GoStage wd, 6, g, p, lo(), pt()
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KTHROW then exit for
next
was = c < wd.lv.n_things
for i = 1 to 240
    Tick wd, g, p, k, lo(), pt()
next
wd.lv.things(c).state = THROWC
g.bones = 5
hid = g.score
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 18, 200
p.roll = -1
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.things(c).state <> 0 and p.inv <= 0 and p.gsp < 0 and g.score = hid
for j = 0 to 1
    g.bones = 5
    p.inv = 0
    p.stun = 0
    p.roll = 0
    wd.lv.things(c).state = THROWAT + 1
    Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 100, 0
    for i = 1 to 4
        Tick wd, g, p, k, lo(), pt()
        if wd.lv.n_lobs then exit for
    next
    was = was and wd.lv.n_lobs = 1
    ' a snowball, as it is drawn
    g.shake = 0
    Render wd, g, p, lo(), pt()
    v = cint(wd.lv.lobs(0).at.x) - int(g.cam.x) - 1
    was = was and point(v, cint(wd.lv.lobs(0).at.y) - int(g.cam.y) - 2) = 116
    if j then Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 160, 0
    for i = 1 to 70
        Tick wd, g, p, k, lo(), pt()
        if wd.lv.n_lobs = 0 then exit for
    next
    was = was and i > 40 and i < 60 and (p.inv > 0) = (j = 0) and (g.bones = 0) = (j = 0)
next
p.inv = 0
p.stun = 0
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 60, 0
g.shake = 0
for j = 0 to 1
    wd.lv.things(c).state = THROWC - j * (THROWC - THROWUP)
    Render wd, g, p, lo(), pt()
    y = wd.lv.things(c).at.y - int(g.cam.y)
    shown = Pix&(wd.lv.things(c).at.x - int(g.cam.x) - 14, 28, y - 25, y - 12)
    if j = 0 then bt = shown
next
was = was and shown <> bt
hid = g.score
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 18, 200
p.roll = -1
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.things(c).state = 0 and g.score = hid + 100
wd.lv.things(c).state = THROWUP - 2
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 40, 0
p.bark_t = 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
if was and abs(wd.lv.things(c).state) > THROWUP then print "thrower ok"
' the pass's icicle hangs while he is off to its side; a bark from there looses it, and it breaks on the ground;
' stood under, he looses it: it trembles TREMBLE of the things' steps, falls, hurts him and breaks; over a
' snow-thrower, a bark brings it down on it
GoStage wd, 6, g, p, lo(), pt()
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KICICLE then exit for
next
was = c < wd.lv.n_things
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 60, 0
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.things(c).state = 0
p.bark_t = 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
was = was and wd.lv.things(c).state > 0
v = 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if Fallen%(wd.lv, c) > v then v = Fallen%(wd.lv, c)
    if wd.lv.things(c).state < 0 then exit for
next
was = was and i < 120 and p.inv <= 0 and v > 60
wd.lv.things(c).state = 0
g.bones = 5
p.at.x = wd.lv.things(c).at.x
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.things(c).state > 0 and Fallen%(wd.lv, c) = 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.things(c).state > 0 and wd.lv.things(c).state <= TREMBLE and Fallen%(wd.lv, c) <> 0 then was = 0
    if p.inv > 0 then exit for
next
was = was and i > TREMBLE * 2 and i < TREMBLE * 2 + 40 and wd.lv.things(c).state = -1 and g.bones = 0
GoStage wd, 7, g, p, lo(), pt()
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KICICLE then exit for
next
for m = 0 to wd.lv.n_things - 1
    if wd.lv.things(m).kind = KTHROW and abs(wd.lv.things(m).at.x - wd.lv.things(c).at.x) < 14 then exit for
next
was = was and c < wd.lv.n_things and m < wd.lv.n_things
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 60, 0
p.bark_t = 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.things(c).state < 0 then exit for
next
if was and i < 120 and wd.lv.things(m).state = 0 and p.inv <= 0 then print "icicle ok"
' the dusk's loaded ledge holds while he stands by it; a bark from there sets it off: it trembles, slides off its
' ledge and comes down on the snow-thrower past it, which is beaten; its heap stops him on foot at either flank,
' and a jump takes him over it; the level begun again, the load is back on its ledge. The heap was the load
' moved: met from its left alone, and left on the road when the level began again
GoStage wd, 7, g, p, lo(), pt()
wd.lv.gale = 0
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KLOAD then exit for
next
for m = c to wd.lv.n_things - 1
    if wd.lv.things(m).kind = KTHROW then exit for
next
j = HeapOf%(wd.lv, c)
was = c < wd.lv.n_things and m < wd.lv.n_things and j <> NONE
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 60, 0
g.bones = 5
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.things(c).state = 0 and wd.lv.things(j).state = 0 and wd.lv.things(m).state <> 0
p.bark_t = 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
was = was and wd.lv.things(c).state > 0
v = 0
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.things(c).state = TREMBLE + LOADSLIDE then v = Slid%(wd.lv, c)
    if wd.lv.things(c).state < 0 then exit for
next
was = was and i < 200 and v = LOADRUN and wd.lv.things(j).state = 1 and wd.lv.things(m).state = 0
was = was and abs(wd.lv.things(j).at.x - wd.lv.things(m).at.x) < HEAPR
for i = 1 to 150
    Tick wd, g, p, k, lo(), pt()
next
Stand wd.lv, wd.gr, p, wd.lv.things(j).at.x - 70, 0
p.inv = 0
p.stun = 0
k.right = -1
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.ground and abs(p.at.x - (wd.lv.things(j).at.x - HEAPR)) < 2 and abs(p.gsp) < 20
k.hit = -1
k.jump = -1
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
    if p.ground then k.jump = 0
next
k.right = 0
was = was and p.at.x > wd.lv.things(j).at.x + HEAPR
Stand wd.lv, wd.gr, p, wd.lv.things(j).at.x + 70, 0
k.left = -1
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
next
k.left = 0
was = was and p.ground and abs(p.at.x - (wd.lv.things(j).at.x + HEAPR)) < 2
GoStage wd, 7, g, p, lo(), pt()
if was and wd.lv.things(c).state = 0 and wd.lv.things(j).state = 0 _
    and wd.lv.things(c).at.y < wd.lv.things(j).at.y - 80 then print "slide ok"
' a crab pinches at its side, a rolling Charlie too; down on its back he goes up off it unhurt and it ducks into
' its shell, and comes out again
GoStage wd, 4, g, p, lo(), pt()
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KCRAB then exit for
next
was = c < wd.lv.n_things
for j = 0 to 1
    g.bones = 5
    p.inv = 0
    p.stun = 0
    Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 16, 200
    if j then p.roll = -1
    Tick wd, g, p, k, lo(), pt()
    was = was and p.inv > 0 and abs(wd.lv.things(c).state) = 1
next
for j = 0 to 1
    ' barked at the second time round, it keeps to its shell: no pinch at its side, and a higher step
    p.inv = 0
    p.stun = 0
    g.bones = 5
    if j then
        wd.lv.things(c).state = 1
        p.bark_t = 0
        Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 40, 0
        k.bark = -1
        Tick wd, g, p, k, lo(), pt()
        k.bark = 0
        was = was and abs(wd.lv.things(c).state) > SHELLT - 4
        Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 12, 0
        Tick wd, g, p, k, lo(), pt()
        was = was and p.inv <= 0
    end if
    p.at.x = wd.lv.things(c).at.x
    p.at.y = wd.lv.things(c).at.y - 30
    p.ground = 0
    p.vel.x = 0
    p.vel.y = 100
    for i = 1 to 20
        Tick wd, g, p, k, lo(), pt()
        if p.vel.y < 0 then exit for
    next
    was = was and p.inv <= 0 and abs(wd.lv.things(c).state) > 1
    if j = 0 then was = was and abs(p.vel.y + BOUNCEV) < 30
    if j = 1 then was = was and abs(p.vel.y + STEPV) < 30
next
' in its shell a roll knocks it off the sand; out of it again after SHELLT of its steps
wd.lv.things(c).state = SHELLT
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 100, 0
for i = 1 to SHELLT * 2 + 4
    Tick wd, g, p, k, lo(), pt()
next
was = was and abs(wd.lv.things(c).state) = 1
wd.lv.things(c).state = SHELLT
hid = g.score
p.inv = 0
p.stun = 0
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 16, 200
p.roll = -1
Tick wd, g, p, k, lo(), pt()
if was and wd.lv.things(c).state = 0 and g.score = hid + 100 and p.inv <= 0 then print "crab ok"
' a jellyfish floats at the sea's top, and lies on the ground where the sea is lower
GoStage wd, 4, g, p, lo(), pt()
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KJELLY then exit for
next
v = wd.lv.top(wd.lv.things(c).at.x \\ TS) * TS
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 70, 0
for j = 0 to 1
    wd.lv.tide_lo = v - 40 + 80 * j
    wd.lv.tide_hi = wd.lv.tide_lo
    for i = 1 to 4
        Tick wd, g, p, k, lo(), pt()
    next
    if j = 0 then was = c < wd.lv.n_things and wd.lv.things(c).at.y = wd.lv.tide + JELLYD
    if j = 1 then was = was and wd.lv.things(c).at.y = v
next
' his feet in it, it stings; his feet over it, as on a boat's deck, it does not: he was stung through the deck
g.bones = 5
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 8, 0
Tick wd, g, p, k, lo(), pt()
was = was and p.inv > 0
for j = 0 to 2
    ' over it and going up; down onto its bell, thrown high and unhurt; in it while it is flat from that, no
    ' sting: he was stung coming back down off a low ceiling
    p.inv = 0
    p.stun = 0
    p.at.x = wd.lv.things(c).at.x
    p.at.y = v - 16 + 24 * (j = 1) - 12 * (j = 2)
    p.ground = 0
    p.vel.x = 0
    p.vel.y = -50 - 150 * (j = 1)
    g.bones = 5
    for i = 1 to 12
        Tick wd, g, p, k, lo(), pt()
        if j <> 1 or p.vel.y < 0 then exit for
    next
    was = was and p.inv <= 0
    if j = 1 then was = was and abs(p.vel.y + JELLYV) < 30 and wd.lv.things(c).state > 1
next
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 70, 0
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
if was and wd.lv.things(c).state = 1 then print "jelly ok"
' a swimmer it stings is thrown out of the sea, and back in it paddles again: hurt into the sea, he floated
' there for good, the keys dead
wd.lv.tide_lo = v - 40
wd.lv.tide_hi = wd.lv.tide_lo
for i = 1 to 4
    Tick wd, g, p, k, lo(), pt()
next
p.inv = 0
g.bones = 5
p.at.x = wd.lv.things(c).at.x - 8
p.at.y = wd.lv.tide + AFLOAT
p.ground = 0
p.swim = -1
p.vel.x = 0
Tick wd, g, p, k, lo(), pt()
was = p.inv > 0 and p.swim = 0 and p.stun
p.at.x = wd.lv.things(c).at.x + 20
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if p.swim then exit for
next
j = cint(p.at.x)
k.right = -1
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
next
k.right = 0
if was and p.swim and p.at.x - j > 15 then print "dazed ok"
' a buoy rides the sea's top, and bumps Charlie off it
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KBUOY then exit for
next
was = c < wd.lv.n_things
p.swim = 0
p.inv = 0
p.stun = 0
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 90, 0
for j = 0 to 1
    wd.lv.tide_lo = p.at.y + 30 + 20 * j
    wd.lv.tide_hi = wd.lv.tide_lo
    for i = 1 to 4
        Tick wd, g, p, k, lo(), pt()
    next
    was = was and wd.lv.things(c).at.y = wd.lv.tide - 4
next
p.at.x = wd.lv.things(c).at.x
p.at.y = wd.lv.things(c).at.y - 24
p.ground = 0
p.vel.x = 0
p.vel.y = 100
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
    if p.vel.y < 0 then exit for
next
if was and p.vel.y < -300 and wd.lv.things(c).state > 0 and not p.swim then print "buoy ok"
' a gull hovers about its home while Charlie is far off; once he stands under it with bones it dives, takes
' five, and climbs home with them, to dive no more
GoStage wd, 4, g, p, lo(), pt()
c = wd.lv.gulls(0).home.x
g.bones = 8
Stand wd.lv, wd.gr, p, c - 300, 0
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = wd.lv.n_gulls > 0 and wd.lv.gulls(0).st = GHOVER and abs(wd.lv.gulls(0).at.x - c) <= 24
was = was and abs(wd.lv.gulls(0).at.y - wd.lv.gulls(0).home.y) <= 4
Stand wd.lv, wd.gr, p, c, 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.gulls(0).loot then exit for
next
was = was and wd.lv.gulls(0).loot = 5 and g.bones = 3 and wd.lv.gulls(0).st = GCLIMB and p.inv <= 0
for i = 1 to 300
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.gulls(0).st = GHOVER and wd.lv.gulls(0).loot = 5 and g.bones = 3
' a bark makes it drop them, loose
p.bark_t = 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
was = was and wd.lv.gulls(0).loot = 0 and wd.se.loose_n = 5
' a Charlie with no bones it leaves alone
Stand wd.lv, wd.gr, p, c - 300, 0
for i = 1 to 260
    Tick wd, g, p, k, lo(), pt()
next
g.bones = 0
Stand wd.lv, wd.gr, p, c, 0
for i = 1 to 150
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.gulls(0).st <> GHOVER then was = 0
next
' and a rolling Charlie knocks it out of the air
hid = g.score
p.ground = 0
p.vel.y = -100
p.roll = -1
wd.lv.gulls(0).st = GDIVE
wd.lv.gulls(0).at.x = p.at.x
wd.lv.gulls(0).at.y = p.at.y - 13
wd.lv.gulls(0).aim.x = p.at.x
wd.lv.gulls(0).aim.y = p.at.y - 13
Tick wd, g, p, k, lo(), pt()
if was and wd.lv.gulls(0).st = 0 and g.score = hid + 100 then print "gull ok"
' Kipper's pedal boat waits on the sea in its channel; Charlie come to it, it goes to the channel's end and
' turns round
wd.lv.cart.st = 0
GoStage wd, 5, g, p, lo(), pt()
' its channel's buoys out of the way of these landings
for i = 0 to wd.lv.n_things - 1
    if wd.lv.things(i).kind = KBUOY and wd.lv.things(i).at.x > wd.lv.chase_x then wd.lv.things(i).kind = 0
next
Tick wd, g, p, k, lo(), pt()
v = wd.lv.cart.st = 0 and wd.lv.cart.at.y = wd.lv.tide - PEDALUP
p.at.x = wd.lv.chase_x + 30
p.at.y = wd.lv.tide + AFLOAT
p.ground = 0
p.swim = -1
p.vel.x = 0
for i = 1 to 10
    Tick wd, g, p, k, lo(), pt()
next
was = v and wd.lv.cart.st = 1 and wd.lv.cart.at.y = wd.lv.tide - PEDALUP
was = was and wd.lv.cart.at.x > wd.lv.chase_x + CARTLEAD and wd.lv.cart.v = PEDALV
for i = 1 to 600
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.v < 0 then exit for
next
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.cart.v = -PEDALV and abs(wd.lv.cart.at.x - (wd.lv.chase_end - PEDALW \\ 2)) < 8
' a swimmer at its side is no bump; Charlie down on its deck out of the air is: it turns round, he goes up off
' it, and while it is dazed a second landing is none. Each bump makes it faster
p.at.x = wd.lv.cart.at.x
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.cart.hits = 0 and p.inv <= 0
hid = g.score
for j = 1 to 3
    for c = 0 to 1
        p.swim = 0
        p.ground = 0
        p.at.x = wd.lv.cart.at.x
        p.at.y = wd.lv.cart.at.y - 8
        p.vel.x = 0
        p.vel.y = 100
        for i = 1 to 12
            Tick wd, g, p, k, lo(), pt()
            if p.vel.y < 0 or p.swim then exit for
        next
        if c = 0 then was = was and p.vel.y < 0
    next
    was = was and wd.lv.cart.hits = j
    if j = 1 then was = was and wd.lv.cart.v > 0 and g.score = hid + 500
    p.at.x = wd.lv.chase_x + 30
    for i = 1 to 90
        Tick wd, g, p, k, lo(), pt()
    next
    if j < 3 then was = was and abs(wd.lv.cart.v) = PEDALV + j * PEDALMORE
next
' its third bump sinks it, and the teddy is his as he comes down there; afloat on the sea, it waits for a
' swimmer to come to it
was = was and wd.lv.cart.st = 3
wd.lv.cart.st = 2
wd.lv.cart.at.x = p.at.x + 60
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.cart.st = 2 and wd.lv.cart.at.y = wd.lv.tide
hid = g.score
p.at.x = wd.lv.cart.at.x
Tick wd, g, p, k, lo(), pt()
if was and wd.lv.cart.st = 3 and g.score = hid + 1000 then print "pedalo ok"
was = 0
' a lift goes up under Charlie to the ground above, and back down once it is left
GoStage wd, 2, g, p, lo(), pt()
for m = 0 to wd.lv.n_movers - 1
    if wd.lv.movers(m).kind = 2 then exit for
next
c = wd.lv.movers(m).sg
p.at.x = wd.gr.pt(c).at.x + 20
p.at.y = wd.gr.pt(c).at.y
p.sg = c
p.ground = -1
p.gsp = 0
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
next
was = wd.lv.movers(m).t = wd.lv.movers(m).reach and p.ground _
    and abs(p.at.y - (wd.lv.movers(m).at.y - wd.lv.movers(m).reach)) < 2
k.right = -1
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
k.right = 0
was = was and p.ground and wd.gr.pt(p.sg).kind = 1 and p.at.x > wd.lv.movers(m).at.x + MOVEW
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
next
if was and wd.lv.movers(m).t = 0 then print "lift ok"
' a basket goes down its cable under Charlie, to the ground past the pit
GoStage wd, 2, g, p, lo(), pt()
for m = 0 to wd.lv.n_movers - 1
    if wd.lv.movers(m).kind = 3 then exit for
next
c = wd.lv.movers(m).sg
p.at.x = wd.gr.pt(c).at.x + 24
p.at.y = wd.gr.pt(c).at.y
p.sg = c
p.ground = -1
p.gsp = 0
for i = 1 to 300
    Tick wd, g, p, k, lo(), pt()
next
was = p.ground and p.sg = c and abs(p.at.x - (wd.lv.movers(m).at.x + wd.lv.movers(m).reach + 24)) < 2
if was and abs(p.at.y - (wd.lv.movers(m).at.y + CABLED)) < 2 then print "cable ok"
' the pass's goat and ice wall: the wall stops him on foot. The goat grazes with him 92 off, further than it sees;
' with him 50 off it lowers its head 24 ticks where it stands, then charges 8 a step of two ticks and hurts him
' where he stood, 36 to 38 ticks on. Jumped as it charges, it stops under him; he comes down on its back and it
' bolts on along the road, no rein taken, through the wall, drawn as the ground lies, level on the flat and
' falling on a fall; two seconds on he is off it, and it runs home to graze
GoStage wd, 6, g, p, lo(), pt()
for m = 0 to wd.lv.n_things - 1
    if wd.lv.things(m).kind = KGOAT then exit for
next
for c = 0 to wd.lv.n_things - 1
    if wd.lv.things(c).kind = KWALL then exit for
next
was = m < wd.lv.n_things and c < wd.lv.n_things
Stand wd.lv, wd.gr, p, wd.lv.things(c).at.x - 60, 200
k.right = -1
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
k.right = 0
was = was and wd.lv.things(c).state = 1 and p.at.x <= wd.lv.things(c).at.x - WALLR and p.gsp = 0
Stand wd.lv, wd.gr, p, wd.lv.things(m).at.x - 92, 0
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
was = was and abs(wd.lv.things(m).state) = 1 and wd.lv.things(m).at.x = wd.lv.things(m).home
g.bones = 5
Stand wd.lv, wd.gr, p, wd.lv.things(m).at.x - 50, 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if i <= 24 and wd.lv.things(m).at.x <> wd.lv.things(m).home then was = 0
    if p.inv > 0 then exit for
next
was = was and i > 33 and i < 41 and g.bones = 0
GoStage wd, 6, g, p, lo(), pt()
g.bones = 5
Stand wd.lv, wd.gr, p, wd.lv.things(m).at.x - 50, 0
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
    if abs(wd.lv.things(m).state) = GCHARGE then exit for
next
k.hit = -1
k.jump = -1
for i = 1 to 80
    Tick wd, g, p, k, lo(), pt()
    if p.goat then exit for
next
k.jump = 0
was = was and p.goat and wd.lv.things(m).state = 0 and p.inv <= 0 and g.bones = 5 _
    and abs(p.at.x - (wd.lv.things(m).home - 50)) < 12
k.left = -1
v = NONE
j = NONE
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if not p.goat then exit for
    if p.ground and p.gsp < 200 then was = 0
    if i = 30 then v = Fall%(wd, g, p, lo(), pt(), 132, -8, -2)
    if j = NONE and p.ground and p.at.y > wd.lv.things(m).at.y + 8 then j = Fall%(wd, g, p, lo(), pt(), 132, -8, -2)
next
k.left = 0
was = was and i > 114 and i < 124 and wd.lv.things(c).state = 0 and v = 0 and j = 3 _
    and p.at.x > wd.lv.things(c).at.x + 100
for i = 1 to 300
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.stray.st = 0 then exit for
next
if was and i < 300 and abs(wd.lv.things(m).state) = 1 _
    and wd.lv.things(m).at.x = wd.lv.things(m).home then print "goat ok"
' the dusk's blizzard, none on the morning: of every 192 ticks it blows the last 72. In its calm he stands where
' he is. In a gust, on his feet, 600 against his friction's 280 bears him back to 200 a second: 165 to 190 by
' its end; crouched he keeps his place; curled at 200 he rolls on 88 to 97 in half a second, as in a calm; a jump
' begun as it starts is carried back 150 to 175 by its landing, 900 to 200 a second. Its snow streams as it blows
GoStage wd, 6, g, p, lo(), pt()
was = wd.lv.gale = 0
GoStage wd, 7, g, p, lo(), pt()
for m = 0 to wd.lv.n_things - 1
    if wd.lv.things(m).kind = KTHROW then exit for
next
wd.lv.things(m).state = 0
j = wd.lv.things(m).at.x - 120
was = was and wd.lv.gale and m < wd.lv.n_things
wd.se.ticks = 1920
Stand wd.lv, wd.gr, p, j, 0
for i = 1 to 100
    Tick wd, g, p, k, lo(), pt()
next
was = was and abs(p.at.x - j) < 1 and Lull%(wd.lv, wd.se) = 20
Render wd, g, p, lo(), pt()
hid = 0
for i = 0 to 39
    for c = 0 to SW - 1
        if point(c, i) = CWHITE then hid = hid - 1
    next
next
wd.se.ticks = 2040
Stand wd.lv, wd.gr, p, j, 0
for i = 1 to 72
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.at.x - j < -165 and p.at.x - j > -190 and Lull%(wd.lv, wd.se) = 120
wd.se.ticks = 2040
Stand wd.lv, wd.gr, p, j, 0
k.down = -1
for i = 1 to 36
    Tick wd, g, p, k, lo(), pt()
next
Render wd, g, p, lo(), pt()
for i = 0 to 39
    for c = 0 to SW - 1
        if point(c, i) = CWHITE then hid = hid + 1
    next
next
for i = 1 to 36
    Tick wd, g, p, k, lo(), pt()
next
was = was and abs(p.at.x - j) < 1 and hid > 20
wd.se.ticks = 2040
Stand wd.lv, wd.gr, p, j - 150, 200
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
k.down = 0
was = was and p.roll and p.at.x - (j - 150) > 88 and p.at.x - (j - 150) < 97
wd.se.ticks = 2040
Stand wd.lv, wd.gr, p, j, 0
k.hit = -1
k.jump = -1
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
    if p.ground and i > 5 then exit for
next
k.jump = 0
was = was and i > 50 and i < 57 and p.at.x - j < -150 and p.at.x - j > -175
' and it is over with its level
GoStage wd, 6, g, p, lo(), pt()
if was and wd.lv.gale = 0 then print "gale ok"
' the dusk's avalanche, none on the morning: it waits while he is short of its head. Stood 10 past it he is taken
' 45 ticks on, its front come from 220 behind its head at 300 a second; 30 ticks on, 80 behind him, a spot 30
' behind its front and 30 over the ground is under its snow, which it was not before. He starts again before it,
' a life the fewer, and it waits again
GoStage wd, 6, g, p, lo(), pt()
was = wd.lv.aval_x = NONE
GoStage wd, 7, g, p, lo(), pt()
was = was and wd.lv.aval_x <> NONE
wd.se.ticks = 1920
Stand wd.lv, wd.gr, p, wd.lv.aval_x - 40, 0
k.down = -1
' till the act's card is off the view, which the spot read below may lie under
for i = 1 to 260
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.aval.st = 0 and not p.dead
wd.se.ticks = 1920
Stand wd.lv, wd.gr, p, wd.lv.aval_x + 10, 0
p.at.x = wd.lv.aval_x + 10
j = g.lives
g.shake = 0
Render wd, g, p, lo(), pt()
v = point(wd.lv.aval_x - 100 - int(g.cam.x), cint(p.at.y) - 30 - int(g.cam.y))
for i = 1 to 80
    Tick wd, g, p, k, lo(), pt()
    if p.dead then exit for
    if i = 30 then
        g.shake = 0
        Render wd, g, p, lo(), pt()
        c = point(wd.lv.aval_x - 100 - int(g.cam.x), cint(p.at.y) - 30 - int(g.cam.y))
    end if
next
' the snow's tones are 116 to 118
was = was and i >= 44 and i <= 46 and (v < 116 or v > 118) and c >= 116 and c <= 118
for i = 1 to 400
    Tick wd, g, p, k, lo(), pt()
    if not p.dead then exit for
next
k.down = 0
if was and i < 400 and g.lives = j - 1 and wd.lv.aval.st = 0 and p.at.x < wd.lv.aval_x then print "avalanche ok"
' the pass's chase: Frost's toboggan is drawn as the ground lies, its slats level on the flat and, from 10 behind
' her middle to 8 before it, 9 lower on a fall of one in two, as Charlie's sled is. On his sled he bumps her and
' is checked to a sled's pace, not thrown; her last bump leaves the camera, and reaching it he has it
GoStage wd, 7, g, p, lo(), pt()
wd.se.ticks = 1920
Stand wd.lv, wd.gr, p, wd.lv.chase_x + 180, 0
k.down = -1
for i = 1 to 150
    Tick wd, g, p, k, lo(), pt()
next
was = wd.lv.cart.st = 1
for j = 0 to 1
    wd.lv.cart.at.x = wd.lv.chase_x + 230 - j * 118
    wd.lv.cart.at.y = Surf%(wd.lv, cint(wd.lv.cart.at.x), cint(p.at.y) + 8)
    g.shake = 0
    Render wd, g, p, lo(), pt()
    v = NONE
    c = NONE
    for i = cint(wd.lv.cart.at.y) - 40 - int(g.cam.y) to cint(wd.lv.cart.at.y) + 20 - int(g.cam.y)
        if i >= 0 and i < SH then
            if point(cint(wd.lv.cart.at.x) - 10 - int(g.cam.x), i) = 59 then v = i
            if point(cint(wd.lv.cart.at.x) + 8 - int(g.cam.x), i) = 59 then c = i
        end if
    next
    was = was and v <> NONE and c - v = j * 9
next
k.down = 0
wd.lv.cart.at.x = p.at.x + 60
wd.lv.cart.at.y = p.at.y
wd.lv.cart.v = 0
p.sled = -1
p.roll = -1
p.gsp = 400
hid = g.score
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.hits then exit for
next
was = was and wd.lv.cart.hits = 1 and wd.lv.cart.t > BOLTS and p.sled and p.ground and abs(p.gsp - SLEDV) < 10
wd.lv.cart.hits = NBUMP - 1
wd.lv.cart.t = 0
wd.lv.cart.v = 0
wd.lv.cart.at.x = p.at.x + 60
p.gsp = 300
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.st = 2 then exit for
next
was = was and wd.lv.cart.st = 2 and wd.lv.cart.hits = NBUMP
p.sled = 0
p.roll = 0
Stand wd.lv, wd.gr, p, cint(wd.lv.cart.at.x), 0
for i = 1 to 150
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.st = 3 then exit for
next
if was and wd.lv.cart.st = 3 and g.score >= hid + 2000 then print "toboggan ok"
wd.lv.cart.st = 0
' the pass's ski lift: its chair takes him up its cable steadily, to the ground CABLED higher past the pit in
' 1 / SEATV seconds, and goes back down once he is off it. He rides and waits crouched: the dusk's gusts would
' have him off it
GoStage wd, 7, g, p, lo(), pt()
for m = 0 to wd.lv.n_movers - 1
    if wd.lv.movers(m).kind = 7 then exit for
next
was = m < wd.lv.n_movers
c = wd.lv.movers(m).sg
p.at.x = wd.gr.pt(c).at.x + 24
p.at.y = wd.gr.pt(c).at.y
p.sg = c
p.ground = -1
p.gsp = 0
k.down = -1
for i = 1 to 400
    Tick wd, g, p, k, lo(), pt()
    if i = 86 then was = was and abs(p.at.y - (wd.lv.movers(m).at.y - CABLED \\ 2)) < 3
    if wd.lv.movers(m).t = 1 then exit for
next
was = was and i >= 171 and i <= 174 and p.ground and p.sg = c _
    and abs(p.at.x - (wd.lv.movers(m).at.x + wd.lv.movers(m).reach + 24)) < 2
was = was and abs(p.at.y - (wd.lv.movers(m).at.y - CABLED)) < 2 and wd.lv.things(wd.lv.movers(m).thing).kind = KSEAT
k.down = 0
k.right = -1
wd.se.ticks = 1920
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
next
k.right = 0
k.down = -1
was = was and p.ground and wd.gr.pt(p.sg).kind = 1 and abs(p.at.y - (wd.lv.movers(m).at.y - CABLED)) < 2
was = was and p.at.x > wd.lv.movers(m).at.x + wd.lv.movers(m).reach + MOVEW
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
next
k.down = 0
if was and wd.lv.movers(m).t = 0 then print "chair ok"
' a button trodden raises its bridge out of the pit, and the bridge carries
GoStage wd, 2, g, p, lo(), pt()
for m = 0 to wd.lv.n_movers - 1
    if wd.lv.movers(m).kind = 4 then exit for
next
j = wd.lv.movers(m).button
c = wd.lv.movers(m).sg
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
was = wd.lv.movers(m).t = 0 and wd.lv.things(j).state = 0
Stand wd.lv, wd.gr, p, wd.lv.things(j).at.x - 12, 60
k.right = -1
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
k.right = 0
was = was and wd.lv.things(j).state = 1
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
next
p.at.x = wd.gr.pt(c).at.x + 24
p.at.y = wd.gr.pt(c).at.y
p.sg = c
p.ground = -1
p.gsp = 0
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.movers(m).t = wd.lv.movers(m).reach and p.ground and p.sg = c
if was and abs(p.at.y - (wd.lv.movers(m).at.y - wd.lv.movers(m).reach)) < 2 then print "bridge ok"
' the Amber Downs at evening played through by the pilot on one life; after its family and its fade the Chrome
' Works, their platforms and lifts ridden, the works by night, then Saltwick Sands at noon and at sunset: each
' cleared on that life
GoStage wd, 1, g, p, lo(), pt()
j = g.lives
was = 0
dim rams as integer
for m = 1 to NSTAGE - 1
    for i = 1 to 3000
        if wd.zn.stage = m and g.mode = PLAYING then exit for
        Advance wd, g, p, k, lo(), pt(), cu()
    next
    rams = 0
    for i = 1 to 16000
        Pilot wd.lv, wd.gr, wd.zn, wd.se, p, k, wd.pilot
        v = wd.lv.cart.hits
        Tick wd, g, p, k, lo(), pt()
        if wd.lv.cart.hits > v and p.sled then rams = rams + 1
        if wd.lv.cart.st = 1 and wd.mu.z_held = CHASETUNE + 1 and wd.lv.cart.at.x > wd.lv.chase_x + 600 then was = -1
        if g.mode <> PLAYING then exit for
    next
    ' the Downs' evening ends in a chase: the cart fled down the race to the chase's tune, was bumped till it broke,
    ' and its keepsake taken, the zone's tune back
    if m = 1 and was and wd.lv.cart.st = 3 and wd.lv.cart.hits = NBUMP and wd.mu.z_held = 8 then print "chase ok"
    ' and the sands' sunset: the pilot hops out of the sea onto Kipper's boat till it sinks, and takes the teddy
    if m = 5 and wd.lv.cart.st = 3 and wd.lv.cart.hits = NBUMP then print "kipper ok"
    ' and the pass's dusk: Frost bumped off her toboggan, twice at the least sled against sled, and the camera
    ' taken. Checked by a bump, his sled never caught her up again: one bump of the three was made on it
    if m = 7 and wd.lv.cart.st = 3 and wd.lv.cart.hits = NBUMP and rams >= 2 then print "frost ok"
    ' and the festival night: each of the relay's five cats bumped, and the guitar taken
    if m = 11 and wd.lv.cart.st = 3 and wd.lv.cart.hits = NLEG then print "relay won ok"
    print "act"; m; g.mode; g.lives - j; wd.zn.stage; wd.zn.id; cint(p.at.x)
    k.left = 0
    k.right = 0
    k.down = 0
    k.jump = 0
    k.hit = 0
next
' and after the last, the title, on the Amber Downs again
for i = 1 to 1600
    Tick wd, g, p, k, lo(), pt()
    if g.mode = TITLE then exit for
next
if g.mode = TITLE and wd.zn.id = 0 and wd.tn.dusk = 16 and wd.mu.z_held = 1 then print "home ok"
' the Amber Downs in the shared colours again: a zone's own are put back when it is left
if zp and wd.tn.now(wd.tn.which(1, 0)) = wd.tn.day(wd.tn.which(1, 0)) then print "palette ok"
' the title's own run ends the same way: it began again the moment Charlie passed the goal, the family never met
for i = 1 to 9000
    Tick wd, g, p, k, lo(), pt()
    if p.at.x >= wd.lv.goal_x then exit for
next
for i = 1 to 300
    Tick wd, g, p, k, lo(), pt()
next
was = g.mode = TITLE and p.win and wd.se.cheer and abs(p.at.x - (wd.lv.fam_x - 106)) < 3
for i = 1 to 900
    Tick wd, g, p, k, lo(), pt()
    if p.at.x < wd.lv.goal_x then exit for
next
if was and p.at.x < wd.lv.goal_x and wd.tn.dusk = 16 then print "title met ok"
' progress is kept at each zone: the last, reached in the game above, is in CHARLIE.SAV and read back; a file that
' is not the game's counts for nothing; a zone reached is written. On the title Right picks a zone reached and no
' further, and a game starts at its first act
wd.se.reached = 0
LoadSave wd.se
was = wd.se.reached = ZONES - 1
open "CHARLIE.SAV" for binary as #2
hid = 12345
put #2, , hid
close #2
LoadSave wd.se
was = was and wd.se.reached = 0
Reach wd.se, 1
wd.se.reached = 0
LoadSave wd.se
was = was and wd.se.reached = 1 and wd.se.pick = 0
k.right = -1
Tick wd, g, p, k, lo(), pt()
Tick wd, g, p, k, lo(), pt()
k.right = 0
Tick wd, g, p, k, lo(), pt()
k.right = -1
Tick wd, g, p, k, lo(), pt()
k.right = 0
was = was and wd.se.pick = 1 and g.mode = TITLE
k.start = -1
Tick wd, g, p, k, lo(), pt()
if was and g.mode = PLAYING and wd.zn.stage = 2 and g.lives = 3 then print "save ok"
end

' 900 frames at hz a second, not in step with the clock, their times read off it at grain moves a second, the first
' 300 to settle: the
' frames whose steps are not as they should be (per > 0: one step every per frames, exactly; most: never more in a
' frame), and whether the steps in all are more than 3 from want.
function Uneven% (hz as single, grain as integer, per as integer, most as integer, want as integer)
    dim c as Clock, i as long, n as integer, bad as integer, tot as long, t as double, was as double, since as integer
    c.span = STEPT
    for i = 1 to 900
        t = int((i / cdbl(hz) + .0031) * grain) / cdbl(grain)
        Pace csng(t - was), c
        was = t
        n = Steps%(c)
        if i > 300 then
            tot = tot + n
            if n > most then bad = bad + 1
            if per then
                since = since + 1
                if n then
                    if since <> per and tot > n then bad = bad + 1
                    since = 0
                end if
            end if
        end if
    next
    if abs(tot - want) > 3 then bad = bad + 1
    Uneven% = bad
end function

' Charlie in the air and still, his middle at the twins' ball.
sub AtBall (lv as Level, p as Hero)
    p.at.x = lv.cart.at.x
    p.at.y = lv.cart.at.y + 5
    p.ground = 0
    p.vel.x = 0
    p.vel.y = 0
    p.jumped = 0
end sub

' What a half bar's PLAY string lasts, in forty-eighths of a bar: each L or P its length, a dotted one half as
' long again.
function HalfLen% (x as string)
    dim i as integer, n as integer, total as integer, one as integer, ch as string
    i = 1
    do while i <= len(x)
        ch = mid$(x, i, 1)
        i = i + 1
        if ch <> "L" and ch <> "P" then continue do
        n = val(mid$(x, i, 2))
        i = i + len(ltrim$(str$(n)))
        one = 48 \\ n
        ' a note's dot is after its number
        if ch = "L" and mid$(x, i, 1) = "N" then i = i + 1 + len(ltrim$(str$(val(mid$(x, i + 1, 2)))))
        if mid$(x, i, 1) = "." then one = one + one \\ 2
        total = total + one
    loop
    HalfLen% = total
end function

' Whether a PLAY string has a triplet's length in it.
function Tripled% (x as string)
    Tripled% = instr(x, "L12") > 0 or instr(x, "L24") > 0 or instr(x, "P12") > 0 or instr(x, "P24") > 0
end function

' How many of a PLAY string's notes are a step from the note before them, two semitones at most; wide: how many
' are further.
function Moves% (x as string, wide as integer)
    dim i as integer, n as integer, last as integer, count as integer
    i = instr(x, "N")
    do while i > 0
        n = val(mid$(x, i + 1, 2))
        if last > 0 and n <> last and (abs(n - last) > 2) = wide then count = count + 1
        last = n
        i = instr(i + 1, x, "N")
    loop
    Moves% = count
end function

' Whether each voice of each of a song's half bars lasts half a bar.
function SongLasts% (sg as Song)
    dim i as integer, h as integer, nBars as integer
    for i = 1 to len(sg.order)
        if asc(mid$(sg.order, i, 1)) - 96 > nBars then nBars = asc(mid$(sg.order, i, 1)) - 96
    next
    SongLasts% = 0
    for h = 0 to nBars * 2 - 1
        if HalfLen%(sg.half(h).bass) <> 24 or HalfLen%(sg.half(h).one) <> 24 then exit function
        if HalfLen%(sg.half(h).lead) <> 24 or HalfLen%(sg.half(h).two) <> 24 then exit function
        if HalfLen%(sg.half(h).under) <> 24 or HalfLen%(sg.half(h).third) <> 24 then exit function
    next
    SongLasts% = -1
end function

' Charlie on the tiles' ground at x, running at v.
' Charlie stood at x under a shelf, on the ground as high as it is at x0, which the shelf is not over.
sub StandUnder (lv as Level, gr as Ground, p as Hero, x as integer, x0 as integer)
    Stand lv, gr, p, x0, 0
    p.at.x = x
    p.sg = SegAt%(lv, gr, x, p.at.y)
end sub

sub Stand (lv as Level, gr as Ground, p as Hero, x as integer, v as single)
    p.at.x = x
    p.at.y = GroundAt%(lv, x)
    p.gsp = v
    p.ground = -1
    p.roll = 0
    p.sg = SegAt%(lv, gr, x, p.at.y)
    p.ang = gr.pt(p.sg).rise
end sub

' How many pixels of Charlie's fur the screen holds, w either side of x from y0 to y1.
function Fur% (x as integer, w as integer, y0 as integer, y1 as integer)
    dim i as integer, j as integer, n as integer
    for j = y0 to y1
        for i = x - w to x + w
            if point(i, j) >= 8 and point(i, j) <= 13 then n = n + 1
        next
    next
    Fur% = n
end function

' The screen's pixels from x, w wide, from y0 to y1, summed by place: the same for the same picture.
function Pix& (x as integer, w as integer, y0 as integer, y1 as integer)
    dim i as integer, j as integer, n as long
    for j = y0 to y1
        for i = x to x + w - 1
            n = n + point(i, j) * ((i + j * 7) mod 31 + 1&)
        next
    next
    Pix& = n
end function

' Column x of the screen, kept in a.
sub KeepCol (x as integer, a() as integer)
    dim y as integer
    for y = 0 to SH - 1
        a(y) = point(x, y)
    next
end sub

' The lowest row where column x of the screen is not as a kept it, -1 where it all is.
function Lowest% (x as integer, a() as integer)
    dim y as integer
    Lowest% = -1
    for y = 0 to SH - 1
        if point(x, y) <> a(y) then Lowest% = y
    next
end function

' The last cue of a kind that is on at beat bt, -1 when none is.
function OnCue% (cu() as Cue, kind as integer, bt as single)
    dim i as integer
    OnCue% = -1
    do while cu(i).kind
        if cu(i).kind = kind and bt >= cu(i).t0 and bt < cu(i).t1 then OnCue% = i
        i = i + 1
    loop
end function

' The ground's top under x, NONE over a gap.
function GroundAt% (lv as Level, x as integer)
    dim y as integer
    GroundAt% = NONE
    for y = 16 to MH * TS - 17
        if Solid%(lv, x, y) then
            GroundAt% = y
            exit function
        end if
    next
end function

' How much lower what he rides is drawn at column b from Charlie's middle than at column a: the lowest row of its
' tone at each, within 40 rows over his feet; 0 level. NONE with none seen, or where the scene without him has
' the tone there too and the measure would read that.
function Fall% (wd as World, g as Game, p as Hero, lo() as Loose, pt() as Part, hue%, a%, b%)
    dim y as integer, ya as integer, yb as integer, sx as integer, sy as integer, pass as integer, was as single
    Fall% = NONE
    g.shake = 0
    sx = cint(p.at.x) - int(g.cam.x)
    sy = cint(p.at.y) - int(g.cam.y)
    for pass = 0 to 1
        ' first with him blinked out, as when he has been hurt
        was = p.inv
        if pass = 0 then
            p.inv = 1
            wd.se.ticks = wd.se.ticks + 4 - (wd.se.ticks and 4)
        end if
        Render wd, g, p, lo(), pt()
        if pass = 0 then wd.se.ticks = wd.se.ticks - 4
        p.inv = was
        ya = NONE
        yb = NONE
        for y = sy - 40 to sy + 20
            if y >= 0 and y < SH then
                if point(sx + a, y) = hue then ya = y
                if point(sx + b, y) = hue then yb = y
            end if
        next
        if pass = 0 and (ya <> NONE or yb <> NONE) then exit function
    next
    if ya <> NONE and yb <> NONE then Fall% = yb - ya
end function

' The first of 16 level columns in a row, bare ice if ice, under snow if not; NONE without them.
function FlatRun% (lv as Level, ice as integer)
    dim c as integer, n as integer
    FlatRun% = NONE
    for c = 9 to lv.w - 1
        if lv.vis(c) >= 1 and lv.vis(c) <= 2 and (lv.glaze(c) <> 0) = (ice <> 0) and lv.top(c) = lv.top(c - 1) then
            n = n + 1
        else
            n = 0
        end if
        if n = 16 then
            FlatRun% = c - 15
            exit function
        end if
    next
end function
"""


def game_files() -> dict[Path, str]:
    """The game's modules and its main program, by path."""
    mods = sorted(p for d in ("barkley", "engine") for p in (DEMO.parent / d).rglob("*.bas"))
    return {p: p.read_text() for p in [*mods, DEMO]}


def outside(files: dict[Path, str]) -> list[str]:
    """Each field assigned outside its record's owner, as module:line record.field."""
    prog = basic_src.Program(files)
    out = []
    for mod in prog.modules:
        where = mod.path.relative_to(DEMO.parent).with_suffix("").as_posix().removeprefix("barkley/")
        for st in mod.stmts:
            for ref in basic_src.assigned(prog, mod, st):
                if not ref.fields and not (st.routine and ref.base in mod.routines[st.routine].params):
                    continue
                # the record the field is of, else the one that holds it, back along the chain
                held = [ref.base_type] + [f[2] for f in ref.fields]
                rec = next((r for r in reversed(held) if r in OWNERS), held[-1])
                if rec not in OWNERS:
                    sys.exit(f"charlie: {where}: {rec} has no owner in charlie_test.py's OWNERS")
                own = OWNERS[rec]
                if where != own and not where.startswith(own + "/"):
                    name = ref.fields[-1][1] if ref.fields else ref.base
                    out.append(f"{where}:{st.toks[0].line} {rec}.{name}")
    return out


def owners_kept() -> int:
    """The count of fields assigned outside their owner, which is OUTSIDE: lowered as it falls. The count is
    checked on planted assignments: three outside their owner, one in a one-line IF and one to a field's
    field, and three inside. A Vec's x was once owned by the last record to hold a Vec, the camera's by
    Charlie's: renaming the camera a Vec raised the count by five."""
    files = game_files()
    got = len(outside(files))
    play = DEMO.parent / "barkley/stage/play.bas"
    moves = DEMO.parent / "barkley/stage/hero/moves.bas"
    states = DEMO.parent / "barkley/flow/states.bas"
    three = "    p = fresh\n    p.gsp = 0\n    if p.dig then p.gsp = 0\n    p.at.x = 0\n"
    planted = {**files, play: files[play].replace("    p = fresh\n", three, 1)}
    inside = {
        **files,
        moves: files[moves].replace("    p.gsp = 0\n", "    p.gsp = 0\n    p.gsp = 0\n    p.at.x = 0\n", 1),
        states: files[states].replace("    g.lives = 3\n", "    g.lives = 3\n    g.cam.x = 0\n", 1),
    }
    if len(outside(planted)) != got + 3 or len(outside(inside)) != got:
        sys.exit("charlie: the count of fields assigned outside their owner misses a planted one")
    if got > OUTSIDE:
        sys.exit(f"charlie: {got} fields assigned outside their record's owner, more than {OUTSIDE}")
    if got < OUTSIDE:
        sys.exit(f"charlie: {got} fields assigned outside their record's owner: lower OUTSIDE to it")
    return got


def pix(p: charlieart.Pic) -> int:
    """A picture summed by place as the driver's Pix& sums the screen."""
    return sum(c * ((x + y * 7) % 31 + 1) for y, row in enumerate(p.p) for x, c in enumerate(row))


def refused(kind: str, text: str) -> bool:
    """Whether the art tool will not pack this text as its levels ("LEVELS") or its songs ("SONG")."""
    kept = getattr(charlieart, kind)
    with tempfile.TemporaryDirectory() as tmp:
        setattr(charlieart, kind, Path(tmp) / "in.txt")
        getattr(charlieart, kind).write_text(text)
        try:
            (charlieart.levels if kind == "LEVELS" else charlieart.song)()
        except SystemExit:
            return True
        finally:
            setattr(charlieart, kind, kept)
    return False


def main() -> None:
    exe = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build/host/basic").resolve()
    kept = owners_kept()
    src = DEMO.read_text()
    assert "\nMain wd\nend\n" in src, "CHARLIE.BAS's main call moved"
    art = subprocess.run([sys.executable, ROOT / "tools/charlieart.py", "--check"], capture_output=True, text=True)
    if art.returncode:
        sys.exit(f"charlie: the art tool does not run, or CHARLIE.DAT is not what it writes: {art.stderr[-300:]}")
    # a level's command the game has no case for it would pass over, and a level without its end it would read past:
    # the art tool packs neither
    lv = charlieart.LEVELS.read_text()
    if (
        refused("LEVELS", lv)
        or not refused("LEVELS", lv.replace("boost,0", "bosst,0", 1))
        or not refused("LEVELS", lv.replace(", end,0", "", 1))
    ):
        sys.exit("charlie: the art tool packs a level with a command misspelt or without its end, or none")
    # a sound PLAY refuses stops the game where its song first plays: the art tool packs no song with one
    sg = charlieart.SONG.read_text()
    if refused("SONG", sg) or not refused("SONG", sg.replace('"W4H0,6,4,5Y0K2,10"', '"W4H0,6,4Y0K2,10"', 1)):
        sys.exit("charlie: the art tool packs a song with a sound PLAY refuses, or none")
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "T.BAS"
        path.write_text(src.replace("\nMain wd\nend\n", "\n" + DRIVER, 1))
        shutil.copy(DEMO.with_suffix(".DAT"), tmp)
        for package in ("barkley", "engine"):
            shutil.copytree(DEMO.parent / package, Path(tmp) / package)
        run = subprocess.run([exe, path.name], cwd=tmp, capture_output=True, timeout=300)
    out = run.stdout.decode(errors="replace") + run.stderr.decode(errors="replace")
    if "edge ok" not in out:
        sys.exit(f"charlie: a cat at the level's left end: {out[-300:]}")
    if "sounds ok" not in out:
        sys.exit(f"charlie: a bar of the tune or a sound effect: {out[-300:]}")
    if "held ok" not in out:
        sys.exit(f"charlie: jump held through a hit, and no jump as it ends: {out[-300:]}")
    if "spill ok" not in out:
        sys.exit(f"charlie: loose bones and dust still in the air five seconds on: {out[-300:]}")
    if "met ok" not in out:
        sys.exit(f"charlie: the level cleared, Charlie not stopped at its family, all of them in view: {out[-300:]}")
    for said, what in (
        ("paced 60 ok", "a step for each frame of a 60 Hz screen, by a coarse clock"),
        ("paced 120 ok", "a step every other frame of a 120 Hz screen"),
        ("paced by time ok", "the game's speed kept at 75 Hz and with no screen"),
        ("slope ok", "a ramp run onto at any speed without a stop, and stood on without a slide"),
        ("buffer ok", "a jump pressed just before landing, made on landing"),
        ("edge jump ok", "a jump pressed just after running off an edge, made"),
        ("ledge ok", "fallen from an edge walked off slowly"),
        ("drop ok", "landed wherever dropped over the ground"),
        ("loop ok", "round a loop when fast, back out of it when slow"),
        ("stall ok", "off a loop's top when too slow for it"),
        ("ghost ok", "down on the ground from a jump before a loop"),
        ("pipe ok", "up a quarter pipe and its wall onto the ground above"),
        ("wave ok", "over a ribbon's humps and off its far end onto the ground"),
        ("twist ok", "at a ribbon's crest Charlie is drawn under its line, upside down"),
        ("turn ok", "a quarter round a ribbon's twist Charlie is drawn across its line, seen from over his back"),
        ("hid ok", "three quarters round a ribbon's twist Charlie is behind it, none of him drawn"),
        ("caught ok", "the level's end played by its own tune, once"),
        ("intro tune ok", "the intro's own tune in the zone's slot, 28 bars that end"),
        ("intro ok", "the intro played through by its script, scene by scene, to the title on the Amber Downs"),
        ("skip ok", "the intro skipped by a key"),
        ("door ok", "at the van's door, before he jumps, Charlie is behind the van's side and shows past its door"),
        ("bark ok", "a cat barked at stands startled a while, then goes on away from Charlie"),
        ("slow ok", "too slow for a ribbon's twist, Charlie drops from its crest to the pit under it"),
        ("carried ok", "carried by the platform stood on"),
        ("lift ok", "taken up by a lift, which goes back down when left"),
        ("cable ok", "taken down a cable by its basket"),
        ("chair ok", "taken up a ski lift's cable by its chair, which goes back down when left"),
        ("bridge ok", "a bridge raised by its button, to stand on"),
        ("works tune ok", "the Chrome Works played to their own tune"),
        ("sands tune ok", "Saltwick Sands played to their own tune"),
        ("arranged ok", "a zone's second act played to its tune arranged again: evening, night, sunset, blizzard"),
        ("swing ok", "a swung song's off-beat eighths on the triplet's last, each half bar still half a bar long"),
        ("sung ok", "the quarter's tune sung: its notes held, by step, the trumpet and the clavinet out of its way"),
        ("kit ok", "the quarter's voices no FM, its drums no noise: saws and pulses, filtered, in the room, no DC"),
        ("drums ok", "the quarter's drums FM, each fitted to a recorded drum and chosen by ear, none drawn by hand"),
        ("layers ok", "the quarter's snare two notes at once on two voices, a thump under its hiss"),
        ("gold ok", "an act's three golden bones: found by a touch, kept in the save at once, a pip each on the HUD"),
        ("packing ok", "after a chase the level's end is the longer one, twelve bars; the eight elsewhere"),
        ("whistled ok", "the quarter's blue hour ends whistled, the guitar gone; its festival night with the guitar"),
        ("over ok", "game over played by its own tune and held while it plays; then the title"),
        ("tally ok", "the level's end's bonus counted on its tune's beats, a step each, and all at once at a key"),
        ("bump ok", "between zones the van's bump on the next zone's road, to its own bars; then that zone"),
        ("cart ok", "a chase's cart hurts Charlie run into it, is bumped by him curled, and not again while dazed"),
        ("chase ok", "the Downs' evening ended in its chase: the cart fled, broken by three bumps, its keepsake taken"),
        ("tilt ok", "on a slope the chase's cart follows it, each wheel on the ground under it"),
        ("crane ok", "the works' crane is in view, out of a jump's reach, and bumped by a jet's throw and a hoop's"),
        ("crate ok", "the crane's crate hurts Charlie stood under it and breaks on the floor when he steps aside"),
        ("toolbox ok", "the crane's third bump breaks it: its toolbox falls, and is Charlie's once it lies"),
        ("home ok", "back at the title on the Amber Downs, to their tune, after the Chrome Works"),
        ("palette ok", "a zone shown in its own colours, the shared ones back after it"),
        ("save ok", "the furthest zone reached kept in CHARLIE.SAV, picked on the title and started at"),
        ("tide ok", "the sands' sea rises and falls by the clock; Charlie floats on it and paddles slowly"),
        ("sea ok", "the sea drawn up to its surface, and the ground under it of the wet pictures"),
        ("bank ok", "a hop from the water reaches a bank near its surface and not one far over it"),
        ("wet ok", "out of the water Charlie is slow till a roll shakes him dry"),
        ("shore ok", "no water of the sands is a trap: its far bank a hop over high water, or waded at low"),
        ("boat ok", "a boat aground and still at low water, afloat and adrift at high, Charlie aboard up with it"),
        ("pier ok", "the old pier's spans totter and fall once trodden, hold under a run, and are back at a restart"),
        ("flume ok", "a flume sweeps Charlie down it curled and throws him from its end; drawn over the ground"),
        ("sweeper ok", "a sweeper's hood hurts a roll too; beaten from behind, or landed on with a bounce"),
        ("spark ok", "a spark goes along its rail, hurts while alight and is safe dark"),
        ("press ok", "presses fall in turn along a row, hurt under and jumped into, and are run under from a stop"),
        ("pass ok", "Whitecap Pass, the fourth zone: its snow white on a clear morning, not at dusk"),
        ("wood ok", "Russet Wood, the fifth zone: its litter russet gold at a misty noon, not on the firefly night"),
        ("quarter ok", "Lantern Quarter, the sixth zone: its roofs lit indigo at the blue hour, its tune swung"),
        ("line ok", "a clothesline throws him up, higher each time in a row; passed through from under; drawn"),
        ("awning ok", "an awning throws him up the same each time; passed through from under, and by past its edge"),
        ("chimney ok", "a chimney takes him down its pot and up out of its other; the way out takes no one in"),
        ("tram ok", "a tram waits at its street's end, carries him along it while he rides, and comes back left"),
        ("slide ok", "a lantern line is ground to slide down, faster as he goes, onto the roof past it"),
        ("pounce ok", "a pouncer waits in its bin, leaps at him when he is near, scratches, and lands a cat"),
        ("pouncer ok", "a bark brings a pouncer out early; rolled into in the air it is beaten for good"),
        ("walker ok", "a rope-walker walks its clothesline, flinches at a bark, is shaken off by a bounce, scratches"),
        ("snatcher ok", "a snatcher runs to a loose bone in its beat and takes it for good; startled it takes none"),
        ("mortar ok", "a mortar fires on its beat of the tune: it burns who is beside it and throws up who is over it"),
        ("mortar pilot ok", "the pilot jumps a mortar's flash: through the festival night it is never hurt"),
        ("tipper ok", "a tipper's pot falls from its sill on its beat, hurts who is under it, breaks on the paving"),
        ("tipper pilot ok", "the pilot waits short of a tipper till its pot has broken, and is never hurt"),
        ("quarter gold ok", "each of the Lantern Quarter's six golden bones can be got"),
        ("golds ok", "each of the first ten acts has three golden bones, and each can be got"),
        ("relay ok", "the quarter's relay: five cats, each its own pace, a bump each; the twins lob; the guitar taken"),
        ("firefly ok", "the wood's night dark but for a few fireflies; a bark wakes them and lights it for a while"),
        ("sapling ok", "a bowed sapling throws him who crosses its crown, higher the faster; sprung it throws no one"),
        ("vine ok", "a vine caught in the air swings him as a weight, lets go at a jump, and hangs straight again"),
        ("bramble ok", "bramble hurts on foot, is cut by a roll with his bones kept, and grows back to hurt again"),
        ("log ok", "a hollow log stops him on foot, is jumped, and shoots him through it curled, out of sight"),
        (
            "burrow ok",
            "a burrow takes him crouched on its mound, unseen and unhurt, to its other, and not back at once",
        ),
        ("squirrel ok", "a squirrel keeps over him, drops an acorn on the slow, is cowed by a bark, beaten by a roll"),
        ("bees ok", "a swarm wakes when he is near, stings, keeps up with a walk, is lost by a run and goes home"),
        ("dig ok", "the pilot digs at a mound whose burrow passes a wall, and runs over one that leads back"),
        ("twins ok", "the oak's twins lob the ball across a pair of limbs; touched on its way it is caught"),
        ("uncurl ok", "the pilot hops out of a roll that nothing ahead asks for, and is at a run again"),
        ("ice ok", "bare ice keeps his speed: a long slide, a long stop, a slow start; drawn bare of snow"),
        ("sled ok", "a sled is boarded, follows the slope, has no brake, keeps him through a hop, ends in deep snow"),
        ("geyser ok", "in the pass a jet is a geyser, the pass's water: it throws him up as it blows"),
        ("snowball ok", "deep snow rolled through fast makes him a snowball: it takes one hit, and ends with the roll"),
        ("avalanche ok", "the dusk's avalanche waits till he is past, comes on behind him, takes him where he stops"),
        ("toboggan ok", "Frost's toboggan lies as the ground does; on his sled a bump checks him; the camera is taken"),
        ("frost ok", "the pass's dusk ended in its chase: Frost bumped twice sled against sled, the camera taken"),
        ("relay won ok", "the festival night ended in its relay: the pilot bumped all five cats and took the guitar"),
        ("gale ok", "the dusk's gusts bear him back on his feet and in the air, not crouched or curled; snow streams"),
        ("goat ok", "an ice wall stops him; a goat's charge hurts; jumped, it is ridden through, as the ground lies"),
        ("thrower ok", "a snow-thrower's wall throws a roll back; its snowball hurts where he stood; up, it is beaten"),
        ("icicle ok", "an icicle is loosed from under it or by a bark, trembles, falls on him or a snow-thrower"),
        ("slide ok", "a bark brings a ledge's snow down on a snow-thrower; its heap stops him, a jump clears it"),
        ("crab ok", "a crab pinches at its side and is a step from above; barked into its shell, a higher one"),
        ("jelly ok", "a jellyfish afloat or aground: a high bounce off its bell, a sting for feet in it only"),
        ("dazed ok", "a Charlie hurt into the sea paddles again"),
        ("buoy ok", "a buoy rides the sea's top and bumps Charlie off it"),
        ("gull ok", "a gull dives for five bones and keeps them till barked at; a roll knocks it away"),
        ("reach ok", "no bone nor spring of the sands lies where the sea never leaves"),
        ("pedalo ok", "Kipper's boat goes up and down its channel, is bumped from above, faster each time, and sunk"),
        ("kipper ok", "the sands' sunset ended in its chase: the boat sunk by three bumps, the teddy taken"),
    ):
        if said not in out:
            sys.exit(f"charlie: not {what}: {out[-300:]}")
    tank, tree = out[out.rfind("zonetank") :].split(), out[out.rfind("zonetree") :].split()
    if tank[1:2] != [str(pix(charlieart.tank()))] or tree[1:4] != [str(pix(charlieart.tree())), "0", "-1"]:
        sys.exit(
            f"charlie: a zone's own pictures not the store's while it is played, or the store fuller: {out[-300:]}"
        )
    cast = [str(pix(one[-1])) for one in charlieart.family()] + [str(pix(charlieart.barons()[-1]))]
    drawn = [ln.split()[1] for ln in out.splitlines() if ln.split()[:1] in (["fam"], ["baron"])]
    if "cast ok" not in out or drawn != cast:
        sys.exit(
            "charlie: the family not each in three pictures waiting and six cheering, the Baron in four and two, "
            f"or not the art tool's: {out[-300:]}"
        )
    if "title met ok" not in out:
        sys.exit(f"charlie: the title's run not ended with the family, held and faded: {out[-300:]}")
    if "faded ok" not in out:
        sys.exit(f"charlie: the level's end not held, faded out and begun again in full colour: {out[-300:]}")
    for act in ("1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"):
        got = out[out.rfind(f"act {act} ") :].split()
        if got[1:6] != [act, "2", "0", act, str(int(act) // 2)]:
            sys.exit(
                f"charlie: stage {act} not cleared on one life (stage, mode, lives lost, stage, zone, x): {out[-300:]}"
            )
    words = out[out.rfind("result") :].split()
    if len(words) < 5 or words[1:3] != ["2", "3"] or words[4] != "10":
        sys.exit(
            f"charlie: not cleared on the first life round 10 loops (mode, lives, bones, loops, x, y): {out[-300:]}"
        )
    print(f"charlie ok: cleared with {words[3]} bones; {kept} fields assigned outside their owner")


if __name__ == "__main__":
    main()
