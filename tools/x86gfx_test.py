#!/usr/bin/env python3
"""x86gfx_test.py: the DOS32 build's graphics and sound through llrm's runtime, read back from video
memory (PEEK at A0000h), not through the runtime that drew it. A PAINT once filled part of the
shape, then said Out of memory: its queue (half a screen's pixels, 8 bytes each) did not fit the
pool; it is now the BASIC heap's free part, lent for the fill."""

import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import x86run

PROGRAM = "\r\n".join([
    "SCREEN 13",
    "PSET (5, 0), 9: q = POINT(5, 0)",
    "LINE (0, 2)-(3, 2), 12",
    "CIRCLE (100, 100), 20, 14",
    "PAINT (100, 100), 1, 14",
    "a = PEEK(&HA0005): b = PEEK(&HA0000 + 643): c = PEEK(&HA0000 + 32100): d = PEEK(&HA0000 + 32120)",
    "SOUND 440, 1",
    "SCREEN 0",
    "PRINT q; a; b; c; d",
    "",
])

# A fill of SCREEN 12's whole screen, 640x480, in the 4 MB the suites run in: the queue was sized for
# the worst case (1.2 MB), which did not fit.
WHOLE = "\r\n".join([
    "SCREEN 12",
    "PAINT (320, 240), 2",
    "a = POINT(0, 0): b = POINT(639, 479)",
    "SCREEN 0",
    "PRINT a; b",
    "",
])


def screen_error() -> str:
    """An error in a graphics mode, standard output the screen: its message was printed on the
    graphics screen, which the exit's text mode cleared (GORILLAS.BAS left without a word)."""
    llrm = Path("/Users/alim/work/personal/llrm-qbmac/tools")
    sys.path[:0] = [str(llrm), str(llrm / "dosbatch")]
    import llrmbin
    llrmbin.stale_binaries = lambda *a, **k: []
    import qbplay

    work = Path(tempfile.mkdtemp())
    (work / "B.EXE").write_bytes(x86run.COM.read_bytes())
    (work / "DOS32A.EXE").write_bytes((x86run.COM.parent / "DOS32A.EXE").read_bytes())
    (work / "E.BAS").write_bytes(b"SCREEN 13\r\nERROR 5\r\n")
    s = qbplay.Session(work)
    s.keys_in_lockstep(["b", "space", "e", ".", "b", "a", "s"], 3000)
    s.keys_in_lockstep(["enter"], 3000)
    s.resume(3000)
    return s.screen()


if __name__ == "__main__":
    for name, program, want in (("gfx", PROGRAM, " 9  9  12  1  14 "), ("whole", WHOLE, " 2  2 ")):
        out = x86run.run({name: program}, Path(tempfile.mkdtemp()) / "w", 60000, mem=4)[name].out
        assert want in out and "Out of memory" not in out, (name, out[-400:])
    if not x86run.is_host(x86run.COM):
        shown = screen_error()
        assert "Illegal function call" in shown, shown
    print("ok")
