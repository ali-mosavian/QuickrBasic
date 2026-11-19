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
import tempfile
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEMO = ROOT / "src/plat/wasm/web/demos/CHARLIE.BAS"

DRIVER = """DECLARE FUNCTION GroundAt% (x AS INTEGER)
DECLARE SUB Stand (p AS Hero, x AS INTEGER, v AS SINGLE)
DECLARE FUNCTION Uneven% (hz AS SINGLE, grain AS INTEGER, per AS INTEGER, most AS INTEGER, want AS INTEGER)
' the game's steps against a screen's frames, timed by a clock of 128 or 256 moves a second: stepped by that clock
' some frames showed two steps and some none, and the picture juddered. At a screen's 59.94 or 60.1 frames a second
' each shows a step; at 119.88 every other; at 75 four of five, never three; and with no screen, looked at 20000
' times a second, the steps still come
IF Uneven%(59.94, 128, 1, 1, 600) + Uneven%(60.1, 256, 1, 1, 600) = 0 THEN PRINT "paced 60 ok"
IF Uneven%(119.88, 128, 2, 1, 300) = 0 THEN PRINT "paced 120 ok"
IF Uneven%(75, 256, 0, 2, 480) + Uneven%(20000, 128, 0, 1, 2) = 0 THEN PRINT "paced by time ok"
OpenScreen
LoadTables
BuildArt
LoadLevel 0
DIM g AS Game, p AS Hero, k AS Pad, i AS LONG, j AS INTEGER, loops AS INTEGER, was AS INTEGER
DIM c AS INTEGER, v AS INTEGER, m AS INTEGER
DIM lo(NLOOSE - 1) AS Loose, pt(NPART - 1) AS Part
Muted = -1
StartGame g, p, lo(), pt()
FOR j = 0 TO NObj - 1
    IF ObK(j) = KCAT THEN ObX(j) = 24: ObA(j) = 24: ObY(j) = StartY: g.camx = 0: EXIT FOR
NEXT
FOR i = 1 TO 300: Things g, lo(), pt(): NEXT
PRINT "edge ok"
Muted = 0
LoadSong
PLAY Beat$(Kit(0), 3), "V1", Horn(0), , "V1", Horn(1)
PLAY , , Keys(0), , , Keys(1)
PLAY "T255", "T255", "T255", , "T255", "T255", "T255", "T255"
FOR j = 0 TO NBAR * 4 - 1
    i = j * 5
    PLAY Trk(i), Trk(i + 1), Trk(i + 2), , Trk(i + 3), Trk(i + 4), Org(j), Third(j)
NEXT
FOR j = SJUMP TO SHISS: Sfx j: NEXT
Muted = -1
PRINT "sounds ok"
StartGame g, p, lo(), pt()
g.bones = 3: k.jump = -1: k.was = -1
Hurt g, p, CINT(p.x) + 10, lo(), SHURT
FOR i = 1 TO 150
    Tick g, p, k, lo(), pt()
    IF p.stun = 0 AND p.jumped THEN was = -1
NEXT
IF was THEN PRINT "held ok"
was = 0: k.jump = 0: k.was = 0
' the first 45 degree ramp after flat ground, c its first column
FOR c = 8 TO LW - 8
    j = GroundAt%(c * TS - 1)
    IF j <> NONE AND GroundAt%(c * TS - 40) = j AND GroundAt%(c * TS + 15) = j - TS THEN EXIT FOR
NEXT
' run onto it at every speed: at some, its rise ahead was taken for a wall and Charlie stopped dead at its foot
StartGame g, p, lo(), pt()
k.r = -1
FOR v = 120 TO 840 STEP 60
    Stand p, c * TS - 30, CSNG(v)
    FOR i = 1 TO 12
        MoveHero p, k, pt()
        IF p.x > c * TS + 24 THEN EXIT FOR
        IF p.ground AND p.gsp < v / 2 THEN was = -1
    NEXT
NEXT
' and stood on it, no key down, it stays: it slid back to the foot
k.r = 0
Stand p, c * TS + 8, 0
FOR i = 1 TO 90: MoveHero p, k, pt(): NEXT
IF was = 0 AND ABS(p.x - (c * TS + 8)) < 2 THEN PRINT "slope ok"
' a jump pressed a moment before landing is made on landing: it was lost
StartGame g, p, lo(), pt()
k.hit = -1: k.jump = -1: was = 0
FOR i = 1 TO 120
    Tick g, p, k, lo(), pt()
    IF was = 0 AND NOT p.ground AND p.vy > 0 AND GroundAt%(CINT(p.x)) - p.y < 10 THEN k.hit = -1: was = 1
    IF was = 1 AND p.ground THEN was = 2
    IF was = 2 AND p.jumped AND p.vy < -200 THEN was = 3
NEXT
IF was = 3 THEN PRINT "buffer ok"
' and one pressed a moment after running off an edge is still made: it was lost too
StartGame g, p, lo(), pt()
FOR c = 8 TO LW - 8
    IF GroundAt%(c * TS) = NONE AND GroundAt%(c * TS - 20) <> NONE THEN EXIT FOR
NEXT
Stand p, c * TS - 20, 200
k.r = -1: k.jump = 0: k.hit = 0: was = 0
FOR i = 1 TO 60
    MoveHero p, k, pt()
    k.hit = 0
    IF NOT p.ground THEN was = was + 1
    IF was = 3 THEN k.hit = -1: k.jump = -1
    IF was = 4 THEN EXIT FOR
NEXT
IF p.jumped AND p.vy < -250 THEN PRINT "edge jump ok"
' walked slowly off an edge, Charlie falls at once: a foot still over the ground put it back on the edge, and it
' hung there half a second, till it was fast enough to clear it in a step
Stand p, c * TS - 20, 60
k.r = -1: k.jump = 0: k.hit = 0
v = CINT(p.y)
FOR i = 1 TO 45: MoveHero p, k, pt(): NEXT
IF p.y > v + 20 THEN PRINT "ledge ok"
was = 0: k.r = 0: k.jump = 0: k.hit = 0: k.was = 0
' dropped anywhere over the ground, Charlie lands: past the first loop it fell through the level
StartGame g, p, lo(), pt()
FOR c = 40 TO LWPX - 40 STEP 7
    p.x = c: p.y = 40: p.vx = 0: p.vy = 700: p.ground = 0: p.sg = 0
    FOR i = 1 TO 200
        MoveHero p, k, pt()
        IF p.ground OR p.y > MH * TS THEN EXIT FOR
    NEXT
    IF p.ground = 0 AND ColTop(c \\ TS) < MH THEN was = was + 1
NEXT
IF was = 0 THEN PRINT "drop ok"
' a loop is ground: met fast, no key held, it is run round, over its top and out the far side; met slowly Charlie
' stalls on its side and slides back to its foot
FOR j = 0 TO NObj - 1
    IF ObK(j) = KLOOP THEN EXIT FOR
NEXT
c = ObX(j)
Stand p, c - 8, 460
was = 0
FOR i = 1 TO 300
    MoveHero p, k, pt()
    IF p.ground THEN IF GU(p.sg) < -.9 THEN was = -1
    IF p.x > c + LOOPR + 12 THEN EXIT FOR
NEXT
v = was AND p.ground AND p.x > c + LOOPR + 12
Stand p, c - 60, 200
was = 0
FOR i = 1 TO 300
    MoveHero p, k, pt()
    IF p.ground THEN IF GU(p.sg) < 0 THEN was = -1
NEXT
IF v AND was = 0 AND p.ground AND ABS(p.x - c) < LOOPR AND p.y > ObY(j) - 8 THEN PRINT "loop ok"
' too slow at its top, Charlie falls from it and lands inside
Stand p, c - 8, 340
was = 0
FOR i = 1 TO 200
    MoveHero p, k, pt()
    IF NOT p.ground AND p.y < ObY(j) - LOOPR THEN was = -1
NEXT
IF was AND p.ground AND p.x < c + LOOPR THEN PRINT "stall ok"
' a jump before a loop comes down on the ground under it, not on the far half of the ring
Stand p, c - 30, 0
k.hit = -1: k.jump = -1: was = 0
FOR i = 1 TO 90
    MoveHero p, k, pt()
    k.hit = 0
    IF p.ground THEN IF GK(p.sg) = 2 THEN was = -1
NEXT
k.jump = 0
IF was = 0 AND p.ground AND ABS(p.y - ObY(j)) < 2 THEN PRINT "ghost ok"
' a quarter pipe is run up, and its wall: off the top and onto the ground above
FOR j = 0 TO NObj - 1
    IF ObK(j) = KPIPE THEN EXIT FOR
NEXT
c = ObX(j)
Stand p, c - 100, 360
k.r = -1: was = 0
FOR i = 1 TO 300
    MoveHero p, k, pt()
    IF p.ground THEN IF GU(p.sg) < .1 THEN was = -1
    IF p.ground AND p.x > c + 8 THEN EXIT FOR
NEXT
IF was AND p.ground AND p.x > c + 8 AND ABS(p.y - ColTop(c \\ TS) * TS) < 2 THEN PRINT "pipe ok"
was = 0: k.r = 0
StartGame g, p, lo(), pt()
FOR i = 1 TO 9000
    Pilot p, k
    Tick g, p, k, lo(), pt()
    j = 0
    IF p.ground THEN j = GU(p.sg) < -.9
    IF j AND was = 0 THEN loops = loops + 1
    was = j
    IF i MOD 30 = 0 THEN Render g, p, lo(), pt()
    IF g.mode <> PLAYING THEN EXIT FOR
NEXT
PRINT "result"; g.mode; g.lives; g.bones; loops; CINT(p.x); CINT(p.y)
k.l = 0: k.r = 0: k.d = 0: k.jump = 0: k.hit = 0
FOR i = 1 TO 400: Tick g, p, k, lo(), pt(): NEXT
Render g, p, lo(), pt()
IF p.win AND ABS(p.x - (FamX - 106)) < 3 AND Cheer AND ABS(g.camx - (FamX - 166)) < 3 THEN PRINT "met ok"
' it stays a while, fades, and the Chrome Works begin in full colour, the score and lives kept
was = 0: j = g.lives
FOR i = 1 TO 900
    Tick g, p, k, lo(), pt()
    IF Dusk < 8 THEN was = i
    IF Zone = 1 THEN EXIT FOR
NEXT
IF was > 60 AND Zone = 1 AND g.mode = PLAYING AND Dusk = 16 AND g.score > 0 AND g.lives = j THEN PRINT "faded ok"
' a platform carries who stands on it, to and fro
FOR m = 0 TO NMv - 1
    IF MvK(m) = 0 THEN EXIT FOR
NEXT
c = MvS(m)
p.x = GX(c) + 24: p.y = GY(c): p.sg = c: p.ground = -1: p.gsp = 0
was = CINT(p.x): v = was
FOR i = 1 TO 240
    Tick g, p, k, lo(), pt()
    IF p.x < was THEN was = CINT(p.x)
    IF p.x > v THEN v = CINT(p.x)
NEXT
IF p.ground AND p.sg = c AND ABS(p.x - GX(c) - 24) < 2 AND v - was > 40 THEN PRINT "carried ok"
' a lift goes up under Charlie to the ground above, and back down once it is left
NextZone g, p, lo(), pt()
FOR m = 0 TO NMv - 1
    IF MvK(m) = 2 THEN EXIT FOR
NEXT
c = MvS(m)
p.x = GX(c) + 20: p.y = GY(c): p.sg = c: p.ground = -1: p.gsp = 0
FOR i = 1 TO 200: Tick g, p, k, lo(), pt(): NEXT
was = MvT(m) = MvA(m) AND p.ground AND ABS(p.y - (MvY(m) - MvA(m))) < 2
k.r = -1
FOR i = 1 TO 60: Tick g, p, k, lo(), pt(): NEXT
k.r = 0
was = was AND p.ground AND GK(p.sg) = 1 AND p.x > MvX(m) + MOVEW
FOR i = 1 TO 200: Tick g, p, k, lo(), pt(): NEXT
IF was AND MvT(m) = 0 THEN PRINT "lift ok"
' the Chrome Works played through by the pilot on one life, its platforms and lifts ridden
NextZone g, p, lo(), pt()
j = g.lives
FOR i = 1 TO 14000
    Pilot p, k
    Tick g, p, k, lo(), pt()
    IF g.mode <> PLAYING THEN EXIT FOR
NEXT
PRINT "works"; g.mode; g.lives - j; Zone; CINT(p.x); CINT(p.y)
k.l = 0: k.r = 0: k.d = 0: k.jump = 0: k.hit = 0
' and after its family and its fade, the title, on the Amber Downs again
FOR i = 1 TO 1600
    Tick g, p, k, lo(), pt()
    IF g.mode = TITLE THEN EXIT FOR
NEXT
IF g.mode = TITLE AND Zone = 0 AND Dusk = 16 THEN PRINT "home ok"
' the title's own run ends the same way: it began again the moment Charlie passed the goal, the family never met
FOR i = 1 TO 9000
    Tick g, p, k, lo(), pt()
    IF p.x >= GoalX THEN EXIT FOR
NEXT
FOR i = 1 TO 300: Tick g, p, k, lo(), pt(): NEXT
was = g.mode = TITLE AND p.win AND Cheer AND ABS(p.x - (FamX - 106)) < 3
FOR i = 1 TO 900
    Tick g, p, k, lo(), pt()
    IF p.x < GoalX THEN EXIT FOR
NEXT
IF was AND p.x < GoalX AND Dusk = 16 THEN PRINT "title met ok"
END

' 900 frames at hz a second, not in step with the clock, their times read off it at grain moves a second, the first
' 300 to settle: the
' frames whose steps are not as they should be (per > 0: one step every per frames, exactly; most: never more in a
' frame), and whether the steps in all are more than 3 from want.
FUNCTION Uneven% (hz AS SINGLE, grain AS INTEGER, per AS INTEGER, most AS INTEGER, want AS INTEGER)
    DIM c AS Clock, i AS LONG, n AS INTEGER, bad AS INTEGER, tot AS LONG, t AS DOUBLE, was AS DOUBLE, since AS INTEGER
    c.span = STEPT
    FOR i = 1 TO 900
        t = INT((i / CDBL(hz) + .0031) * grain) / CDBL(grain)
        Pace CSNG(t - was), c
        was = t
        n = Steps%(c)
        IF i > 300 THEN
            tot = tot + n
            IF n > most THEN bad = bad + 1
            IF per THEN
                since = since + 1
                IF n THEN
                    IF since <> per AND tot > n THEN bad = bad + 1
                    since = 0
                END IF
            END IF
        END IF
    NEXT
    IF ABS(tot - want) > 3 THEN bad = bad + 1
    Uneven% = bad
END FUNCTION

' Charlie on the tiles' ground at x, running at v.
SUB Stand (p AS Hero, x AS INTEGER, v AS SINGLE)
    p.x = x: p.y = GroundAt%(x): p.gsp = v: p.ground = -1: p.roll = 0
    p.sg = SegAt%(x, p.y): p.ang = GA(p.sg)
END SUB

' The ground's top under x, NONE over a gap.
FUNCTION GroundAt% (x AS INTEGER)
    DIM y AS INTEGER
    GroundAt% = NONE
    FOR y = 16 TO MH * TS - 17
        IF Solid%(x, y) THEN GroundAt% = y: EXIT FUNCTION
    NEXT
END FUNCTION
"""


def main() -> None:
    exe = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build/host/basic").resolve()
    src = DEMO.read_text()
    assert "\nMain\nEND\n" in src, "CHARLIE.BAS's main call moved"
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "T.BAS"
        path.write_text(src.replace("\nMain\nEND\n", "\n" + DRIVER, 1))
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
        ("carried ok", "carried by the platform stood on"),
        ("lift ok", "taken up by a lift, which goes back down when left"),
        ("home ok", "back at the title on the Amber Downs after the Chrome Works"),
    ):
        if said not in out:
            sys.exit(f"charlie: not {what}: {out[-300:]}")
    if "title met ok" not in out:
        sys.exit(f"charlie: the title's run not ended with the family, held and faded: {out[-300:]}")
    if "faded ok" not in out:
        sys.exit(f"charlie: the level's end not held, faded out and begun again in full colour: {out[-300:]}")
    works = out[out.rfind("works") :].split()
    if works[1:4] != ["2", "0", "1"]:
        sys.exit(f"charlie: the Chrome Works not cleared on one life (mode, lives lost, zone, x, y): {out[-300:]}")
    words = out[out.rfind("result") :].split()
    if len(words) < 5 or words[1:3] != ["2", "3"] or words[4] != "5":
        sys.exit(
            f"charlie: not cleared on the first life round 5 loops (mode, lives, bones, loops, x, y): {out[-300:]}"
        )
    print(f"charlie ok: cleared with {words[3]} bones")


if __name__ == "__main__":
    main()
