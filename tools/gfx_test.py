#!/usr/bin/env python3
"""gfx_test.py: the DOS32 build's graphics and sound through llrm's runtime, read back from video
memory (PEEK at A0000h), not through the runtime that drew it. A PAINT once filled part of the
shape, then said Out of memory: its queue (half a screen's pixels, 8 bytes each) did not fit the
pool; it is now the BASIC heap's free part, lent for the fill. gfx.qbx's DOS slice loads: a file
over 64 KB read whole was File not found, as a read's count kept its low 16 bits. Its screen closed,
the cursor is where it was: the mode's set had put it top left, where the next PRINT went. The
DOS slice draws the host's pixels: SAME_PIXELS's checksum is the same from both builds of the one C
renderer, the x87's arithmetic and the host's."""

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


def extension() -> str:
    """gfx.qbx, from lib/ beside the interpreter: its monitor, and more than mode 13h."""
    program = "import gfx.display\r\nprint gfx.display.count; gfx.display.modes(1) > 1\r\n"
    return dosrun.run({"gfx": program}, Path(tempfile.mkdtemp()) / "w", 60000, file=True, files=gfx_lib(),
                      mem=16)["gfx"].out


def extension_row() -> str:
    """The text cursor's row after gfx's screen opened and closed, the program's first output after."""
    program = "\r\n".join([
        "import gfx.display",
        "gfx.display.open 320, 200, 8, 0",
        "gfx.display.close",
        'r = CSRLIN: OPEN "R.TXT" FOR OUTPUT AS 1: PRINT #1, r: CLOSE',
        "",
    ])
    work = Path(tempfile.mkdtemp()) / "w"
    dosrun.run({"row": program}, work, 60000, file=True, files=gfx_lib(), mem=16, screen=True)
    return (work / "R.TXT").read_text() if (work / "R.TXT").exists() else "(none)"


# Each of the renderer's paths at a depth: spans of each length, rows keyed and opaque, flipped or
# not, clipped; walks turned and scaled, and wrapped along a strip; polygons flat, affine and in
# perspective, with depth tested and set, their planes crossing.
# The picture's bytes make keyed holes at each depth: a 0, or at 32 bits an alpha under a half.
SAME_PIXELS = """import gfx.display, gfx.draw, gfx.surface, gfx.pen
from gfx.draw import Vec2, Vertex
gfx.display.open 320, 200, {depth}, 0
s& = gfx.surface.new(37, 23)
dim pic(37 * 23 * {bytes} - 1) as byte
for i = 0 to 37 * 23 * {bytes} - 1
    pic(i) = ((i * 7 + i \\ 37) mod 9) * 29
next
gfx.surface.write s&, 0, 0, 37, 23, pic()
gfx.draw.clear 3
dim w(3) as Vertex
w(1).x = 319: w(2).x = 319: w(2).y = 199: w(3).y = 199
w(0).z = 1: w(1).z = 1.5: w(2).z = 7: w(3).z = 5
w(1).u = 37: w(2).u = 37: w(2).v = 23: w(3).v = 23
gfx.draw.poly s&, w(), 0, 4, 0, gfx.draw.PERSP
for i = 0 to 40
    x = (i * 7) mod 300 + i mod 5
    gfx.draw.rect x, i * 3, x + i, i * 3 + 1, ((16 + i) * {spread}) and &HFFFFFF, gfx.draw.FILL
next
for i = 0 to 15
    f% = (i and 1) * gfx.draw.FLIPX + (i and 2) * 4
    gfx.draw.sprite s&, -10 + i * 21, 130 + (i mod 3) * 9, f%
next
t& = gfx.surface.new(32, 8)
dim tile(32 * 8 * {bytes} - 1) as byte
for i = 0 to 32 * 8 * {bytes} - 1
    tile(i) = pic(i)
next
gfx.surface.write t&, 0, 0, 32, 8, tile()
for i = 0 to 31
    f% = (i \\ 4 and 1) * gfx.draw.FLIPX + (i \\ 8 and 1) * 8
    gfx.draw.sprite t&, -i, 40 + i * 3, f%
next
for i = 0 to 11
    gfx.draw.sprite_turned s&, 20 + i * 26, 172, i * 0.5, 0.6 + i * 0.15, (i and 1) * 8
next
dim pts(2) as Vec2
pts(0).x = 10: pts(0).y = 150: pts(1).x = 150: pts(1).y = 140: pts(2).x = 300: pts(2).y = 160
gfx.draw.strip s&, pts(), 0, 3, 0
for i = 0 to 11
    gfx.draw.sprite_turned t&, 20 + i * 26, 100, i * 0.5 + 0.2, 0.8 + i * 0.2, (i and 1) * 8
next
for i = 0 to 2
    pts(i).y = pts(i).y - 60
next
gfx.draw.strip t&, pts(), 0, 3, 0
dim q(3) as Vertex
q(1).u = 32: q(2).u = 32: q(2).v = 8: q(3).v = 8
gfx.draw.clear_depth
for i = 0 to 17
    x = 20 + i * 16: y = 30 + (i mod 2) * 6: z = 1 + (i * 5) mod 4
    q(0).x = x: q(0).y = y: q(1).x = x + 30: q(1).y = y + 4
    q(2).x = x + 26: q(2).y = y + 30: q(3).x = x - 3: q(3).y = y + 26
    q(0).z = z: q(1).z = z: q(2).z = z * 3: q(3).z = z * 3
    f% = ((i mod 3) and 1) * gfx.draw.ZSET + ((i mod 3) \\ 2) * (gfx.draw.ZTEST + gfx.draw.ZSET)
    select case i \\ 6
    case 0
        gfx.draw.poly 0, q(), 0, 4, ((50 + i) * {spread}) and &HFFFFFF, f% + gfx.draw.FILL
    case 1
        gfx.draw.poly t&, q(), 0, 4, 0, f% + (i and 1) * 8
    case else
        gfx.draw.poly s&, q(), 0, 4, 0, f% + gfx.draw.PERSP
    end select
next
for i = 0 to 23
    x = 6 + i * 13.3: y = 70 + (i mod 3) * 7.7: z = 1 + (i mod 4) * 0.37
    q(0).x = x: q(0).y = y: q(1).x = x + 21.6: q(1).y = y + 2.2
    q(2).x = x + 19.1: q(2).y = y + 24.9: q(3).x = x - 1.4: q(3).y = y + 22.3
    q(0).z = z: q(1).z = z * 2.9: q(2).z = z * 2.9: q(3).z = z
    gfx.draw.poly s&, q(), 0, 4, 0, gfx.draw.PERSP + gfx.draw.ZTEST + gfx.draw.ZSET
    q(0).z = z * 2.9: q(1).z = z: q(2).z = z: q(3).z = z * 2.9
    f% = gfx.draw.ZTEST + gfx.draw.ZSET
    select case i mod 3
    case 0
        gfx.draw.poly 0, q(), 0, 4, ((90 + i) * {spread}) and &HFFFFFF, f% + gfx.draw.FILL
    case 1
        gfx.draw.poly t&, q(), 0, 4, 0, f%
    case else
        gfx.draw.poly t&, q(), 0, 4, 0, f% + gfx.draw.PERSP
    end select
next
dim px(320& * 200 * {bytes} - 1) as byte
gfx.surface.read gfx.surface.CANVAS, 0, 0, 320, 200, px()
for i& = 0 to 320& * 200 * {bytes} - 1
    sum& = (sum& * 31 + px(i&)) and &HFFFFF
next
gfx.display.close
print sum&
"""


DEPTHS = (8, 15, 16, 32)


def pixel_sums(build: str, depths: tuple[int, ...] = DEPTHS, root: Path = dosrun.ROOT) -> dict[int, str]:
    """SAME_PIXELS's output at each depth: build "dos", root's DOS slice in one DOSBox, or "host", root's host
    build natively."""
    qbx = (dosrun.COM.parent if build == "dos" else root / "build/host") / "lib/gfx.qbx"
    data = qbx.read_bytes()
    jobs = {
        f"{build}{d}": (SAME_PIXELS.format(depth=d, bytes=(d + 7) // 8, spread=1 if d == 8 else 40503), "", "",
                        {"gfx.qbx": data})
        for d in depths
    }
    com = dosrun.COM
    dosrun.COM = com if build == "dos" else root / "build/host/basic"
    try:
        got = dosrun.run(jobs, Path(tempfile.mkdtemp()) / "w", 120000, file=True, mem=16)
    finally:
        dosrun.COM = com
    return {d: got[f"{build}{d}"].out for d in depths}


def same_pixels(depths: tuple[int, ...] = DEPTHS) -> dict[tuple[int, str], str]:
    """SAME_PIXELS's output at each depth by (depth, build): the DOS slice's and the host build's."""
    got = {name: pixel_sums(name, depths) for name in ("dos", "host")}
    return {(d, name): got[name][d] for d in depths for name in got}


def gfx_lib() -> dict[str, bytes]:
    return {"lib/gfx.qbx": (dosrun.COM.parent / "lib/gfx.qbx").read_bytes()}


if __name__ == "__main__":
    if sys.argv[1:2] == ["--sums"]:
        root = Path(sys.argv[2]) if len(sys.argv) > 2 else dosrun.ROOT
        for depth, out in pixel_sums("host", root=root).items():
            print(depth, out.split()[-1] if out.split() else out)
        sys.exit()
    for name, program, want in (("gfx", PROGRAM, " 9  9  12  1  14 "), ("whole", WHOLE, " 2  2 ")):
        out = dosrun.run({name: program}, Path(tempfile.mkdtemp()) / "w", 60000, mem=4)[name].out
        assert want in out and "Out of memory" not in out, (name, out[-400:])
    got = dosrun.run({k: v[0] for k, v in PAGES.items()}, Path(tempfile.mkdtemp()) / "w", 60000, mem=4)
    for name, (_, want) in PAGES.items():
        assert want in got[name].out, (name, got[name].out[-400:])
    out = extension()
    assert " 1 -1 " in out, out[-400:]
    row = extension_row()
    assert row.split() and int(row.split()[0]) > 1, row
    got = same_pixels()
    for depth in DEPTHS:
        dos, host = got[depth, "dos"], got[depth, "host"]
        assert dos.split() and dos == host, (depth, dos[-300:], host[-300:])
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
