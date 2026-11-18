#!/usr/bin/env python3
"""pangdash_test.py [EXE]: PANGDASH.BAS's level played through by an autopilot on the host build.

The main loop is swapped for one that plays with the game's own pilot, which holds Right and jumps
at pits, walls and beetles ahead. The level must be cleared on the first life, round all five of
its loops: a gap too wide, a wall too high, a loop entered too slowly or a landing that misses the
ground fails it.

Every bar of the tune and every sound effect is played: a PLAY string the game builds wrong is an
Illegal function call mid-game. First a beetle is walked to the level's left end: its look ahead,
left of column 0, was a negative tile column, and the game stopped with Subscript out of range.
"""

import sys
import tempfile
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEMO = ROOT / "src/plat/wasm/web/demos/PANGDASH.BAS"

DRIVER = """OpenScreen
LoadTables
LoadLevel
BuildArt
DIM g AS Game, p AS Hero, k AS Pad, i AS LONG, j AS INTEGER, loops AS INTEGER, was AS INTEGER
DIM lo(NLOOSE - 1) AS Loose, pt(NPART - 1) AS Part
Muted = -1
StartGame g, p, lo(), pt()
FOR j = 0 TO NObj - 1
    IF ObK(j) = KBUG THEN ObX(j) = 24: ObA(j) = 24: ObY(j) = StartY: g.camx = 0: EXIT FOR
NEXT
FOR i = 1 TO 300: Things g, lo(), pt(): NEXT
PRINT "edge ok"
Muted = 0
LoadSong
FOR j = 0 TO 7: PLAY Lead(j), Bass(j), Drum(j): NEXT
FOR j = SJUMP TO SBOOST: Sfx j: NEXT
Muted = -1
PRINT "sounds ok"
StartGame g, p, lo(), pt()
FOR i = 1 TO 9000
    Pilot p, k
    Tick g, p, k, lo(), pt()
    IF p.onloop <> 0 AND was = 0 THEN loops = loops + 1
    was = p.onloop
    IF i MOD 30 = 0 THEN Render g, p, lo(), pt()
    IF g.mode <> PLAYING THEN EXIT FOR
NEXT
PRINT "result"; g.mode; g.lives; g.beads; loops; CINT(p.x); CINT(p.y)
END
"""


def main() -> None:
    exe = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build/host/basic").resolve()
    src = DEMO.read_text()
    assert "\nMain\nEND\n" in src, "PANGDASH.BAS's main call moved"
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "T.BAS"
        path.write_text(src.replace("\nMain\nEND\n", "\n" + DRIVER, 1))
        run = subprocess.run([exe, path.name], cwd=tmp, capture_output=True, timeout=120)
    out = run.stdout.decode(errors="replace") + run.stderr.decode(errors="replace")
    if "edge ok" not in out:
        sys.exit(f"pangdash: a beetle at the level's left end: {out[-300:]}")
    if "sounds ok" not in out:
        sys.exit(f"pangdash: a bar of the tune or a sound effect: {out[-300:]}")
    words = out[out.rfind("result") :].split()
    if len(words) < 5 or words[1:3] != ["2", "3"] or words[4] != "5":
        sys.exit(
            f"pangdash: not cleared on the first life round 5 loops (mode, lives, beads, loops, x, y): {out[-300:]}"
        )
    print(f"pangdash ok: cleared with {words[3]} beads")


if __name__ == "__main__":
    main()
