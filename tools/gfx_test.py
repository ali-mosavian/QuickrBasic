#!/usr/bin/env python3
"""gfx_test.py: the DOS32 build's graphics and sound through llrm's runtime, read back from video
memory (PEEK at A0000h), not through the runtime that drew it. A PAINT once filled part of the
shape, then said Out of memory: its queue (half a screen's pixels, 8 bytes each) did not fit the
pool; it is now the BASIC heap's free part, lent for the fill. gfx.qbx's DOS slice loads: a file
over 64 KB read whole was File not found, as a read's count kept its low 16 bits. Its screen closed,
the cursor is where it was: the mode's set had put it top left, where the next PRINT went. The
DOS slice draws the host's pixels: SAME_PIXELS's checksum is the same from both builds of the one C
renderer, the x87's arithmetic and the host's. --gpu [DEPTH...]: GPU_SCENE in the Mac app, drawn by the GPU
against pages, cell by cell: 32 bits' lights, tints, SOLID and ADD were the GPU's floats, a level or three off;
BLEND, the blend constant's, a level off. At 8 bits every cell but shapes, sprites and text differed: the target
held colours, read back as the nearest index, and lights, tints, rows and blends were worked as colours. At 15 and
16 bits every cell was up to 7 off: the target kept 8 bits a channel. A virtual Mac has no GPU: --gpu compares
nothing there. --card CARD [--set NAME=VALUE]... [--shot FILE] [DEPTH...]: GPU_SCENE drawn by the DOS build on
86Box's 3D card, its driver's, against the Mac app's pages, cell by cell, as --gpu is; the card's screenshot kept in
FILE. A Banshee's overlay, its CLUT bypassed, showed 565 with zeros below: flat colours up to 7 off."""

import os
import subprocess
import sys
import tempfile
import time
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
    then a PCOPY. A mode has the adapter's pages only, as QBasic: 1, 2, 12 and 13 one."""
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


# mode, the dot, the page shown's byte, the hidden page's, the byte seen after the flip; then the bytes PRINTed.
# The one-page modes' second page is Illegal function call: it was a page in memory, 300 KB in 12's.
PAGES = {
    "pages7": (pages(7, "PSET (0, 0), 1", 0xA0000, 0xA2000, 0xA2000), " 0  128  128  0  128 "),
    **{f"pages{m}": (f"SCREEN {m}, , 1, 0\r\n", "Illegal function call") for m in (1, 2, 12, 13)},
}

# Text mode's: standard output the screen, so CLS draws (on page 1, attribute 17h); the bytes to a file.
TEXT = pages(0, "COLOR 7, 1: CLS", 0xB8001, 0xB9001, 0x462).replace(
    "PRINT a; h; b; c; d", 'OPEN "R.TXT" FOR OUTPUT AS 1: PRINT #1, a; h; b; c; d: CLOSE')


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
    program = "import gfx.display\r\nprint gfx.display.monitors; gfx.display.modes(1) > 1\r\n"
    return dosrun.run({"gfx": program}, Path(tempfile.mkdtemp()) / "w", 60000, file=True, files=gfx_lib(),
                      mem=16)["gfx"].out


def extension_row() -> str:
    """The text cursor's row after gfx's screen opened and closed, the program's first output after."""
    program = "\r\n".join([
        "import gfx.display",
        "canvas& = gfx.display.open(320, 200, 8, 0)",
        "gfx.display.close",
        'r = CSRLIN: OPEN "R.TXT" FOR OUTPUT AS 1: PRINT #1, r: CLOSE',
        "",
    ])
    work = Path(tempfile.mkdtemp()) / "w"
    dosrun.run({"row": program}, work, 60000, file=True, files=gfx_lib(), mem=16, screen=True)
    return (work / "R.TXT").read_text() if (work / "R.TXT").exists() else "(none)"


# Each of the renderer's paths at a depth: spans of each length, rows keyed and opaque, flipped or
# not, clipped; walks turned and scaled, and wrapped along a strip; polygons flat, affine and in
# perspective, with depth tested and set, their planes crossing; arcs, circles and lines. A
# perspective row walked held, plain, then held: no row went on past a held run, so the DOS asm's
# resume, its x87 state kept across it, was never drawn.
# The texture's bytes make keyed holes at each depth: a 0, or at 32 bits an alpha under a half.
SAME_PIXELS = """import gfx.display, gfx.draw, gfx.surface
from gfx.draw import Vec2, Vertex
canvas& = gfx.display.open(320, 200, {depth}, 0)
s& = gfx.surface.new(37, 23)
dim pic(37 * 23 * {bytes} - 1) as byte
for i = 0 to 37 * 23 * {bytes} - 1
    pic(i) = ((i * 7 + i \\ 37) mod 9) * 29
next
gfx.surface.write s&, 0, 0, 37, 23, pic()
gfx.draw.clear canvas&, 3
dim w(3) as Vertex
w(1).x = 319: w(2).x = 319: w(2).y = 199: w(3).y = 199
w(0).z = 1: w(1).z = 1.5: w(2).z = 7: w(3).z = 5
w(1).u = 37: w(2).u = 37: w(2).v = 23: w(3).v = 23
gfx.draw.poly canvas&, w(), 0, 4, s&, 0, gfx.draw.PERSPECTIVE
for i = 0 to 40
    x = (i * 7) mod 300 + i mod 5
    gfx.draw.rect canvas&, x, i * 3, x + i, i * 3 + 1, ((16 + i) * {spread}) and &HFFFFFF, gfx.draw.FILL
next
for i = 0 to 15
    f% = (i and 1) * gfx.draw.FLIP_X + (i and 2) * 4
    gfx.draw.sprite canvas&, -10 + i * 21, 130 + (i mod 3) * 9, s&, f%
next
t& = gfx.surface.new(32, 8)
dim tile(32 * 8 * {bytes} - 1) as byte
for i = 0 to 32 * 8 * {bytes} - 1
    tile(i) = pic(i)
next
gfx.surface.write t&, 0, 0, 32, 8, tile()
for i = 0 to 31
    f% = (i \\ 4 and 1) * gfx.draw.FLIP_X + (i \\ 8 and 1) * 8
    gfx.draw.sprite canvas&, -i, 40 + i * 3, t&, f%
next
for i = 0 to 11
    gfx.draw.sprite_turned canvas&, 20 + i * 26, 172, i * 0.5, 0.6 + i * 0.15, s&, (i and 1) * 8
next
dim pts(2) as Vec2
pts(0).x = 10: pts(0).y = 150: pts(1).x = 150: pts(1).y = 140: pts(2).x = 300: pts(2).y = 160
gfx.draw.strip canvas&, pts(), 0, 3, s&, 0
for i = 0 to 11
    gfx.draw.sprite_turned canvas&, 20 + i * 26, 100, i * 0.5 + 0.2, 0.8 + i * 0.2, t&, (i and 1) * 8
next
for i = 0 to 2
    pts(i).y = pts(i).y - 60
next
gfx.draw.strip canvas&, pts(), 0, 3, t&, 0
dim q(3) as Vertex
q(1).u = 32: q(2).u = 32: q(2).v = 8: q(3).v = 8
gfx.surface.clear_depth canvas&
for i = 0 to 17
    x = 20 + i * 16: y = 30 + (i mod 2) * 6: z = 1 + (i * 5) mod 4
    q(0).x = x: q(0).y = y: q(1).x = x + 30: q(1).y = y + 4
    q(2).x = x + 26: q(2).y = y + 30: q(3).x = x - 3: q(3).y = y + 26
    q(0).z = z: q(1).z = z: q(2).z = z * 3: q(3).z = z * 3
    f% = ((i mod 3) and 1) * gfx.draw.DEPTH_WRITE + ((i mod 3) \\ 2) * (gfx.draw.DEPTH_TEST + gfx.draw.DEPTH_WRITE)
    select case i \\ 6
    case 0
        gfx.draw.poly canvas&, q(), 0, 4, 0, ((50 + i) * {spread}) and &HFFFFFF, f% + gfx.draw.FILL
    case 1
        gfx.draw.poly canvas&, q(), 0, 4, t&, 0, f% + (i and 1) * 8
    case else
        gfx.draw.poly canvas&, q(), 0, 4, s&, 0, f% + gfx.draw.PERSPECTIVE
    end select
next
for i = 0 to 23
    x = 6 + i * 13.3: y = 70 + (i mod 3) * 7.7: z = 1 + (i mod 4) * 0.37
    q(0).x = x: q(0).y = y: q(1).x = x + 21.6: q(1).y = y + 2.2
    q(2).x = x + 19.1: q(2).y = y + 24.9: q(3).x = x - 1.4: q(3).y = y + 22.3
    q(0).z = z: q(1).z = z * 2.9: q(2).z = z * 2.9: q(3).z = z
    gfx.draw.poly canvas&, q(), 0, 4, s&, 0, gfx.draw.PERSPECTIVE + gfx.draw.DEPTH_TEST + gfx.draw.DEPTH_WRITE
    q(0).z = z * 2.9: q(1).z = z: q(2).z = z: q(3).z = z * 2.9
    f% = gfx.draw.DEPTH_TEST + gfx.draw.DEPTH_WRITE
    select case i mod 3
    case 0
        gfx.draw.poly canvas&, q(), 0, 4, 0, ((90 + i) * {spread}) and &HFFFFFF, f% + gfx.draw.FILL
    case 1
        gfx.draw.poly canvas&, q(), 0, 4, t&, 0, f%
    case else
        gfx.draw.poly canvas&, q(), 0, 4, t&, 0, f% + gfx.draw.PERSPECTIVE
    end select
next
dim e(3) as Vertex
e(0).x = 30: e(0).y = 110: e(1).x = 290: e(1).y = 104
e(2).x = 296: e(2).y = 128: e(3).x = 24: e(3).y = 124
e(0).z = 1: e(1).z = 4: e(2).z = 4: e(3).z = 1
e(0).u = -9: e(1).u = 46: e(2).u = 46: e(2).v = 23: e(3).u = -9: e(3).v = 23
f% = gfx.draw.PERSPECTIVE + gfx.draw.DEPTH_TEST + gfx.draw.DEPTH_WRITE
gfx.draw.poly canvas&, e(), 0, 4, s&, 0, f%
e(0).z = 3: e(1).z = 1.5: e(2).z = 1.5: e(3).z = 3
gfx.draw.poly canvas&, e(), 0, 4, t&, 0, f%
for i = 0 to 23
    x = 7.3 + i * 13.4: y = 12.6 + (i mod 4) * 11.3: r = 3 + i * 1.7
    c& = ((120 + i) * {spread}) and &HFFFFFF
    gfx.draw.arc canvas&, x, y, r, i * 0.7 - 2, i * 0.7 + 0.4 + (i mod 5) * 1.3, c&
    gfx.draw.circle canvas&, x, y + 140, 1 + (i mod 7) * 2.3, c&, (i and 1) * gfx.draw.FILL
    gfx.draw.line canvas&, x - 9.5, y + 80, x + i * 3.1, y + 97 - i, c&
next
dim px(320& * 200 * {bytes} - 1) as byte
gfx.surface.read canvas&, 0, 0, 320, 200, px()
for i& = 0 to 320& * 200 * {bytes} - 1
    sum& = (sum& * 31 + px(i&)) and &HFFFFF
next
gfx.display.close
print sum&
"""


# The GPU's frame against pages', a feature to a 160 by 120 cell of a 640 by 480 screen, each drawn
# in a view of its cell: shapes; sprites keyed, flipped, OPAQUE; turned and a strip; polygons flat,
# affine, in perspective; by depth; sprites lit, tinted; shapes lit and tinted; SOLID; BLEND and
# ADD, shapes and sprites; SMOOTH; palette rows (8 bits); text; a surface drawn into, then drawn;
# a surface drawn into, its pixels read back and drawn as colours, two indices of one colour among
# them, then row 0 put back. {depth}, {bytes} a pixel, {spread} a colour's step. The frame is drawn once,
# then shown.
GPU_CELLS = (
    "shapes", "sprites", "turned", "polys", "depth", "lit", "tinted", "lit shapes",
    "solid", "blend", "add", "smooth", "rows", "text, tiles", "drawn into", "read back",
)
# Cells whose texels the GPU picks by its own interpolation, not software's 16.16 walk: told, not failed.
GPU_LOOSE = ("turned", "polys", "depth", "smooth")
# Cells the GPU's own blending draws, by depth: the most a channel may be off. 32 bits: the blend constant's float
# mix, a level. 15 and 16 bits: the mix is not narrowed after, each layer up to a 5-bit step off: told, not failed.
GPU_NEAR = {("blend", 32): 1} | {(name, depth): 255 for name in ("blend", "add") for depth in (15, 16)}
# A 3D card's, besides: its texels are ARGB1555, green a bit short, and it multiplies texel by colour
# truncating, a 5-bit step; a line a pixel wide has its corners at 1/16 pixel, each emulator sampling its own way.
CARD_NEAR = 8
CARD_LOOSE = ("shapes",)
# A card's cells of flat colours: exact, as it shows 565 widened as gfx widens it; not at 8 bits, where the VGA
# shows the palette at 6 bits a channel.
CARD_EXACT = ("lit shapes", "solid", "rows")
# A card's blends at 8 bits mix colours, where software's take the palette's nearest: told, not failed.
CARD_NEAR_AT = {(name, 8): 255 for name in ("blend", "add")}
GPU_SCENE = """import gfx.display, gfx.draw, gfx.surface
from gfx.draw import Vec2, Vertex
canvas& = gfx.display.open(640, 480, {depth}, 0)
dim cell&(15)
for k = 0 to 15
    cell&(k) = gfx.surface.new_view(canvas&, (k mod 4) * 160, (k \\ 4) * 120, 160, 120)
next
s& = gfx.surface.new(37, 23)
dim pic(37 * 23 * {bytes} - 1) as byte
for i = 0 to 37 * 23 * {bytes} - 1
    pic(i) = ((i * 7 + i \\ 37) mod 9) * 29
next
gfx.surface.write s&, 0, 0, 37, 23, pic()
gfx.draw.clear canvas&, 3
dim q(3) as Vertex
q(1).u = 37: q(2).u = 37: q(2).v = 23: q(3).v = 23
' shapes
c& = cell&(0)
for i = 0 to 9
    k& = ((20 + i) * {spread}) and &HFFFFFF
    gfx.draw.rect c&, i * 7, i * 5, i * 7 + 20, i * 5 + 9, k&, (i and 1) * gfx.draw.FILL
    gfx.draw.line c&, 150 - i * 9, 5, 80 + i * 3.3, 110, k&
    gfx.draw.circle c&, 40 + i * 9.5, 80 + (i mod 3) * 6.3, 2 + i * 1.3, k&, (i and 1) * gfx.draw.FILL
    gfx.draw.arc c&, 120, 40, 4 + i * 3, i * 0.6, i * 0.6 + 2.5, k&
    gfx.draw.pset c&, 5 + i * 3, 115, k&
next
' sprites
c& = cell&(1)
for i = 0 to 11
    f% = (i and 1) * gfx.draw.FLIP_X + (i \\ 2 and 1) * gfx.draw.FLIP_Y + (i \\ 4 and 1) * gfx.draw.OPAQUE
    gfx.draw.sprite c&, -9 + (i mod 4) * 41, 4 + (i \\ 4) * 38, s&, f%
next
' turned and a strip
c& = cell&(2)
for i = 0 to 5
    gfx.draw.sprite_turned c&, 20 + i * 25, 30 + (i mod 2) * 20, i * 0.7, 0.6 + i * 0.2, s&, (i and 1) * 8
next
dim pts(2) as Vec2
pts(0).x = 5: pts(0).y = 80: pts(1).x = 70: pts(1).y = 95: pts(2).x = 155: pts(2).y = 78
gfx.draw.strip c&, pts(), 0, 3, s&, 0
' polygons
c& = cell&(3)
for i = 0 to 8
    x = 6 + (i mod 3) * 52: y = 5 + (i \\ 3) * 38
    q(0).x = x: q(0).y = y: q(1).x = x + 44.5: q(1).y = y + 3.2
    q(2).x = x + 40.1: q(2).y = y + 33.7: q(3).x = x - 2.6: q(3).y = y + 30.3
    q(0).z = 1: q(1).z = 1: q(2).z = 3.1: q(3).z = 3.1
    select case i mod 3
    case 0
        gfx.draw.poly c&, q(), 0, 4, 0, ((60 + i) * {spread}) and &HFFFFFF, gfx.draw.FILL
    case 1
        gfx.draw.poly c&, q(), 0, 4, s&, 0, 0
    case else
        gfx.draw.poly c&, q(), 0, 4, s&, 0, gfx.draw.PERSPECTIVE
    end select
next
' by depth
c& = cell&(4)
gfx.surface.clear_depth c&
for i = 0 to 11
    x = 4 + i * 11.7: y = 10 + (i mod 3) * 25: z = 1 + (i mod 4) * 0.6
    q(0).x = x: q(0).y = y: q(1).x = x + 30: q(1).y = y + 4
    q(2).x = x + 26: q(2).y = y + 50: q(3).x = x - 3: q(3).y = y + 46
    q(0).z = z: q(1).z = z * 2: q(2).z = z * 2: q(3).z = z
    f% = gfx.draw.DEPTH_TEST + gfx.draw.DEPTH_WRITE + (i and 1) * gfx.draw.PERSPECTIVE
    gfx.draw.poly c&, q(), 0, 4, s&, 0, f%
next
' sprites lit, tinted
for k = 5 to 6
    c& = cell&(k)
    for i = 0 to 7
        if k = 5 then gfx.draw.set_light 7 + i * 8 else gfx.draw.set_tint &H3F1F7F * (i + 1) and &HFFFFFF
        gfx.draw.sprite c&, (i mod 4) * 40, (i \\ 4) * 50 + 5, s&, (i and 1) * gfx.draw.OPAQUE
    next
    gfx.draw.reset
next
' shapes lit and tinted
c& = cell&(7)
for i = 0 to 7
    gfx.draw.set_light 7 + i * 8
    gfx.draw.set_tint &H7F3F1F * (i + 1) and &HFFFFFF
    k& = ((30 + i * 3) * {spread}) and &HFFFFFF
    gfx.draw.rect c&, (i mod 4) * 40, (i \\ 4) * 60, (i mod 4) * 40 + 35, (i \\ 4) * 60 + 25, k&, gfx.draw.FILL
    gfx.draw.circle c&, (i mod 4) * 40 + 18, (i \\ 4) * 60 + 45, 12, k&, gfx.draw.FILL
next
gfx.draw.reset
' SOLID
c& = cell&(8)
for i = 0 to 7
    gfx.draw.set_light 20 + i * 6
    gfx.draw.set_tint &H8F4FCF * (i + 1) and &HFFFFFF
    gfx.draw.sprite c&, (i mod 4) * 40, (i \\ 4) * 50 + 5, s&, gfx.draw.SOLID
next
gfx.draw.reset
' BLEND and ADD
for k = 9 to 10
    c& = cell&(k)
    f% = gfx.draw.BLEND
    if k = 10 then f% = gfx.draw.ADD
    for i = 0 to 7
        gfx.draw.set_alpha 5 + i * 8
        k& = ((70 + i * 5) * {spread}) and &HFFFFFF
        gfx.draw.rect c&, i * 9, i * 6, i * 9 + 60, i * 6 + 40, k&, gfx.draw.FILL + f%
        gfx.draw.sprite c&, 80 + (i mod 2) * 30, (i \\ 2) * 28, s&, f%
    next
    gfx.draw.reset
next
' SMOOTH
c& = cell&(11)
for i = 0 to 5
    f% = gfx.draw.SMOOTH + (i and 1) * gfx.draw.OPAQUE
    gfx.draw.sprite_turned c&, 25 + (i mod 3) * 50, 30 + (i \\ 3) * 60, i * 0.4, 1.1 + i * 0.25, s&, f%
next
' palette rows, 8 bits
c& = cell&(12)
if {depth} = 8 then
    dim row&(255)
    for r = 1 to 3
        for i = 0 to 255
            row&(i) = (i * 977 * r + r * 40503) and &HFFFFFF
        next
        gfx.display.set_palette r, row&()
    next
    for i = 0 to 7
        gfx.draw.set_palette_row i mod 4
        gfx.draw.sprite c&, (i mod 4) * 40, (i \\ 4) * 50 + 5, s&, 0
    next
    gfx.draw.reset
end if
' text
c& = cell&(13)
for i = 0 to 9
    gfx.draw.text c&, (i mod 2) * 7, i * 11 + 2, "GFX " + str$(i * 1234), ((80 + i) * {spread}) and &HFFFFFF
next
' a 16 by 16 texture tiled, u and v -16 to 32 under 48 by 48 pixels
t& = gfx.surface.new(16, 16)
dim tile(16 * 16 * {bytes} - 1) as byte
for i = 0 to 16 * 16 * {bytes} - 1
    tile(i) = ((i * 5 + i \\ 16) mod 11) * 23
next
gfx.surface.write t&, 0, 0, 16, 16, tile()
q(0).x = 87.5: q(0).y = 7.5: q(1).x = 135.5: q(1).y = 7.5
q(2).x = 135.5: q(2).y = 55.5: q(3).x = 87.5: q(3).y = 55.5
q(0).u = -16: q(0).v = -16: q(1).u = 32: q(1).v = -16
q(2).u = 32: q(2).v = 32: q(3).u = -16: q(3).v = 32
for i = 0 to 3
    q(i).z = 1
next
gfx.draw.poly c&, q(), 0, 4, t&, 0, gfx.draw.OPAQUE
' a surface drawn into, then drawn
c& = cell&(14)
d& = gfx.surface.new(70, 50)
gfx.draw.clear d&, 0
gfx.draw.rect d&, 5, 5, 40, 30, (90 * {spread}) and &HFFFFFF, gfx.draw.FILL
gfx.draw.circle d&, 45, 25, 18, (95 * {spread}) and &HFFFFFF, gfx.draw.FILL
gfx.draw.sprite d&, 20, 20, s&, 0
for i = 0 to 3
    gfx.draw.sprite c&, (i mod 2) * 80, (i \\ 2) * 60, d&, (i and 1) * gfx.draw.OPAQUE
next
' a surface drawn into, its pixels read back
c& = cell&(15)
e& = gfx.surface.new(16, 12)
if {depth} = 8 then
    dim same&(255)
    gfx.display.get_palette 0, same&()
    was& = same&(201)
    same&(201) = same&(200)
    gfx.display.set_palette 0, same&()
end if
for i = 0 to 15
    gfx.draw.rect e&, i, 0, i, 11, (200 + (i and 1)) * {spread} and &HFFFFFF, gfx.draw.FILL
next
gfx.draw.sprite e&, -10, -3, s&, 0
for y = 0 to 11
    for x = 0 to 15
        v& = gfx.surface.point(e&, x, y)
        if {depth} = 8 then v& = (v& * 7 + 16) and 255
        gfx.draw.rect c&, x * 9, y * 9, x * 9 + 8, y * 9 + 8, v&, gfx.draw.FILL
    next
next
' row 0 as the cells were drawn in it: a card holds the colours drawn
if {depth} = 8 then
    same&(201) = was&
    gfx.display.set_palette 0, same&()
end if
gfx.display.flip
do
    gfx.display.flip
loop
"""


DEPTHS = (8, 15, 16, 32)
# GPU_SCENE drawn once and summed as SAME_PIXELS is: the styles' pixels too, for --sums
STYLE_PIXELS = GPU_SCENE.replace("""gfx.display.flip
do
    gfx.display.flip
loop
""", """dim px(640& * 480 * {bytes} - 1) as byte
gfx.surface.read canvas&, 0, 0, 640, 480, px()
for i& = 0 to 640& * 480 * {bytes} - 1
    sum& = (sum& * 31 + px(i&)) and &HFFFFF
next
gfx.display.close
print sum&
""")


def pixel_sums(
    build: str, depths: tuple[int, ...] = DEPTHS, root: Path = dosrun.ROOT, scene: str = SAME_PIXELS
) -> dict[int, str]:
    """scene's output at each depth: build "dos", root's DOS slice in one DOSBox, or "host", root's host build
    natively."""
    qbx = (dosrun.COM.parent if build == "dos" else root / "build/host") / "lib/gfx.qbx"
    data = qbx.read_bytes()
    jobs = {
        f"{build}{d}": (scene.format(depth=d, bytes=(d + 7) // 8, spread=1 if d == 8 else 40503), "", "",
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


def gpu_shot(depth: int, how: str) -> tuple[int, bytes]:
    """GPU_SCENE's frame drawn `how`, "soft" or "gpu", in the Mac app: its width and RGB rows."""
    app = dosrun.ROOT / "build/mac/QuickrBasic.app/Contents/MacOS/QuickrBasic"
    with tempfile.TemporaryDirectory() as tmp:
        prog, shot = Path(tmp) / "SCENE.BAS", Path(tmp) / "shot.ppm"
        prog.write_text(GPU_SCENE.format(depth=depth, bytes=(depth + 7) // 8, spread=1 if depth == 8 else 40503))
        env = os.environ | {"QB_GFX": how, "QB_KEYS": "3F00", "QB_SHOT": f"{shot}@3s", "SDL_AUDIO_DRIVER": "dummy"}
        subprocess.run([str(app), prog.name], env=env, cwd=tmp, timeout=60, check=True)
        head, pixels = shot.read_bytes().split(b"\n", 1)
    return int(head.split()[1]), pixels


CARD_SHOT_MS = 300000  # emulated: the most the scene is given to be drawn
CARD_DRAWN = "\x04"  # written to port 0xE9 once the scene is drawn and shown


def card_scene(depth: int) -> str:
    """GPU_SCENE at depth as a card draws it: CARD_DRAWN said once it is shown."""
    scene = GPU_SCENE.format(depth=depth, bytes=(depth + 7) // 8, spread=1 if depth == 8 else 40503)
    loop = "do\n    gfx.display.flip\nloop\n"
    assert scene.count(loop) == 1, "the scene's last loop not found once"
    return scene.replace(loop, f"out &HE9, {ord(CARD_DRAWN)}\n{loop}")


def most_off(name: str, depth: int, card: bool) -> int:
    """The most a channel of cell name may be off at depth, drawn by the Mac's GPU or a card."""
    if not card:
        return GPU_NEAR.get((name, depth), 0)
    if name in CARD_EXACT and depth != 8:
        return 0
    return max(GPU_NEAR.get((name, depth), 0), CARD_NEAR_AT.get((name, depth), CARD_NEAR))


def card_vm(card: str) -> Path:
    """The VM card is on: the Pentium 100's."""
    import bench

    return bench.HL86_VMS / bench.card_vm(card, None)


def card_shot(card: str, depth: int, sets: tuple[str, ...] = (), keep: str = "") -> tuple[int, bytes]:
    """GPU_SCENE drawn by the DOS build on 86Box's card, through lib/drivers/: its width and RGB rows,
    the emulator's screenshot once CARD_SHOT_MS have run. sets: NAME=VALUE, in DOS's environment; keep: a
    file the screenshot is copied to, a PPM."""
    import bench

    hl86 = bench.hl86_module()
    vm = card_vm(card)
    image, built = vm / "c.img", dosrun.COM.parent
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        qb = d / "QB"
        (qb / "LIB" / "DRIVERS").mkdir(parents=True)
        for name in ("basic32.exe", "DOS32A.EXE", "lib/gfx.qbx", *(str(f.relative_to(built)) for f in
                                                                     (built / "lib/drivers").glob("*.DRV"))):
            (qb / name.upper()).write_bytes((built / name).read_bytes())
        (qb / "SCENE.BAS").write_text(card_scene(depth))
        bat = d / "AUTOEXEC.BAT"
        bat.write_bytes("".join(f"{n}\r\n" for n in ("@ECHO OFF", *(f"SET {v}" for v in sets), "CD \\QB",
                                                    "BASIC32 SCENE.BAS")).encode())
        was = hl86.dos_get(image, "AUTOEXEC.BAT")
        bench.mtools("mdeltree", image, "::QB", check=False)
        bench.mtools("mcopy", image, "-s", "-o", str(qb), "::")
        hl86.dos_put(image, bat, "AUTOEXEC.BAT")
        shot = d / "shot.ppm"
        try:
            proc = hl86.launch(bench.HL86 / "build/src/86Box-headless", vm, bench.HL86_ROMS, d / "s",
                               unthrottled=True)
            try:
                with hl86.Client(d / "s", timeout=30.0) as c:
                    said = ""
                    while CARD_DRAWN not in said:
                        ms = int(c.status()["emu_ms"])
                        assert ms < CARD_SHOT_MS, f"the scene not drawn in {ms} emulated ms: {said[-400:]}"
                        if "Press F1 to continue" in c.screen_text():
                            c.keys(keys=["F1"])
                        said += c.debug_out()
                        time.sleep(0.2)
                    c.screenshot(shot)
                    print(f"  drawn by {ms} emulated ms")
                    if said := (said + c.debug_out()).replace(CARD_DRAWN, ""):
                        print(said)
                    c.quit()
            finally:
                proc.kill()
        finally:
            bat.write_bytes(was)
            hl86.dos_put(image, bat, "AUTOEXEC.BAT")
            bench.mtools("mdeltree", image, "::QB", check=False)
        if keep:
            Path(keep).write_bytes(shot.read_bytes())
        head, pixels = ppm(shot.read_bytes())
    return head, pixels


def ppm(data: bytes) -> tuple[int, bytes]:
    """A binary PPM's width and RGB rows."""
    magic, w, h, top, pixels = data.split(maxsplit=4)
    assert magic == b"P6" and top == b"255", "not an 8-bit PPM"
    return int(w), pixels[: int(w) * int(h) * 3]


def gpu_parity(depth: int, card: str = "", sets: tuple[str, ...] = (), keep: str = "") -> list[tuple[str, int, int]]:
    """Each GPU_CELLS cell: the pixels the GPU drew other than pages did, and the most a channel was off;
    the GPU the Mac app's, or 86Box's card's, sets in DOS's environment."""
    w, soft = gpu_shot(depth, "soft")
    _, gpu = card_shot(card, depth, sets, keep) if card else gpu_shot(depth, "gpu")
    out = []
    for k, name in enumerate(GPU_CELLS):
        x0, y0 = k % 4 * 160, k // 4 * 120
        n = most = 0
        for y in range(y0, y0 + 120):
            row = (y * w + x0) * 3
            a, b = soft[row : row + 480], gpu[row : row + 480]
            for x in range(0, 480, 3):
                d = max(abs(a[x] - b[x]), abs(a[x + 1] - b[x + 1]), abs(a[x + 2] - b[x + 2]))
                n += d > 0
                most = max(most, d)
        out.append((name, n, most))
    return out


def gfx_lib() -> dict[str, bytes]:
    return {"lib/gfx.qbx": (dosrun.COM.parent / "lib/gfx.qbx").read_bytes()}


if __name__ == "__main__":
    if sys.argv[1:2] in (["--gpu"], ["--card"]):
        from mac_test import virtual

        card = sys.argv.pop(2) if sys.argv[1] == "--card" else ""
        sets = []
        keep = ""
        while sys.argv[2:3] in (["--set"], ["--shot"]):
            if sys.argv[2] == "--shot":
                keep = sys.argv.pop(3)
            else:
                sets.append(sys.argv.pop(3))
            sys.argv.pop(2)
        if not card and virtual():
            print("a virtual Mac, no GPU: GPU_SCENE not compared")
            sys.exit()
        wrong = []
        for depth in [int(d) for d in sys.argv[2:]] or [8, 15, 16, 32]:
            print(f"{depth} bits: pixels the GPU drew other than pages, the most a channel was off")
            for name, n, most in gpu_parity(depth, card, tuple(sets), keep):
                print(f"  {name:12}{n:7}{most:5}")
                near = most <= most_off(name, depth, bool(card))
                loose = GPU_LOOSE + (CARD_LOOSE if card else ())
                wrong += [f"{name} at {depth} bits"] if not near and name not in loose else []
        sys.exit(f"GPU differs: {', '.join(wrong)}" if wrong else None)
    if sys.argv[1:2] == ["--sums"]:
        root = Path(sys.argv[2]) if len(sys.argv) > 2 else dosrun.ROOT
        styles = pixel_sums("host", root=root, scene=STYLE_PIXELS)
        for depth, out in pixel_sums("host", root=root).items():
            print(depth, *(o.split()[-1] if o.split() else o for o in (out, styles[depth])))
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
        shown = screen_error()
        assert "Illegal function call" in shown, shown
    print("ok")
