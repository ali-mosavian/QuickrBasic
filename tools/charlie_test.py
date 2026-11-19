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

DRIVER = """declare function GroundAt% (x as integer)
declare sub Stand (p as Hero, x as integer, v as single)
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
LoadTables
BuildArt
LoadLevel 0
dim g as Game, p as Hero, k as Pad, i as long, j as integer, loops as integer, was as integer
dim c as integer, v as integer, m as integer, zp as integer, seen as string
dim hid as long, shown as long, bt as single, colA(SH - 1) as integer, colB(SH - 1) as integer
dim lo(NLOOSE - 1) as Loose, pt(NPART - 1) as Part, cu(NCUE) as Cue
Muted = -1
StartGame g, p, lo(), pt()
for j = 0 to NObj - 1
    if ObK(j) = KCAT then
        ObX(j) = 24
        ObA(j) = 24
        ObY(j) = StartY
        g.camx = 0
        exit for
    end if
next
for i = 1 to 300
    Things g, lo(), pt()
next
print "edge ok"
Muted = 0
LoadSongs
for j = 0 to NSLOT - 1
    Coming = j
    StartSong
    play Beat$(j, Kit(j * NBAR * 2), 3)
    for i = 0 to NVOICE - 1
        play , , VoOne(j, i) + "V1", , , VoTwo(j, i) + "V1"
    next
next
play "T255", "T255", "T255", , "T255", "T255", "T255", "T255"
for j = 0 to NSLOT * NBAR * 2 - 1
    i = j * 5
    play Trk(i), Trk(i + 1), Trk(i + 2), , Trk(i + 3), Trk(i + 4), Org(j), Third(j)
next
for j = SJUMP to SBUMP
    Sfx j
next
print "sounds ok"
' the level's end has a tune of its own, played once through its eight bars: it was Charlie's theme from its
' hits, round and round
g.mode = CLEARED
PickSong SongFor%(g), -1
for i = 1 to 40
    NextHalf
    if Ended then exit for
next
if Sng = CAUGHT and i = 16 then print "caught ok"
' the intro has its own tune in the zone's slot, 28 bars that end: its kit, its voicings and its first half bar played
StartIntro g, cu()
PickSong SongFor%(g), 0
NextHalf
for i = 0 to NVOICE - 1
    play , , VoOne(ZTUNE, i) + "V1", , , VoTwo(ZTUNE, i) + "V1"
next
if Sng = ZTUNE and ZHeld = 4 and len(Order(ZTUNE)) = 28 and Ends(ZTUNE) then print "intro tune ok"
' the intro, played through and drawn: the view where the script has it, on the house, the hedge, the bump and
' where Charlie lands; a cue of every kind; Charlie stood by the road, then away at full tilt; at its end the
' title, on the Amber Downs in full colour with their own tune
Muted = -1
seen = string$(QSND, "0")
for i = 0 to NCUE
    if cu(i).kind = 0 then exit for
    mid$(seen, cu(i).kind, 1) = "1"
next
was = seen = string$(QSND, "1")
for i = 1 to 4000
    Advance g, p, k, lo(), pt(), cu()
    if g.mode <> INTRO then exit for
    if i mod 15 = 0 then IntroDraw g, p, cu()
    select case i
    case 60
        if point(0, 0) <> 0 or point(30, 100) < 130 or point(30, 100) > 136 then was = 0
    case 300
        if g.camx < 0 or g.camx > 60 or p.x < 70 or p.x > 250 then was = 0
    case 1200
        if g.camx <> 1400 then was = 0
    case 1680
        if g.camx <> 2140 then was = 0
    case 2700
        if g.camx <> 3900 or p.x <> 4060 or p.gsp <> 0 then was = 0
    case 3300
        if p.gsp < 400 or p.x - g.camx < 150 or p.x - g.camx > 300 then was = 0
    end select
next
if was and abs(i - ILEN * 30) < 3 and g.mode = TITLE and Stage = 0 and ZHeld = 1 and Dusk = 16 then print "intro ok"
was = 0
' a key skips it
StartIntro g, cu()
for i = 1 to 10
    Advance g, p, k, lo(), pt(), cu()
next
was = g.mode = INTRO
k.hit = -1
Advance g, p, k, lo(), pt(), cu()
if was and g.mode = TITLE and Stage = 0 and k.hit = 0 then print "skip ok"
was = 0
' before he jumps from the van Charlie is at its door, behind it: the van's side hides him and he shows past the
' door. He was drawn over them both. The picture with him and without, where the van's side is and past its door
StartIntro g, cu()
for i = 1 to 2145
    Advance g, p, k, lo(), pt(), cu()
next
bt = g.modeT * BEATS
m = OnCue%(cu(), QHERO, bt)
j = OnCue%(cu(), QVAN, bt)
CueAt cu(), j, bt, c, v
c = c - cint(g.camx)
IntroDraw g, p, cu()
hid = Pix&(c - 60, 8, HORIZON - 45, HORIZON - 14)
shown = Pix&(c - 92, 16, HORIZON - 45, HORIZON - 14)
cu(m).y0 = cu(m).y0 - 300
cu(m).y1 = cu(m).y1 - 300
IntroDraw g, p, cu()
hid = hid - Pix&(c - 60, 8, HORIZON - 45, HORIZON - 14)
shown = shown - Pix&(c - 92, 16, HORIZON - 45, HORIZON - 14)
if hid = 0 and shown <> 0 then print "door ok"
ResetWorld g, p, lo(), pt()
Muted = 0
g.mode = TITLE
Muted = -1
StartGame g, p, lo(), pt()
g.bones = 3
k.jump = -1
k.was = -1
Hurt g, p, cint(p.x) + 10, lo(), SHURT
for i = 1 to 150
    Tick g, p, k, lo(), pt()
    if p.stun = 0 and p.jumped then was = -1
next
if was then print "held ok"
was = 0
k.jump = 0
k.was = 0
' the first 45 degree ramp after flat ground, c its first column
for c = 8 to LW - 8
    j = GroundAt%(c * TS - 1)
    if j <> NONE and GroundAt%(c * TS - 40) = j and GroundAt%(c * TS + 15) = j - TS then exit for
next
' run onto it at every speed: at some, its rise ahead was taken for a wall and Charlie stopped dead at its foot
StartGame g, p, lo(), pt()
k.r = -1
for v = 120 to 840 step 60
    Stand p, c * TS - 30, csng(v)
    for i = 1 to 12
        MoveHero p, k, pt()
        if p.x > c * TS + 24 then exit for
        if p.ground and p.gsp < v / 2 then was = -1
    next
next
' and stood on it, no key down, it stays: it slid back to the foot
k.r = 0
Stand p, c * TS + 8, 0
for i = 1 to 90
    MoveHero p, k, pt()
next
if was = 0 and abs(p.x - (c * TS + 8)) < 2 then print "slope ok"
' a jump pressed a moment before landing is made on landing: it was lost
StartGame g, p, lo(), pt()
k.hit = -1
k.jump = -1
was = 0
for i = 1 to 120
    Tick g, p, k, lo(), pt()
    if was = 0 and not p.ground and p.vy > 0 and GroundAt%(cint(p.x)) - p.y < 10 then
        k.hit = -1
        was = 1
    end if
    if was = 1 and p.ground then was = 2
    if was = 2 and p.jumped and p.vy < -200 then was = 3
next
if was = 3 then print "buffer ok"
' and one pressed a moment after running off an edge is still made: it was lost too
StartGame g, p, lo(), pt()
for c = 8 to LW - 8
    if GroundAt%(c * TS) = NONE and GroundAt%(c * TS - 20) <> NONE then exit for
next
Stand p, c * TS - 20, 200
k.r = -1
k.jump = 0
k.hit = 0
was = 0
for i = 1 to 60
    MoveHero p, k, pt()
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
Stand p, c * TS - 20, 60
k.r = -1
k.jump = 0
k.hit = 0
v = cint(p.y)
for i = 1 to 45
    MoveHero p, k, pt()
next
if p.y > v + 20 then print "ledge ok"
was = 0
k.r = 0
k.jump = 0
k.hit = 0
k.was = 0
' dropped anywhere over the ground, Charlie lands: past the first loop it fell through the level
StartGame g, p, lo(), pt()
for c = 40 to LWPX - 40 step 7
    p.x = c
    p.y = 40
    p.vx = 0
    p.vy = 700
    p.ground = 0
    p.sg = 0
    for i = 1 to 200
        MoveHero p, k, pt()
        if p.ground or p.y > MH * TS then exit for
    next
    if p.ground = 0 and ColTop(c \\ TS) < MH then was = was + 1
next
if was = 0 then print "drop ok"
' a loop is ground: met fast, no key held, it is run round, over its top and out the far side; met slowly Charlie
' stalls on its side and slides back to its foot
for j = 0 to NObj - 1
    if ObK(j) = KLOOP then exit for
next
c = ObX(j)
Stand p, c - 8, 460
was = 0
for i = 1 to 300
    MoveHero p, k, pt()
    if p.ground then if GU(p.sg) < -.9 then was = -1
    if p.x > c + LOOPR + 12 then exit for
next
v = was and p.ground and p.x > c + LOOPR + 12
Stand p, c - 60, 200
was = 0
for i = 1 to 300
    MoveHero p, k, pt()
    if p.ground then if GU(p.sg) < 0 then was = -1
next
if v and was = 0 and p.ground and abs(p.x - c) < LOOPR and p.y > ObY(j) - 8 then print "loop ok"
' too slow at its top, Charlie falls from it and lands inside
Stand p, c - 8, 340
was = 0
for i = 1 to 200
    MoveHero p, k, pt()
    if not p.ground and p.y < ObY(j) - LOOPR then was = -1
next
if was and p.ground and p.x < c + LOOPR then print "stall ok"
' a jump before a loop comes down on the ground under it, not on the far half of the ring
Stand p, c - 30, 0
k.hit = -1
k.jump = -1
was = 0
for i = 1 to 90
    MoveHero p, k, pt()
    k.hit = 0
    if p.ground then if GK(p.sg) = 2 then was = -1
next
k.jump = 0
if was = 0 and p.ground and abs(p.y - ObY(j)) < 2 then print "ghost ok"
' a quarter pipe is run up, and its wall: off the top and onto the ground above
for j = 0 to NObj - 1
    if ObK(j) = KPIPE then exit for
next
c = ObX(j)
Stand p, c - 100, 360
k.r = -1
was = 0
for i = 1 to 300
    MoveHero p, k, pt()
    if p.ground then if GU(p.sg) < .1 then was = -1
    if p.ground and p.x > c + 8 then exit for
next
if was and p.ground and p.x > c + 8 and abs(p.y - ColTop(c \\ TS) * TS) < 2 then print "pipe ok"
' a ribbon is ground: run onto at speed it is ridden over each hump and off its far end onto the ground
for j = 0 to NObj - 1
    if ObK(j) = KWAVE then exit for
next
c = ObX(j)
Stand p, c - 40, 420
k.r = -1
was = 0
v = 0
for i = 1 to 400
    MoveHero p, k, pt()
    if p.ground then if GK(p.sg) = 2 and p.y < ObY(j) - WAVEA + 8 then was = was + 1
    ' down its far sides too: with only the rising halves there it was flown from crest to crest
    if p.ground then if GK(p.sg) = 2 and GV(p.sg) > .3 then v = v + 1
    if p.ground and GK(p.sg) = 1 and p.x > c + 384 then exit for
next
if was > 3 and v > 6 and p.ground and p.x > c + 384 and abs(p.y - ObY(j)) < 2 then print "wave ok"
' a ribbon is a twist, run on its inside: at its crest Charlie hangs under its line, not stood on it as on a hill
Stand p, c - 40, 420
was = 0
for i = 1 to 200
    MoveHero p, k, pt()
    if p.ground and abs(p.x - c - WAVEL) < 6 then
        g.camx = p.x - 160
        g.camy = p.y - 100
        Render g, p, lo(), pt()
        if Fur%(160, 8, 104, 124) > 40 and Fur%(160, 8, 76, 96) = 0 then was = -1
        exit for
    end if
next
if was then print "twist ok"
' a quarter round he is seen from over his back, across the line and none of him high over it as when stood
' on the rise; three quarters round the ribbon is before him
Stand p, c - 40, 420
was = 0
for i = 1 to 200
    MoveHero p, k, pt()
    if p.ground and (was and 1) = 0 and abs(p.x - c - WAVEL / 2) < 6 then
        g.camx = p.x - 160
        g.camy = p.y - 100
        Render g, p, lo(), pt()
        if Fur%(160, 2, 92, 99) > 20 and Fur%(160, 2, 103, 107) > 12 and Fur%(160, 8, 70, 87) = 0 then print "turn ok"
        was = was or 1
    end if
    if p.ground and abs(p.x - c - WAVEL * 1.5) < 6 then
        g.camx = p.x - 160
        g.camy = p.y - 100
        Render g, p, lo(), pt()
        if Fur%(160, 16, 60, 140) = 0 then print "hid ok"
        exit for
    end if
next
' and too slow for it, drops from under the crest to the pit's floor
Stand p, c - 40, 420
for i = 1 to 300
    if p.ground and p.x >= c + 150 and p.x < c + 230 then
        p.gsp = 100
        k.r = 0
    end if
    MoveHero p, k, pt()
    if p.ground and p.y > ObY(j) + 40 then exit for
next
if p.ground and GK(p.sg) = 1 and p.y > ObY(j) + 40 and p.x < c + 384 then print "slow ok"
' a bark stops a prowler where it is a while, and it goes on away from him: it walked on as if deaf
k.r = 0
StartGame g, p, lo(), pt()
j = -1
for i = 0 to NObj - 1
    c = ObA(i) - 40
    if ObK(i) = KCAT and j < 0 and abs(GroundAt%(c) - ObY(i)) < 8 then
        if SegAt%(c, csng(GroundAt%(c))) >= 0 then j = i
    end if
next
ObX(j) = ObA(j)
ObS(j) = -1
Stand p, ObA(j) - 40, 0
k.bark = -1
Tick g, p, k, lo(), pt()
was = 0
for i = 1 to 20
    Tick g, p, k, lo(), pt()
    if ObX(j) <> ObA(j) then was = was + 1
next
for i = 1 to 60
    Tick g, p, k, lo(), pt()
next
if was = 0 and ObX(j) > ObA(j) then print "bark ok"
was = 0
StartGame g, p, lo(), pt()
for i = 1 to 9000
    Pilot p, k
    Tick g, p, k, lo(), pt()
    j = 0
    if p.ground then j = GU(p.sg) < -.9
    if j and was = 0 then loops = loops + 1
    was = j
    if i mod 30 = 0 then Render g, p, lo(), pt()
    if g.mode <> PLAYING then exit for
next
print "result"; g.mode; g.lives; g.bones; loops; cint(p.x); cint(p.y)
k.l = 0
k.r = 0
k.d = 0
k.jump = 0
k.hit = 0
for i = 1 to 400
    Tick g, p, k, lo(), pt()
next
Render g, p, lo(), pt()
if p.win and abs(p.x - (FamX - 106)) < 3 and Cheer and abs(g.camx - (FamX - 166)) < 3 then print "met ok"
' it stays a while, fades, and the Downs' evening begins in full colour, the score and lives kept
was = 0
j = g.lives
for i = 1 to 900
    Tick g, p, k, lo(), pt()
    if Dusk < 8 then was = i
    if Stage = 1 then exit for
next
was = was > 60 and Stage = 1 and Zone = 0
if was and g.mode = PLAYING and Dusk = 16 and g.score > 0 and g.lives = j then print "faded ok"
' and in its own colours, over the shared ones
zp = ZPN(1) > 0 and Pal(ZPK(1, 0)) = ZPC(1, 0) and ZPC(1, 0) <> Pal0(ZPK(1, 0))
' the works have their own tune in the zone's slot, at 132: its kit, its voicings and its first half bar played. They
' had the Downs'
GoStage 2, g, p, lo(), pt()
PickSong SongFor%(g), 0
NextHalf
for i = 0 to NVOICE - 1
    play , , VoOne(ZTUNE, i) + "V1", , , VoTwo(ZTUNE, i) + "V1"
next
if Sng = ZTUNE and ZHeld = 5 and Tempo(ZTUNE) = 132 then print "works tune ok"
' a platform carries who stands on it, to and fro
for m = 0 to NMv - 1
    if MvK(m) = 0 then exit for
next
c = MvS(m)
p.x = GX(c) + 24
p.y = GY(c)
p.sg = c
p.ground = -1
p.gsp = 0
was = cint(p.x)
v = was
for i = 1 to 240
    Tick g, p, k, lo(), pt()
    if p.x < was then was = cint(p.x)
    if p.x > v then v = cint(p.x)
next
if p.ground and p.sg = c and abs(p.x - GX(c) - 24) < 2 and v - was > 40 then print "carried ok"
' who flees in a chase, met at the race's end: run into, its cart hurts Charlie and takes no bump; rolled into, it
' takes one and is dazed; dazed, it takes no other
GoStage 1, g, p, lo(), pt()
Cart.st = 1
Cart.x = ChaseEnd
Cart.y = GroundAt%(ChaseEnd)
g.bones = 3
k.r = -1
Stand p, ChaseEnd - 60, 200
for i = 1 to 40
    Tick g, p, k, lo(), pt()
next
was = p.inv > 0 and g.bones = 0 and Cart.hits = 0
g.bones = 3
p.inv = 0
p.stun = 0
Stand p, ChaseEnd - 60, 300
p.roll = -1
for i = 1 to 30
    Tick g, p, k, lo(), pt()
    if Cart.hits then exit for
next
c = Cart.hits = 1 and Cart.t > BOLTS and g.bones = 3 and not p.ground
Stand p, ChaseEnd - 10, 0
p.roll = -1
for i = 1 to 10
    Tick g, p, k, lo(), pt()
next
if was and c and Cart.hits = 1 and g.bones = 3 then print "cart ok"
was = 0
k.r = 0
' on a slope the cart follows it, each wheel on the ground under it: it was drawn level, a wheel sunk in the slope
' and a wheel in the air. The picture without the cart and with it, down its wheels' columns: halfway down a slope
' of the race, then at the slope's foot, its front wheel on the level
GoStage 1, g, p, lo(), pt()
g.modeT = 5
g.shake = 0
was = -1
c = ChaseX + 104
for j = 0 to 1
    do while c < ChaseEnd - 104
        v = GroundAt%(c + 13) - GroundAt%(c - 13)
        if j = 0 and v = 13 and GroundAt%(c + 30) - GroundAt%(c - 30) = 30 then exit do
        if j = 1 and v >= 5 and v <= 8 and GroundAt%(c + 30) = GroundAt%(c + 13) then exit do
        c = c + 8 - 6 * j
    loop
    Cart.st = 3
    Cart.x = c
    Cart.y = GroundAt%(c)
    Stand p, c - 200, 0
    g.camx = c - 160
    g.camy = Cart.y - 124
    Render g, p, lo(), pt()
    KeepCol 147, colA()
    KeepCol 173, colB()
    Cart.st = 1
    Render g, p, lo(), pt()
    v = Lowest%(147, colA()) - (GroundAt%(c - 13) - cint(g.camy))
    m = Lowest%(173, colB()) - (GroundAt%(c + 13) - cint(g.camy))
    if c >= ChaseEnd - 104 or abs(v) > 1 or abs(m) > 1 then was = 0
next
if was then print "tilt ok"
was = 0
' the works' night ends under Sprocket's crane, on a rail over a hall. The view holds the rail while it is fought; a
' jump does not reach it; a jet's throw does and bumps it: it is dazed and Charlie knocked back down
GoStage 3, g, p, lo(), pt()
for j = 0 to NObj - 1
    if ObK(j) = KJET and ObX(j) > ChaseX then exit for
next
c = ObX(j)
Stand p, c - 40, 0
Tick g, p, k, lo(), pt()
Box.t = 9
k.hit = -1
k.jump = -1
m = 0
for i = 1 to 70
    Cart.x = p.x
    Tick g, p, k, lo(), pt()
    if not p.ground then m = -1
next
k.jump = 0
v = cint(Cart.y) - RAILUP - cint(g.camy)
was = Cart.st = 1 and ZHeld = CHASETUNE + 1 and m and p.ground and Cart.hits = 0 and v >= 0 and v < 40
Ticks = 180 - (c mod 180)
Stand p, c, 0
Cart.x = c
for i = 1 to 40
    Tick g, p, k, lo(), pt()
    if Cart.hits then exit for
next
was = was and Cart.hits = 1 and Cart.t > BOLTS and p.vy > 0 and not p.ground
' and a hoop's throw does, from the side
for j = 0 to NObj - 1
    if ObK(j) = KHOOP and ObX(j) > ChaseX then exit for
next
Cart.t = 0
Cart.x = ObX(j) + 200
Box.t = 9
p.x = ObX(j) - 12
p.y = ObY(j) + 12
p.ground = 0
p.vx = 300
p.vy = 0
m = 0
for i = 1 to 40
    Tick g, p, k, lo(), pt()
    if p.vx > 500 then m = -1
    if Cart.hits > 1 then exit for
next
if was and m and Cart.hits = 2 then print "crane ok"
' its crate is held over Charlie stood still, let go, and hurts him; he aside, it breaks on the floor and the next
' is a while off
GoStage 3, g, p, lo(), pt()
Stand p, ChaseX + 300, 0
Tick g, p, k, lo(), pt()
Cart.x = p.x
Box.t = 0
g.bones = 3
m = 0
for i = 1 to 120
    Tick g, p, k, lo(), pt()
    if Box.st = 1 then m = -1
    if p.inv > 0 then exit for
next
was = m and p.inv > 0 and g.bones = 0 and Box.st = 0
p.inv = 0
p.stun = 0
g.bones = 3
Stand p, ChaseX + 300, 0
Cart.x = p.x
Cart.v = 0
Box.t = 0
m = 0
for i = 1 to 120
    Tick g, p, k, lo(), pt()
    if Box.st = 2 and m = 0 then
        Stand p, ChaseX + 240, 0
        m = -1
    end if
    if m and Box.st = 0 then exit for
next
if was and m and p.inv = 0 and g.bones = 3 and Box.st = 0 and Box.t > 1 then print "crate ok"
' its third bump breaks it: the toolbox falls, is not Charlie's while it does, and is once it lies; the works' own
' tune is back
Cart.hits = NBUMP - 1
Cart.t = 0
Box.t = 9
Ticks = 180 - (c mod 180)
Stand p, c, 0
Cart.x = c
hid = g.score
for i = 1 to 40
    Tick g, p, k, lo(), pt()
    if Cart.st = 2 then exit for
next
was = Cart.st = 2 and Cart.hits = NBUMP and ZHeld = 5
for i = 1 to 10
    Tick g, p, k, lo(), pt()
next
was = was and Cart.st = 2
for i = 1 to 60
    Stand p, cint(Cart.x), 0
    Tick g, p, k, lo(), pt()
next
if was and Cart.st = 3 and g.score = hid + 1500 then print "toolbox ok"
was = 0
' a zone's own pictures are in the store only while it is played, where the last zone's were: after the Downs the
' works' tank is as the art tool drew it, after the works the Downs' tree, and the store no fuller for the change
GoStage 0, g, p, lo(), pt()
hid = NextAt
GoStage 2, g, p, lo(), pt()
line (0, 0)-(63, 79), 0, BF
Blit 0, 0, ITANK, 32, 40
print "zonetank"; Pix&(0, 32, 0, 39)
GoStage 0, g, p, lo(), pt()
line (0, 0)-(63, 79), 0, BF
Blit 0, 0, ITREE, 64, 80
print "zonetree"; Pix&(0, 64, 0, 79); NextAt - hid; ZoneAt > 0
' a lift goes up under Charlie to the ground above, and back down once it is left
GoStage 2, g, p, lo(), pt()
for m = 0 to NMv - 1
    if MvK(m) = 2 then exit for
next
c = MvS(m)
p.x = GX(c) + 20
p.y = GY(c)
p.sg = c
p.ground = -1
p.gsp = 0
for i = 1 to 200
    Tick g, p, k, lo(), pt()
next
was = MvT(m) = MvA(m) and p.ground and abs(p.y - (MvY(m) - MvA(m))) < 2
k.r = -1
for i = 1 to 60
    Tick g, p, k, lo(), pt()
next
k.r = 0
was = was and p.ground and GK(p.sg) = 1 and p.x > MvX(m) + MOVEW
for i = 1 to 200
    Tick g, p, k, lo(), pt()
next
if was and MvT(m) = 0 then print "lift ok"
' a basket goes down its cable under Charlie, to the ground past the pit
GoStage 2, g, p, lo(), pt()
for m = 0 to NMv - 1
    if MvK(m) = 3 then exit for
next
c = MvS(m)
p.x = GX(c) + 24
p.y = GY(c)
p.sg = c
p.ground = -1
p.gsp = 0
for i = 1 to 300
    Tick g, p, k, lo(), pt()
next
was = p.ground and p.sg = c and abs(p.x - (MvX(m) + MvA(m) + 24)) < 2
if was and abs(p.y - (MvY(m) + CABLED)) < 2 then print "cable ok"
' a button trodden raises its bridge out of the pit, and the bridge carries
GoStage 2, g, p, lo(), pt()
for m = 0 to NMv - 1
    if MvK(m) = 4 then exit for
next
j = MvB(m)
c = MvS(m)
for i = 1 to 60
    Tick g, p, k, lo(), pt()
next
was = MvT(m) = 0 and ObS(j) = 0
Stand p, ObX(j) - 12, 60
k.r = -1
for i = 1 to 20
    Tick g, p, k, lo(), pt()
next
k.r = 0
was = was and ObS(j) = 1
for i = 1 to 90
    Tick g, p, k, lo(), pt()
next
p.x = GX(c) + 24
p.y = GY(c)
p.sg = c
p.ground = -1
p.gsp = 0
for i = 1 to 30
    Tick g, p, k, lo(), pt()
next
was = was and MvT(m) = MvA(m) and p.ground and p.sg = c
if was and abs(p.y - (MvY(m) - MvA(m))) < 2 then print "bridge ok"
' the Amber Downs at evening played through by the pilot on one life; after its family and its fade the Chrome
' Works, their platforms and lifts ridden, the works by night, then Saltwick Sands at noon and at sunset: each
' cleared on that life
GoStage 1, g, p, lo(), pt()
j = g.lives
was = 0
for m = 1 to NSTAGE - 1
    for i = 1 to 1600
        if Stage = m and g.mode = PLAYING then exit for
        Tick g, p, k, lo(), pt()
    next
    for i = 1 to 16000
        Pilot p, k
        Tick g, p, k, lo(), pt()
        if Cart.st = 1 and ZHeld = CHASETUNE + 1 and Cart.x > ChaseX + 600 then was = -1
        if g.mode <> PLAYING then exit for
    next
    ' the Downs' evening ends in a chase: the cart fled down the race to the chase's tune, was bumped till it broke,
    ' and its keepsake taken, the zone's tune back
    if m = 1 and was and Cart.st = 3 and Cart.hits = NBUMP and ZHeld = 1 then print "chase ok"
    print "act"; m; g.mode; g.lives - j; Stage; Zone; cint(p.x)
    k.l = 0
    k.r = 0
    k.d = 0
    k.jump = 0
    k.hit = 0
next
' and after the last, the title, on the Amber Downs again
for i = 1 to 1600
    Tick g, p, k, lo(), pt()
    if g.mode = TITLE then exit for
next
if g.mode = TITLE and Zone = 0 and Dusk = 16 and ZHeld = 1 then print "home ok"
' the Amber Downs in the shared colours again: a zone's own are put back when it is left
if zp and Pal(ZPK(1, 0)) = Pal0(ZPK(1, 0)) then print "palette ok"
' the title's own run ends the same way: it began again the moment Charlie passed the goal, the family never met
for i = 1 to 9000
    Tick g, p, k, lo(), pt()
    if p.x >= GoalX then exit for
next
for i = 1 to 300
    Tick g, p, k, lo(), pt()
next
was = g.mode = TITLE and p.win and Cheer and abs(p.x - (FamX - 106)) < 3
for i = 1 to 900
    Tick g, p, k, lo(), pt()
    if p.x < GoalX then exit for
next
if was and p.x < GoalX and Dusk = 16 then print "title met ok"
' progress is kept at each zone: the last, reached in the game above, is in CHARLIE.SAV and read back; a file that
' is not the game's counts for nothing; a zone reached is written. On the title Right picks a zone reached and no
' further, and a game starts at its first act
Reached = 0
LoadSave
was = Reached = ZONES - 1
open "CHARLIE.SAV" for binary as #2
hid = 12345
put #2, , hid
close #2
LoadSave
was = was and Reached = 0
Reach 1
Reached = 0
LoadSave
was = was and Reached = 1 and Pick = 0
k.r = -1
Tick g, p, k, lo(), pt()
Tick g, p, k, lo(), pt()
k.r = 0
Tick g, p, k, lo(), pt()
k.r = -1
Tick g, p, k, lo(), pt()
k.r = 0
was = was and Pick = 1 and g.mode = TITLE
k.start = -1
Tick g, p, k, lo(), pt()
if was and g.mode = PLAYING and Stage = 2 and g.lives = 3 then print "save ok"
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
sub Stand (p as Hero, x as integer, v as single)
    p.x = x
    p.y = GroundAt%(x)
    p.gsp = v
    p.ground = -1
    p.roll = 0
    p.sg = SegAt%(x, p.y)
    p.ang = GA(p.sg)
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
function GroundAt% (x as integer)
    dim y as integer
    GroundAt% = NONE
    for y = 16 to MH * TS - 17
        if Solid%(x, y) then
            GroundAt% = y
            exit function
        end if
    next
end function
"""


def pix(p: charlieart.Pic) -> int:
    """A picture summed by place as the driver's Pix& sums the screen."""
    return sum(c * ((x + y * 7) % 31 + 1) for y, row in enumerate(p.p) for x, c in enumerate(row))


def refused(levels: str) -> bool:
    """Whether the art tool will not pack these levels."""
    kept = charlieart.LEVELS
    with tempfile.TemporaryDirectory() as tmp:
        charlieart.LEVELS = Path(tmp) / "levels.txt"
        charlieart.LEVELS.write_text(levels)
        try:
            charlieart.levels()
        except SystemExit:
            return True
        finally:
            charlieart.LEVELS = kept
    return False


def main() -> None:
    exe = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build/host/basic").resolve()
    src = DEMO.read_text()
    assert "\nMain\nend\n" in src, "CHARLIE.BAS's main call moved"
    art = subprocess.run([sys.executable, ROOT / "tools/charlieart.py", "--check"], capture_output=True, text=True)
    if art.returncode:
        sys.exit(f"charlie: the art tool does not run, or CHARLIE.DAT is not what it writes: {art.stderr[-300:]}")
    # a level's command the game has no case for it would pass over, and a level without its end it would read past:
    # the art tool packs neither
    lv = charlieart.LEVELS.read_text()
    if refused(lv) or not refused(lv.replace("boost,0", "bosst,0", 1)) or not refused(lv.replace(", end,0", "", 1)):
        sys.exit("charlie: the art tool packs a level with a command misspelt or without its end, or none")
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "T.BAS"
        path.write_text(src.replace("\nMain\nend\n", "\n" + DRIVER, 1))
        shutil.copy(DEMO.with_suffix(".DAT"), tmp)
        run = subprocess.run([exe, path.name], cwd=tmp, capture_output=True, timeout=120)
    out = run.stdout.decode(errors="replace") + run.stderr.decode(errors="replace")
    if "edge ok" not in out:
        sys.exit(f"charlie: a cat at the level's left end: {out[-300:]}")
    if "sounds ok" not in out:
        sys.exit(f"charlie: a bar of the tune or a sound effect: {out[-300:]}")
    if "held ok" not in out:
        sys.exit(f"charlie: jump held through a hit, and no jump as it ends: {out[-300:]}")
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
        ("bridge ok", "a bridge raised by its button, to stand on"),
        ("works tune ok", "the Chrome Works played to their own tune"),
        ("cart ok", "a chase's cart hurts Charlie run into it, is bumped by him curled, and not again while dazed"),
        ("chase ok", "the Downs' evening ended in its chase: the cart fled, broken by three bumps, its keepsake taken"),
        ("tilt ok", "on a slope the chase's cart follows it, each wheel on the ground under it"),
        ("crane ok", "the works' crane is in view, out of a jump's reach, and bumped by a jet's throw and a hoop's"),
        ("crate ok", "the crane's crate hurts Charlie stood under it and breaks on the floor when he steps aside"),
        ("toolbox ok", "the crane's third bump breaks it: its toolbox falls, and is Charlie's once it lies"),
        ("home ok", "back at the title on the Amber Downs, to their tune, after the Chrome Works"),
        ("palette ok", "a zone shown in its own colours, the shared ones back after it"),
        ("save ok", "the furthest zone reached kept in CHARLIE.SAV, picked on the title and started at"),
    ):
        if said not in out:
            sys.exit(f"charlie: not {what}: {out[-300:]}")
    tank, tree = out[out.rfind("zonetank") :].split(), out[out.rfind("zonetree") :].split()
    if tank[1:2] != [str(pix(charlieart.tank()))] or tree[1:4] != [str(pix(charlieart.tree())), "0", "-1"]:
        sys.exit(
            f"charlie: a zone's own pictures not the store's while it is played, or the store fuller: {out[-300:]}"
        )
    if "title met ok" not in out:
        sys.exit(f"charlie: the title's run not ended with the family, held and faded: {out[-300:]}")
    if "faded ok" not in out:
        sys.exit(f"charlie: the level's end not held, faded out and begun again in full colour: {out[-300:]}")
    for act in ("1", "2", "3", "4", "5"):
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
