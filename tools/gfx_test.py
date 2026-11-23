#!/usr/bin/env python3
"""gfx_test.py: the DOS32 build's graphics and sound through llrm's runtime, read back from video
memory (PEEK at A0000h), not through the runtime that drew it. A PAINT once filled part of the
shape, then said Out of memory: its queue (half a screen's pixels, 8 bytes each) did not fit the
pool; it is now the BASIC heap's free part, lent for the fill."""

import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import dosrun

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


def pages(mode: int, dot: str, shown: int, hidden: int, sees: int) -> str:
    """A dot drawn on page 1 while page 0 shows: the memory shown (PEEK at `shown`) unchanged until the flip,
    then a PCOPY. Modes 1, 2, 12 and 13 have one page on the adapter, the other in memory."""
    return "\r\n".join([
        f"SCREEN {mode}, , 1, 0",
        f"{dot}: a = PEEK({shown}): h = PEEK({hidden})",
        f"SCREEN , , 1, 1: b = PEEK({sees})",
        f"SCREEN , , 0, 0: c = PEEK({shown})",
        f"PCOPY 1, 0: d = PEEK({shown})",
        "SCREEN 0",
        "PRINT a; h; b; c; d",
        "",
    ])


# mode, the dot, the page shown's byte, the hidden page's (the adapter's, or the shown one's where it is in
# memory), the byte seen after the flip; then the bytes PRINTed
PAGES = {
    "pages13": (pages(13, "PSET (5, 0), 9", 0xA0005, 0xA0005, 0xA0005), " 0  0  9  0  9 "),
    "pages12": (pages(12, "PSET (0, 0), 1", 0xA0000, 0xA0000, 0xA0000), " 0  0  128  0  128 "),
    "pages1": (pages(1, "PSET (0, 0), 3", 0xB8000, 0xB8000, 0xB8000), " 0  0  192  0  192 "),
    "pages2": (pages(2, "PSET (0, 0), 1", 0xB8000, 0xB8000, 0xB8000), " 0  0  128  0  128 "),
    "pages7": (pages(7, "PSET (0, 0), 1", 0xA0000, 0xA2000, 0xA2000), " 0  128  128  0  128 "),
}

# Text mode's: standard output the screen, so CLS draws (on page 1, attribute 17h); the bytes to a file.
TEXT = pages(0, "COLOR 7, 1: CLS", 0xB8001, 0xB9001, 0x462).replace(
    "PRINT a; h; b; c; d", 'OPEN "R.TXT" FOR OUTPUT AS 1: PRINT #1, a; h; b; c; d: CLOSE')


def flip12(flips: int) -> str:
    """A circle on SCREEN 12's page 1 and a row of every colour, the pages swapped `flips` times; then the circle's
    top and bottom rows read from the adapter, and the row's pixels wrong once page 1 is back in memory."""
    return "\r\n".join([
        "SCREEN 12, , 1, 0",
        "CIRCLE (320, 240), 100, 15",
        "FOR x = 0 TO 639: PSET (x, 5), (x * 7 + x \\ 16) MOD 16: NEXT",
        f"FOR i = 1 TO {flips}: SCREEN , , 1, i MOD 2: NEXT",
        "a = PEEK(&HA0000 + 140 * 80 + 40): b = PEEK(&HA0000 + 340 * 80 + 40)",
        "SCREEN , , 1, 0",
        "wrong = 0",
        "FOR x = 0 TO 639",
        "IF POINT(x, 5) <> (x * 7 + x \\ 16) MOD 16 THEN wrong = wrong + 1",
        "NEXT",
        "SCREEN 0",
        "PRINT a; b; wrong",
        "",
    ])


# A hand count of a swap: 480 rows, each converted both ways, 320 plane bytes at about 12 instructions.
SWAP_MOST = 5_000_000


def flip_cost() -> float:
    """Instructions a SCREEN 12 swap takes. It converted a pixel's bit at a time, 13.1M instructions (650 ms at
    the suites' 20 MIPS); a look while one ran saw only the top of a circle, as if the rest were not drawn."""
    jobs = {"one": (flip12(1), ""), "many": (flip12(21), "")}
    got = dosrun.run(jobs, Path(tempfile.mkdtemp()) / "w", 120000, file=True, mem=8)
    for name, result in got.items():
        assert " 255  255  0 " in result.out, (name, result.out[-400:])
    return (got["many"].instructions - got["one"].instructions) / 20


def screen_error() -> str:
    """An error in a graphics mode, standard output the screen: its message was printed on the
    graphics screen, which the exit's text mode cleared (GORILLAS.BAS left without a word)."""
    import dosplay

    work = Path(tempfile.mkdtemp())
    (work / "B.EXE").write_bytes(dosrun.COM.read_bytes())
    (work / "DOS32A.EXE").write_bytes((dosrun.COM.parent / "DOS32A.EXE").read_bytes())
    (work / "E.BAS").write_bytes(b"SCREEN 13\r\nERROR 5\r\n")
    s = dosplay.Session(work)
    s.keys_in_lockstep(["b", "space", "e", ".", "b", "a", "s"], 3000)
    s.keys_in_lockstep(["enter"], 3000)
    s.resume(3000)
    return s.screen()


if __name__ == "__main__":
    for name, program, want in (("gfx", PROGRAM, " 9  9  12  1  14 "), ("whole", WHOLE, " 2  2 ")):
        out = dosrun.run({name: program}, Path(tempfile.mkdtemp()) / "w", 60000, mem=4)[name].out
        assert want in out and "Out of memory" not in out, (name, out[-400:])
    got = dosrun.run({k: v[0] for k, v in PAGES.items()}, Path(tempfile.mkdtemp()) / "w", 60000, mem=4)
    for name, (_, want) in PAGES.items():
        assert want in got[name].out, (name, got[name].out[-400:])
    work = Path(tempfile.mkdtemp()) / "w"
    dosrun.run({"text": TEXT}, work, 60000, mem=4, screen=True)
    text = (work / "R.TXT").read_text() if (work / "R.TXT").exists() else "(none)"
    assert " 7  23  1  7  23 " in text, text
    if not dosrun.is_host(dosrun.COM):
        cost = flip_cost()
        assert cost < SWAP_MOST, cost
        shown = screen_error()
        assert "Illegal function call" in shown, shown
    print("ok")
