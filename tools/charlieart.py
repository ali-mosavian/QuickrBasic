#!/usr/bin/env python3
"""charlieart.py [--check] [--sheet PNG]: CHARLIE.BAS's palette and pictures (Charlie, tiles, things, props),
drawn here as shaded, dithered pixels and written into the game as DATA between its CharlieArt markers:
drawn by the game they would cost its start-up a pixel at a time. --check: fail when the DATA is
stale. --sheet: every picture to a PNG, to look at.

A picture's DATA: its index, width, height, masked, mirror's index (-1 none), then its GET buffer's
LONGs and, masked, its mask's: a count n then n values; -n then one value n times; 0 the end."""

import sys
import math
import struct
import argparse
import itertools
from pathlib import Path
from collections.abc import Callable

ROOT = Path(__file__).resolve().parents[1]
DEMO = ROOT / "src/plat/wasm/web/demos/CHARLIE.BAS"
BEGIN, END = "' CharlieArt's begin.\n", "' CharlieArt's end.\n"
WIDTH = 100  # a DATA line's most characters
INK, WHITE = 2, 1
BAYER = ((0, 8, 2, 10), (12, 4, 14, 6), (3, 11, 1, 9), (15, 7, 13, 5))
PAL: dict[int, tuple[int, int, int]] = {0: (2, 2, 8), 1: (63, 63, 63), 2: (5, 3, 8), 3: (63, 56, 20), 4: (60, 14, 12)}


def ramp(first: int, a: tuple[int, int, int], b: tuple[int, int, int], n: int) -> list[int]:
    """n palette entries from first, a to b; their indices."""
    for i in range(n):
        t = i / (n - 1)
        PAL[first + i] = (
            round(a[0] + (b[0] - a[0]) * t),
            round(a[1] + (b[1] - a[1]) * t),
            round(a[2] + (b[2] - a[2]) * t),
        )
    return list(range(first, first + n))


FUR = ramp(8, (33, 30, 31), (63, 63, 61), 6)
SKIN = ramp(14, (38, 24, 18), (62, 47, 38), 4)
TONGUE = ramp(18, (46, 16, 24), (63, 38, 44), 3)
SCARF = ramp(21, (30, 3, 8), (63, 20, 14), 4)
GRASS = ramp(25, (4, 26, 6), (42, 63, 10), 5)
EARTH = ramp(30, (18, 6, 3), (63, 38, 8), 8)
STONE = ramp(38, (22, 20, 24), (56, 54, 54), 4)
GOLD = ramp(42, (40, 20, 2), (63, 62, 34), 5)
TEAL = ramp(47, (2, 16, 24), (30, 60, 58), 5)
METAL = ramp(52, (16, 18, 24), (58, 60, 63), 4)
WOOD = ramp(56, (16, 9, 5), (42, 28, 14), 4)
LEAF = ramp(60, (40, 6, 2), (63, 54, 6), 6)
PETAL = ramp(66, (48, 16, 34), (63, 56, 60), 3)
SKY = ramp(96, (2, 6, 46), (26, 50, 63), 32)
FAR = ramp(128, (18, 12, 46), (46, 42, 63), 4)
MID = ramp(132, (2, 24, 16), (26, 58, 18), 6)
CLOUD = ramp(138, (48, 44, 52), (63, 62, 60), 3)
SUN = ramp(141, (63, 50, 30), (63, 63, 52), 3)


class Pic:
    """A w by h picture of palette indices, 0 clear."""

    def __init__(self, w: int, h: int) -> None:
        self.w, self.h = w, h
        self.p = [[0] * w for _ in range(h)]

    def put(self, x: int, y: int, c: int) -> None:
        if 0 <= x < self.w and 0 <= y < self.h:
            self.p[y][x] = c

    def get(self, x: int, y: int) -> int:
        return self.p[y][x] if 0 <= x < self.w and 0 <= y < self.h else 0

    def shade(self, x: int, y: int, r: list[int], v: float) -> None:
        """r's entry for v in 0..1, dithered between neighbours."""
        f = max(0.0, min(1.0, v)) * (len(r) - 1)
        i = int(f)
        if i < len(r) - 1 and f - i > (BAYER[y & 3][x & 3] + 0.5) / 16:
            i += 1
        self.put(x, y, r[i])

    def ball(
        self,
        cx: float,
        cy: float,
        rx: float,
        ry: float,
        r: list[int],
        lo: float = 0.0,
        hi: float = 1.0,
        over: bool = False,
    ) -> None:
        """An ellipse lit from the upper left as a ball; over: only on what is drawn."""
        for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
            for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
                dx, dy = (x - cx) / rx, (y - cy) / ry
                d = dx * dx + dy * dy
                if d <= 1 and (not over or self.get(x, y)):
                    lit = -0.5 * dx - 0.62 * dy + 0.6 * math.sqrt(1 - d)
                    self.shade(x, y, r, lo + (hi - lo) * (0.12 + 0.88 * max(lit, 0.0)))

    def line(self, x0: float, y0: float, x1: float, y1: float, c: int, w: float = 1) -> None:
        n = int(max(abs(x1 - x0), abs(y1 - y0)) * 2) + 1
        for i in range(n + 1):
            x, y = x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n
            for yy in range(round(y - (w - 1) / 2), round(y - (w - 1) / 2) + round(w)):
                for xx in range(round(x - (w - 1) / 2), round(x - (w - 1) / 2) + round(w)):
                    self.put(xx, yy, c)

    def box(self, x0: int, y0: int, x1: int, y1: int, c: int) -> None:
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.put(x, y, c)

    def outline(self, c: int = INK) -> "Pic":
        edge = [
            (x, y)
            for y in range(self.h)
            for x in range(self.w)
            if not self.p[y][x] and any(self.get(x + i, y + j) for i, j in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        ]
        for x, y in edge:
            self.p[y][x] = c
        return self

    def longs(self, mask: bool = False) -> list[int]:
        """The GET buffer's LONGs: width in bits and height, then the pixels by rows; mask: 255 clear, 0 drawn."""
        px = bytes((0 if c else 255) if mask else c for row in self.p for c in row)
        raw = struct.pack("<HH", self.w * 8, self.h) + px
        raw += bytes(-len(raw) % 4)
        return list(struct.unpack(f"<{len(raw) // 4}i", raw))


def noise(x: int, y: int, seed: int = 0) -> float:
    n = (x * 374761393 + y * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFF) / 65536


# ---- Charlie ----


def paw(p: Pic, hx: float, hy: float, a: float, far: bool, lift: float = 0) -> None:
    """A short leg from the hip at angle a from straight down, its paw a tuft."""
    fx, fy = hx + math.sin(a) * 5.5, min(hy + math.cos(a) * 5.5, 28.5) - lift
    p.line(hx, hy, fx, fy, FUR[1] if far else FUR[3], 3)
    p.ball(fx + 0.8, fy + 0.6, 2.5, 1.7, FUR, 0.0 if far else 0.4, 0.5 if far else 1.1)


def plume(p: Pic, x: float, y: float, wag: float, stream: bool) -> None:
    """The tail, a plume carried over the back; stream: flat out behind."""
    arc = ((0, 0, 2.2), (-2, -3, 2.6), (-1.5, -6.5, 2.8), (1.5, -8.5, 2.6), (4.5, -7.5, 2.0))
    flat = ((0, 0, 2.2), (-2.5, -1, 2.5), (-5, -1.5, 2.4), (-7, -1, 2.0), (-8.5, 0, 1.4))
    for i, (dx, dy, r) in enumerate(flat if stream else arc):
        p.ball(x + dx + wag * i * 0.6, y + dy + (wag * (i % 2) if stream else 0), r, r, FUR, 0.3, 1.1)


def ears(p: Pic, hx: float, hy: float, back: float) -> None:
    """A drop ear from the head's side, hung at angle back from straight down."""
    for d, r in ((1.0, 2.6), (3.2, 2.7), (5.4, 2.1)):
        p.ball(hx - 3.6 - math.sin(back) * d, hy - 1.5 + math.cos(back) * d, r, r, FUR, 0.1, 0.85)


def dog(
    bob: float = 0,
    lean: float = 0,
    front: float = 0.15,
    back: float = -0.15,
    eye: str = "open",
    wag: float = 0,
    ear: float = 0.1,
    wheel: int = -1,
    lift: float = 0,
    squat: float = 0,
    stretch: float = 0,
    pant: bool = False,
) -> Pic:
    """Charlie standing, facing right, paws on row 30. bob drops the body, lean tips the head forward; front and
    back are the leg pairs' angles; wag the tail's sway, ear its ears' fly; wheel a blur of legs instead; squat
    lowers it, stretch lengthens it; pant: its tongue out."""
    p = Pic(32, 32)
    by = 19.5 + bob + squat
    hip = 23 + bob * 0.5 + squat
    plume(p, 5.5 - stretch, by - 3, wag, wheel >= 0)
    if wheel >= 0:
        for i in range(7):
            a = (i + wheel * 0.5) * math.pi / 3.5
            p.line(
                13 + math.cos(a) * 2,
                27 + math.sin(a) * 1.2,
                13 + math.cos(a) * 9,
                27 + math.sin(a) * 3.5,
                (FUR[2], FUR[4], FUR[1])[i % 3],
                1,
            )
    else:
        paw(p, 8, hip, back * 0.6 + 0.15, True)
        paw(p, 18, hip, front * 0.6 - 0.15, True)
    p.ball(12.5, by, 8.5 + stretch, 6 - stretch * 0.5, FUR, 0.3, 1.15)
    p.ball(18.5 + stretch * 0.5, by - 0.5, 4.6, 5.4 - stretch * 0.4, FUR, 0.35, 1.2)
    if wheel < 0:
        p.ball(12.5, by + 3, 8, 3.6, FUR, 0.25, 1.0)
        for x in range(6, 20, 2):
            p.put(x, round(by + 6), FUR[2])
        paw(p, 8, hip, back, False)
        paw(p, 18, hip, front, False, lift)
    hx, hy = 22.5 + lean + stretch * 0.5, 12 + bob + squat * 1.2 + lean * 0.3 + stretch * 0.6
    p.ball(hx, hy, 6.2, 5.8, FUR, 0.4, 1.2)
    p.ball(hx + 3.6, hy + 4.2, 3.6, 2.6, FUR, 0.3, 1.0)
    p.ball(hx + 4.8, hy + 2.2, 3.4, 2.5, FUR, 0.5, 1.25)
    ears(p, hx, hy, ear)
    p.box(round(hx + 6), round(hy + 1), round(hx + 8), round(hy + 2), INK)
    p.put(round(hx + 6), round(hy + 1), FUR[5])
    p.line(hx + 4.5, hy + 4.4, hx + 6.5, hy + 4.4, INK)
    if pant or eye == "happy":
        p.box(round(hx + 5), round(hy + 5), round(hx + 6), round(hy + 6), TONGUE[1])
        p.put(round(hx + 5), round(hy + 7), TONGUE[0])
    ex, ey = round(hx + 1), round(hy - 1)
    if eye == "open":
        p.box(ex, ey, ex + 1, ey + 1, INK)
        p.put(ex, ey - 1, INK)
        p.put(ex + 1, ey - 1, INK)
        p.put(ex, ey - 1, WHITE)
    elif eye == "happy":
        p.line(ex - 1, ey + 1, ex + 0.5, ey - 0.5, INK)
        p.line(ex + 0.5, ey - 0.5, ex + 2, ey + 1, INK)
    elif eye == "hurt":
        p.line(ex - 1, ey - 1.5, ex + 2, ey + 1.5, INK)
        p.line(ex - 1, ey + 1.5, ex + 2, ey - 1.5, INK)
    else:
        p.line(ex - 1, ey + 0.5, ex + 2, ey + 0.5, INK)
    return p.outline()


def afraid(f: int = 0) -> Pic:
    """Charlie up on its back legs, side on, its face turned out, afraid: its eyes wide, its paws hung before it;
    f its shiver's frame."""
    p = Pic(32, 32)
    for x, y, r in ((8, 26, 2.3), (5.5, 24, 2.2), (4, 21.5, 1.7)):
        p.ball(x - f, y, r, r, FUR, 0.2, 0.9)
    p.ball(12, 24.5, 4.6, 4.0, FUR, 0.3, 1.0)
    p.ball(14, 19.5, 4.8, 6.6, FUR, 0.35, 1.15)
    for x, lo in ((12.5, 0.2), (16, 0.45)):
        p.ball(x + 1.5, 28.4, 3.0, 1.7, FUR, lo, lo + 0.7)
    for y, lo in ((16.5, 0.3), (19.5, 0.6)):
        p.line(17, y, 20, y + 1 + f, FUR[3], 2)
        p.ball(21, y + 1.6 + f, 2.0, 1.8, FUR, lo, lo + 0.7)
        p.line(20, y + 3.6 + f, 22, y + 3.6 + f, FUR[0])
    for side in (-1, 1):
        for y, r in ((8.5, 2.5), (11.5, 2.6), (14, 2.0)):
            p.ball(17 + side * (7.4 + f * 0.8), y + f, r, r, FUR, 0.1, 0.85)
    p.ball(17, 9.5, 6.6, 6.0, FUR, 0.45, 1.2)
    p.ball(17, 12.5, 3.6, 2.8, FUR, 0.6, 1.3)
    p.box(16, 11, 18, 12, INK)
    p.box(16, 14, 17, 15, INK)
    for side in (-1, 1):
        ex = 17 + side * 4
        p.box(ex - 1, 7, ex + 1, 9, INK)
        p.put(ex - 1, 7, WHITE)
        p.put(ex + 1, 9, FUR[2])
        p.line(ex - side * 1.5, 5, ex + side * 1.5, 6, FUR[1])
    p.put(26, 3 + f * 2, TEAL[3])
    p.box(26, 4 + f * 2, 26, 5 + f * 2, TEAL[4])
    return p.outline()


def curled(turn: float) -> Pic:
    """Charlie rolled up: a lit ball of fluff in a spiral, an ear and the nose round it, turned."""
    p = Pic(32, 32)
    cx, cy, r = 15.5, 17.5, 12.5
    for y in range(32):
        for x in range(32):
            dx, dy = x - cx, y - cy
            d = math.hypot(dx, dy)
            if d <= r:
                a = math.atan2(dy, dx) - turn
                band = (a / (2 * math.pi) * 7 + d * 0.16) % 1
                lit = (-0.5 * dx - 0.62 * dy) / r + 0.6 * math.sqrt(max(0, 1 - (d / r) ** 2))
                p.shade(x, y, FUR, 0.42 + 0.7 * max(lit, 0) - (0.22 if band < 0.2 else 0))
    for d in (5.5, 8.0):
        p.ball(cx + math.cos(turn) * d, cy + math.sin(turn) * d, 2.4, 2.4, FUR, 0.0, 0.6)
    p.ball(cx + math.cos(turn + 2.4) * 6, cy + math.sin(turn + 2.4) * 6, 1.6, 1.4, [INK, INK])
    return p.outline()


def hero() -> list[Pic]:
    """Idle, blink, two pants, six of the run, two of the dash, four curled, skid, crouch, afraid, sprung, two wins,
    afraid's shiver."""
    out = [dog(), dog(eye="blink"), dog(pant=True, wag=0.8), dog(pant=True, wag=-0.8, lift=1.5, front=0.4)]
    for f in range(6):
        ph = 2 * math.pi * f / 6
        out.append(
            dog(
                bob=round(-abs(math.sin(ph))),
                lean=1,
                front=1.0 * math.sin(ph),
                back=-1.0 * math.sin(ph),
                ear=0.7 + 0.4 * math.cos(ph),
                wag=0.5 * math.sin(ph),
                stretch=abs(math.sin(ph)),
            )
        )
    for f in range(2):
        out.append(dog(bob=1, lean=2, wheel=f, ear=1.5, wag=f - 0.5, stretch=2, eye="happy"))
    out += [curled(i * math.pi / 14) for i in range(4)]
    out.append(dog(bob=1, lean=-2, front=1.0, back=0.8, ear=-0.6, wag=-1))
    out.append(dog(squat=4, lean=2, front=1.3, back=-1.2, ear=0.2))
    out.append(afraid())
    out.append(dog(bob=-2, front=0.2, back=-0.2, ear=-0.5, wag=-0.6, stretch=1))
    out.append(dog(eye="happy", wag=1.0))
    out.append(dog(bob=-2, eye="happy", wag=-1.0, front=0.5, back=-0.5, ear=0.8))
    out.append(afraid(1))
    return out


def tilted(src: Pic, deg: float) -> Pic:
    """src turned deg degrees counterclockwise about its middle, (16, 18): nose up a rise to the right."""
    a = math.radians(deg)
    p = Pic(src.w, src.h)
    for y in range(p.h):
        for x in range(p.w):
            dx, dy = x - 16, y - 18
            p.put(
                x,
                y,
                src.get(
                    round(16 + dx * math.cos(a) - dy * math.sin(a)), round(18 + dx * math.sin(a) + dy * math.cos(a))
                ),
            )
    return p


def tilts() -> list[Pic]:
    """Three of the run and the two of the dash, turned each 22.5 degrees from level to all but upside down: the
    game mirrors them and turns them over for the rest of the way round."""
    h = hero()
    return [tilted(h[f], i * 22.5) for i in range(8) for f in (4, 6, 8, 10, 11)]


# ---- the family ----

HAIR = ramp(69, (18, 9, 5), (52, 35, 18), 4)
BLOND = ramp(73, (38, 25, 9), (63, 58, 34), 4)
DARK = ramp(77, (5, 4, 8), (30, 30, 36), 4)
DENIM = ramp(81, (26, 34, 46), (52, 57, 62), 3)
NAVY = ramp(84, (7, 10, 26), (22, 32, 52), 3)
TAN = ramp(87, (34, 19, 12), (58, 41, 30), 3)
BLUSH, BROWN, BLUE, LIP, ASH = 90, 91, 92, 93, 94
PAL.update({BLUSH: (62, 38, 38), BROWN: (26, 13, 5), BLUE: (14, 34, 62), LIP: (48, 16, 20), ASH: (40, 40, 44)})
PALE = SKIN[1:]  # a skin: its lines, its shadow, itself
FAMW, FAMH, FEET, HEAD = 180, 82, 80, 9.5


def oval(p: Pic, cx: float, cy: float, rx: float, ry: float, r: list[int], sh: float = 0.45, hl: float = -0.6) -> None:
    """An ellipse in flat tones, lit from the upper left: r's first in its shadow, its third, if any, in its light."""
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            dx, dy = (x - cx) / rx, (y - cy) / ry
            if dx * dx + dy * dy <= 1:
                s = 0.6 * dx + 0.8 * dy
                p.put(x, y, r[0] if s > sh else r[2] if len(r) > 2 and s < hl else r[1])


def limb(p: Pic, x0: float, y0: float, x1: float, y1: float, w0: float, w1: float, r: list[int]) -> None:
    """A round ended limb, w0 wide to w1, r's first down its shadowed side."""
    n = int(max(abs(x1 - x0), abs(y1 - y0)) * 2) + 1
    near: dict[tuple[int, int], tuple[float, float]] = {}
    for i in range(n + 1):
        t = i / n
        x, y, w = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, (w0 + (w1 - w0) * t) / 2
        for yy in range(int(y - w) - 1, int(y + w) + 2):
            for xx in range(int(x - w) - 1, int(x + w) + 2):
                d = (xx - x) ** 2 + (yy - y) ** 2
                if d <= w * w and d < near.get((xx, yy), (1e9, 0))[0]:
                    near[(xx, yy)] = (d, (0.85 * (xx - x) + 0.35 * (yy - y)) / w)
    for (xx, yy), (_, s) in near.items():
        p.put(xx, yy, r[0] if s > 0.38 else r[1])


def trunk(p: Pic, cx: float, rows: list[tuple[float, float]], r: list[int]) -> None:
    """A body between rows of (y, half its width), shadowed down its right, lit, with a third tone, down its left."""
    for (ya, wa), (yb, wb) in itertools.pairwise(rows):
        for y in range(round(ya), round(yb) + 1):
            w = wa + (wb - wa) * (y - ya) / max(yb - ya, 1)
            for x in range(round(cx - w), round(cx + w) + 1):
                u = (x - cx) / w
                p.put(x, y, r[0] if u > 0.5 else r[2] if len(r) > 2 and u < -0.6 else r[1])


def tri(p: Pic, a: tuple[float, float], b: tuple[float, float], c: tuple[float, float], col: int) -> None:
    def side(u: tuple[float, float], v: tuple[float, float], x: float, y: float) -> float:
        return (v[0] - u[0]) * (y - u[1]) - (v[1] - u[1]) * (x - u[0])

    for y in range(int(min(a[1], b[1], c[1])), int(max(a[1], b[1], c[1])) + 2):
        for x in range(int(min(a[0], b[0], c[0])), int(max(a[0], b[0], c[0])) + 2):
            d = side(a, b, x, y), side(b, c, x, y), side(c, a, x, y)
            if all(v >= 0 for v in d) or all(v <= 0 for v in d):
                p.put(x, y, col)


def body(
    p: Pic,
    cx: int,
    tall: int,
    top: list[int],
    legs: list[int],
    skin: list[int],
    cheer: int,
    sw: int,
    leg: int,
    tee: bool = False,
    shorts: int = 0,
    hem: int = 2,
    lx: int = 4,
    lw: tuple[float, float] = (7, 5),
    aw: float = 5,
    arms: tuple[int, int] = (1, 1),
) -> tuple[int, int, int]:
    """Someone stood facing out, feet on row FEET, tall high under a head HEAD round: a top to hem rows under the
    hips, sw half its width (tee: its sleeves short), legs leg rows long of a ramp (shorts: their rows, bare under
    them); cheer: arms up; arms: which are drawn, the left and the right. The rows of the head's middle, the
    shoulders and the hips."""
    hy = FEET - tall + 10
    sy, hip = hy + 11, FEET - leg
    for side in (-1, 1):
        limb(p, cx + side * lx, hip, cx + side * lx, FEET - 4, lw[0], lw[1], skin[1:] if shorts else legs)
    if shorts:
        trunk(p, cx, [(hip - 2, sw - 2), (hip + shorts, lx + lw[0] / 2 + 1.5)], legs)
        p.line(cx, hip + shorts - 5, cx, hip + shorts - 2, legs[0])
        p.p[hip + shorts][cx] = p.p[hip + shorts - 1][cx] = 0
        for side in (-1, 1):
            p.line(cx + side * 2, hip + shorts, cx + side * (lx + lw[0] / 2 + 1), hip + shorts, legs[0])
    for i, side in enumerate((-1, 1)):
        if not arms[i]:
            continue
        sx, ay, n = cx + side * (sw - 1), sy + 3, hip - sy - 2
        if cheer:
            pts = [(sx, ay), (sx + side * 5, ay - 3), (sx + side * 6, ay - 12)]
        else:
            pts = [(sx, ay), (sx + side * 2, ay + n * 0.5), (sx + side * 2, ay + n)]
        limb(p, *pts[0], *pts[1], aw + 1, aw, top)
        limb(p, *pts[1], *pts[2], aw, aw - 1, skin[1:] if tee else top)
        if tee:
            p.line(pts[1][0] - 2, pts[1][1], pts[1][0] + 2, pts[1][1], top[0])
        oval(p, pts[2][0], pts[2][1] + (-2 if cheer else 2), 2.6, 2.6, skin[1:])
    trunk(p, cx, [(sy, sw - 4), (sy + 2, sw - 1), (sy + 4, sw), (hip + hem, sw - 1)], top)
    p.box(cx - 2, hy + 8, cx + 2, sy + 1, skin[1])
    return hy, sy, hip


def shoes(p: Pic, cx: int, lx: int, upper: list[int], sole: int, sock: int = 0) -> None:
    """A pair of shoes under legs lx from cx, of a shadow and a tone: socks above them, rows of sock."""
    for side in (-1, 1):
        x0 = cx + side * lx
        if sock:
            p.box(x0 - 2, FEET - 4 - sock, x0 + 2, FEET - 4, FUR[5])
            p.box(x0 + 2, FEET - 4 - sock, x0 + 2, FEET - 4, FUR[3])
            p.box(x0 - 2, FEET - 4 - sock, x0 + 2, FEET - 4 - sock, FUR[4])
        p.box(x0 - 2, FEET - 4, x0 + 2, FEET - 4, upper[1])
        p.box(x0 - 3 - (side < 0), FEET - 3, x0 + 3 + (side > 0), FEET - 1, upper[1])
        p.box(x0 - 3 - (side < 0), FEET - 1, x0 + 3 + (side > 0), FEET - 1, upper[0])
        p.box(x0 - 3 - (side < 0), FEET, x0 + 3 + (side > 0), FEET, sole)


def head(p: Pic, cx: int, hy: int, skin: list[int], ears: bool = True) -> None:
    if ears:
        for side in (-1, 1):
            oval(p, cx + side * 9.6, hy + 2, 1.9, 2.7, skin[1:])
            p.put(cx + side * 10, hy + 2, skin[0])
    oval(p, cx, hy, HEAD, 10, skin[1:], sh=0.66)


def crown(
    p: Pic, cx: int, hy: int, r: list[int], line: Callable[[int], float], puff: float = 1.0, top: float = 0.0
) -> None:
    """Hair on a head: what of an ellipse puff bigger than it, top higher, is over row line(dx) from its middle."""
    rx, ry = HEAD + puff, 10 + puff + top
    for y in range(int(hy - ry) - 1, hy + 12):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            dx, dy = (x - cx) / rx, (y - hy) / ry
            if dx * dx + dy * dy <= 1 and y <= hy + line(x - cx):
                s = 0.6 * dx + 0.8 * dy
                p.put(x, y, r[2] if -0.8 < s < -0.55 else r[0] if dx > 0.6 else r[1])


def face(
    p: Pic,
    cx: int,
    ey: int,
    iris: int,
    brow: int,
    skin: list[int],
    lip: int,
    wide: int = 2,
    lash: bool = False,
    blush: bool = False,
    teeth: bool = True,
    heavy: bool = False,
    nose: int = 3,
) -> None:
    """A face, its eyes on rows ey and under: irises of a colour under lids and brows, a nose, a smile wide a side."""
    for side in (-1, 1):
        xo = cx + side * 6
        for i in range(4):
            x = xo - side * i
            p.put(x, ey - 1, INK)
            p.put(x, ey - 3, brow)
            if heavy and i:
                p.put(x, ey - 4, brow)
            p.put(x, ey, iris if i in (1, 2) else WHITE)
            p.put(x, ey + 1, WHITE if i in (0, 3) else INK if i == 2 else iris)
        if lash:
            p.put(xo + side, ey - 2, INK)
        if blush:
            p.box(min(xo, xo + side), ey + 3, max(xo, xo + side), ey + 3, BLUSH)
    p.put(cx + 1, ey + nose - 1, skin[1])
    p.box(cx, ey + nose, cx + 1, ey + nose, skin[0])
    my = ey + 6
    p.box(cx - wide, my, cx + wide, my, lip)
    if teeth:
        p.box(cx - wide + 1, my, cx + wide - 1, my, WHITE)
        p.box(cx - wide + 1, my + 1, cx + wide - 1, my + 1, lip)
    for side in (-1, 1):
        p.put(cx + side * (wide + 1), my - 1, skin[0])


def pomchi(p: Pic, cx: int, hop: int) -> None:
    """The Pomchi: small, black and fluffy, a fox's face going grey at the muzzle, big pricked ears, a ruff, a plume
    of a tail curled over its back; sat facing out, its tongue out."""
    f = FEET - hop
    fur = [DARK[0], DARK[1], DARK[2]]
    for x, y, r in ((10, -9, 4), (12, -14, 4.2), (11, -19, 3.6), (8, -23, 2.8)):
        oval(p, cx + x, f + y, r, r, [DARK[0], DARK[1], DARK[3]], hl=-0.3)
    oval(p, cx, f - 8, 9, 8, fur, hl=-0.75)
    for x, y, r in ((-5, -12, 3.2), (0, -10, 4.2), (5, -12, 3.2), (-3, -7, 2.6), (3, -7, 2.6)):
        oval(p, cx + x, f + y, r, r, [DARK[1], DARK[2], DARK[3]], hl=-0.5)
    for side in (-1, 1):
        oval(p, cx + side * 4.5, f - 1.6, 3.2, 2, [DARK[1], DARK[2], DARK[3]], hl=-0.4)
        tri(p, (cx + side * 2, f - 25), (cx + side * 8, f - 23), (cx + side * 7, f - 33), DARK[1])
        tri(p, (cx + side * 4, f - 25), (cx + side * 7, f - 24), (cx + side * 6.5, f - 30), BLUSH)
        p.line(cx + side * 2, f - 25, cx + side * 6, f - 32, DARK[0])
        oval(p, cx + side * 7.5, f - 18, 3, 3.2, fur)
    oval(p, cx, f - 20, 8, 6.5, fur, hl=-0.7)
    oval(p, cx, f - 17.5, 3.6, 3, [ASH, STONE[2], STONE[3]], hl=-0.5)
    for side in (-1, 1):
        x = cx + side * 4
        p.box(x - (side > 0), f - 22, x + (side < 0), f - 21, EARTH[5])
        p.put(x, f - 21, INK)
        p.put(x - side, f - 22, WHITE)
        p.put(x, f - 24, DARK[3])
    p.box(cx - 1, f - 19, cx, f - 18, INK)
    p.put(cx - 1, f - 19, STONE[1])
    p.box(cx - 2, f - 17, cx + 1, f - 17, INK)
    p.box(cx - 1, f - 16, cx, f - 14, TONGUE[1])
    p.put(cx, f - 15, TONGUE[2])


def little(p: Pic, cx: int, cheer: int) -> None:
    """The little one: brown eyed, wavy light brown hair to her shoulders under a white cap spotted with paint, its
    brim out; a pale zipped hoodie with ruffled shoulders, leggings spotted as a leopard, white shoes."""
    pale, hair = [FUR[3], FUR[4], FUR[5]], [HAIR[1], HAIR[2], HAIR[3]]
    hy, sy = FEET - 45 + 10, FEET - 45 + 21
    for side in (-1, 1):
        for y in range(hy - 4, sy + 5):
            x = cx + side * (9.5 + math.sin(y * 1.1 + side) * 1.2 + min(2.0, (y - hy + 4) * 0.25))
            oval(p, x, y, 2.6, 1.6, hair if (y // 2) & 1 else [HAIR[1], HAIR[2], HAIR[2]])
    hy, sy, hip = body(p, cx, 45, pale, [FUR[3], FUR[5]], PALE, cheer, sw=8, leg=13, lx=3, lw=(6, 5), aw=4)
    for x, y in ((-5, 1), (-2, 3), (-4, 5), (2, 1), (5, 3), (3, 5), (-2, 0), (4, 0)):
        if hip + 2 + y < FEET - 4:
            p.put(cx + x, hip + 2 + y, INK)
            p.put(cx + x - 1, hip + 2 + y, GOLD[1])
    shoes(p, cx, 3, [FUR[3], FUR[5]], FUR[2])
    p.line(cx, sy + 1, cx, hip + 2, STONE[2])
    p.put(cx, sy + 2, GOLD[3])
    for side in (-1, 1):
        for i in range(3):
            oval(p, cx + side * (4 + i * 2), sy + 1 + i * 0.6, 1.7, 1.5, [FUR[4], WHITE])
    head(p, cx, hy, PALE, ears=False)
    face(p, cx, hy + 2, BROWN, HAIR[1], PALE, LIP, wide=3, blush=True)
    crown(p, cx, hy, [FUR[3], FUR[5], WHITE], lambda dx: -5, puff=1.4, top=1.0)
    p.box(cx - 13, hy - 4, cx + 12, hy - 4, FUR[5])
    p.box(cx - 13, hy - 3, cx + 12, hy - 3, FUR[3])
    p.box(cx - 13, hy - 5, cx - 11, hy - 5, FUR[5])
    spots = ((-6, -8, LIP, 2), (0, -11, GRASS[3], 2), (5, -8, PETAL[1], 2), (-2, -7, TEAL[3], 1), (3, -10, GOLD[3], 1))
    for x, y, c, n in spots:
        p.box(cx + x, hy + y, cx + x + n - 1, hy + y + (n > 1), c)


def big(p: Pic, cx: int, cheer: int) -> None:
    """The big one: brown eyed, brown hair pulled back and tied up, white headphones, a black hoodie, pale jeans
    worn through at a knee, black shoes on white soles; a guitar on a strap, amber going dark at its edge, two
    pickups, its long pale neck up."""
    black, hair = [DARK[0], DARK[1], DARK[2]], [HAIR[0], HAIR[1], HAIR[2]]
    oval(p, cx, FEET - 58 - 2, 4.2, 3.6, hair)
    hy, sy, hip = body(p, cx, 58, black, DENIM[:2], PALE, cheer, sw=10, leg=19, hem=1, arms=(cheer, 0))
    for side in (-1, 1):
        p.line(cx + side * 4 - 1, hip + 3, cx + side * 4 - 1, FEET - 6, DENIM[2])
    shoes(p, cx, 4, [DARK[0], DARK[1]], FUR[5])
    p.box(cx - 6, hip + 8, cx - 3, hip + 9, PALE[2])
    p.box(cx - 6, hip + 7, cx - 2, hip + 7, WHITE)
    p.box(cx - 5, hip + 10, cx - 2, hip + 10, WHITE)
    oval(p, cx, sy + 1, 6, 2.6, [DARK[1], DARK[2], DARK[3]])
    for side in (-1, 1):
        p.line(cx + side * 2, sy + 3, cx + side * 2, sy + 7, FUR[4])
    gy = hip - 3
    p.line(cx - 8, sy + 1, cx + 6, gy - 5, ASH, 2)
    if not cheer:
        limb(p, cx - 9, sy + 3, cx - 13, sy + 10, 6, 5, black)
        limb(p, cx - 13, sy + 10, cx - 5, gy - 1, 5, 4, black)
    limb(p, cx + 9, sy + 3, cx + 13, sy + 9, 6, 5, black)
    nx, ny = cx + 25, gy - 17 - cheer * 2
    p.line(cx + 6, gy - 1, nx, ny, HAIR[1], 4)
    p.line(cx + 6, gy - 2, nx, ny - 1, BLOND[3], 2)
    for i in range(1, 7):
        fx, fy = cx + 6 + (nx - cx - 6) * i / 7, gy - 2 + (ny - gy + 1) * i / 7
        p.put(round(fx), round(fy), ASH)
    oval(p, nx + 2, ny - 2, 3, 3.6, [HAIR[1], HAIR[2], HAIR[3]])
    p.put(nx + 1, ny - 3, WHITE)
    p.put(nx + 3, ny - 1, WHITE)
    for bx, by, rx, ry in ((cx - 2, gy + 2, 9.5, 7), (cx + 5, gy - 2, 5, 4.6)):
        for y in range(int(by - ry) - 1, int(by + ry) + 2):
            for x in range(int(bx - rx) - 1, int(bx + rx) + 2):
                d = ((x - bx) / rx) ** 2 + ((y - by) / ry) ** 2
                if d <= 1:
                    p.put(x, y, EARTH[1] if d > 0.72 else EARTH[4] if d > 0.42 else GOLD[2])
    oval(p, cx - 3, gy + 1, 5.5, 4, [GOLD[2], GOLD[3]])
    for x in (-5, 0):
        p.box(cx + x, gy - 1, cx + x + 1, gy + 4, DARK[1])
        p.box(cx + x, gy, cx + x, gy + 3, ASH)
    p.box(cx - 9, gy, cx - 8, gy + 3, METAL[3])
    p.line(cx - 8, gy + 2, cx + 6, gy - 1, METAL[3])
    if not cheer:
        oval(p, cx - 4, gy + 1, 2.6, 2.6, PALE[1:])
    oval(p, cx + 14, gy - 8, 2.8, 2.8, PALE[1:])
    head(p, cx, hy, PALE)
    crown(p, cx, hy, hair, lambda dx: -6 + abs(dx) * 0.2 + (abs(dx) > 7) * 3, puff=0.7)
    for dx in (-5, -2, 2, 5):
        p.line(cx + dx, hy - 7, cx + dx * 0.5, hy - 10, HAIR[0])
    face(p, cx, hy + 1, BROWN, HAIR[0], PALE, LIP, wide=2, blush=True)
    for x in range(cx - 11, cx + 12):
        y = hy - 12 + ((x - cx) / 11) ** 2 * 9
        p.box(x, round(y), x, round(y) + 1, FUR[5])
        p.put(x, round(y) + 2, FUR[3])
    for side in (-1, 1):
        oval(p, cx + side * 10.5, hy + 1, 3, 4.6, [FUR[3], FUR[5], WHITE])
        oval(p, cx + side * 11, hy + 1, 1.2, 2.6, [STONE[1], STONE[2]])


def fair(p: Pic, cx: int, cheer: int) -> None:
    """A grown up: blue eyed, tanned, a fair bob swept across her brow and down to her chin, darker at its roots; a
    big grey hoodie, its hood behind and a pocket in front, dark shorts, white socks."""
    grey, blond = [STONE[1], STONE[2], STONE[3]], [BLOND[1], BLOND[2], BLOND[3]]
    oval(p, cx, FEET - 69 + 22, 10, 4.5, grey)
    hy, sy, hip = body(p, cx, 69, grey, NAVY, TAN, cheer, sw=12, leg=28, shorts=9, hem=4, lw=(6, 5))
    shoes(p, cx, 4, [FUR[3], FUR[5]], FUR[2], sock=3)
    p.line(cx - 6, hip - 6, cx + 6, hip - 6, STONE[1])
    p.line(cx - 6, hip - 6, cx - 8, hip + 1, STONE[1])
    p.line(cx + 6, hip - 6, cx + 8, hip + 1, STONE[1])
    p.line(cx - 11, hip + 2, cx + 11, hip + 2, STONE[1])
    p.line(cx - 5, sy, cx, sy + 4, STONE[1])
    p.line(cx + 5, sy, cx, sy + 4, STONE[1])
    for side in (-1, 1):
        p.line(cx + side * 2, sy + 4, cx + side * 2, sy + 10, WHITE)
    for y in range(hy - 6, hy + 9):
        w = 11.5 - max(0, y - hy - 6) * 1.2
        for x in range(round(cx - w), round(cx + w) + 1):
            p.put(x, y, BLOND[1] if x > cx + 6 or y > hy + 6 else BLOND[2])
    head(p, cx, hy, TAN, ears=False)

    def fringe(dx: int) -> float:
        if abs(dx) >= 8:
            return 7
        return -7 + (3 - dx) * 0.45 if dx < 3 else -7 + (dx - 3) * 0.9

    crown(p, cx, hy, blond, fringe, puff=1.6, top=0.6)
    p.line(cx + 3, hy - 11, cx + 3, hy - 8, BLOND[0])
    p.line(cx + 2, hy - 8, cx - 8, hy - 3, BLOND[1])
    p.line(cx + 1, hy - 10, cx - 9, hy - 6, BLOND[3])
    p.line(cx + 5, hy - 9, cx + 9, hy - 3, BLOND[1])
    for side in (-1, 1):
        p.line(cx + side * 10, hy - 2, cx + side * 9, hy + 7, BLOND[1 if side > 0 else 3])
    face(p, cx, hy + 1, BLUE, BLOND[0], TAN, LIP, wide=3, lash=True)


def bearded(p: Pic, cx: int, cheer: int) -> None:
    """A grown up: tanned, short dark hair up in front and close at the sides, heavy dark brows, a short beard going
    grey; a big white shirt, its sleeves to the elbow, loose dark shorts to the knee, white socks."""
    white, hair = [FUR[3], FUR[5]], [DARK[0], DARK[1], DARK[2]]
    hy, sy, hip = body(p, cx, 74, white, [DARK[1], DARK[2], DARK[3]], TAN, cheer, sw=12, leg=27, tee=True, shorts=14)
    shoes(p, cx, 4, [FUR[3], FUR[5]], FUR[2], sock=3)
    oval(p, cx, sy, 4.5, 2.4, TAN[1:])
    p.line(cx - 4, sy + 2, cx + 4, sy + 2, FUR[3])
    p.line(cx - 11, hip + 2, cx + 11, hip + 2, FUR[3])
    p.line(cx - 6, sy + 12, cx - 4, sy + 16, FUR[4])
    p.line(cx + 5, sy + 9, cx + 3, sy + 15, FUR[4])
    head(p, cx, hy, TAN)
    for y in range(hy + 1, hy + 13):
        for x in range(cx - 11, cx + 12):
            dx, dy = (x - cx) / 9.9, (y - hy - 0.6) / 11
            if dx * dx + dy * dy <= 1 and (y >= hy + 6 or abs(x - cx) >= 9 - (y - hy - 1) * 0.6):
                grey = y >= hy + 8 and abs(x - cx) < 6 and (x * 7 + y * 3) % 4 == 0
                p.put(x, y, ASH if grey else DARK[2] if (x + y) % 5 == 0 else DARK[1])
    crown(p, cx, hy, hair, lambda dx: -5 + (abs(dx) > 7) * 6, puff=0.5, top=1.6)
    oval(p, cx + 1, hy - 10, 6.5, 3, hair, hl=-0.3)
    p.line(cx - 3, hy - 11, cx + 4, hy - 12, DARK[2])
    face(p, cx, hy + 1, BROWN, DARK[0], TAN, LIP, wide=2, heavy=True, nose=2)


def family(cheer: int) -> Pic:
    """Who meets Charlie at the level's end, left to right: the Pomchi, the little one, the big one with her guitar,
    and the two grown ups. cheer: arms up, the dog off the ground."""
    p = Pic(FAMW, FAMH)
    pomchi(p, 12, cheer * 3)
    little(p, 42, cheer)
    big(p, 76, cheer)
    fair(p, 124, cheer)
    bearded(p, 160, cheer)
    return p.outline()


# ---- the words ----

# A letter: how wide, then its strokes, on a grid 6 high.
LETTERS: dict[str, tuple[float, list[list[tuple[float, float]]]]] = {
    "A": (4, [[(0, 6), (2, 0), (4, 6)], [(1, 4), (3, 4)]]),
    "B": (
        4,
        [
            [(0, 0), (0, 6)],
            [(0, 0), (2.4, 0), (3.5, 0.8), (3.5, 2.2), (2.4, 3), (0, 3)],
            [(2.4, 3), (4, 3.8), (4, 5.2), (3, 6), (0, 6)],
        ],
    ),
    "C": (4, [[(4, 1.2), (3, 0), (1, 0), (0, 1), (0, 5), (1, 6), (3, 6), (4, 4.8)]]),
    "D": (4, [[(0, 0), (0, 6)], [(0, 0), (2.4, 0), (4, 1.5), (4, 4.5), (2.4, 6), (0, 6)]]),
    "E": (3.6, [[(3.6, 0), (0, 0), (0, 6), (3.6, 6)], [(0, 3), (2.8, 3)]]),
    "H": (4, [[(0, 0), (0, 6)], [(4, 0), (4, 6)], [(0, 3), (4, 3)]]),
    "I": (2.4, [[(1.2, 0), (1.2, 6)], [(0, 0), (2.4, 0)], [(0, 6), (2.4, 6)]]),
    "K": (4, [[(0, 0), (0, 6)], [(4, 0), (0, 3.6)], [(1.5, 2.5), (4, 6)]]),
    "L": (3.6, [[(0, 0), (0, 6), (3.6, 6)]]),
    "M": (5, [[(0, 6), (0, 0), (2.5, 3.6), (5, 0), (5, 6)]]),
    "N": (4, [[(0, 6), (0, 0), (4, 6), (4, 0)]]),
    "O": (4, [[(1, 0), (3, 0), (4, 1), (4, 5), (3, 6), (1, 6), (0, 5), (0, 1), (1, 0)]]),
    "R": (4, [[(0, 0), (0, 6)], [(0, 0), (2.4, 0), (3.8, 0.8), (3.8, 2.4), (2.4, 3.2), (0, 3.2)], [(2, 3.2), (4, 6)]]),
    "S": (
        4,
        [[(4, 1.1), (3, 0), (1, 0), (0, 1), (0, 2.2), (1, 3), (3, 3), (4, 3.8), (4, 5), (3, 6), (1, 6), (0, 4.9)]],
    ),
    "W": (5.4, [[(0, 0), (1.3, 6), (2.7, 1.8), (4.1, 6), (5.4, 0)]]),
    "Y": (4, [[(0, 0), (2, 3.2), (4, 0)], [(2, 3.2), (2, 6)]]),
    " ": (2, []),
}


def words(s: str, w: int, h: int, u: float, pen: float) -> Pic:
    """s as a w by h picture: round stroked letters u a grid step, pen thick, gold from light to deep with a lit
    upper edge and a dark lower one, an ink edge and a shadow. The 8 by 8 font drawn large was all squares."""
    gap = pen + 2
    wide = sum(LETTERS[c][0] * u + gap for c in s) - 2
    x0, y0 = (w - 2 - wide) / 2 + pen / 2, (h - 2 - 6 * u) / 2
    p = Pic(w, h)
    on: set[tuple[int, int]] = set()
    for c in s:
        lw, strokes = LETTERS[c]
        for st in strokes:
            for (ax, ay), (bx, by) in itertools.pairwise(st):
                n = int(max(abs(bx - ax), abs(by - ay)) * u * 2) + 1
                for i in range(n + 1):
                    cx, cy = x0 + (ax + (bx - ax) * i / n) * u, y0 + (ay + (by - ay) * i / n) * u
                    for y in range(int(cy - pen), int(cy + pen) + 1):
                        for x in range(int(cx - pen), int(cx + pen) + 1):
                            if (x - cx) ** 2 + (y - cy) ** 2 <= (pen / 2) ** 2:
                                on.add((x, y))
        x0 += lw * u + gap
    for x, y in on:
        v = 1 - (y - y0 + pen / 2) / (6 * u + pen)
        c = GOLD[1 + min(3, int(v * 3.6))]
        if (x - 1, y - 1) not in on or (x, y - 1) not in on:
            c = WHITE if v > 0.5 else GOLD[4]
        elif (x + 1, y + 1) not in on or (x, y + 1) not in on:
            c = GOLD[0]
        p.put(x, y, c)
    p.outline(EARTH[0])
    edge = {(x, y) for y in range(h) for x in range(w) if p.p[y][x]}
    for x, y in edge:
        if not p.get(x + 1, y + 1):
            p.put(x + 1, y + 1, INK)
    return p.outline()


# ---- the ground ----


def surface(kind: int, i: int) -> int:
    """Tile kind's top row at column i, as the game's Prof: 1 full, 2 and 3 the 45s, 4 to 7 the gentle halves."""
    h = {1: 16, 2: i + 1, 3: 16 - i, 4: (i + 2) // 2, 5: 8 + (i + 2) // 2, 6: 16 - i // 2, 7: 8 - i // 2}[kind]
    return 16 - h


def soil(p: Pic, x: int, y: int, depth: float, seed: int) -> None:
    """Earth at depth pixels under the surface: a zigzag weave of two tones, of period 8 across and 16 down so
    tiles meet; pebbles; darker down."""
    zig = (x + abs(y % 16 - 8)) // 4 % 2
    v = 0.66 - min(depth, 70) * 0.006 + (0.2 if zig else -0.12) + (noise(x % 16, y % 16, seed) - 0.5) * 0.1
    p.shade(x, y, EARTH, v)
    if noise(x % 16 // 2, y % 16 // 2, seed + 7) > 0.975:
        p.put(x, y, STONE[2 if y % 2 == 0 else 0])


def turf(p: Pic, x: int, y: int, t: int, seed: int) -> bool:
    """Grass t pixels under the surface: bright tips, a ragged dark hem. False below the grass."""
    n = 4 + int(noise(x % 16, 3, seed) * 3)
    if t > n:
        return False
    p.shade(x, y, GRASS, 1.0 if t == 0 else 0.85 - t * 0.17 if t < n else 0.0)
    return True


def tile(kind: int, seed: int, ledge: bool = False) -> Pic:
    p = Pic(16, 16)
    for x in range(16):
        top = surface(kind, x)
        for y in range(top, 16):
            if ledge and y >= 12 + int(noise(x, 9, 5) * 3):
                continue
            if not turf(p, x, y, y - top, seed):
                soil(p, x, y, y - top if not ledge else (y - top) * 4, seed)
    return p


def earth(n: int) -> Pic:
    """The nth 16 rows of earth under a surface tile."""
    p = Pic(16, 16)
    for x in range(16):
        for y in range(16):
            soil(p, x, y, 16 * (n + 1) + y, 11)
    return p


# ---- things and props ----


def bone(turn: float) -> Pic:
    """A bone, turned: a shaft, two knobs an end."""
    p = Pic(10, 10)
    c, s = math.cos(turn), math.sin(turn)
    p.line(4.5 - c * 2.4, 4.5 - s * 2.4, 4.5 + c * 2.4, 4.5 + s * 2.4, FUR[4], 2)
    for end in (-2.6, 2.6):
        for side in (-1.2, 1.2):
            p.ball(4.5 + c * end - s * side, 4.5 + s * end + c * side, 1.3, 1.3, FUR, 0.5, 1.2)
    return p.outline()


def cat(f: int) -> Pic:
    """A ginger cat, prowling right: its tail up, its back striped."""
    p = Pic(20, 14)
    fur = EARTH[2:]
    p.line(3, 8, 1.5, 4, fur[2])
    p.line(1.5, 4, 2 + f, 2, fur[3])
    for i, x in enumerate((4, 6, 11, 13)):
        p.line(x, 10, x + (1 if (i + f) % 2 else -1), 12, fur[1 + i % 2])
    p.ball(8, 8.5, 5.5, 2.8, fur, 0.2, 1.0)
    for x in (6, 9):
        p.line(x, 6, x - 1, 8, fur[0])
    for x, y in ((13, 1), (13, 2), (14, 2), (17, 1), (16, 2), (17, 2)):
        p.put(x, y, fur[3])
    p.ball(15, 5.5, 3.2, 3.0, fur, 0.3, 1.1)
    p.put(16, 5, GRASS[4])
    p.put(17, 5, INK)
    p.put(18, 6, PETAL[0])
    p.line(16, 7, 17, 7, INK)
    return p.outline()


def spring(pressed: bool) -> Pic:
    p = Pic(16, 16)
    top = 8 if pressed else 2
    for y in range(top + 3, 13, 2):
        p.line(4, y, 11, y + 1, METAL[2 if y % 4 else 3])
    p.box(2, 13, 13, 14, METAL[1])
    p.box(2, 13, 13, 13, METAL[3])
    p.box(1, top, 14, top + 2, SCARF[2])
    p.box(1, top, 14, top, SCARF[3])
    p.box(1, top + 2, 14, top + 2, SCARF[0])
    return p.outline()


def boost(f: int) -> Pic:
    p = Pic(24, 10)
    p.box(1, 6, 22, 8, METAL[0])
    p.box(1, 5, 22, 5, METAL[2])
    for i in range(4):
        x = 2 + i * 6 + f * 3
        c = GOLD[4 if (i + f) % 2 else 2]
        for j in range(4):
            p.put(x + j, 1 + j, c)
            p.put(x + j, 8 - j - 1, c)
            p.put(x + j + 1, 1 + j, c)
            p.put(x + j + 1, 8 - j - 1, c)
    return p.outline()


def mark(lit: bool) -> Pic:
    p = Pic(12, 32)
    p.box(5, 9, 6, 30, METAL[1])
    p.box(5, 9, 5, 30, METAL[3])
    p.box(3, 29, 8, 30, METAL[0])
    p.ball(5.5, 5.5, 4.2, 4.2, GOLD if lit else METAL, 0.2, 1.0)
    return p.outline()


def goal(rx: float) -> Pic:
    p = Pic(24, 48)
    for y in range(18, 47):
        for x in range(10, 14):
            p.shade(x, y, WOOD, 0.9 - (x - 10) * 0.25 - (0.3 if y % 7 == 0 else 0))
    p.box(7, 45, 16, 46, WOOD[0])
    p.ball(11.5, 10, max(rx, 0.8), 9, GOLD, 0.1, 1.0)
    if rx > 5:
        p.ball(11.5, 10, rx - 4, 5, SCARF, 0.3, 1.0)
    return p.outline()


def tree() -> Pic:
    p = Pic(48, 72)
    for y in range(30, 71):
        w = 3 + (y - 30) * 0.06 + (2.5 if y > 66 else 0)
        for x in range(int(24 - w), int(24 + w) + 1):
            p.shade(x, y, WOOD, 0.85 - (x - (24 - w)) / (2 * w) * 0.7 + (noise(x, y // 3, 4) - 0.5) * 0.3)
    p.line(24, 44, 31, 37, WOOD[1], 2)
    blobs = [
        (24, 14, 13),
        (12, 22, 10),
        (36, 22, 10),
        (18, 30, 9),
        (31, 31, 9),
        (24, 24, 12),
        (9, 30, 6),
        (39, 31, 6),
        (16, 12, 8),
        (33, 12, 8),
        (24, 7, 7),
    ]
    for i, (x, y, r) in enumerate(blobs):
        p.ball(x, y, r, r * 0.85, LEAF, 0.0 if i < 5 else 0.15, 0.8 if i < 5 else 1.0)
    for i in range(60):
        x, y = 4 + int(noise(i, 1, 9) * 40), 3 + int(noise(i, 2, 9) * 34)
        if p.get(x, y) in LEAF[1:]:
            p.put(x, y, LEAF[min(LEAF.index(p.get(x, y)) + 1, 5)])
            p.put(x, y + 1, LEAF[max(LEAF.index(p.get(x, y)) - 2, 0)])
    return p.outline()


def bush() -> Pic:
    p = Pic(24, 14)
    for x, y, r in ((6, 9, 5), (17, 9, 5.5), (11.5, 7, 6.5)):
        p.ball(x, y, r, r * 0.8, GRASS, 0.0, 0.9)
    for x, y in ((8, 6), (14, 9), (18, 6), (5, 10)):
        p.put(x, y, SCARF[3])
        p.put(x, y + 1, SCARF[1])
    return p.outline()


def flower(f: int) -> Pic:
    p = Pic(12, 16)
    cx = 5.5 + f
    p.line(5.5, 15, cx, 6, GRASS[2])
    p.line(5.5, 12, 8, 10, GRASS[3])
    for i in range(5):
        a = i * 2 * math.pi / 5 - 1.57
        p.ball(cx + math.cos(a) * 2.6, 4.5 + math.sin(a) * 2.6, 1.7, 1.7, PETAL, 0.3, 1.0)
    p.ball(cx, 4.5, 1.5, 1.5, GOLD, 0.4, 1.0)
    return p.outline()


def rock() -> Pic:
    p = Pic(20, 12)
    p.ball(8, 7.5, 7.5, 4.5, STONE)
    p.ball(14, 8.5, 5, 3.4, STONE, 0.0, 0.8)
    p.line(6, 5, 9, 8, STONE[0])
    return p.outline()


def lamp() -> Pic:
    """A lantern plant: a tall bent stalk, leaves, three glowing pods hung from it."""
    p = Pic(20, 44)
    for y in range(8, 44):
        x = 9 + round(2.5 * math.sin((43 - y) * 0.09))
        p.put(x, y, GRASS[2])
        p.put(x + 1, y, GRASS[3])
    for x, y, d in ((8, 34, -1), (12, 27, 1), (9, 20, -1)):
        p.line(x, y, x + d * 5, y - 3, GRASS[3], 2)
        p.put(x + d * 5, y - 4, GRASS[4])
    for x, y, r in ((5, 13, 3.4), (14, 10, 3.8), (10, 4, 3.2)):
        p.line(x, y - r, 10, 9, GRASS[1])
        p.ball(x, y, r, r * 1.25, LEAF, 0.35, 1.0)
        p.put(round(x - 1), round(y - 1), WHITE)
    return p.outline()


def thorn() -> Pic:
    """A bramble: dark canes, pale points."""
    p = Pic(16, 12)
    for x, top in ((2, 5), (5, 2), (8, 4), (11, 1), (13, 6)):
        p.line(x, 11, x, top + 2, SCARF[0], 2)
        p.line(x, top + 2, x, top, SCARF[1])
        p.put(x, top, WHITE)
        p.put(x - 1, top + 4, STONE[3])
    return p.outline()


def pictures() -> list[tuple[int, Pic, bool, int]]:
    """Each picture's index in the game's Img table, whether it is masked, and its mirror's index or -1."""
    out: list[tuple[int, Pic, bool, int]] = []
    itile, iearth, ihero, nf = 10, 30, 34, 23
    for v, p in (
        (1, tile(1, 1)),
        (2, tile(1, 2)),
        (4, tile(2, 3)),
        (5, tile(3, 4)),
        (6, tile(4, 5)),
        (7, tile(5, 6)),
        (8, tile(6, 7)),
        (9, tile(7, 8)),
        (10, tile(1, 9, ledge=True)),
    ):
        out.append((itile + (v - 1) * 2, p, True, -1))
    out += [(iearth + n, earth(n), False, -1) for n in range(4)]
    frames = hero()
    assert len(frames) == nf
    out += [(ihero + f * 2, p, True, ihero + (nf + f) * 2) for f, p in enumerate(frames)]
    k = ihero + nf * 4
    out += [(k + f * 2, bone(f * math.pi / 4), True, -1) for f in range(4)]
    out += [(k + 8 + f * 2, cat(f), True, k + 12 + f * 2) for f in range(2)]
    out += [(k + 16 + f * 2, spring(bool(f)), True, -1) for f in range(2)]
    out += [(k + 20 + f * 2, boost(f), True, -1) for f in range(2)]
    out += [(k + 24 + f * 2, mark(bool(f)), True, -1) for f in range(2)]
    out += [(k + 28 + f * 2, goal(rx), True, -1) for f, rx in enumerate((10, 7, 1.5, 7))]
    out += [
        (k + 36, tree(), True, -1),
        (k + 38, bush(), True, -1),
        (k + 40, flower(0), True, -1),
        (k + 42, flower(1), True, -1),
        (k + 44, rock(), True, -1),
        (k + 46, thorn(), True, -1),
        (k + 48, lamp(), True, -1),
    ]
    out += [(k + 50 + i * 2, t, True, k + 130 + i * 2) for i, t in enumerate(tilts())]
    out += [(k + 370 + f * 2, family(f), True, -1) for f in range(2)]
    out += [
        (7, words("CHARLIE", 170, 26, 2.8, 5), True, -1),
        (k + 374, words("BARKLEY", 170, 26, 2.8, 5), True, -1),
        (k + 376, words("AMBER DOWNS", 178, 18, 1.8, 3.2), True, -1),
    ]
    return out


def encode(vals: list[int]) -> list[int]:
    """The run-length stream: runs of 3 or more alike, else literals."""
    out: list[int] = []
    lit: list[int] = []
    i = 0
    while i < len(vals):
        j = i
        while j < len(vals) and vals[j] == vals[i]:
            j += 1
        if j - i >= 3:
            if lit:
                out += [len(lit), *lit]
                lit = []
            out += [-(j - i), vals[i]]
        else:
            lit += vals[i:j]
        i = j
    if lit:
        out += [len(lit), *lit]
    return [*out, 0]


def block() -> str:
    """The DATA between the markers: the palette, then the pictures."""
    pics = pictures()
    vals = [len(PAL)]
    for i, (r, g, b) in sorted(PAL.items()):
        vals += [i, r + 256 * g + 65536 * b]
    vals.append(len(pics))
    for k, p, masked, k2 in pics:
        vals += [k, p.w, p.h, int(masked), k2, *encode(p.longs())]
        if masked:
            vals += encode(p.longs(mask=True))
    lines, cur = ["' The palette and pictures, as tools/charlieart.py draws them.", "CharlieArt:"], ""
    for v in vals:
        item = str(v)
        if cur and len(cur) + 1 + len(item) > WIDTH:
            lines.append(cur)
            cur = ""
        cur = f"{cur},{item}" if cur else f"DATA {item}"
    return BEGIN + "\n".join([*lines, cur]) + "\n" + END


def sheet(path: Path) -> None:
    """Every picture, four times its size, on grey."""
    from PIL import Image

    pics = [p for _, p, _, _ in pictures()]
    img = Image.new("RGB", (1200, 760), (70, 90, 120))
    x = y = row = 0
    for p in pics:
        if x + p.w * 4 > 1200:
            x, y, row = 0, y + row + 8, 0
        for j in range(p.h):
            for i in range(p.w):
                if p.p[j][i]:
                    r, g, b = PAL[p.p[j][i]]
                    img.paste((r * 4, g * 4, b * 4), (x + i * 4, y + j * 4, x + i * 4 + 4, y + j * 4 + 4))
        x, row = x + p.w * 4 + 8, max(row, p.h * 4)
    img.save(path)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true", help="fail when the DATA is stale")
    ap.add_argument("--sheet", type=Path, help="the pictures to this PNG")
    a = ap.parse_args()
    if a.sheet:
        sheet(a.sheet)
        return
    src = DEMO.read_text()
    pre, rest = src.split(BEGIN, 1)
    new = pre + block() + rest.split(END, 1)[1]
    if a.check:
        sys.exit(0 if new == src else "charlieart: CHARLIE.BAS's CharlieArt is stale; run tools/charlieart.py")
    DEMO.write_text(new)
    print(f"charlieart: {len(pictures())} pictures, {len(block())} characters")


if __name__ == "__main__":
    main()
