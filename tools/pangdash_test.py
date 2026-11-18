#!/usr/bin/env python3
"""pangdash_test.py [EXE]: PANGDASH.BAS's level played through by an autopilot on the host build.

The main loop is swapped for one that holds Right and jumps at gaps, walls and beetles ahead. The
level must be cleared on the first life: a gap too wide, a wall too high or a landing that misses
the ground fails it.

First a beetle is walked to the level's left end: its look ahead, left of column 0, was a negative
tile column, and the game stopped with Subscript out of range.
"""

import sys
import tempfile
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEMO = ROOT / "src/plat/wasm/web/demos/PANGDASH.BAS"

DRIVER = """OpenScreen
LoadPalette
LoadTables
LoadLevel
BuildArt
DIM g AS Game, p AS Hero, k AS Pad, i AS INTEGER, hold AS INTEGER, j AS INTEGER, s AS INTEGER, want AS INTEGER
DIM lo(NLOOSE - 1) AS Loose, pt(NPART - 1) AS Part
Muted = -1
StartGame g, p, lo(), pt()
Bugs(0).x = 20: Bugs(0).y = StartY
FOR i = 1 TO 300: MoveThings g, lo(), pt(): NEXT
PRINT "edge ok"
StartGame g, p, lo(), pt()
k.r = -1
FOR i = 1 TO 7200
    want = 0
    IF p.ground THEN
        s = Surf%(CINT(p.x) + 22, CINT(p.y))
        IF ABS(s - p.y) > 14 THEN want = -1
        IF Solid%(CINT(p.x) + 12, CINT(p.y) - 9) OR Solid%(CINT(p.x) + 12, CINT(p.y) - 20) THEN want = -1
        FOR j = 0 TO NBugs - 1
            IF Bugs(j).alive AND Bugs(j).x > p.x AND Bugs(j).x - p.x < 44 THEN want = -1
        NEXT
    END IF
    IF want AND hold = 0 THEN k.hit = -1: hold = 24
    IF hold > 0 THEN k.jump = -1: hold = hold - 1 ELSE k.jump = 0
    Tick g, p, k, lo(), pt()
    IF i MOD 30 = 0 THEN Render g, p, lo(), pt()
    IF g.mode <> PLAYING THEN EXIT FOR
NEXT
PRINT "result"; g.mode; g.lives; g.beads
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
    words = out[out.rfind("result") :].split()
    if len(words) < 4 or words[1:3] != ["2", "3"]:
        sys.exit(f"pangdash: not cleared on the first life (mode, lives, beads): {out[-400:]}")
    print(f"pangdash ok: cleared with {words[3]} beads")


if __name__ == "__main__":
    main()
