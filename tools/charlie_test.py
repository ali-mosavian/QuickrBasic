#!/usr/bin/env python3
"""charlie_test.py [EXE]: CHARLIE.BAS's level played through by an autopilot on the host build.

The main loop is swapped for one that plays with the game's own pilot, which holds Right and jumps
at pits, walls and cats ahead. The level must be cleared on the first life, round all five of
its loops: a gap too wide, a wall too high, a loop entered too slowly or a landing that misses the
ground fails it.

Every half bar of both tunes and every sound effect is played: a PLAY string the game builds wrong is an
Illegal function call mid-game. Jump held through a hit must jump as the hit's hold ends: it was forgotten.
First a cat is walked to the level's left end: its look ahead,
left of column 0, was a negative tile column, and the game stopped with Subscript out of range.
"""

import sys
import tempfile
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEMO = ROOT / "src/plat/wasm/web/demos/CHARLIE.BAS"

DRIVER = """OpenScreen
LoadTables
LoadLevel
BuildArt
DIM g AS Game, p AS Hero, k AS Pad, i AS LONG, j AS INTEGER, loops AS INTEGER, was AS INTEGER
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
PLAY "T255", "T255", "T255", , "T255", "T255", "T255"
FOR j = 0 TO NBAR * 4 - 1: i = j * 5: PLAY Trk(i), Trk(i + 1), Trk(i + 2), , Trk(i + 3), Trk(i + 4), Org(j): NEXT
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
StartGame g, p, lo(), pt()
FOR i = 1 TO 9000
    Pilot p, k
    Tick g, p, k, lo(), pt()
    IF p.onloop <> 0 AND was = 0 THEN loops = loops + 1
    was = p.onloop
    IF i MOD 30 = 0 THEN Render g, p, lo(), pt()
    IF g.mode <> PLAYING THEN EXIT FOR
NEXT
PRINT "result"; g.mode; g.lives; g.bones; loops; CINT(p.x); CINT(p.y)
END
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
    words = out[out.rfind("result") :].split()
    if len(words) < 5 or words[1:3] != ["2", "3"] or words[4] != "5":
        sys.exit(
            f"charlie: not cleared on the first life round 5 loops (mode, lives, bones, loops, x, y): {out[-300:]}"
        )
    print(f"charlie ok: cleared with {words[3]} bones")


if __name__ == "__main__":
    main()
