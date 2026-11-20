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
"""

import sys
import shutil
import tempfile
import subprocess
from pathlib import Path

import charlieart

ROOT = Path(__file__).resolve().parents[1]
DEMO = ROOT / "src/plat/wasm/web/demos/CHARLIE.BAS"

DRIVER = """declare function GroundAt% (lv as Level, x as integer)
declare sub Stand (lv as Level, gr as Ground, p as Hero, x as integer, v as single)
declare sub StandUnder (lv as Level, gr as Ground, p as Hero, x as integer, x0 as integer)
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
for j = 0 to wd.lv.nObj - 1
    if wd.lv.ob(j).k = KCAT then
        wd.lv.ob(j).x = 24
        wd.lv.ob(j).a = 24
        wd.lv.ob(j).y = wd.lv.startY
        g.camx = 0
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
    StartSong wd.mu, wd.se
    play Beat$(wd.mu, j, wd.mu.song(j).half(0).kit, 3)
    for i = 0 to NVOICE - 1
        play , , wd.mu.song(j).voOne(i) + "V1", , , wd.mu.song(j).voTwo(i) + "V1"
    next
next
play "T255", "T255", "T255", , "T255", "T255", "T255", "T255"
dim hf as Half
for j = 0 to NSLOT - 1
    for i = 0 to NBAR * 2 - 1
        hf = wd.mu.song(j).half(i)
        play hf.drums, hf.bass, hf.one, , hf.lead, hf.two, hf.under, hf.third
    next
next
for j = SJUMP to SCLANK
    Sfx wd.mu, wd.se, j
next
print "sounds ok"
' the level's end has a tune of its own, played once through its eight bars: it was Charlie's theme from its
' hits, round and round
g.mode = CLEARED
PickSong wd.mu, SongFor%(g), -1
for i = 1 to 40
    NextHalf wd.mu, wd.se
    if wd.mu.ended then exit for
next
if wd.mu.slot = CAUGHT and i = 16 then print "caught ok"
' the intro has its own tune in the zone's slot, 28 bars that end: its kit, its voicings and its first half bar played
StartIntro wd, g, cu()
PickSong wd.mu, SongFor%(g), 0
NextHalf wd.mu, wd.se
for i = 0 to NVOICE - 1
    play , , wd.mu.song(ZTUNE).voOne(i) + "V1", , , wd.mu.song(ZTUNE).voTwo(i) + "V1"
next
was = wd.mu.slot = ZTUNE and wd.mu.zHeld = 4
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
    case 300: if g.camx < 0 or g.camx > 60 or p.x < 70 or p.x > 250 then was = 0
    case 1200: if g.camx <> 1400 then was = 0
    case 1680: if g.camx <> 2140 then was = 0
    case 2700: if g.camx <> 3900 or p.x <> 4060 or p.gsp <> 0 then was = 0
    case 3300: if p.gsp < 400 or p.x - g.camx < 150 or p.x - g.camx > 300 then was = 0
    end select
next
was = was and abs(i - ILEN * 30) < 3 and g.mode = TITLE
if was and wd.zn.stage = 0 and wd.mu.zHeld = 1 and wd.tn.dusk = 16 then print "intro ok"
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
bt = g.modeT * BEATS
m = OnCue%(cu(), QHERO, bt)
j = OnCue%(cu(), QVAN, bt)
CueAt wd.lv, cu(), j, bt, c, v
c = c - cint(g.camx)
IntroDraw wd, g, p, cu()
hid = Pix&(c - 60, 8, HORIZON - 45, HORIZON - 14)
shown = Pix&(c - 92, 16, HORIZON - 45, HORIZON - 14)
cu(m).y0 = cu(m).y0 - 300
cu(m).y1 = cu(m).y1 - 300
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
Hurt wd.mu, wd.se, g, p, cint(p.x) + 10, lo(), SHURT
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
Hurt wd.mu, wd.se, g, p, cint(p.x) + 10, lo(), SHURT
Burst wd.se, pt(), p.x, p.y - 40, 8, 100, CWHITE
bx = lo(0).x
by = lo(0).y
was = wd.se.looseN = 3 and wd.se.partN >= 8
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = was and lo(0).x <> bx and lo(0).y <> by
for i = 1 to 300
    Tick wd, g, p, k, lo(), pt()
next
if was and wd.se.looseN = 0 and wd.se.partN = 0 then print "spill ok"
was = 0
' the first 45 degree ramp after flat ground, c its first column
for c = 8 to wd.lv.w - 8
    j = GroundAt%(wd.lv, c * TS - 1)
    if j <> NONE and GroundAt%(wd.lv, c * TS - 40) = j and GroundAt%(wd.lv, c * TS + 15) = j - TS then exit for
next
' run onto it at every speed: at some, its rise ahead was taken for a wall and Charlie stopped dead at its foot
StartGame wd, g, p, lo(), pt()
k.r = -1
for v = 120 to 840 step 60
    Stand wd.lv, wd.gr, p, c * TS - 30, csng(v)
    for i = 1 to 12
        MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
        if p.x > c * TS + 24 then exit for
        if p.ground and p.gsp < v / 2 then was = -1
    next
next
' and stood on it, no key down, it stays: it slid back to the foot
k.r = 0
Stand wd.lv, wd.gr, p, c * TS + 8, 0
for i = 1 to 90
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
next
if was = 0 and abs(p.x - (c * TS + 8)) < 2 then print "slope ok"
' a jump pressed a moment before landing is made on landing: it was lost
StartGame wd, g, p, lo(), pt()
k.hit = -1
k.jump = -1
was = 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if was = 0 and not p.ground and p.vy > 0 and GroundAt%(wd.lv, cint(p.x)) - p.y < 10 then
        k.hit = -1
        was = 1
    end if
    if was = 1 and p.ground then was = 2
    if was = 2 and p.jumped and p.vy < -200 then was = 3
next
if was = 3 then print "buffer ok"
' and one pressed a moment after running off an edge is still made: it was lost too
StartGame wd, g, p, lo(), pt()
for c = 8 to wd.lv.w - 8
    if GroundAt%(wd.lv, c * TS) = NONE and GroundAt%(wd.lv, c * TS - 20) <> NONE then exit for
next
Stand wd.lv, wd.gr, p, c * TS - 20, 200
k.r = -1
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
if p.jumped and p.vy < -250 then print "edge jump ok"
' walked slowly off an edge, Charlie falls at once: a foot still over the ground put it back on the edge, and it
' hung there half a second, till it was fast enough to clear it in a step
Stand wd.lv, wd.gr, p, c * TS - 20, 60
k.r = -1
k.jump = 0
k.hit = 0
v = cint(p.y)
for i = 1 to 45
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
next
if p.y > v + 20 then print "ledge ok"
was = 0
k.r = 0
k.jump = 0
k.hit = 0
k.was = 0
' dropped anywhere over the ground, Charlie lands: past the first loop it fell through the level
StartGame wd, g, p, lo(), pt()
for c = 40 to wd.lv.wpx - 40 step 7
    p.x = c
    p.y = 40
    p.vx = 0
    p.vy = 700
    p.ground = 0
    p.sg = 0
    for i = 1 to 200
        MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
        if p.ground or p.y > MH * TS then exit for
    next
    if p.ground = 0 and wd.lv.top(c \\ TS) < MH then was = was + 1
next
if was = 0 then print "drop ok"
' a loop is ground: met fast, no key held, it is run round, over its top and out the far side; met slowly Charlie
' stalls on its side and slides back to its foot
for j = 0 to wd.lv.nObj - 1
    if wd.lv.ob(j).k = KLOOP then exit for
next
c = wd.lv.ob(j).x
Stand wd.lv, wd.gr, p, c - 8, 460
was = 0
for i = 1 to 300
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if p.ground then if wd.gr.pt(p.sg).u < -.9 then was = -1
    if p.x > c + LOOPR + 12 then exit for
next
v = was and p.ground and p.x > c + LOOPR + 12
Stand wd.lv, wd.gr, p, c - 60, 200
was = 0
for i = 1 to 300
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if p.ground then if wd.gr.pt(p.sg).u < 0 then was = -1
next
if v and was = 0 and p.ground and abs(p.x - c) < LOOPR and p.y > wd.lv.ob(j).y - 8 then print "loop ok"
' too slow at its top, Charlie falls from it and lands inside
Stand wd.lv, wd.gr, p, c - 8, 340
was = 0
for i = 1 to 200
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if not p.ground and p.y < wd.lv.ob(j).y - LOOPR then was = -1
next
if was and p.ground and p.x < c + LOOPR then print "stall ok"
' a jump before a loop comes down on the ground under it, not on the far half of the ring
Stand wd.lv, wd.gr, p, c - 30, 0
k.hit = -1
k.jump = -1
was = 0
for i = 1 to 90
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    k.hit = 0
    if p.ground then if wd.gr.pt(p.sg).k = 2 then was = -1
next
k.jump = 0
if was = 0 and p.ground and abs(p.y - wd.lv.ob(j).y) < 2 then print "ghost ok"
' a quarter pipe is run up, and its wall: off the top and onto the ground above
for j = 0 to wd.lv.nObj - 1
    if wd.lv.ob(j).k = KPIPE then exit for
next
c = wd.lv.ob(j).x
Stand wd.lv, wd.gr, p, c - 100, 360
k.r = -1
was = 0
for i = 1 to 300
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if p.ground then if wd.gr.pt(p.sg).u < .1 then was = -1
    if p.ground and p.x > c + 8 then exit for
next
if was and p.ground and p.x > c + 8 and abs(p.y - wd.lv.top(c \\ TS) * TS) < 2 then print "pipe ok"
' a ribbon is ground: run onto at speed it is ridden over each hump and off its far end onto the ground
for j = 0 to wd.lv.nObj - 1
    if wd.lv.ob(j).k = KWAVE then exit for
next
c = wd.lv.ob(j).x
Stand wd.lv, wd.gr, p, c - 40, 420
k.r = -1
was = 0
v = 0
for i = 1 to 400
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if p.ground then if wd.gr.pt(p.sg).k = 2 and p.y < wd.lv.ob(j).y - WAVEA + 8 then was = was + 1
    ' down its far sides too: with only the rising halves there it was flown from crest to crest
    if p.ground then if wd.gr.pt(p.sg).k = 2 and wd.gr.pt(p.sg).v > .3 then v = v + 1
    if p.ground and wd.gr.pt(p.sg).k = 1 and p.x > c + 384 then exit for
next
if was > 3 and v > 6 and p.ground and p.x > c + 384 and abs(p.y - wd.lv.ob(j).y) < 2 then print "wave ok"
' a ribbon is a twist, run on its inside: at its crest Charlie hangs under its line, not stood on it as on a hill
Stand wd.lv, wd.gr, p, c - 40, 420
was = 0
for i = 1 to 200
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if p.ground and abs(p.x - c - WAVEL) < 6 then
        g.camx = p.x - 160
        g.camy = p.y - 100
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
    if p.ground and (was and 1) = 0 and abs(p.x - c - WAVEL / 2) < 6 then
        g.camx = p.x - 160
        g.camy = p.y - 100
        Render wd, g, p, lo(), pt()
        if Fur%(160, 2, 92, 99) > 20 and Fur%(160, 2, 103, 107) > 12 and Fur%(160, 8, 70, 87) = 0 then print "turn ok"
        was = was or 1
    end if
    if p.ground and abs(p.x - c - WAVEL * 1.5) < 6 then
        g.camx = p.x - 160
        g.camy = p.y - 100
        Render wd, g, p, lo(), pt()
        if Fur%(160, 16, 60, 140) = 0 then print "hid ok"
        exit for
    end if
next
' and too slow for it, drops from under the crest to the pit's floor
Stand wd.lv, wd.gr, p, c - 40, 420
for i = 1 to 300
    if p.ground and p.x >= c + 150 and p.x < c + 230 then
        p.gsp = 100
        k.r = 0
    end if
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    if p.ground and p.y > wd.lv.ob(j).y + 40 then exit for
next
if p.ground and wd.gr.pt(p.sg).k = 1 and p.y > wd.lv.ob(j).y + 40 and p.x < c + 384 then print "slow ok"
' a bark stops a prowler where it is a while, and it goes on away from him: it walked on as if deaf
k.r = 0
StartGame wd, g, p, lo(), pt()
j = -1
for i = 0 to wd.lv.nObj - 1
    c = wd.lv.ob(i).a - 40
    if wd.lv.ob(i).k = KCAT and j < 0 and abs(GroundAt%(wd.lv, c) - wd.lv.ob(i).y) < 8 then
        if SegAt%(wd.lv, wd.gr, c, csng(GroundAt%(wd.lv, c))) >= 0 then j = i
    end if
next
wd.lv.ob(j).x = wd.lv.ob(j).a
wd.lv.ob(j).s = -1
Stand wd.lv, wd.gr, p, wd.lv.ob(j).a - 40, 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
was = 0
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.ob(j).x <> wd.lv.ob(j).a then was = was + 1
next
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
if was = 0 and wd.lv.ob(j).x > wd.lv.ob(j).a then print "bark ok"
was = 0
StartGame wd, g, p, lo(), pt()
for i = 1 to 9000
    Pilot wd.lv, wd.gr, wd.zn, wd.se, p, k
    Tick wd, g, p, k, lo(), pt()
    j = 0
    if p.ground then j = wd.gr.pt(p.sg).u < -.9
    if j and was = 0 then loops = loops + 1
    was = j
    if i mod 30 = 0 then Render wd, g, p, lo(), pt()
    if g.mode <> PLAYING then exit for
next
print "result"; g.mode; g.lives; g.bones; loops; cint(p.x); cint(p.y)
k.l = 0
k.r = 0
k.d = 0
k.jump = 0
k.hit = 0
for i = 1 to 400
    Tick wd, g, p, k, lo(), pt()
next
Render wd, g, p, lo(), pt()
was = p.win and abs(p.x - (wd.lv.famX - 106)) < 3 and wd.se.cheer
if was and abs(g.camx - (wd.lv.famX - 166)) < 3 then print "met ok"
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
NextHalf wd.mu, wd.se
for i = 0 to NVOICE - 1
    play , , wd.mu.song(ZTUNE).voOne(i) + "V1", , , wd.mu.song(ZTUNE).voTwo(i) + "V1"
next
if wd.mu.slot = ZTUNE and wd.mu.zHeld = 5 and wd.mu.song(ZTUNE).tempo = 132 then print "works tune ok"
' a platform carries who stands on it, to and fro
for m = 0 to wd.lv.nMv - 1
    if wd.lv.mv(m).k = 0 then exit for
next
c = wd.lv.mv(m).s
p.x = wd.gr.pt(c).x + 24
p.y = wd.gr.pt(c).y
p.sg = c
p.ground = -1
p.gsp = 0
was = cint(p.x)
v = was
for i = 1 to 240
    Tick wd, g, p, k, lo(), pt()
    if p.x < was then was = cint(p.x)
    if p.x > v then v = cint(p.x)
next
if p.ground and p.sg = c and abs(p.x - wd.gr.pt(c).x - 24) < 2 and v - was > 40 then print "carried ok"
' who flees in a chase, met at the race's end: run into, its cart hurts Charlie and takes no bump; rolled into, it
' takes one and is dazed; dazed, it takes no other
GoStage wd, 1, g, p, lo(), pt()
wd.lv.cart.st = 1
wd.lv.cart.x = wd.lv.chaseEnd
wd.lv.cart.y = GroundAt%(wd.lv, wd.lv.chaseEnd)
g.bones = 3
k.r = -1
Stand wd.lv, wd.gr, p, wd.lv.chaseEnd - 60, 200
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
next
was = p.inv > 0 and g.bones = 0 and wd.lv.cart.hits = 0
g.bones = 3
p.inv = 0
p.stun = 0
Stand wd.lv, wd.gr, p, wd.lv.chaseEnd - 60, 300
p.roll = -1
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.hits then exit for
next
c = wd.lv.cart.hits = 1 and wd.lv.cart.t > BOLTS and g.bones = 3 and not p.ground
Stand wd.lv, wd.gr, p, wd.lv.chaseEnd - 10, 0
p.roll = -1
for i = 1 to 10
    Tick wd, g, p, k, lo(), pt()
next
if was and c and wd.lv.cart.hits = 1 and g.bones = 3 then print "cart ok"
was = 0
k.r = 0
' on a slope the cart follows it, each wheel on the ground under it: it was drawn level, a wheel sunk in the slope
' and a wheel in the air. The picture without the cart and with it, down its wheels' columns: halfway down a slope
' of the race, then at the slope's foot, its front wheel on the level
GoStage wd, 1, g, p, lo(), pt()
g.modeT = 5
g.shake = 0
was = -1
c = wd.lv.chaseX + 104
for j = 0 to 1
    do while c < wd.lv.chaseEnd - 104
        v = GroundAt%(wd.lv, c + 13) - GroundAt%(wd.lv, c - 13)
        if j = 0 and v = 13 and GroundAt%(wd.lv, c + 30) - GroundAt%(wd.lv, c - 30) = 30 then exit do
        if j = 1 and v >= 5 and v <= 8 and GroundAt%(wd.lv, c + 30) = GroundAt%(wd.lv, c + 13) then exit do
        c = c + 8 - 6 * j
    loop
    wd.lv.cart.st = 3
    wd.lv.cart.x = c
    wd.lv.cart.y = GroundAt%(wd.lv, c)
    Stand wd.lv, wd.gr, p, c - 200, 0
    g.camx = c - 160
    g.camy = wd.lv.cart.y - 124
    Render wd, g, p, lo(), pt()
    KeepCol 147, colA()
    KeepCol 173, colB()
    wd.lv.cart.st = 1
    Render wd, g, p, lo(), pt()
    v = Lowest%(147, colA()) - (GroundAt%(wd.lv, c - 13) - cint(g.camy))
    m = Lowest%(173, colB()) - (GroundAt%(wd.lv, c + 13) - cint(g.camy))
    if c >= wd.lv.chaseEnd - 104 or abs(v) > 1 or abs(m) > 1 then was = 0
next
if was then print "tilt ok"
was = 0
' the works' night ends under Sprocket's crane, on a rail over a hall. The view holds the rail while it is fought; a
' jump does not reach it; a jet's throw does and bumps it: it is dazed and Charlie knocked back down
GoStage wd, 3, g, p, lo(), pt()
for j = 0 to wd.lv.nObj - 1
    if wd.lv.ob(j).k = KJET and wd.lv.ob(j).x > wd.lv.chaseX then exit for
next
c = wd.lv.ob(j).x
Stand wd.lv, wd.gr, p, c - 40, 0
Tick wd, g, p, k, lo(), pt()
wd.lv.box.t = 9
k.hit = -1
k.jump = -1
m = 0
for i = 1 to 70
    wd.lv.cart.x = p.x
    Tick wd, g, p, k, lo(), pt()
    if not p.ground then m = -1
next
k.jump = 0
v = cint(wd.lv.cart.y) - RAILUP - cint(g.camy)
was = wd.lv.cart.st = 1 and wd.mu.zHeld = CHASETUNE + 1 and m and p.ground and wd.lv.cart.hits = 0 and v >= 0 and v < 40
wd.se.ticks = 180 - (c mod 180)
Stand wd.lv, wd.gr, p, c, 0
wd.lv.cart.x = c
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.hits then exit for
next
was = was and wd.lv.cart.hits = 1 and wd.lv.cart.t > BOLTS and p.vy > 0 and not p.ground
' and a hoop's throw does, from the side
for j = 0 to wd.lv.nObj - 1
    if wd.lv.ob(j).k = KHOOP and wd.lv.ob(j).x > wd.lv.chaseX then exit for
next
wd.lv.cart.t = 0
wd.lv.cart.x = wd.lv.ob(j).x + 200
wd.lv.box.t = 9
p.x = wd.lv.ob(j).x - 12
p.y = wd.lv.ob(j).y + 12
p.ground = 0
p.vx = 300
p.vy = 0
m = 0
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
    if p.vx > 500 then m = -1
    if wd.lv.cart.hits > 1 then exit for
next
if was and m and wd.lv.cart.hits = 2 then print "crane ok"
' its crate is held over Charlie stood still, let go, and hurts him; he aside, it breaks on the floor and the next
' is a while off, his bones his own still, with any of the first hit's he has picked up
GoStage wd, 3, g, p, lo(), pt()
Stand wd.lv, wd.gr, p, wd.lv.chaseX + 300, 0
Tick wd, g, p, k, lo(), pt()
wd.lv.cart.x = p.x
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
Stand wd.lv, wd.gr, p, wd.lv.chaseX + 300, 0
wd.lv.cart.x = p.x
wd.lv.cart.v = 0
wd.lv.box.t = 0
m = 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.box.st = 2 and m = 0 then
        Stand wd.lv, wd.gr, p, wd.lv.chaseX + 240, 0
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
wd.lv.cart.x = c
hid = g.score
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.st = 2 then exit for
next
was = wd.lv.cart.st = 2 and wd.lv.cart.hits = NBUMP and wd.mu.zHeld = 9
for i = 1 to 10
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.cart.st = 2
for i = 1 to 60
    Stand wd.lv, wd.gr, p, cint(wd.lv.cart.x), 0
    Tick wd, g, p, k, lo(), pt()
next
if was and wd.lv.cart.st = 3 and g.score = hid + 1500 then print "toolbox ok"
was = 0
' a zone's own pictures are in the store only while it is played, where the last zone's were: after the Downs the
' works' tank is as the art tool drew it, after the works the Downs' tree, and the store no fuller for the change
GoStage wd, 0, g, p, lo(), pt()
hid = wd.art.nextAt
GoStage wd, 2, g, p, lo(), pt()
line (0, 0)-(63, 79), 0, BF
Blit wd.art, 0, 0, ITANK, 32, 40
print "zonetank"; Pix&(0, 32, 0, 39)
GoStage wd, 0, g, p, lo(), pt()
line (0, 0)-(63, 79), 0, BF
Blit wd.art, 0, 0, ITREE, 64, 80
print "zonetree"; Pix&(0, 64, 0, 79); wd.art.nextAt - hid; wd.art.zoneAt > 0
' the sands have their own tune in the zone's slot, at 120: its kit, its voicings and its first half bar played.
' They had the Downs'
GoStage wd, 4, g, p, lo(), pt()
PickSong wd.mu, SongFor%(g), 0
NextHalf wd.mu, wd.se
for i = 0 to NVOICE - 1
    play , , wd.mu.song(ZTUNE).voOne(i) + "V1", , , wd.mu.song(ZTUNE).voTwo(i) + "V1"
next
was = wd.mu.slot = ZTUNE and wd.mu.zHeld = 7
if was and wd.mu.song(ZTUNE).tempo = 120 and len(wd.mu.song(ZTUNE).order) = 36 then print "sands tune ok"
' a zone's second act plays its tune arranged again, a song of its own over the first act's bars, its first half
' bar and voicings played: the sands' sunset at 108 with a second pan under the first, silent at noon; the works'
' night with its lead an octave down; the Downs' evening on the organs from its first bar. Each had its first act's
was = wd.mu.song(ZTUNE).half(8).third = "P2" and wd.mu.song(ZTUNE).lvl(5) = 0
GoStage wd, 5, g, p, lo(), pt()
was = was and wd.mu.zHeld = 10 and wd.mu.song(ZTUNE).tempo = 108 and len(wd.mu.song(ZTUNE).order) = 36
was = was and instr(wd.mu.song(ZTUNE).half(8).third, "N49") = 3
GoStage wd, 2, g, p, lo(), pt()
act1 = wd.mu.song(ZTUNE).half(8).lead
GoStage wd, 3, g, p, lo(), pt()
was = was and wd.mu.zHeld = 9 and wd.mu.song(ZTUNE).tempo = 132 and instr(act1, "N53") = 3
was = was and instr(wd.mu.song(ZTUNE).half(8).lead, "N41") = 3
GoStage wd, 0, g, p, lo(), pt()
was = was and wd.mu.zHeld = 1 and left$(wd.mu.song(ZTUNE).voicing, 1) = "h"
GoStage wd, 1, g, p, lo(), pt()
was = was and wd.mu.zHeld = 8 and wd.mu.song(ZTUNE).voicing = string$(28, "o") and len(wd.mu.song(ZTUNE).order) = 76
' the pass's blizzard in F sharp minor, its third voice the wind: it played the morning's tune, in A
GoStage wd, 6, g, p, lo(), pt()
was = was and wd.mu.zHeld = 14 and instr(wd.mu.song(ZTUNE).half(8).bass, "N22") > 0
GoStage wd, 7, g, p, lo(), pt()
was = was and wd.mu.zHeld = 15 and wd.mu.song(ZTUNE).tempo = 150 and len(wd.mu.song(ZTUNE).order) = 36
was = was and instr(wd.mu.song(ZTUNE).half(8).bass, "N19") > 0 and instr(wd.mu.song(ZTUNE).midP, "W3") > 0
' the wood's reel in D minor at 126, a fiddle's saw leading; its night at 112, the flute's triangle leading: both
' played the Downs' tunes
GoStage wd, 8, g, p, lo(), pt()
was = was and wd.mu.zHeld = 16 and wd.mu.song(ZTUNE).tempo = 126 and len(wd.mu.song(ZTUNE).order) = 36
was = was and instr(wd.mu.song(ZTUNE).half(8).bass, "N15") > 0 and instr(wd.mu.song(ZTUNE).leadP, "W2") > 0
GoStage wd, 9, g, p, lo(), pt()
was = was and wd.mu.zHeld = 17 and wd.mu.song(ZTUNE).tempo = 112 and instr(wd.mu.song(ZTUNE).leadP, "W1") > 0
for m = 1 to 9 step 2
    GoStage wd, m, g, p, lo(), pt()
    PickSong wd.mu, SongFor%(g), 0
    NextHalf wd.mu, wd.se
    for i = 0 to NVOICE - 1
        play , , wd.mu.song(ZTUNE).voOne(i) + "V1", , , wd.mu.song(ZTUNE).voTwo(i) + "V1"
    next
    was = was and wd.mu.slot = ZTUNE
next
GoStage wd, 0, g, p, lo(), pt()
if was then print "arranged ok"
' after a chase the level's end is the longer one, twelve bars that end, the cat sent packing first, its first half
' bar played; where there is no chase it is the eight. It was the eight everywhere
GoStage wd, 1, g, p, lo(), pt()
was = wd.mu.cHeld = PACKING + 1 and len(wd.mu.song(CAUGHT).order) = 12 and wd.mu.song(CAUGHT).ends
g.mode = CLEARED
PickSong wd.mu, SongFor%(g), -1
NextHalf wd.mu, wd.se
was = was and wd.mu.slot = CAUGHT and wd.mu.bar = 0 and wd.mu.half = 1
GoStage wd, 0, g, p, lo(), pt()
if was and wd.mu.cHeld = CAUGHTUP + 1 and len(wd.mu.song(CAUGHT).order) = 8 then print "packing ok"
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
was = g.mode = OVER and wd.mu.cHeld = LEFTBEHIND + 1 and SongFor%(g) = CAUGHT and wd.mu.song(CAUGHT).tempo = 80
PickSong wd.mu, SongFor%(g), -1
NextHalf wd.mu, wd.se
was = was and wd.mu.slot = CAUGHT and len(wd.mu.song(CAUGHT).order) = 4 and wd.mu.song(CAUGHT).ends
for i = 1 to 600
    Tick wd, g, p, k, lo(), pt()
next
was = was and g.mode = OVER
for i = 1 to 400
    Tick wd, g, p, k, lo(), pt()
    if g.mode = TITLE then exit for
next
if was and g.mode = TITLE and i < 400 and wd.mu.cHeld = CAUGHTUP + 1 then print "over ok"
GoStage wd, 0, g, p, lo(), pt()
' the level's end counts the bonus into the score in time with its tune: none before the bars its lead plays, a
' step on each of their beats, the last of it on their last; a key takes it all at once. The beat is the tune's
' own, counted from where it began, and the scene's with no music. It ran out in a second, off the beat
g.mode = CLEARED
g.modeT = 0
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
g.modeT = 0
g.tallied = 0
g.bonus = 1230
hid = g.score
k.hit = -1
Tick wd, g, p, k, lo(), pt()
k.hit = 0
was = was and g.bonus = 0 and g.score = hid + 1230
wd.mu.muted = 0
wd.mu.slot = CAUGHT
wd.mu.songAt = wd.se.ticks - 150
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
wd.se.metT = 7
for i = 1 to 200
    Advance wd, g, p, k, lo(), pt(), cu()
    if g.mode = INTRO then exit for
next
was = g.mode = INTRO and wd.zn.stage = SINTRO and wd.zn.id = 1 and wd.se.sceneNext = 2
was = was and wd.mu.zHeld = BETWEEN + 1 and wd.tn.dusk = 16
was = was and Sudden%(wd.se, g) and abs(g.modeT * BEATS - BUMP0) < 1
was = was and wd.tn.n(2) > 0 and wd.tn.now(wd.tn.which(2, 0)) = wd.tn.own(2, 0)
for i = 1 to 12 * 30
    Advance wd, g, p, k, lo(), pt(), cu()
    if (i mod 60) = 0 then IntroDraw wd, g, p, cu()
next
was = was and g.mode = INTRO and abs(g.camx - 2140) < 1
for i = 1 to 200
    Advance wd, g, p, k, lo(), pt(), cu()
    if g.mode <> INTRO then exit for
next
was = was and i < 200 and g.mode = PLAYING and wd.zn.stage = 2
was = was and wd.mu.zHeld = 5 and wd.tn.dusk = 16 and g.score = 1234
GoStage wd, 3, g, p, lo(), pt()
g.mode = CLEARED
g.bonus = 0
wd.se.metT = 7
for i = 1 to 200
    Advance wd, g, p, k, lo(), pt(), cu()
    if g.mode = INTRO then exit for
next
was = was and g.mode = INTRO and wd.zn.id = 2 and wd.se.sceneNext = 4
IntroDraw wd, g, p, cu()
k.hit = -1
Advance wd, g, p, k, lo(), pt(), cu()
k.hit = 0
was = was and g.mode = PLAYING and wd.zn.stage = 4 and wd.mu.zHeld = 7
GoStage wd, 0, g, p, lo(), pt()
if was then print "bump ok"
' Saltwick Sands' sea rises and falls by the clock, from low water to high and back, and from low water at the
' level's start whatever the clock then: it began where the game's clock had it, a sunset's high water by chance
GoStage wd, 4, g, p, lo(), pt()
g.modeT = 5
hid = wd.se.ticks
Flow wd.lv, wd.se
was = wd.lv.tide = wd.lv.tideLo and wd.lv.tideLo > wd.lv.tideHi and wd.se.ticks > 0
wd.se.ticks = wd.se.ticks + TIDEP \\ 2
Flow wd.lv, wd.se
was = was and wd.lv.tide = wd.lv.tideHi
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
wd.lv.tideLo = v + 24
wd.lv.tideHi = wd.lv.tideLo
p.x = c * TS + 24
p.y = v - 20
p.ground = 0
p.vx = 0
p.vy = 0
j = g.lives
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.swim and not p.dead and g.lives = j and abs(p.y - (wd.lv.tide + AFLOAT)) < 1
j = cint(p.x)
k.r = -1
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
next
if was and p.x - j > 20 and p.x - j < 50 then print "tide ok"
' the sea is drawn in the gap up to its surface and not over it; ground the sea is over is under it too, and of
' the wet pictures: its top tile was sand
g.camx = c * TS - 100
g.camy = wd.lv.tide - 100
Render wd, g, p, lo(), pt()
was = point(104, 104) = CSEA + 2 and (point(104, 94) < CSEA or point(104, 94) > CSEA + 6)
wd.lv.tideLo = wd.lv.top(c - 2) * TS - 20
wd.lv.tideHi = wd.lv.tideLo
Tick wd, g, p, k, lo(), pt()
g.camx = c * TS - 100
g.camy = wd.lv.tide - 100
Render wd, g, p, lo(), pt()
j = point(72, 130)
if was and point(72, 104) = CSEA + 2 and j >= CSEA and j <= CSEA + 6 then print "sea ok"
' a hop from the water gets him onto a bank two tiles over its surface, which the levels count on, and not onto
' one far over it: the tide opens and closes the way
was = 0
for j = 0 to 1
    wd.lv.tideLo = v + 2 * TS + 28 * j
    wd.lv.tideHi = wd.lv.tideLo
    Flow wd.lv, wd.se
    p.x = m * TS - 12
    p.y = wd.lv.tide + AFLOAT
    p.swim = -1
    p.ground = 0
    p.vx = 0
    k.r = -1
    k.jump = -1
    for i = 1 to 90
        if i = 20 then k.hit = -1
        Tick wd, g, p, k, lo(), pt()
        if p.ground then exit for
    next
    if j = 0 and p.ground and p.x > m * TS - 8 then was = -1
    if j = 1 and p.ground then was = 0
next
k.jump = 0
if was and p.swim then print "bank ok"
' out of the water he is wet and slow, till a roll shakes him dry
wd.lv.tideLo = v + 24
wd.lv.tideHi = wd.lv.tideLo
Stand wd.lv, wd.gr, p, m * TS + 24, WETTOP - 20
p.swim = 0
p.wet = DRYT
k.r = -1
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = p.wet > 0 and p.gsp > WETTOP - 20 and p.gsp <= WETTOP + 1
Stand wd.lv, wd.gr, p, m * TS + 24, WETTOP - 20
p.roll = -1
k.r = 0
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.wet <= 0
Stand wd.lv, wd.gr, p, m * TS + 24, WETTOP - 20
k.r = -1
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
k.r = 0
if was and p.gsp > WETTOP + 60 then print "wet ok"
' no water is a trap: the bank past each stretch swum at high water is within a hop of it, two tiles, or the
' stretch is waded at low water, to its spring. A sea gap's far bank was seven tiles up, Charlie afloat for good.
GoStage wd, 0, g, p, lo(), pt()
was = -1
zp = -1
for m = 4 to 5
    GoStage wd, m, g, p, lo(), pt()
    brim = wd.lv.tideLo
    ebb = wd.lv.tideHi
    ' the level's own tide, not one a check before this held it at: the sands' noon was read at such a one
    if brim = ebb then was = 0
    if wd.lv.tideHi < brim then swap brim, ebb
    for c = 1 to wd.lv.w - 2
        v = wd.lv.top(c) * TS
        j = wd.lv.top(c + 1) * TS
        if v > brim + WADE and j <= brim + WADE and brim - j > 2 * TS and v > ebb + WADE then was = 0
    next
    ' and no bone nor spring lies where the sea never leaves: the sunset's pools had bones on their floors
    for i = 0 to wd.lv.nObj - 1
        if wd.lv.ob(i).k = KBONE and wd.lv.ob(i).y > ebb + 9 then zp = 0
        if wd.lv.ob(i).k = KSPRING and wd.lv.ob(i).y > ebb + WADE then zp = 0
    next
next
if was and brim < ebb then print "shore ok"
if zp then print "reach ok"
' a boat lies on the floor of its pool at low water, and still; afloat it drifts, its deck just over the sea, and
' Charlie aboard goes up with it
GoStage wd, 4, g, p, lo(), pt()
for m = 0 to wd.lv.nMv - 1
    if wd.lv.mv(m).k = 5 then exit for
next
c = wd.lv.mv(m).s
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = m < wd.lv.nMv and wd.gr.pt(c).y = wd.lv.mv(m).y and wd.gr.pt(c).x = wd.lv.mv(m).x
was = was and wd.lv.tide - FREEB > wd.lv.mv(m).y
wd.lv.tideLo = wd.lv.mv(m).y - 20
wd.lv.tideHi = wd.lv.tideLo
p.x = wd.gr.pt(c).x + 20
p.y = wd.gr.pt(c).y
p.sg = c
p.ground = -1
p.gsp = 0
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.gr.pt(c).y = wd.lv.tide - FREEB and wd.gr.pt(c).x <> wd.lv.mv(m).x
was = was and p.ground and p.sg = c and abs(p.y - wd.gr.pt(c).y) < 1
if was then print "boat ok"
' the sands' old pier: a span stays while no one is on it, totters under Charlie stood on it, then falls from under
' him into its pool, where he is not lost, and is back when he starts again; run over at speed every span holds
' till he is past, he is on the far bank dry, and all of them have fallen behind him. At the level's own low
' water, its pool dry: the check before held the tide high
GoStage wd, 0, g, p, lo(), pt()
GoStage wd, 4, g, p, lo(), pt()
for m = 0 to wd.lv.nMv - 1
    if wd.lv.mv(m).k = 6 then exit for
next
c = wd.lv.mv(m).s
spans = 0
for i = m to wd.lv.nMv - 1
    if wd.lv.mv(i).k = 6 then spans = spans + 1
next
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
was = m < wd.lv.nMv and spans > 5 and wd.gr.pt(c).y = wd.lv.mv(m).y
was = was and wd.lv.mv(m).t = 0 and wd.lv.ob(wd.lv.mv(m).o).k = KSPAN
p.x = wd.gr.pt(c).x + 24
p.y = wd.gr.pt(c).y
p.sg = c
p.ground = -1
p.gsp = 0
for i = 1 to 12
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.gr.pt(c).y = wd.lv.mv(m).y and p.ground and p.sg = c and wd.lv.ob(wd.lv.mv(m).o).s <> 0
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.gr.pt(c).y > wd.lv.mv(m).y + 200 and wd.lv.ob(wd.lv.mv(m).o).y > wd.lv.mv(m).y + 200
was = was and not (p.ground and p.sg = c)
' down on the pool's dry floor, not through it
was = was and not p.dead and p.ground and abs(p.y - (wd.lv.mv(m).y + PITD * TS)) < 2
Spawn wd.lv, wd.gr, wd.zn, wd.mu, g, p
Tick wd, g, p, k, lo(), pt()
was = was and wd.gr.pt(c).y = wd.lv.mv(m).y and wd.lv.mv(m).t = 0 and wd.lv.ob(wd.lv.mv(m).o).s = 0
Stand wd.lv, wd.gr, p, wd.lv.mv(m).e - 40, 300
k.r = -1
v = 0
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if p.swim or p.y > wd.lv.mv(m).y + 8 then v = -1
    if p.x > wd.lv.mv(m).e + spans * MOVEW + 30 then exit for
next
k.r = 0
was = was and i < 200 and v = 0 and p.ground and abs(p.y - wd.lv.mv(m).y) < 2
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
next
j = 0
for i = m to wd.lv.nMv - 1
    if wd.lv.mv(i).k = 6 and wd.gr.pt(wd.lv.mv(i).s).y > wd.lv.mv(i).y + 200 then j = j + 1
next
if was and j = spans then print "pier ok"
' the sands' flume: walked onto, Charlie is swept down it curled, no slower than its water runs, and thrown from
' its end over the pool; its trough is drawn over the ground, the planks under the ground's line as dark as its
' end's cap over it. A slope took him at his own pace
GoStage wd, 4, g, p, lo(), pt()
for j = 0 to wd.lv.nObj - 1
    if wd.lv.ob(j).k = KCISTERN then exit for
next
was = j < wd.lv.nObj
Stand wd.lv, wd.gr, p, wd.lv.ob(j).x - 30, 80
k.r = -1
v = 0
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if p.ground and p.x > wd.lv.ob(j).x + 30 and p.x < wd.lv.ob(j).x + 70 then
        v = v + 1
        if p.gsp < FLUMEV or not p.roll then was = 0
    end if
    if not p.ground and p.x > wd.lv.ob(j).x + 96 then exit for
next
k.r = 0
was = was and v > 2 and i < 60 and p.vx >= FLUMEV
g.camx = wd.lv.ob(j).x - 100
g.shake = 0
Render wd, g, p, lo(), pt()
c = 100
m = wd.lv.ob(j).y - int(g.camy)
if was and point(c + 40, m + 42) = point(c + 95, m + 77) and wd.art.at(ICISTERN) > 0 then print "flume ok"
' a works' sweeper hurts whoever meets its hood, a rolling Charlie too; rolled into from behind it is beaten, and
' landed on, with a bounce. Jumped at 50 pixels like a cat, its hood caught the pilot rising
GoStage wd, 2, g, p, lo(), pt()
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KSWEEP then exit for
next
was = c < wd.lv.nObj
for j = 0 to 1
    g.bones = 5
    p.inv = 0
    p.stun = 0
    wd.lv.ob(c).s = -1
    Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 14, 200
    if j then p.roll = -1
    Tick wd, g, p, k, lo(), pt()
    was = was and p.inv > 0 and wd.lv.ob(c).s <> 0
next
hid = g.score
p.inv = 0
p.stun = 0
wd.lv.ob(c).s = -1
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x + 14, -200
p.roll = -1
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.ob(c).s = 0 and g.score = hid + 100 and p.inv <= 0
' rising past its hood, his feet over its top, he is clear of it
wd.lv.ob(c).s = -1
p.x = wd.lv.ob(c).x - 15
p.y = wd.lv.ob(c).y - 22
p.ground = 0
p.roll = 0
p.vx = 0
p.vy = -200
Tick wd, g, p, k, lo(), pt()
was = was and p.inv <= 0 and wd.lv.ob(c).s <> 0
p.x = wd.lv.ob(c).x
p.y = wd.lv.ob(c).y - 30
p.ground = 0
p.roll = 0
p.vx = 0
p.vy = 100
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
    if p.vy < 0 then exit for
next
if was and wd.lv.ob(c).s = 0 and p.inv <= 0 and abs(p.vy + BOUNCEV) < 30 then print "sweeper ok"
' a works' spark goes SPARKR either way along its rail; its carriage hurts for the SPARKLIT steps of SPARKP it is
' alight, and is safe dark
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KSPARK then exit for
next
was = c < wd.lv.nObj
Stand wd.lv, wd.gr, p, wd.lv.ob(c).a - 100, 0
m = wd.lv.ob(c).x
v = wd.lv.ob(c).x
for i = 1 to 240
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.ob(c).x < m then m = wd.lv.ob(c).x
    if wd.lv.ob(c).x > v then v = wd.lv.ob(c).x
next
was = was and m >= wd.lv.ob(c).a - SPARKR and m < wd.lv.ob(c).a - SPARKR + 4
was = was and v <= wd.lv.ob(c).a + SPARKR and v > wd.lv.ob(c).a + SPARKR - 4
for j = 0 to 1
    do until (wd.se.ticks + wd.lv.ob(c).a) mod SPARKP = 10 + j * SPARKLIT
        wd.se.ticks = wd.se.ticks + 1
    loop
    g.bones = 5
    p.inv = 0
    p.stun = 0
    Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 4, 0
    Tick wd, g, p, k, lo(), pt()
    was = was and (p.inv > 0) = (j = 0)
next
if was then print "spark ok"
' the works' presses: a head comes down in its turn, a press along a row after the one before it by the pixels
' between them over PRESSLAG; it hurts who is under it once it is down to him, and who jumps into it while it is
' up; from a stop, a run begun as the first has risen goes under the whole row; the head is drawn where it is
GoStage wd, 3, g, p, lo(), pt()
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KPRESS then exit for
next
for m = c + 1 to wd.lv.nObj - 1
    if wd.lv.ob(m).k = KPRESS then exit for
next
was = m < wd.lv.nObj
do until Stroke%(wd.lv, wd.se, c) = 0 and PressT%(wd.lv, wd.se, c) < 20
    wd.se.ticks = wd.se.ticks + 1
loop
g.bones = 5
p.inv = 0
p.stun = 0
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x, 0
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
was = was and (wd.se.ticks + 1 - j) mod PRESSP = (wd.lv.ob(m).x - wd.lv.ob(c).x) \\ PRESSLAG
do until Stroke%(wd.lv, wd.se, c) = 0 and PressT%(wd.lv, wd.se, c) < 20
    wd.se.ticks = wd.se.ticks + 1
loop
g.bones = 5
p.inv = 0
p.stun = 0
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x, 0
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
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 60, 0
for j = m to wd.lv.nObj - 1
    if wd.lv.ob(j).k = KLEG then exit for
next
k.r = -1
for i = 1 to 400
    Tick wd, g, p, k, lo(), pt()
    if p.inv > 0 then was = 0
    if p.x > wd.lv.ob(j).x + 20 then exit for
next
k.r = 0
was = was and i < 400
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 60, 0
g.camx = wd.lv.ob(c).x - 160
g.shake = 0
for j = 0 to 1
    do until PressT%(wd.lv, wd.se, c) = 20 + j * (PRESSFALL - 4)
        wd.se.ticks = wd.se.ticks + 1
    loop
    Render wd, g, p, lo(), pt()
    colA(j) = point(160, wd.lv.ob(c).y - int(g.camy) - 4)
    colB(j) = point(160, wd.lv.ob(c).y - int(g.camy) - PRESSUP - 4)
next
if was and colA(0) <> colA(1) and colB(0) <> colB(1) then print "press ok"
' Whitecap Pass is the fourth zone, by name and in its own colours: its snow's top the same picture at both hours,
' white on a clear morning and not at dusk
was = ZoneName$(3) = "WHITECAP PASS"
for j = 0 to 1
    GoStage wd, 6 + j, g, p, lo(), pt()
    Tick wd, g, p, k, lo(), pt()
    g.camx = 150
    g.shake = 0
    Render wd, g, p, lo(), pt()
    colA(j) = point(100, GroundAt%(wd.lv, 250) - int(g.camy))
    if j = 0 then hid = wd.tn.now(colA(0))
    was = was and wd.zn.id = 3
next
if was and colA(0) = colA(1) and hid = &H3F3F3F and wd.tn.now(colA(1)) <> hid then print "pass ok"
' Russet Wood is the fifth zone, by name and in its own colours: its litter's top the same picture at both hours,
' russet gold at a misty noon and not on the firefly night
was = ZoneName$(4) = "RUSSET WOOD"
for j = 0 to 1
    GoStage wd, 8 + j, g, p, lo(), pt()
    Tick wd, g, p, k, lo(), pt()
    g.camx = 150
    g.shake = 0
    Render wd, g, p, lo(), pt()
    colA(j) = point(100, GroundAt%(wd.lv, 250) - int(g.camy))
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
    for i = 0 to SH - 1
        for j = 0 to SW - 1
            if point(j, i) = CFLY or point(j, i) = CFLY + 1 then colA(m) = colA(m) + 1
        next
    next
    if m = 1 then exit for
    p.barkT = 0
    k.bark = -1
    Tick wd, g, p, k, lo(), pt()
    k.bark = 0
    Tick wd, g, p, k, lo(), pt()
    was = was and wd.tn.now(96) = &H100706
next
was = was and colA(0) >= 1 and colA(0) <= FLYREST and colA(1) > 8 * FLYREST
for i = 2 to GLOWT - GLOWFADE \\ 2
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.tn.now(96) = &H0D0504
for i = 1 to GLOWFADE \\ 2
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.tn.now(96) = &H0A0403
p.barkT = 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
Tick wd, g, p, k, lo(), pt()
was = was and wd.tn.now(96) = &H100706
GoStage wd, 9, g, p, lo(), pt()
Tick wd, g, p, k, lo(), pt()
was = was and wd.tn.now(96) = &H0A0403
GoStage wd, 8, g, p, lo(), pt()
Tick wd, g, p, k, lo(), pt()
if was and wd.lv.murk = 0 and wd.tn.now(96) = wd.tn.own(8, 0) then print "firefly ok"
' a sapling bowed to the ground throws him who crosses its crown, the higher the faster he came: from a walk over a
' jump's height, from a run 60 higher yet; sprung it stands upright and throws no one, then bows again
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KSAPLING then exit for
next
was = c < wd.lv.nObj
k.r = -1
for m = 0 to 1
    wd.lv.ob(c).s = 0
    Stand wd.lv, wd.gr, p, wd.lv.ob(c).x, 0
    p.gsp = 100 + m * 250
    for i = 1 to 60
        Tick wd, g, p, k, lo(), pt()
        if not p.ground then exit for
    next
    was = was and p.sprung and wd.lv.ob(c).s > 0 and abs(p.x - wd.lv.ob(c).x - SAPREACH) < 20
    ' the vine in his way is not for this throw
    p.vineT = 9
    for i = 1 to 200
        Tick wd, g, p, k, lo(), pt()
        if p.vy >= 0 then exit for
    next
    colA(m) = wd.lv.ob(c).y - cint(p.y)
next
k.r = 0
was = was and colA(0) > JUMPV * JUMPV / (2 * GRAV) and colA(1) > colA(0) + 60
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 60, 0
for i = 1 to 2 * SAPT + 4
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.ob(c).s = 0
wd.lv.ob(c).s = SAPT
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x + SAPREACH, 0
for i = 1 to 4
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.ground and wd.lv.ob(c).s > 0
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 60, 0
' its leaves are at its crown's place bowed, and not sprung
for m = 0 to 1
    g.shake = 0
    Render wd, g, p, lo(), pt()
    colB(m) = point(wd.lv.ob(c).x + SAPREACH + 5 - int(g.camx), wd.lv.ob(c).y - 6 - int(g.camy))
    wd.lv.ob(c).s = 0
next
if was and (colB(0) < 134 or colB(0) > 137) and colB(1) >= 134 and colB(1) <= 137 then print "sapling ok"
' a vine's end caught in the air: he hangs from it and swings as a weight would, 200 a second at its foot to 58
' degrees, 1.02 of a radian, and back as far, the vine drawn with him; a jump at its foot lets go with the swing's
' speed and a hop; the vine is not caught again at once, and hangs straight again
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KVINE then exit for
next
was = c < wd.lv.nObj
p.vineT = 0
p.ground = 0
p.x = wd.lv.ob(c).x
p.y = wd.lv.ob(c).y + VINELEN + VINEGRIP
p.vx = 200
p.vy = 0
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
    m = wd.lv.ob(c).x + cint(28 * sin(p.sway)) - int(g.camx)
    colB(0) = point(m, wd.lv.ob(c).y + cint(28 * cos(p.sway)) - int(g.camy))
    colB(1) = point(wd.lv.ob(c).x - int(g.camx), wd.lv.ob(c).y + 28 - int(g.camy))
next
was = was and p.vine = c + 1 and colA(0) > 97 and colA(0) < 107 and colA(1) < -97 and colA(1) > -107
was = was and colB(0) = CVINE and colB(1) <> CVINE and colB(1) <> CVINED
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if p.swayV > 0 and abs(p.sway) < .05 then exit for
next
k.hit = -1
Tick wd, g, p, k, lo(), pt()
k.hit = 0
was = was and p.vine = 0 and abs(p.vx - 200) < 15 and p.vy < -100
for i = 1 to 3
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.vine = 0
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 60, 0
for i = 1 to 80
    Tick wd, g, p, k, lo(), pt()
next
g.shake = 0
g.camy = wd.lv.ob(c).y - 40
Render wd, g, p, lo(), pt()
j = point(wd.lv.ob(c).x - int(g.camx), 68)
if was and wd.lv.ob(c).s = 0 and j = CVINE then print "vine ok"
' bramble hurts him who walks into it, his bones lost; curled he goes through it, his bones kept, and it is drawn
' cut, its leaves in the air; BRAMBLET steps on it is grown again and hurts
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KBRAMBLE then exit for
next
was = c < wd.lv.nObj
for m = 0 to 2
    p.inv = 0
    p.stun = 0
    g.bones = 5
    wd.se.partN = 0
    Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 40, 300
    p.roll = m = 1
    k.r = m <> 1
    for i = 1 to 60
        Tick wd, g, p, k, lo(), pt()
        if g.bones = 0 or p.x > wd.lv.ob(c).x + 24 then exit for
    next
    k.r = 0
    if m <> 1 then was = was and g.bones = 0 and wd.lv.ob(c).s = 0
    if m <> 1 then continue for
    was = was and g.bones = 5 and wd.lv.ob(c).s > 0 and p.x > wd.lv.ob(c).x and wd.se.partN > 0
    Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 60, 0
    for j = 0 to 1
        for i = 1 to 4 + j * 2 * BRAMBLET
            Tick wd, g, p, k, lo(), pt()
        next
        g.shake = 0
        Render wd, g, p, lo(), pt()
        colB(j) = point(wd.lv.ob(c).x - 19 + 22 - int(g.camx), wd.lv.ob(c).y - 25 + 7 - int(g.camy))
    next
    ' a thorn's pale point is there grown, and not cut
    was = was and wd.lv.ob(c).s = 0 and colB(0) <> 125 and colB(1) = 125
next
if was then print "bramble ok"
' a hollow log stops him on foot at its end, and a jump clears it, as does a throw over it; curled he is shot
' through it and leaves it at LOGV, none of his fur seen while he is in it
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KLOG then exit for
next
was = c < wd.lv.nObj
p.inv = 0
p.stun = 0
' the bones the bramble knocked loose are white as he is: gone before his fur is looked for
for i = 0 to NLOOSE - 1
    lo(i).t = 0
next
wd.se.looseN = 0
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 60, 0
k.r = -1
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
next
was = was and abs(p.x - (wd.lv.ob(c).x - LOGR)) < 2 and p.ground
k.hit = -1
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
    k.hit = 0
    if p.ground and p.x > wd.lv.ob(c).x + LOGR then exit for
next
was = was and p.ground and p.x > wd.lv.ob(c).x + LOGR
k.r = 0
' thrown over it uncurled he is not stopped in the air
p.ground = 0
p.roll = 0
p.x = wd.lv.ob(c).x - LOGR - 20
p.y = wd.lv.ob(c).y - LOGH - 40
p.vx = 200
p.vy = -150
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.x > wd.lv.ob(c).x
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 60, 200
p.roll = -1
colA(0) = 0
colA(1) = 0
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
    if p.x > wd.lv.ob(c).x + LOGR + 2 then exit for
    if abs(p.x - wd.lv.ob(c).x) >= 8 then continue for
    g.shake = 0
    Render wd, g, p, lo(), pt()
    colA(0) = Fur%(cint(p.x) - int(g.camx), 14, cint(p.y) - 26 - int(g.camy), cint(p.y) - int(g.camy))
    colA(1) = 1
next
was = was and colA(1) = 1 and colA(0) = 0 and p.x > wd.lv.ob(c).x + LOGR and p.roll
if was and abs(p.gsp - LOGV) < 30 then print "log ok"
' a burrow's mound leaves him be who runs over it; crouched on it he digs in, and goes DIGV for its other mound
' unseen, the bones and the bramble over his way and a hit nothing to him; there he is thrown up DIGPOP, and it
' does not take him back while he stays on it; stepped off and on again it does, to the first
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KMOUND then exit for
next
was = c < wd.lv.nObj
v = wd.lv.ob(c).s - 1
if v < 0 then v = c
was = was and v > c and wd.lv.ob(v).k = KMOUND and wd.lv.ob(v).s = c + 1 and wd.lv.ob(v).x = wd.lv.ob(c).x + 224
p.inv = 0
p.stun = 0
p.roll = 0
p.dug = 0
k.d = 0
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 40, 0
k.r = -1
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
    was = was and p.dig = 0
    if p.x > wd.lv.ob(c).x + MOUNDR + 2 then exit for
next
k.r = 0
was = was and p.x > wd.lv.ob(c).x + MOUNDR and p.x < wd.lv.ob(c).x + 20
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 40, 0
g.shake = 0
Render wd, g, p, lo(), pt()
j = wd.lv.ob(c).x - int(g.camx) - 1
m = wd.lv.ob(c).y - int(g.camy)
was = was and point(j, m - 3) = 137 and point(j, m - 9) = 123
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x + 3, 0
k.d = -1
Tick wd, g, p, k, lo(), pt()
was = was and p.dig = v + 1
bx = p.x
m = g.bones
for i = 1 to 15
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.dig = v + 1 and abs(p.x - bx - 15 * DIGV / 60) < 1
Hurt wd.mu, wd.se, g, p, 0, lo(), SHURT
g.shake = 0
Render wd, g, p, lo(), pt()
was = was and Fur%(cint(p.x) - int(g.camx), 14, cint(p.y) - 26 - int(g.camy), cint(p.y) - int(g.camy)) = 0
for i = 16 to 60
    Tick wd, g, p, k, lo(), pt()
    if p.dig = 0 then exit for
next
' 224 pixels at 400 a second: 33 steps, and out on the 34th
was = was and i = 34 and p.x = wd.lv.ob(v).x and g.bones = m and p.stun = 0 and not p.dead
by = p.y
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if p.y < by then by = p.y
    was = was and p.dig = 0
next
' thrown up at 260 against 750: 45 pixels
was = was and abs(wd.lv.ob(v).y - by - 45) < 4 and p.ground and abs(p.x - wd.lv.ob(v).x) < 2
k.d = 0
Stand wd.lv, wd.gr, p, wd.lv.ob(v).x + MOUNDR + 4, 0
Tick wd, g, p, k, lo(), pt()
Stand wd.lv, wd.gr, p, wd.lv.ob(v).x - 2, 0
k.d = -1
for i = 1 to 80
    Tick wd, g, p, k, lo(), pt()
    if p.dug = c + 1 then exit for
next
k.d = 0
if was and p.dug = c + 1 and p.x = wd.lv.ob(c).x then print "burrow ok"
' a squirrel stands on its bough, and keeps over him who is under it, turned his way; SQWAIT of the things' steps
' on, the last SQAIM of them with an acorn held out, it lets one fall: 39 pixels to his head, 19 steps, and it
' hurts; run under, he is gone before it lands; a bark cows it, still and without acorns; on its bough he is
' scratched by it on foot, and beats it curled
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KSQUIRREL then exit for
next
v = wd.lv.ob(c).a
was = c < wd.lv.nObj and wd.lv.ob(c).s <> 0 and GroundAt%(wd.lv, v) = wd.lv.ob(c).y
was = was and GroundAt%(wd.lv, v + 100) = wd.lv.ob(c).y + 64
p.inv = 0
p.stun = 0
p.roll = 0
k.d = 0
StandUnder wd.lv, wd.gr, p, v + 30, v + 100
Camera wd.lv, wd.zn, g, p, -1
if wd.se.ticks and 1 then Tick wd, g, p, k, lo(), pt()
wd.lv.ob(c).x = v
wd.lv.ob(c).s = SQWAIT
g.bones = 5
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
next
g.shake = 0
Render wd, g, p, lo(), pt()
j = wd.lv.ob(c).x - int(g.camx)
m = wd.lv.ob(c).y - int(g.camy)
was = was and abs(wd.lv.ob(c).x - v - 30) < SQV and wd.lv.ob(c).s = SQWAIT - 20
was = was and point(j - 4, m - 13) = 160 and point(j + 6, m - 5) <> 116
for i = 41 to 119
    Tick wd, g, p, k, lo(), pt()
next
g.shake = 0
Render wd, g, p, lo(), pt()
was = was and point(j + 6, m - 5) = 116 and wd.lv.lobN = 0 and p.stun = 0
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.lobN = 1 and wd.lv.ob(c).s = SQWAIT
for i = 1 to 18
    Tick wd, g, p, k, lo(), pt()
next
g.shake = 0
Render wd, g, p, lo(), pt()
j = cint(wd.lv.lobs(0).x) - int(g.camx) - 1
was = was and wd.lv.lobN = 1 and p.stun = 0 and g.bones = 5
was = was and point(j, cint(wd.lv.lobs(0).y) - int(g.camy) - 2) = 161
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.lobN = 0 and p.stun <> 0 and g.bones = 0 and not p.dead
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
wd.lv.ob(c).x = v - SQRUN
wd.lv.ob(c).s = -2
k.r = -1
bx = NONE
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.lobN > 0 and bx = NONE then bx = p.x - wd.lv.ob(c).x
next
k.r = 0
was = was and abs(bx) < SQNEAR and wd.lv.lobN = 0 and p.stun = 0 and g.bones >= 5 and p.x > v + 60 and g.mode = PLAYING
' a bark under it: it keeps still, and nothing falls
StandUnder wd.lv, wd.gr, p, v + 30, v + 100
p.gsp = 0
p.barkT = 0
wd.lv.ob(c).x = v
wd.lv.ob(c).s = SQWAIT
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
was = was and abs(wd.lv.ob(c).s) > SQWAIT
for i = 1 to 100
    Tick wd, g, p, k, lo(), pt()
    was = was and wd.lv.ob(c).x = v and wd.lv.lobN = 0
next
' on its bough: on foot it scratches him, curled he beats it
for j = 0 to 1
    wd.lv.ob(c).x = v
    wd.lv.ob(c).s = SQWAIT
    p.inv = 0
    p.stun = 0
    g.bones = 5
    hid = g.score
    Stand wd.lv, wd.gr, p, v - 40, 200
    p.roll = -j
    for i = 1 to 20
        Tick wd, g, p, k, lo(), pt()
        if p.stun or wd.lv.ob(c).s = 0 then exit for
    next
    colA(j) = (p.stun <> 0) + 2 * (wd.lv.ob(c).s = 0) + 4 * (g.score = hid + 100)
next
if was and colA(0) = -1 and colA(1) = -6 then print "squirrel ok"
' a swarm of bees keeps at home while he is far, drawn there; near it he wakes it, and it is on him and stings
' on the 15th step, 53 pixels at BEEV; a walk does not lose it, a run has by the 50th step, BEELOSE behind, and it
' goes home; so it does when he starts again
was = wd.lv.nSwarm = 1
v = wd.lv.swarms(0).hx
m = wd.lv.swarms(0).hy
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
was = was and wd.lv.swarms(0).st = 0 and wd.lv.swarms(0).x = v and wd.lv.swarms(0).y = m
g.shake = 0
Render wd, g, p, lo(), pt()
c = 0
for j = m - int(g.camy) - 12 to m - int(g.camy) + 12
    for i = v - int(g.camx) - 18 to v - int(g.camx) + 18
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
wd.lv.swarms(0).x = p.x
wd.lv.swarms(0).y = p.y - 13
for i = 1 to 60
    p.inv = 9
    p.gsp = 100
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.swarms(0).st = 1 and abs(wd.lv.swarms(0).x - p.x) < 8 and p.x > v
' run from, it is lost
wd.lv.swarms(0).x = p.x
k.r = -1
for i = 1 to 70
    p.inv = 9
    p.gsp = TOP
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.swarms(0).st = 2 then exit for
next
k.r = 0
was = was and i = 50
bx = wd.lv.swarms(0).x
Stand wd.lv, wd.gr, p, cint(p.x), 0
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
next
was = was and bx > v + 100 and wd.lv.swarms(0).st = 0 and wd.lv.swarms(0).x = v and wd.lv.swarms(0).y = m
wd.lv.swarms(0).st = 1
wd.lv.swarms(0).x = v + 50
Spawn wd.lv, wd.gr, wd.zn, wd.mu, g, p
if was and wd.lv.swarms(0).st = 0 and wd.lv.swarms(0).x = v then print "bees ok"
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
    k.l = -1
    for i = 1 to 10
        Tick wd, g, p, k, lo(), pt()
    next
    k.l = 0
    colB(j) = cint(p.gsp)
    Stand wd.lv, wd.gr, p, c * TS + 8, 0
    k.r = -1
    for i = 1 to 30
        Tick wd, g, p, k, lo(), pt()
    next
    k.r = 0
    colA(2 + j) = cint(p.gsp)
    g.camx = c * TS - 32
    g.shake = 0
    Render wd, g, p, lo(), pt()
    v = 0
    for i = 32 to 287
        if wd.tn.now(point(i, wd.lv.top(c) * TS - int(g.camy))) = &H3F3F3F then v = v + 1
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
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KSLED then exit for
next
for m = c to wd.lv.nObj - 1
    if wd.lv.ob(m).k = KDRIFT then exit for
next
was = c < wd.lv.nObj and m < wd.lv.nObj
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 16, 40
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
    if p.sled then exit for
next
was = was and p.sled and wd.lv.ob(c).s = 1 and p.gsp >= SLEDV and Fall%(wd, g, p, lo(), pt(), 59, -10, 8) = 0
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
k.l = -1
for i = 1 to 15
    Tick wd, g, p, k, lo(), pt()
next
k.l = 0
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
was = was and i < 600 and abs(p.x - wd.lv.ob(m).x) < 40 and p.inv <= 0 and p.ground and not p.roll and wd.lv.ob(c).s = 1
Spawn wd.lv, wd.gr, wd.zn, wd.mu, g, p
was = was and wd.lv.ob(c).s = 0
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 16, 40
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
g.bones = 5
was = was and p.sled
Hurt wd.mu, wd.se, g, p, cint(p.x) + 10, lo(), SHURT
Tick wd, g, p, k, lo(), pt()
if was and not p.sled and p.inv > 0 then print "sled ok"
' in the pass a jet is a geyser: its column the pass's water and not the works' steam; who is over it while it
' blows goes up, and stays down while it sleeps
GoStage wd, 6, g, p, lo(), pt()
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KJET then exit for
next
was = c < wd.lv.nObj
for j = 0 to 1
    do until (wd.se.ticks + wd.lv.ob(c).x) mod 180 = 10 + j * 90
        wd.se.ticks = wd.se.ticks + 1
    loop
    Stand wd.lv, wd.gr, p, wd.lv.ob(c).x, 0
    p.vy = 0
    Tick wd, g, p, k, lo(), pt()
    was = was and (not p.ground) = (j = 0)
    g.camx = wd.lv.ob(c).x - 100
    g.shake = 0
    Render wd, g, p, lo(), pt()
    colA(j) = point(100, wd.lv.ob(c).y - 34 - int(g.camy))
next
if was and colA(0) >= 116 and colA(0) <= 126 and colA(1) <> colA(0) then print "geyser ok"
' deep snow rolled through at speed packs round him, and not walked through nor rolled through slowly: a snowball,
' none of him seen in it; it takes a hit for him, his bones kept, and flies apart; it lasts no longer than his roll
GoStage wd, 7, g, p, lo(), pt()
for m = 0 to wd.lv.nObj - 1
    if wd.lv.ob(m).k = KDRIFT then exit for
next
was = m < wd.lv.nObj
' till the act's card is off the screen
for i = 1 to 240
    Tick wd, g, p, k, lo(), pt()
next
for j = 0 to 2
    Stand wd.lv, wd.gr, p, wd.lv.ob(m).x - 40, 200 - 140 * (j and 1)
    if j then p.roll = -1
    for i = 1 to 30
        Tick wd, g, p, k, lo(), pt()
        if p.snow then exit for
    next
    was = was and (p.snow <> 0) = (j = 2)
next
g.shake = 0
Render wd, g, p, lo(), pt()
c = cint(p.x) - int(g.camx)
v = cint(p.y) - int(g.camy)
was = was and Fur%(c, 14, v - 28, v) = 0 and point(c, v - 14) >= 116 and point(c, v - 14) <= 120
' rolled on out of the snow, it is his still
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.snow < 0 and p.x > wd.lv.ob(m).x + 30
g.bones = 5
p.inv = 0
Hurt wd.mu, wd.se, g, p, cint(p.x) + 10, lo(), SHURT
was = was and g.bones = 5 and p.inv > 0 and p.roll
Tick wd, g, p, k, lo(), pt()
was = was and p.snow = 0
p.inv = 0
Stand wd.lv, wd.gr, p, wd.lv.ob(m).x - 40, 200
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
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KTHROW then exit for
next
was = c < wd.lv.nObj
for i = 1 to 240
    Tick wd, g, p, k, lo(), pt()
next
wd.lv.ob(c).s = THROWC
g.bones = 5
hid = g.score
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 18, 200
p.roll = -1
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.ob(c).s <> 0 and p.inv <= 0 and p.gsp < 0 and g.score = hid
for j = 0 to 1
    g.bones = 5
    p.inv = 0
    p.stun = 0
    p.roll = 0
    wd.lv.ob(c).s = THROWAT + 1
    Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 100, 0
    for i = 1 to 4
        Tick wd, g, p, k, lo(), pt()
        if wd.lv.lobN then exit for
    next
    was = was and wd.lv.lobN = 1
    ' a snowball, as it is drawn
    g.shake = 0
    Render wd, g, p, lo(), pt()
    v = cint(wd.lv.lobs(0).x) - int(g.camx) - 1
    was = was and point(v, cint(wd.lv.lobs(0).y) - int(g.camy) - 2) = 116
    if j then Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 160, 0
    for i = 1 to 70
        Tick wd, g, p, k, lo(), pt()
        if wd.lv.lobN = 0 then exit for
    next
    was = was and i > 40 and i < 60 and (p.inv > 0) = (j = 0) and (g.bones = 0) = (j = 0)
next
p.inv = 0
p.stun = 0
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 60, 0
g.shake = 0
for j = 0 to 1
    wd.lv.ob(c).s = THROWC - j * (THROWC - THROWUP)
    Render wd, g, p, lo(), pt()
    y = wd.lv.ob(c).y - int(g.camy)
    shown = Pix&(wd.lv.ob(c).x - int(g.camx) - 14, 28, y - 25, y - 12)
    if j = 0 then bt = shown
next
was = was and shown <> bt
hid = g.score
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 18, 200
p.roll = -1
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.ob(c).s = 0 and g.score = hid + 100
wd.lv.ob(c).s = THROWUP - 2
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 40, 0
p.barkT = 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
if was and abs(wd.lv.ob(c).s) > THROWUP then print "thrower ok"
' the pass's icicle hangs while he is off to its side; a bark from there looses it, and it breaks on the ground;
' stood under, he looses it: it trembles TREMBLE of the things' steps, falls, hurts him and breaks; over a
' snow-thrower, a bark brings it down on it
GoStage wd, 6, g, p, lo(), pt()
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KICICLE then exit for
next
was = c < wd.lv.nObj
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 60, 0
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.ob(c).s = 0
p.barkT = 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
was = was and wd.lv.ob(c).s > 0
v = 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if Fallen%(wd.lv, c) > v then v = Fallen%(wd.lv, c)
    if wd.lv.ob(c).s < 0 then exit for
next
was = was and i < 120 and p.inv <= 0 and v > 60
wd.lv.ob(c).s = 0
g.bones = 5
p.x = wd.lv.ob(c).x
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.ob(c).s > 0 and Fallen%(wd.lv, c) = 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.ob(c).s > 0 and wd.lv.ob(c).s <= TREMBLE and Fallen%(wd.lv, c) <> 0 then was = 0
    if p.inv > 0 then exit for
next
was = was and i > TREMBLE * 2 and i < TREMBLE * 2 + 40 and wd.lv.ob(c).s = -1 and g.bones = 0
GoStage wd, 7, g, p, lo(), pt()
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KICICLE then exit for
next
for m = 0 to wd.lv.nObj - 1
    if wd.lv.ob(m).k = KTHROW and abs(wd.lv.ob(m).x - wd.lv.ob(c).x) < 14 then exit for
next
was = was and c < wd.lv.nObj and m < wd.lv.nObj
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 60, 0
p.barkT = 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.ob(c).s < 0 then exit for
next
if was and i < 120 and wd.lv.ob(m).s = 0 and p.inv <= 0 then print "icicle ok"
' the dusk's loaded ledge holds while he stands by it; a bark from there sets it off: it trembles, slides off its
' ledge and comes down on the snow-thrower past it, which is beaten; its heap stops him on foot at either flank,
' and a jump takes him over it; the level begun again, the load is back on its ledge. The heap was the load
' moved: met from its left alone, and left on the road when the level began again
GoStage wd, 7, g, p, lo(), pt()
wd.lv.gale = 0
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KLOAD then exit for
next
for m = c to wd.lv.nObj - 1
    if wd.lv.ob(m).k = KTHROW then exit for
next
j = HeapOf%(wd.lv, c)
was = c < wd.lv.nObj and m < wd.lv.nObj and j <> NONE
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 60, 0
g.bones = 5
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.ob(c).s = 0 and wd.lv.ob(j).s = 0 and wd.lv.ob(m).s <> 0
p.barkT = 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
was = was and wd.lv.ob(c).s > 0
v = 0
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.ob(c).s = TREMBLE + LOADSLIDE then v = Slid%(wd.lv, c)
    if wd.lv.ob(c).s < 0 then exit for
next
was = was and i < 200 and v = LOADRUN and wd.lv.ob(j).s = 1 and wd.lv.ob(m).s = 0
was = was and abs(wd.lv.ob(j).x - wd.lv.ob(m).x) < HEAPR
for i = 1 to 150
    Tick wd, g, p, k, lo(), pt()
next
Stand wd.lv, wd.gr, p, wd.lv.ob(j).x - 70, 0
p.inv = 0
p.stun = 0
k.r = -1
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
next
was = was and p.ground and abs(p.x - (wd.lv.ob(j).x - HEAPR)) < 2 and abs(p.gsp) < 20
k.hit = -1
k.jump = -1
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
    if p.ground then k.jump = 0
next
k.r = 0
was = was and p.x > wd.lv.ob(j).x + HEAPR
Stand wd.lv, wd.gr, p, wd.lv.ob(j).x + 70, 0
k.l = -1
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
next
k.l = 0
was = was and p.ground and abs(p.x - (wd.lv.ob(j).x + HEAPR)) < 2
GoStage wd, 7, g, p, lo(), pt()
if was and wd.lv.ob(c).s = 0 and wd.lv.ob(j).s = 0 and wd.lv.ob(c).y < wd.lv.ob(j).y - 80 then print "slide ok"
' a crab pinches at its side, a rolling Charlie too; down on its back he goes up off it unhurt and it ducks into
' its shell, and comes out again
GoStage wd, 4, g, p, lo(), pt()
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KCRAB then exit for
next
was = c < wd.lv.nObj
for j = 0 to 1
    g.bones = 5
    p.inv = 0
    p.stun = 0
    Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 16, 200
    if j then p.roll = -1
    Tick wd, g, p, k, lo(), pt()
    was = was and p.inv > 0 and abs(wd.lv.ob(c).s) = 1
next
for j = 0 to 1
    ' barked at the second time round, it keeps to its shell: no pinch at its side, and a higher step
    p.inv = 0
    p.stun = 0
    g.bones = 5
    if j then
        wd.lv.ob(c).s = 1
        p.barkT = 0
        Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 40, 0
        k.bark = -1
        Tick wd, g, p, k, lo(), pt()
        k.bark = 0
        was = was and abs(wd.lv.ob(c).s) > SHELLT - 4
        Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 12, 0
        Tick wd, g, p, k, lo(), pt()
        was = was and p.inv <= 0
    end if
    p.x = wd.lv.ob(c).x
    p.y = wd.lv.ob(c).y - 30
    p.ground = 0
    p.vx = 0
    p.vy = 100
    for i = 1 to 20
        Tick wd, g, p, k, lo(), pt()
        if p.vy < 0 then exit for
    next
    was = was and p.inv <= 0 and abs(wd.lv.ob(c).s) > 1
    if j = 0 then was = was and abs(p.vy + BOUNCEV) < 30
    if j = 1 then was = was and abs(p.vy + STEPV) < 30
next
' in its shell a roll knocks it off the sand; out of it again after SHELLT of its steps
wd.lv.ob(c).s = SHELLT
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 100, 0
for i = 1 to SHELLT * 2 + 4
    Tick wd, g, p, k, lo(), pt()
next
was = was and abs(wd.lv.ob(c).s) = 1
wd.lv.ob(c).s = SHELLT
hid = g.score
p.inv = 0
p.stun = 0
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 16, 200
p.roll = -1
Tick wd, g, p, k, lo(), pt()
if was and wd.lv.ob(c).s = 0 and g.score = hid + 100 and p.inv <= 0 then print "crab ok"
' a jellyfish floats at the sea's top, and lies on the ground where the sea is lower
GoStage wd, 4, g, p, lo(), pt()
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KJELLY then exit for
next
v = wd.lv.top(wd.lv.ob(c).x \\ TS) * TS
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 70, 0
for j = 0 to 1
    wd.lv.tideLo = v - 40 + 80 * j
    wd.lv.tideHi = wd.lv.tideLo
    for i = 1 to 4
        Tick wd, g, p, k, lo(), pt()
    next
    if j = 0 then was = c < wd.lv.nObj and wd.lv.ob(c).y = wd.lv.tide + JELLYD
    if j = 1 then was = was and wd.lv.ob(c).y = v
next
' his feet in it, it stings; his feet over it, as on a boat's deck, it does not: he was stung through the deck
g.bones = 5
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 8, 0
Tick wd, g, p, k, lo(), pt()
was = was and p.inv > 0
for j = 0 to 2
    ' over it and going up; down onto its bell, thrown high and unhurt; in it while it is flat from that, no
    ' sting: he was stung coming back down off a low ceiling
    p.inv = 0
    p.stun = 0
    p.x = wd.lv.ob(c).x
    p.y = v - 16 + 24 * (j = 1) - 12 * (j = 2)
    p.ground = 0
    p.vx = 0
    p.vy = -50 - 150 * (j = 1)
    g.bones = 5
    for i = 1 to 12
        Tick wd, g, p, k, lo(), pt()
        if j <> 1 or p.vy < 0 then exit for
    next
    was = was and p.inv <= 0
    if j = 1 then was = was and abs(p.vy + JELLYV) < 30 and wd.lv.ob(c).s > 1
next
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 70, 0
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
if was and wd.lv.ob(c).s = 1 then print "jelly ok"
' a swimmer it stings is thrown out of the sea, and back in it paddles again: hurt into the sea, he floated
' there for good, the keys dead
wd.lv.tideLo = v - 40
wd.lv.tideHi = wd.lv.tideLo
for i = 1 to 4
    Tick wd, g, p, k, lo(), pt()
next
p.inv = 0
g.bones = 5
p.x = wd.lv.ob(c).x - 8
p.y = wd.lv.tide + AFLOAT
p.ground = 0
p.swim = -1
p.vx = 0
Tick wd, g, p, k, lo(), pt()
was = p.inv > 0 and p.swim = 0 and p.stun
p.x = wd.lv.ob(c).x + 20
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if p.swim then exit for
next
j = cint(p.x)
k.r = -1
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
next
k.r = 0
if was and p.swim and p.x - j > 15 then print "dazed ok"
' a buoy rides the sea's top, and bumps Charlie off it
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KBUOY then exit for
next
was = c < wd.lv.nObj
p.swim = 0
p.inv = 0
p.stun = 0
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 90, 0
for j = 0 to 1
    wd.lv.tideLo = p.y + 30 + 20 * j
    wd.lv.tideHi = wd.lv.tideLo
    for i = 1 to 4
        Tick wd, g, p, k, lo(), pt()
    next
    was = was and wd.lv.ob(c).y = wd.lv.tide - 4
next
p.x = wd.lv.ob(c).x
p.y = wd.lv.ob(c).y - 24
p.ground = 0
p.vx = 0
p.vy = 100
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
    if p.vy < 0 then exit for
next
if was and p.vy < -300 and wd.lv.ob(c).s > 0 and not p.swim then print "buoy ok"
' a gull hovers about its home while Charlie is far off; once he stands under it with bones it dives, takes
' five, and climbs home with them, to dive no more
GoStage wd, 4, g, p, lo(), pt()
c = wd.lv.gulls(0).hx
g.bones = 8
Stand wd.lv, wd.gr, p, c - 300, 0
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = wd.lv.nGulls > 0 and wd.lv.gulls(0).st = GHOVER and abs(wd.lv.gulls(0).x - c) <= 24
was = was and abs(wd.lv.gulls(0).y - wd.lv.gulls(0).hy) <= 4
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
p.barkT = 0
k.bark = -1
Tick wd, g, p, k, lo(), pt()
k.bark = 0
was = was and wd.lv.gulls(0).loot = 0 and wd.se.looseN = 5
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
p.vy = -100
p.roll = -1
wd.lv.gulls(0).st = GDIVE
wd.lv.gulls(0).x = p.x
wd.lv.gulls(0).y = p.y - 13
wd.lv.gulls(0).tx = p.x
wd.lv.gulls(0).ty = p.y - 13
Tick wd, g, p, k, lo(), pt()
if was and wd.lv.gulls(0).st = 0 and g.score = hid + 100 then print "gull ok"
' Kipper's pedal boat waits on the sea in its channel; Charlie come to it, it goes to the channel's end and
' turns round
wd.lv.cart.st = 0
GoStage wd, 5, g, p, lo(), pt()
' its channel's buoys out of the way of these landings
for i = 0 to wd.lv.nObj - 1
    if wd.lv.ob(i).k = KBUOY and wd.lv.ob(i).x > wd.lv.chaseX then wd.lv.ob(i).k = 0
next
Tick wd, g, p, k, lo(), pt()
v = wd.lv.cart.st = 0 and wd.lv.cart.y = wd.lv.tide - PEDALUP
p.x = wd.lv.chaseX + 30
p.y = wd.lv.tide + AFLOAT
p.ground = 0
p.swim = -1
p.vx = 0
for i = 1 to 10
    Tick wd, g, p, k, lo(), pt()
next
was = v and wd.lv.cart.st = 1 and wd.lv.cart.y = wd.lv.tide - PEDALUP
was = was and wd.lv.cart.x > wd.lv.chaseX + CARTLEAD and wd.lv.cart.v = PEDALV
for i = 1 to 600
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.v < 0 then exit for
next
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.cart.v = -PEDALV and abs(wd.lv.cart.x - (wd.lv.chaseEnd - PEDALW \\ 2)) < 8
' a swimmer at its side is no bump; Charlie down on its deck out of the air is: it turns round, he goes up off
' it, and while it is dazed a second landing is none. Each bump makes it faster
p.x = wd.lv.cart.x
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.cart.hits = 0 and p.inv <= 0
hid = g.score
for j = 1 to 3
    for c = 0 to 1
        p.swim = 0
        p.ground = 0
        p.x = wd.lv.cart.x
        p.y = wd.lv.cart.y - 8
        p.vx = 0
        p.vy = 100
        for i = 1 to 12
            Tick wd, g, p, k, lo(), pt()
            if p.vy < 0 or p.swim then exit for
        next
        if c = 0 then was = was and p.vy < 0
    next
    was = was and wd.lv.cart.hits = j
    if j = 1 then was = was and wd.lv.cart.v > 0 and g.score = hid + 500
    p.x = wd.lv.chaseX + 30
    for i = 1 to 90
        Tick wd, g, p, k, lo(), pt()
    next
    if j < 3 then was = was and abs(wd.lv.cart.v) = PEDALV + j * PEDALMORE
next
' its third bump sinks it, and the teddy is his as he comes down there; afloat on the sea, it waits for a
' swimmer to come to it
was = was and wd.lv.cart.st = 3
wd.lv.cart.st = 2
wd.lv.cart.x = p.x + 60
Tick wd, g, p, k, lo(), pt()
was = was and wd.lv.cart.st = 2 and wd.lv.cart.y = wd.lv.tide
hid = g.score
p.x = wd.lv.cart.x
Tick wd, g, p, k, lo(), pt()
if was and wd.lv.cart.st = 3 and g.score = hid + 1000 then print "pedalo ok"
was = 0
' a lift goes up under Charlie to the ground above, and back down once it is left
GoStage wd, 2, g, p, lo(), pt()
for m = 0 to wd.lv.nMv - 1
    if wd.lv.mv(m).k = 2 then exit for
next
c = wd.lv.mv(m).s
p.x = wd.gr.pt(c).x + 20
p.y = wd.gr.pt(c).y
p.sg = c
p.ground = -1
p.gsp = 0
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
next
was = wd.lv.mv(m).t = wd.lv.mv(m).a and p.ground and abs(p.y - (wd.lv.mv(m).y - wd.lv.mv(m).a)) < 2
k.r = -1
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
k.r = 0
was = was and p.ground and wd.gr.pt(p.sg).k = 1 and p.x > wd.lv.mv(m).x + MOVEW
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
next
if was and wd.lv.mv(m).t = 0 then print "lift ok"
' a basket goes down its cable under Charlie, to the ground past the pit
GoStage wd, 2, g, p, lo(), pt()
for m = 0 to wd.lv.nMv - 1
    if wd.lv.mv(m).k = 3 then exit for
next
c = wd.lv.mv(m).s
p.x = wd.gr.pt(c).x + 24
p.y = wd.gr.pt(c).y
p.sg = c
p.ground = -1
p.gsp = 0
for i = 1 to 300
    Tick wd, g, p, k, lo(), pt()
next
was = p.ground and p.sg = c and abs(p.x - (wd.lv.mv(m).x + wd.lv.mv(m).a + 24)) < 2
if was and abs(p.y - (wd.lv.mv(m).y + CABLED)) < 2 then print "cable ok"
' the pass's goat and ice wall: the wall stops him on foot. The goat grazes with him 92 off, further than it sees;
' with him 50 off it lowers its head 24 ticks where it stands, then charges 8 a step of two ticks and hurts him
' where he stood, 36 to 38 ticks on. Jumped as it charges, it stops under him; he comes down on its back and it
' bolts on along the road, no rein taken, through the wall, drawn as the ground lies, level on the flat and
' falling on a fall; two seconds on he is off it, and it runs home to graze
GoStage wd, 6, g, p, lo(), pt()
for m = 0 to wd.lv.nObj - 1
    if wd.lv.ob(m).k = KGOAT then exit for
next
for c = 0 to wd.lv.nObj - 1
    if wd.lv.ob(c).k = KWALL then exit for
next
was = m < wd.lv.nObj and c < wd.lv.nObj
Stand wd.lv, wd.gr, p, wd.lv.ob(c).x - 60, 200
k.r = -1
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
k.r = 0
was = was and wd.lv.ob(c).s = 1 and p.x <= wd.lv.ob(c).x - WALLR and p.gsp = 0
Stand wd.lv, wd.gr, p, wd.lv.ob(m).x - 92, 0
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
was = was and abs(wd.lv.ob(m).s) = 1 and wd.lv.ob(m).x = wd.lv.ob(m).a
g.bones = 5
Stand wd.lv, wd.gr, p, wd.lv.ob(m).x - 50, 0
for i = 1 to 120
    Tick wd, g, p, k, lo(), pt()
    if i <= 24 and wd.lv.ob(m).x <> wd.lv.ob(m).a then was = 0
    if p.inv > 0 then exit for
next
was = was and i > 33 and i < 41 and g.bones = 0
GoStage wd, 6, g, p, lo(), pt()
g.bones = 5
Stand wd.lv, wd.gr, p, wd.lv.ob(m).x - 50, 0
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
    if abs(wd.lv.ob(m).s) = GCHARGE then exit for
next
k.hit = -1
k.jump = -1
for i = 1 to 80
    Tick wd, g, p, k, lo(), pt()
    if p.goat then exit for
next
k.jump = 0
was = was and p.goat and wd.lv.ob(m).s = 0 and p.inv <= 0 and g.bones = 5 and abs(p.x - (wd.lv.ob(m).a - 50)) < 12
k.l = -1
v = NONE
j = NONE
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
    if not p.goat then exit for
    if p.ground and p.gsp < 200 then was = 0
    if i = 30 then v = Fall%(wd, g, p, lo(), pt(), 132, -8, -2)
    if j = NONE and p.ground and p.y > wd.lv.ob(m).y + 8 then j = Fall%(wd, g, p, lo(), pt(), 132, -8, -2)
next
k.l = 0
was = was and i > 114 and i < 124 and wd.lv.ob(c).s = 0 and v = 0 and j = 3 and p.x > wd.lv.ob(c).x + 100
for i = 1 to 300
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.stray.st = 0 then exit for
next
if was and i < 300 and abs(wd.lv.ob(m).s) = 1 and wd.lv.ob(m).x = wd.lv.ob(m).a then print "goat ok"
' the dusk's blizzard, none on the morning: of every 192 ticks it blows the last 72. In its calm he stands where
' he is. In a gust, on his feet, 600 against his friction's 280 bears him back to 200 a second: 165 to 190 by
' its end; crouched he keeps his place; curled at 200 he rolls on 88 to 97 in half a second, as in a calm; a jump
' begun as it starts is carried back 150 to 175 by its landing, 900 to 200 a second. Its snow streams as it blows
GoStage wd, 6, g, p, lo(), pt()
was = wd.lv.gale = 0
GoStage wd, 7, g, p, lo(), pt()
for m = 0 to wd.lv.nObj - 1
    if wd.lv.ob(m).k = KTHROW then exit for
next
wd.lv.ob(m).s = 0
j = wd.lv.ob(m).x - 120
was = was and wd.lv.gale and m < wd.lv.nObj
wd.se.ticks = 1920
Stand wd.lv, wd.gr, p, j, 0
for i = 1 to 100
    Tick wd, g, p, k, lo(), pt()
next
was = was and abs(p.x - j) < 1 and Lull%(wd.lv, wd.se) = 20
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
was = was and p.x - j < -165 and p.x - j > -190 and Lull%(wd.lv, wd.se) = 120
wd.se.ticks = 2040
Stand wd.lv, wd.gr, p, j, 0
k.d = -1
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
was = was and abs(p.x - j) < 1 and hid > 20
wd.se.ticks = 2040
Stand wd.lv, wd.gr, p, j - 150, 200
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
k.d = 0
was = was and p.roll and p.x - (j - 150) > 88 and p.x - (j - 150) < 97
wd.se.ticks = 2040
Stand wd.lv, wd.gr, p, j, 0
k.hit = -1
k.jump = -1
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
    if p.ground and i > 5 then exit for
next
k.jump = 0
was = was and i > 50 and i < 57 and p.x - j < -150 and p.x - j > -175
' and it is over with its level
GoStage wd, 6, g, p, lo(), pt()
if was and wd.lv.gale = 0 then print "gale ok"
' the dusk's avalanche, none on the morning: it waits while he is short of its head. Stood 10 past it he is taken
' 45 ticks on, its front come from 220 behind its head at 300 a second; 30 ticks on, 80 behind him, a spot 30
' behind its front and 30 over the ground is under its snow, which it was not before. He starts again before it,
' a life the fewer, and it waits again
GoStage wd, 6, g, p, lo(), pt()
was = wd.lv.avalX = NONE
GoStage wd, 7, g, p, lo(), pt()
was = was and wd.lv.avalX <> NONE
wd.se.ticks = 1920
Stand wd.lv, wd.gr, p, wd.lv.avalX - 40, 0
k.d = -1
' till the act's card is off the view, which the spot read below may lie under
for i = 1 to 260
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.aval.st = 0 and not p.dead
wd.se.ticks = 1920
Stand wd.lv, wd.gr, p, wd.lv.avalX + 10, 0
p.x = wd.lv.avalX + 10
j = g.lives
g.shake = 0
Render wd, g, p, lo(), pt()
v = point(wd.lv.avalX - 100 - int(g.camx), cint(p.y) - 30 - int(g.camy))
for i = 1 to 80
    Tick wd, g, p, k, lo(), pt()
    if p.dead then exit for
    if i = 30 then
        g.shake = 0
        Render wd, g, p, lo(), pt()
        c = point(wd.lv.avalX - 100 - int(g.camx), cint(p.y) - 30 - int(g.camy))
    end if
next
' the snow's tones are 116 to 118
was = was and i >= 44 and i <= 46 and (v < 116 or v > 118) and c >= 116 and c <= 118
for i = 1 to 400
    Tick wd, g, p, k, lo(), pt()
    if not p.dead then exit for
next
k.d = 0
if was and i < 400 and g.lives = j - 1 and wd.lv.aval.st = 0 and p.x < wd.lv.avalX then print "avalanche ok"
' the pass's chase: Frost's toboggan is drawn as the ground lies, its slats level on the flat and, from 10 behind
' her middle to 8 before it, 9 lower on a fall of one in two, as Charlie's sled is. On his sled he bumps her and
' is checked to a sled's pace, not thrown; her last bump leaves the camera, and reaching it he has it
GoStage wd, 7, g, p, lo(), pt()
wd.se.ticks = 1920
Stand wd.lv, wd.gr, p, wd.lv.chaseX + 180, 0
k.d = -1
for i = 1 to 150
    Tick wd, g, p, k, lo(), pt()
next
was = wd.lv.cart.st = 1
for j = 0 to 1
    wd.lv.cart.x = wd.lv.chaseX + 230 - j * 118
    wd.lv.cart.y = Surf%(wd.lv, cint(wd.lv.cart.x), cint(p.y) + 8)
    g.shake = 0
    Render wd, g, p, lo(), pt()
    v = NONE
    c = NONE
    for i = cint(wd.lv.cart.y) - 40 - int(g.camy) to cint(wd.lv.cart.y) + 20 - int(g.camy)
        if i >= 0 and i < SH then
            if point(cint(wd.lv.cart.x) - 10 - int(g.camx), i) = 59 then v = i
            if point(cint(wd.lv.cart.x) + 8 - int(g.camx), i) = 59 then c = i
        end if
    next
    was = was and v <> NONE and c - v = j * 9
next
k.d = 0
wd.lv.cart.x = p.x + 60
wd.lv.cart.y = p.y
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
wd.lv.cart.x = p.x + 60
p.gsp = 300
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
    if wd.lv.cart.st = 2 then exit for
next
was = was and wd.lv.cart.st = 2 and wd.lv.cart.hits = NBUMP
p.sled = 0
p.roll = 0
Stand wd.lv, wd.gr, p, cint(wd.lv.cart.x), 0
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
for m = 0 to wd.lv.nMv - 1
    if wd.lv.mv(m).k = 7 then exit for
next
was = m < wd.lv.nMv
c = wd.lv.mv(m).s
p.x = wd.gr.pt(c).x + 24
p.y = wd.gr.pt(c).y
p.sg = c
p.ground = -1
p.gsp = 0
k.d = -1
for i = 1 to 400
    Tick wd, g, p, k, lo(), pt()
    if i = 86 then was = was and abs(p.y - (wd.lv.mv(m).y - CABLED \\ 2)) < 3
    if wd.lv.mv(m).t = 1 then exit for
next
was = was and i >= 171 and i <= 174 and p.ground and p.sg = c and abs(p.x - (wd.lv.mv(m).x + wd.lv.mv(m).a + 24)) < 2
was = was and abs(p.y - (wd.lv.mv(m).y - CABLED)) < 2 and wd.lv.ob(wd.lv.mv(m).o).k = KSEAT
k.d = 0
k.r = -1
wd.se.ticks = 1920
for i = 1 to 40
    Tick wd, g, p, k, lo(), pt()
next
k.r = 0
k.d = -1
was = was and p.ground and wd.gr.pt(p.sg).k = 1 and abs(p.y - (wd.lv.mv(m).y - CABLED)) < 2
was = was and p.x > wd.lv.mv(m).x + wd.lv.mv(m).a + MOVEW
for i = 1 to 200
    Tick wd, g, p, k, lo(), pt()
next
k.d = 0
if was and wd.lv.mv(m).t = 0 then print "chair ok"
' a button trodden raises its bridge out of the pit, and the bridge carries
GoStage wd, 2, g, p, lo(), pt()
for m = 0 to wd.lv.nMv - 1
    if wd.lv.mv(m).k = 4 then exit for
next
j = wd.lv.mv(m).b
c = wd.lv.mv(m).s
for i = 1 to 60
    Tick wd, g, p, k, lo(), pt()
next
was = wd.lv.mv(m).t = 0 and wd.lv.ob(j).s = 0
Stand wd.lv, wd.gr, p, wd.lv.ob(j).x - 12, 60
k.r = -1
for i = 1 to 20
    Tick wd, g, p, k, lo(), pt()
next
k.r = 0
was = was and wd.lv.ob(j).s = 1
for i = 1 to 90
    Tick wd, g, p, k, lo(), pt()
next
p.x = wd.gr.pt(c).x + 24
p.y = wd.gr.pt(c).y
p.sg = c
p.ground = -1
p.gsp = 0
for i = 1 to 30
    Tick wd, g, p, k, lo(), pt()
next
was = was and wd.lv.mv(m).t = wd.lv.mv(m).a and p.ground and p.sg = c
if was and abs(p.y - (wd.lv.mv(m).y - wd.lv.mv(m).a)) < 2 then print "bridge ok"
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
        Pilot wd.lv, wd.gr, wd.zn, wd.se, p, k
        v = wd.lv.cart.hits
        Tick wd, g, p, k, lo(), pt()
        if wd.lv.cart.hits > v and p.sled then rams = rams + 1
        if wd.lv.cart.st = 1 and wd.mu.zHeld = CHASETUNE + 1 and wd.lv.cart.x > wd.lv.chaseX + 600 then was = -1
        if g.mode <> PLAYING then exit for
    next
    ' the Downs' evening ends in a chase: the cart fled down the race to the chase's tune, was bumped till it broke,
    ' and its keepsake taken, the zone's tune back
    if m = 1 and was and wd.lv.cart.st = 3 and wd.lv.cart.hits = NBUMP and wd.mu.zHeld = 8 then print "chase ok"
    ' and the sands' sunset: the pilot hops out of the sea onto Kipper's boat till it sinks, and takes the teddy
    if m = 5 and wd.lv.cart.st = 3 and wd.lv.cart.hits = NBUMP then print "kipper ok"
    ' and the pass's dusk: Frost bumped off her toboggan, twice at the least sled against sled, and the camera
    ' taken. Checked by a bump, his sled never caught her up again: one bump of the three was made on it
    if m = 7 and wd.lv.cart.st = 3 and wd.lv.cart.hits = NBUMP and rams >= 2 then print "frost ok"
    print "act"; m; g.mode; g.lives - j; wd.zn.stage; wd.zn.id; cint(p.x)
    k.l = 0
    k.r = 0
    k.d = 0
    k.jump = 0
    k.hit = 0
next
' and after the last, the title, on the Amber Downs again
for i = 1 to 1600
    Tick wd, g, p, k, lo(), pt()
    if g.mode = TITLE then exit for
next
if g.mode = TITLE and wd.zn.id = 0 and wd.tn.dusk = 16 and wd.mu.zHeld = 1 then print "home ok"
' the Amber Downs in the shared colours again: a zone's own are put back when it is left
if zp and wd.tn.now(wd.tn.which(1, 0)) = wd.tn.day(wd.tn.which(1, 0)) then print "palette ok"
' the title's own run ends the same way: it began again the moment Charlie passed the goal, the family never met
for i = 1 to 9000
    Tick wd, g, p, k, lo(), pt()
    if p.x >= wd.lv.goalX then exit for
next
for i = 1 to 300
    Tick wd, g, p, k, lo(), pt()
next
was = g.mode = TITLE and p.win and wd.se.cheer and abs(p.x - (wd.lv.famX - 106)) < 3
for i = 1 to 900
    Tick wd, g, p, k, lo(), pt()
    if p.x < wd.lv.goalX then exit for
next
if was and p.x < wd.lv.goalX and wd.tn.dusk = 16 then print "title met ok"
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
k.r = -1
Tick wd, g, p, k, lo(), pt()
Tick wd, g, p, k, lo(), pt()
k.r = 0
Tick wd, g, p, k, lo(), pt()
k.r = -1
Tick wd, g, p, k, lo(), pt()
k.r = 0
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

' Charlie on the tiles' ground at x, running at v.
' Charlie stood at x under a shelf, on the ground as high as it is at x0, which the shelf is not over.
sub StandUnder (lv as Level, gr as Ground, p as Hero, x as integer, x0 as integer)
    Stand lv, gr, p, x0, 0
    p.x = x
    p.sg = SegAt%(lv, gr, x, p.y)
end sub

sub Stand (lv as Level, gr as Ground, p as Hero, x as integer, v as single)
    p.x = x
    p.y = GroundAt%(lv, x)
    p.gsp = v
    p.ground = -1
    p.roll = 0
    p.sg = SegAt%(lv, gr, x, p.y)
    p.ang = gr.pt(p.sg).a
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
    sx = cint(p.x) - int(g.camx)
    sy = cint(p.y) - int(g.camy)
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
        ("packing ok", "after a chase the level's end is the longer one, twelve bars; the eight elsewhere"),
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
        ("ice ok", "bare ice keeps his speed: a long slide, a long stop, a slow start; drawn bare of snow"),
        ("sled ok", "a sled is boarded, follows the slope, has no brake, keeps him through a hop, ends in deep snow"),
        ("geyser ok", "in the pass a jet is a geyser, the pass's water: it throws him up as it blows"),
        ("snowball ok", "deep snow rolled through fast makes him a snowball: it takes one hit, and ends with the roll"),
        ("avalanche ok", "the dusk's avalanche waits till he is past, comes on behind him, takes him where he stops"),
        ("toboggan ok", "Frost's toboggan lies as the ground does; on his sled a bump checks him; the camera is taken"),
        ("frost ok", "the pass's dusk ended in its chase: Frost bumped twice sled against sled, the camera taken"),
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
    for act in ("1", "2", "3", "4", "5", "6", "7", "8", "9"):
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
    print(f"charlie ok: cleared with {words[3]} bones")


if __name__ == "__main__":
    main()
