#!/usr/bin/env python3
"""charlieart.py [--check] [--sheet PNG]: CHARLIE.BAS's palette and pictures (Charlie, tiles, things, props),
drawn here in flat tones and written to CHARLIE.DAT beside the game, a little-endian LONG a value:
drawn by the game they would cost its start-up a pixel at a time. --check: fail when the file is
stale. --sheet: every picture to a PNG, to look at. The game's tunes, tools/charliesong.txt, go in after them.

A picture in the file: its index, width, height, masked, mirror's index (-1 none), then its GET buffer's
LONGs and, masked, its mask's: a count n then n values; -n then one value n times; 0 the end."""

import re
import sys
import functools
import math
import struct
import argparse
import itertools
from pathlib import Path
from collections.abc import Callable

ROOT = Path(__file__).resolve().parents[1]
DAT = ROOT / "src/plat/wasm/web/demos/CHARLIE.DAT"
SONG = ROOT / "tools/charliesong.txt"
INK, WHITE = 2, 1
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


# A zone's own colours, over the shared ones while it is played: the indices only the other zone's pictures and
# backdrop use (CLOUD between them is shared), and those nothing uses.
SPARE = [5, 6, 7, 148, 149, 172, 173, 174, *range(179, 185), *range(225, 256)]
POOL = {1: [*range(96, 138), *range(141, 148), *range(156, 172), *range(185, 193), *SPARE]}
ZPAL: dict[int, dict[int, tuple[int, int, int]]] = {0: {}, 1: {}, 2: {}, 3: {}}


def pal(z: int) -> dict[int, tuple[int, int, int]]:
    """The colours on screen in zone z."""
    return PAL | ZPAL[z]


class Pic:
    """A w by h picture of palette indices, 0 clear."""

    def __init__(self, w: int, h: int) -> None:
        self.w, self.h = w, h
        self.p = [[0] * w for _ in range(h)]

    def put(self, x: int, y: int, c: int) -> None:
        if 0 <= x < self.w and 0 <= y < self.h:
            self.p[y][x] = c

    def count(self) -> int:
        """How many of its dots are drawn."""
        return sum(1 for row in self.p for c in row if c)

    def get(self, x: int, y: int) -> int:
        return self.p[y][x] if 0 <= x < self.w and 0 <= y < self.h else 0

    def shade(self, x: int, y: int, r: list[int], v: float) -> None:
        """r's entry nearest v in 0..1: flat tones, none dithered."""
        self.put(x, y, r[int(max(0.0, min(1.0, v)) * (len(r) - 1) + 0.5)])

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
CLOUD = ramp(138, (48, 44, 52), (63, 62, 60), 3)


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


ARC = ((0, 0, 2.2), (-2, -3, 2.6), (-1.5, -6.5, 2.8), (1.5, -8.5, 2.6), (4.5, -7.5, 2.0))  # the tail carried


def plume(p: Pic, x: float, y: float, wag: float, stream: bool) -> None:
    """The tail, a plume carried over the back; stream: flat out behind."""
    flat = ((0, 0, 2.2), (-2.5, -1, 2.5), (-5, -1.5, 2.4), (-7, -1, 2.0), (-8.5, 0, 1.4))
    for i, (dx, dy, r) in enumerate(flat if stream else ARC):
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


def turned(run: Pic, turn: int, f: int) -> Pic:
    """Charlie running a ribbon's twist, turn twelfths of the way round it, stride f of run's two: the balls dog()
    draws his side with, set in depth and rolled about the line under his paws, the far ones drawn first, so his
    back, then his belly, comes round as his side goes. The line is on row 16 + 12 cos; upright he is run as drawn."""
    p = Pic(32, 32)
    c, s = math.cos(turn * math.pi / 6), math.sin(turn * math.pi / 6)
    if turn == 0:
        p.p[:28] = [list(row) for row in run.p[3:31]]
        return p
    ph = 2 * math.pi * (1 + f * 3) / 6
    st, swing = abs(math.sin(ph)), math.sin(ph)
    by, hip, hx, hy = 18.5, 22.5, 23.5 + st * 0.5, 11.3 + st * 0.6
    ear = 0.7 + 0.4 * math.cos(ph)
    line = 16 + round(12 * c) - 0.5
    balls = []

    def row(y: float, z: float) -> float:
        return line - (30.5 - y) * c + z * s

    def add(
        x: float,
        y: float,
        z: float,
        rx: float,
        rh: float,
        rz: float,
        lo: float,
        hi: float,
        r: list[int] = FUR,
        fore: float = 0,
    ) -> None:
        """A ball at x along him, on row y side on, z to our side of him; its radii those three ways; fore: how
        much nearer it counts, a part that lies on another."""
        balls.append(
            ((30.5 - y) * s + z * c + fore, x, row(y, z), rx, max(math.hypot(rh * c, rz * s), 1.0), r, lo, hi, fore)
        )

    add(12.5, by, 0, 8.5 + st, 6 - st * 0.5, 4.8, 0.3, 1.15)
    add(18.5 + st * 0.5, by - 0.5, 0, 4.6, 5.4 - st * 0.4, 4.4, 0.35, 1.2)
    add(12.5, by + 3, 0, 8, 3.6, 4.2, 0.25, 1.0)
    add(hx, hy, 0, 6.2, 5.8, 5.6, 0.4, 1.2, fore=1)
    add(hx + 3.6, hy + 4.2, 0, 3.6, 2.6, 2.8, 0.3, 1.0, fore=1)
    add(hx + 4.8, hy + 2.2, 0, 3.4, 2.5, 2.6, 0.5, 1.25, fore=1)
    for i, (dx, dy, r) in enumerate(ARC):
        add(5.5 - st + dx + swing * i * 0.3, by - 3 + dy, swing * i * 0.5, r, r, r * 0.8, 0.3, 1.1, fore=1)
    for side in (-1, 1):
        for d, r in ((1.0, 2.6), (3.2, 2.7), (5.4, 2.1)):
            add(hx - 3.6 - math.sin(ear) * d, hy - 1.5 + math.cos(ear) * d, side * 6.4, r, r, 2.2, 0.2, 1.0, FUR[:4], 2)
        for x0, way in ((8, -swing), (18, swing)):  # a leg under each corner, the far pair's reach the shorter
            a = way if side > 0 else way * 0.6 + (0.15 if x0 == 8 else -0.15)
            fx, fy = x0 + math.sin(a) * 5.5, min(hip + math.cos(a) * 5.5, 28.5)
            lo = 0.0 if side * c + 0.4 * s < 0 else 0.4  # the pair in his own shade
            add((x0 + fx) / 2, (hip + fy) / 2, side * 5.0, 1.7, 2.8, 1.7, lo, lo + 0.5)
            add(fx + 0.8, fy + 0.6, side * 5.0, 2.5, 1.7, 2.4, lo, lo + 0.7)
    if s < -0.2:  # from under, his belly's skin
        add(13, by + 5.8, 0, 5.5, 1.2, 3.0, 0.5, 1.0, SKIN, 1)
    for _, x, y, rx, ry, r, lo, hi, fore in sorted(balls, key=lambda b: b[0]):
        if fore:  # its shade on what it lies on
            p.ball(x + 1, y + 1, rx, ry, [FUR[2], FUR[2]], over=True)
        p.ball(x, y, rx, ry, r, lo, hi)
    ny = round(row(hy + 1.5, 0) - 0.5)
    p.box(round(hx + 6), ny, round(hx + 8), ny + 1, INK)
    p.put(round(hx + 6), ny, FUR[5])
    if abs(c) > 0.8:
        p.line(hx + 4.5, row(hy + 4.4, 0), hx + 6.5, row(hy + 4.4, 0), INK)
    for side in (-1, 1):  # an eye on the side of his head that is to us
        if 0.3 * s + 0.95 * side * c > 0.4:
            ex, ey = round(hx + 1), round(row(hy - 0.5, side * 4.6))
            p.box(ex, ey, ex + 1, ey + 1, INK)
            p.put(ex, ey - 1, WHITE)
            p.put(ex + 1, ey - 1, INK)
    return p.outline()


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


def startled() -> Pic:
    """The cat barked at: its back arched, its legs stiff, its tail a brush, an eye wide."""
    p = Pic(20, 14)
    fur = EARTH[2:]
    p.line(3, 7, 2, 3, fur[2], 2)
    p.ball(2.5, 2.5, 2.2, 2.2, fur, 0.4, 1.1)
    for x in (4, 6, 11, 13):
        p.line(x, 9, x, 12, fur[1 + x % 2])
    p.ball(8, 6.5, 5.2, 3.2, fur, 0.2, 1.0)
    for x in (4, 6, 8, 10):  # its fur on end
        p.put(x, 2, fur[3])
    for x in (6, 9):
        p.line(x, 4, x - 1, 6, fur[0])
    for x, y in ((13, 1), (13, 2), (14, 2), (17, 1), (16, 2), (17, 2)):
        p.put(x, y, fur[3])
    p.ball(15, 5.5, 3.2, 3.0, fur, 0.3, 1.1)
    p.box(16, 4, 17, 5, WHITE)
    p.put(17, 5, INK)
    p.put(18, 6, PETAL[0])
    p.box(16, 7, 17, 8, INK)
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


# ---- Amber Downs: every tone placed by rule, none dithered; its colours its own ----


def tones(first: int, *hexes: str, zone: int = -1) -> list[int]:
    """Palette entries from first, a colour each; their indices. zone: only that zone's, over the shared ones."""
    for i, h in enumerate(hexes):
        rgb = (int(h[1:3], 16) * 63 // 255, int(h[3:5], 16) * 63 // 255, int(h[5:7], 16) * 63 // 255)
        (PAL if zone < 0 else ZPAL[zone])[first + i] = rgb
    return list(range(first, first + len(hexes)))


AIR = tones(96, "#2B4FB8", "#3565CC", "#437EDC", "#5A9AE8", "#7BB6EE", "#A3D0F0", "#CDE5EC", "#F2EBD0", "#FFDFA8")
FARM = tones(105, "#F8FAFF", "#D4DCF8", "#B8C2F0", "#A2ACE6", "#8E98DA")  # snow, rim, lit, mid, shade
NEARM = tones(110, "#B0B8EC", "#9AA4E2", "#848ED4", "#6E78C4", "#5C66B4")
SHORE = tones(115, "#8CC0CC", "#6CA4BC", "#5488AC")
HILL = tones(118, "#2E7C7C", "#3A9484", "#4CAC88", "#68C48C", "#8CD898", "#B8ECA8", "#1E5C68")
PUFF = [WHITE, *tones(125, "#E2EEFA", "#B8D0F0")]
LAKE = tones(128, "#4C90DE", "#C4E8FA")  # its near band, its far edge
AMBER = tones(130, "#FFF0A8", "#FFD060", "#F8A830", "#E07C20", "#B85420", "#883424", "#5C2030")
PLUM = tones(137, "#2C1430")[0]
DAWN = tones(141, "#FFFFF0", "#FFF4C0", "#FFE8A0")
WATER = tones(144, "#3A74CC", "#78B8EE", "#C4E8FA", "#FFFFFF")  # the game turns the last three
TURF = tones(225, "#F0FF90", "#B4F048", "#6CD028", "#38A828", "#1C7C34", "#105040")
CROWN = tones(231, "#FFF0A0", "#FFCC48", "#F89820", "#D86420", "#A03C28", "#642030")
BARK = tones(237, "#D89860", "#A86840", "#784030", "#4C2430")
SHRUB = tones(241, "#C8F870", "#78D838", "#38A830", "#1C7838", "#104838")
BLOOM = tones(246, "#FFD0E4", "#E86CA8", "#FFF088", "#F8B020")
BOULDER = tones(250, "#F0ECF8", "#BCB4D4", "#8880AC", "#5C5484", "#34284C")
HORIZON, SUNX, SUNY = 122, 232, 78


def zig(v: float, period: float) -> float:
    """A triangle wave, 0 to 1."""
    t = (v / period) % 1
    return 2 * t if t < 0.5 else 2 - 2 * t


def paste(p: Pic, o: Pic, x0: int, y0: int) -> None:
    for y in range(o.h):
        for x in range(o.w):
            if o.p[y][x]:
                p.put(x0 + x, y0 + y, o.p[y][x])


PUFFS = {
    "big": [(0, 0, 9), (-11, 3, 7), (11, 2, 8), (-20, 6, 5), (21, 6, 5), (4, -5, 6), (-5, -3, 6)],
    "mid": [(0, 0, 6), (-8, 2, 5), (8, 2, 5), (15, 4, 3), (-14, 4, 3)],
    "small": [(0, 0, 4), (-6, 1, 3), (6, 1, 3)],
}


def cloud(p: Pic, cx: int, cy: int, kind: str) -> None:
    """A cloud of puffs on a flat base: white where the sun reaches, a tone where the cloud itself is in the way."""
    puffs = PUFFS[kind]
    base = max(py + r // 2 for _, py, r in puffs)

    def inside(x: float, y: float) -> bool:
        return y <= base and any((x - px) ** 2 + ((y - py) * 1.25) ** 2 <= r * r for px, py, r in puffs)

    for y in range(-20, 14):
        for x in range(-30, 31):
            if not inside(x, y):
                continue
            t = 0
            if inside(x - 2, y - 3) and inside(x - 3, y - 2):
                t = 1
            deep = inside(x - 5, y - 6) and inside(x - 7, y - 5) and y >= base - 4
            if deep or (y == base and inside(x - 3, y - 4)):
                t = 2
            for dx in (-320, 0, 320):
                p.put(cx + x + dx, cy + y, PUFF[t])


# a range: its skyline's corners, then its peaks, each lit on its left
FAR_LINE = [(0, 100), (22, 84), (30, 88), (52, 66), (64, 78), (72, 74), (98, 98), (118, 80), (140, 58), (152, 70)]
FAR_LINE += [(160, 66), (186, 94), (204, 86), (226, 104), (252, 72), (262, 80), (270, 76), (296, 98), (310, 90)]
FAR_LINE += [(320, 100)]
FAR_PEAKS = [(22, 84), (52, 66), (72, 74), (118, 80), (140, 58), (160, 66), (204, 86), (252, 72), (270, 76), (310, 90)]
NEAR_LINE = [(0, 104), (14, 96), (34, 108), (60, 100), (84, 112), (108, 104), (126, 110), (158, 98), (176, 106)]
NEAR_LINE += [(196, 112), (222, 108), (238, 114), (274, 100), (292, 108), (320, 104)]
NEAR_PEAKS = [(14, 96), (60, 100), (108, 104), (158, 98), (222, 108), (274, 100)]


def peaks(p: Pic, line: list[tuple[int, int]], tops: list[tuple[int, int]], r: list[int], snowline: int) -> None:
    """A mountain range: each peak's ridge zigzags down from it, sunlit on its left and in shade on its right."""
    for x in range(320):
        (xa, ya), (xb, yb) = next((a, b) for a, b in itertools.pairwise(line) if a[0] <= x <= b[0])
        top = round(ya + (yb - ya) * (x - xa) / (xb - xa))
        px, py = min(tops, key=lambda t: abs(t[0] - x) + (t[1] - top) * 0.6)
        for y in range(top, HORIZON):
            v = y - py
            ridge = px + v * 0.3 + (zig(v, 14) - 0.5) * (3 + v * 0.14)
            lit = x <= ridge
            if y < snowline + zig(x + (0 if lit else 3), 8) * 5 - (0 if lit else 3) and y - top < 14:
                t = 0 if lit else 1
            elif lit:
                t = 1 if y == top else 2
            else:
                t = 3 if x - ridge < 3 + v * 0.25 + zig(v + 7, 18) * 3 else 4
            p.put(x, y, r[t])


def downs() -> Pic:
    """The far backdrop, of period 320: a banded dawn sky, the sun, clouds, two ranges, the far shore, the lake."""
    p = Pic(320, 200)
    edges = [0, 14, 28, 42, 56, 70, 84, 98, 110, HORIZON]
    for y in range(HORIZON):
        k = max(i for i in range(9) if edges[i] <= y)
        if k < 8 and y == edges[k + 1] - 2:  # a line of the next band before it starts
            k += 1
        p.box(0, y, 319, y, AIR[k])
    for y in range(SUNY - 20, SUNY + 21):
        for x in range(SUNX - 20, SUNX + 21):
            d = math.hypot(x - SUNX + 0.5, y - SUNY + 0.5)
            if d < 14:
                p.put(x, y, DAWN[0] if d < 12 else DAWN[1])
            elif d < 18:
                p.put(x, y, AIR[min(AIR.index(p.get(x, y)) + 2, 8)])  # its glow: the sky two bands lighter
    for x, y, kind in ((40, 22, "big"), (128, 44, "mid"), (196, 18, "mid"), (286, 40, "big"), (92, 70, "small")):
        cloud(p, x, y, kind)
    cloud(p, 160, 86, "small")
    peaks(p, FAR_LINE, FAR_PEAKS, FARM, 76)
    peaks(p, NEAR_LINE, NEAR_PEAKS, NEARM, 0)
    for x in range(320):
        t = 2 * math.pi * x / 320
        top = round(117.5 - 2.2 * math.sin(3 * t + 1) - 1.4 * math.sin(8 * t) + (0.8 if x % 5 < 2 else 0))
        for y in range(top, HORIZON):
            p.put(x, y, SHORE[0] if y == top else SHORE[1] if y < HORIZON - 1 else SHORE[2])
    for y in range(HORIZON, 142):
        d = y - HORIZON
        p.box(0, y, 319, y, LAKE[1] if d == 0 else LAKE[0] if d < 5 or d == 6 else WATER[0])
        for x in range(320):
            # the sun's path down the lake, then stray glints
            if d and abs(x - SUNX) <= 3 + d * 0.9 and d % 2 == 1 and (x + d * 3) % 7 < 5:
                p.put(x, y, WATER[3] if abs(x - SUNX) < (3 + d * 0.9) * 0.45 else WATER[2])
            elif d % 3 == 2 and (x * 5 + d * 37) % 61 < 6 + d // 4:
                p.put(x, y, WATER[1])
    for y in range(142, 200):  # under the hills, their ploughed bands run on
        p.box(0, y, 319, y, HILL[1 + int(math.sqrt((y - 142) * 2.2 + 2)) % 2])
    return p


HILLS_BACK = [(30, 14, 70, 30), (120, 6, 80, 34), (215, 16, 75, 28), (292, 10, 60, 30)]  # middle, top, half width, drop
HILLS_FRONT = [(-10, 36, 80, 30), (75, 42, 70, 22), (160, 30, 85, 34), (255, 40, 75, 26)]


def copse(p: Pic, x0: int, y0: int, n: int) -> None:
    """A row of n round trees on a hill, and their shadows."""
    for i in range(n):
        cx, cy, r = x0 + i * 7, y0 + (i * 5) % 3 - (i % 2), 3 + (i % 2)
        for x in range(-r + 2, r + 5):
            p.put(cx + x, cy + r, HILL[6])
            p.put(cx + x + 1, cy + r + 1, HILL[6])
        for y in range(-r, r + 1):
            for x in range(-r, r + 1):
                if x * x + y * y <= r * r + 1:
                    rim = x * x + y * y > (r - 1) ** 2 + 1 and x + y > 0
                    p.put(cx + x, cy + y, HILL[6] if rim else HILL[1 if x + y > 1 else 3 if x + y > -r else 5])


def hills() -> Pic:
    """The near hills, a masked band of period 320: two rows of downs, ploughed in bands along their crests."""
    p = Pic(320, 80)
    for row, r in ((HILLS_BACK, (2, 3, 4, 5)), (HILLS_FRONT, (0, 1, 2, 4))):
        for n, (cx0, top, hw, drop) in enumerate(row):
            for cx in (cx0 - 320, cx0, cx0 + 320):
                for x in range(320):
                    u = (x - cx) / hw
                    if abs(u) >= 1.3:
                        continue
                    y0 = top + drop * u * u
                    for y in range(max(0, round(y0)), 80):
                        d = y - y0
                        band = int(math.sqrt(d * 2.2 + 2) + n * 0.5) % 2
                        dark = u > 0.2 + d * 0.006  # the side away from the sun
                        t = r[1 if dark else 3] if d < 2 else r[1 - band] if dark else r[2 - band]
                        p.put(x, y, HILL[t])
    copse(p, 100, 16, 5)
    copse(p, 196, 34, 4)
    copse(p, 276, 22, 3)
    return p


# the ground: a honeycomb of amber, 16 across and 12 down a cell, under turf with a scalloped hem
VARIETY = ("0010", "2000", "0001", "0120")  # by a cell's row and column: 0 comb, 1 full of honey, 2 empty
FRINGE = (8, 9, 10, 10, 9, 7, 6, 7, 8, 8, 7, 6, 8, 9, 9, 8, 7, 6, 6, 7, 9, 10, 10, 10, 9, 8, 6, 6, 7, 8, 8, 7)
SPARK, BLADE = (2, 3, 9, 14, 19, 20, 27), (1, 6, 12, 17, 22, 29)


@functools.cache
def cell(x: int, y: int) -> tuple[int, int, int, int]:
    """The comb's cell at a pixel: its column and row, and the pixel's place from its middle."""
    best = (99, 0, 0, 0, 0)
    for j in range(y // 12 - 1, y // 12 + 3):
        off = 8 if j & 1 else 0
        for i in range((x - off + 8) // 16 - 1, (x - off + 8) // 16 + 2):
            dx, dy = x - (i * 16 + off), y - j * 12
            best = min(best, (max(2 * abs(dx), 2 * abs(dy) + abs(dx)), j, i, dx, dy))
    return best[2], best[1], best[3], best[4]


@functools.cache
def comb(x: int, y: int) -> int:
    """The comb's amber tone at a world pixel, 0 the lightest: walls, each cell's lit and shaded edges, its fill."""
    i, j, dx, dy = cell(x, y)
    if cell(x + 1, y)[:2] != (i, j) or cell(x, y + 1)[:2] != (i, j):
        return 5
    kind = int(VARIETY[j % 4][i % 4])
    edge = cell(x - 1, y)[:2] != (i, j) or cell(x, y - 1)[:2] != (i, j) or max(2 * abs(dx), 2 * abs(dy) + abs(dx)) >= 13
    upper = dy < 0 if 2 * abs(dy) + abs(dx) > 2 * abs(dx) else dx < 0
    if kind == 2:  # a hole: its upper left in the wall's shadow, the lower right wall lit
        return (6 if upper else 3) if edge else 5 if dx + dy * 1.5 < 2 else 4
    base = 2 - kind
    if edge:
        if upper and dx < 0:
            return base - 1
        if upper or (dx < 0 and dy < 3):
            return base - 1 + kind
        return base + 1 if dx < 0 or dy < 3 else base + 2
    if kind == 1 and (dx, dy) in ((-3, -3), (-2, -3), (-3, -2)):
        return 0
    return base + (dx + dy * 1.5 > 5)


def turf(x: int, d: int) -> int:
    """The turf's colour d pixels under the surface at world column x; 0 under its hem."""
    f = FRINGE[x % 32]
    if d > f:
        return TURF[5] if d == f + 1 else 0
    if d == 0:
        return TURF[0] if x % 32 in SPARK else TURF[1]
    t = 1 if d <= 2 else 2 if d <= 5 else 3 if d <= f - 2 else 4
    if x % 32 in BLADE and 2 <= d <= 4:
        t = 1 if d < 4 else 2
    if (x + 3) % 32 in BLADE and 5 <= d <= 7 and d <= f - 2:
        t = 2
    return TURF[t - (FRINGE[(x - 1) % 32] < f and d > 5)]  # a scallop's sunlit side


def soil(x: int, y: int, d: int, hem: int) -> int:
    """The ground's colour at a world pixel d under the surface, the turf's hem at column hem: turf, the comb in
    its shadow, the comb."""
    return turf(hem, d) or AMBER[min(comb(x, y) + max(0, 2 - (d - FRINGE[hem % 32] - 2) // 2), 6)]


def top(kind: int, col: int, row: int, under: int = 0) -> Pic:
    """A surface tile of the comb's col-th column of 4 and row-th of 3; under: the tile below a slope's."""
    p = Pic(16, 16)
    for x in range(16):
        s = surface(kind, x) - under * 16
        for y in range(max(s, 0), 16):
            p.put(x, y, soil(col * 16 + x, row * 16 + under * 16 + y, y - s, col * 16 + x))
    return p


def deep(col: int, row: int, h: int) -> Pic:
    """The comb far under the turf."""
    p = Pic(16, h)
    for x in range(16):
        for y in range(h):
            p.put(x, y, AMBER[comb(col * 16 + x, row * 16 + y)])
    return p


def ledge() -> Pic:
    """A floating shelf's 16: turf over a slab of amber."""
    p = Pic(16, 16)
    for x in range(16):
        f = (4, 5, 5, 4, 3, 3, 4, 5, 5, 4, 3, 4, 5, 5, 4, 4)[x]
        for y in range(15):
            if y <= f:
                t = TURF[0 if y == 0 and x in SPARK else 1 if y < 2 else 2 if y < f - 1 else 3]
            elif y == f + 1:
                t = TURF[5]
            elif y == 14:
                t = PLUM
            else:
                lit = x == 1 or y == f + 4
                t = AMBER[5 if x == 0 or y == 13 else 4 if y <= f + 3 else 1 if lit else 3 if x == 15 else 2]
            p.put(x, y, t)
    return p


def bare(right: bool, body: bool) -> Pic:
    """A tile of the ground's bare side: the turf's end, then the comb's, lit on the left and dark on the right."""
    p = Pic(16, 16)
    x, into = (15, -1) if right else (0, 1)
    for y in range(16):
        p.put(x, y, PLUM if body or y > 9 else TURF[5])
        if body or y > 12:
            p.put(x + into, y, AMBER[5 if right else 1])
    return p


def ring(size: int, r0: float, r1: float, cap: Callable[[int, int], int], body: list[int], ink: int) -> Pic:
    """A loop: its surface, cap, round its inside r0, then 40 blocks of body's tones, light to dark, out to r1."""
    p = Pic(size, size)
    m = size / 2
    flat: dict[tuple[int, int], int] = {}
    walls: set[tuple[int, int]] = set()
    for y in range(size):
        for x in range(size):
            r = math.hypot(x + 0.5 - m, y + 0.5 - m)
            a = math.atan2(y + 0.5 - m, x + 0.5 - m) % (2 * math.pi)
            if not r0 - 0.5 <= r < r1 + 0.5:
                continue
            hem = int(a / (2 * math.pi) * 448)  # the cap's 32 or 64 goes round 14 or 7 times
            d = int(r - r0 + 0.5)
            under = d - next(i for i in range(40) if not cap(hem, i))
            if cap(hem, d):
                p.put(x, y, cap(hem, d))
            elif r >= r1 - 0.5:
                p.put(x, y, ink)
            elif a % (math.pi / 20) * r < 1 or r >= r1 - 1.5:
                walls.add((x, y))
                p.put(x, y, body[5])
            else:
                flat[x, y] = (1 if int(a / (math.pi / 20)) % 5 == 2 else 2) + (2 if under < 2 else under < 4)
    for (x, y), tone in flat.items():  # each block's edge by a wall above or left of it lit, below or right shaded
        lit = (x - 1, y) in walls or (x, y - 1) in walls
        p.put(x, y, body[tone - 1 if lit else tone + ((x + 1, y) in walls or (x, y + 1) in walls)])
    return p


def pipe(col: int, row: int) -> Pic:
    """A quarter pipe and the row of ground under it, the comb's col-th column and row-th row at its top left: the
    turf follows the curve, from the floor's on its left up to the wall's top."""
    p = Pic(48, 64)
    for y in range(64):
        for x in range(48):
            r = math.hypot(x + 0.5, y + 0.5)
            if y >= 48 or r >= 48.5:
                hem = col * 16 + int(math.atan2(x + 0.5, y + 0.5) * 48.5)
                p.put(x, y, soil(col * 16 + x, row * 16 + y, int(r - 48.5), hem))
    return p


def ribbon(half: int, r: list[int], ink: int) -> Pic:
    """Half a ribbon's twist, 192 along: a plank road 40 wide and 10 thick wound once round a line in a turn of 384,
    seen from the side. Its running face is to the line; the path is that face's middle. On edge at its foot and at
    its crest, flat to us between: the rising half its lit face, the falling its dark back, nearer than what runs
    it. r: the tones, light to dark."""
    p = Pic(192, 132)
    for i in range(192):
        x = half * 192 + i
        a = math.pi * x / 192
        s, c = math.sin(a), math.cos(a)
        y = 22 + 96 - 96 * (1 - c) / 2
        # the road's section here: its face from one edge to the other, then its back, 10 from the line's side
        ya, yb = y + s * 20, y - s * 20
        faces = [(ya, yb, 0 if s > 0 else -1), (ya + c * 10, yb + c * 10, -1 if s > 0 else 1)]
        faces += [(ya, ya + c * 10, 2 if c > 0 else -1), (yb, yb + c * 10, 2 if c < 0 else -1)]
        rows = {}
        for y0, y1, kind in faces:
            if kind >= 0:
                for yy in range(round(min(y0, y1)), round(max(y0, y1)) + 1):
                    rows[yy] = (kind, (yy - min(y0, y1)) / max(abs(y1 - y0), 1))
        for yy, (kind, across) in rows.items():
            seam = x % 16 == 0
            if yy in (min(rows), max(rows)):
                tone = ink
            elif kind == 0:  # the face run on: planks, a line down its middle
                tone = r[4] if seam else r[0] if abs(across - 0.5) < 0.06 else r[1 + (x // 16) % 2]
            elif kind == 1:
                tone = r[5] if seam else r[3 + (x // 16) % 2]
            else:
                tone = r[4] if seam else r[2 if yy == min(rows) + 1 else 3]
            p.put(i, yy, tone)
    return p


def raft() -> Pic:
    """Amber Downs' moving platform: a slab of comb in a frame."""
    p = Pic(48, 12)
    for x in range(1, 47):
        for y in range(1, 11):
            if y < 3:
                t = AMBER[y - 1]
            elif y == 10 or x in (1, 46):
                t = AMBER[5]
            elif (x + (4 if (y - 3) // 4 % 2 else 0)) % 8 == 0 or y == 6:
                t = AMBER[4]
            else:
                t = AMBER[2 + (y > 7)]
            p.put(x, y, t)
    return p.outline(PLUM)


# ---- its props ----


def clumps(p: Pic, parts: list[tuple[int, int, int, int]], r: list[int], flat: int = 999) -> None:
    """Leaf clumps (x, y, radius, tone), back to front: each a lit crescent, a body and a shaded hem; the ramp's
    last tone round the lot."""
    for px, py, rad, lv in parts:
        for y in range(py - rad, min(py + rad, flat) + 1):
            for x in range(px - rad, px + rad + 1):
                if (x - px) ** 2 + (y - py) ** 2 > rad * rad + rad * 0.4:
                    continue
                t = lv + 1
                if (x - px - 2) ** 2 + (y - py - 3) ** 2 > rad * rad:
                    t = lv
                elif (x - px + 2) ** 2 + (y - py + 3) ** 2 > rad * rad:
                    t = lv + 2
                p.put(x, y, r[min(t, len(r) - 2)])
    p.outline(r[-1])


def tree() -> Pic:
    p = Pic(64, 80)
    for y in range(40, 80):  # the trunk, flaring to its roots
        half = 4 + (y > 70) + 2 * (y > 75) + (y > 77)
        for x in range(-half, half + 1):
            t = 0 if x <= -half + 1 else 1 if x < 1 else 2
            if -half + 1 < x < half and (x + y // 5) % 4 == 0 and y % 5:
                t += 1  # the bark's furrows
            p.put(32 + x, y, BARK[min(t + 1, 2) if y < 47 else t])  # darker in the crown's shadow
    back = [(44, 34, 10, 3), (32, 38, 9, 3), (20, 35, 9, 2), (50, 24, 9, 2), (38, 28, 11, 2), (14, 26, 9, 1)]
    clumps(p, [*back, (26, 26, 11, 1), (42, 15, 10, 1), (30, 13, 11, 0), (19, 16, 9, 0), (34, 22, 8, 1)], CROWN)
    return p


def bush() -> Pic:
    p = Pic(40, 20)
    clumps(p, [(30, 13, 6, 2), (8, 14, 5, 1), (21, 11, 8, 1), (13, 10, 6, 0)], SHRUB, flat=18)
    return p


FLOWER = ("..pp..pp..", ".pPPppPPp.", ".pPPyyPPp.", "..pyYYyp..", ".pPPyyPPp.", ".pPPppPPp.", "..pp.gpp..")
FLOWER += (".....g....", "..G..g.G..", "..gG.gGg..", "...gGgg...", "....gg....")


def flower(f: int) -> Pic:
    """A flower, its head a pixel aside on its second frame."""
    p = Pic(12, 16)
    ink = {"P": BLOOM[0], "p": BLOOM[1], "Y": BLOOM[2], "y": BLOOM[3], "g": TURF[4], "G": TURF[2]}
    for y, row in enumerate(FLOWER):
        for x, ch in enumerate(row):
            if ch != ".":
                p.put(1 + x + (f if y < 7 else 0), 3 + y, ink[ch])
    return p


def rock() -> Pic:
    p = Pic(20, 12)
    clumps(p, [(13, 8, 5, 2), (7, 7, 6, 0)], BOULDER, flat=10)
    return p


# ---- the Chrome Works: machine decks under a dusk sky, a city behind; its colours its own ----

CYAN, PINK = 177, 178
PAL[CYAN], PAL[PINK] = (10, 60, 58), (63, 14, 44)
COPPER = tones(193, "#4C1C24", "#8C3828", "#C86430", "#F09848", "#FFD088")
CHROME = tones(198, "#1C1C38", "#404868", "#7480A0", "#A8B8D0", "#D8E4F0", "#FFFFFF")
PANEL = tones(204, "#141A44", "#1E2C64", "#2C4488", "#3C60AC", "#5884CC", "#84B0EC")
GLOWC = ramp(210, (4, 34, 46), (56, 63, 63), 3)
# colours the game turns as it runs: a deck's four lights, a fan's blades, the glints along a pipe
CHASE, FAN, FLOW = [213, 214, 215, 216], [217, 218, 219, 220], [221, 222, 223, 224]
PAL.update(dict.fromkeys(CHASE, (4, 34, 46)) | dict.fromkeys(FAN, (5, 4, 8)) | dict.fromkeys(FLOW, (50, 31, 15)))
DUSK = tones(96, "#241858", "#3C2070", "#5C2C88", "#84389C", "#B048A4", "#D8609C", "#F08088", "#FFA878", zone=1)
DUSK += tones(104, "#FFD488", zone=1)
STAR = tones(105, "#FFF0FF", "#B894D8", zone=1)
WISP = tones(107, "#FFC8A0", "#E888A8", "#A85CA8", zone=1)  # a cloud's lit underside, its body, its top
TOWN = tones(110, "#B060A0", "#94488C", "#D888B0", zone=1)  # the far city: a face, its shade, its lit edge
CITY = tones(113, "#4C2C78", "#382060", "#6C4498", "#241448", zone=1)  # the near: and the dark under it
PANE = tones(117, "#FFE890", "#FFB060", "#7C54A8", zone=1)  # a window lit, warm, dark
WORKS = tones(120, "#0C1438", "#142454", "#1E3C74", "#2C5C98", "#58A4CC", "#A8E4F0", zone=1)
LAMP = tones(126, "#FFE478", "#F04858", "#F0E8F8", zone=1)
DECK = tones(129, "#F4FCFF", "#C4D8EC", "#8CA4CC", "#5C7098", "#343C68", "#1C1838", zone=1)
HAZARD = tones(135, "#FFE478", "#FFC428", "#C88818", zone=1)
SUNSET = tones(141, "#FFFCD8", "#FFE890", zone=1)

# each zone again at another hour: the Amber Downs at evening, the works by night, their pictures recoloured
tones(96, "#3A1C6C", "#5C2880", "#883490", "#B84890", "#E06080", "#F88468", "#FFAC60", "#FFD070", "#FFF0A0", zone=2)
tones(105, "#FFE8E0", "#E8B8D8", "#C898CC", "#A87CBC", "#8C64A8", "#A074B8", "#8860A8", "#704C98", "#5C3C84", zone=2)
tones(114, "#4C3074", "#7C6CA8", "#644C98", "#503C84", zone=2)
tones(118, "#2C4C74", "#38648C", "#4C7C98", "#6898A0", "#90B4A4", "#C0D0A8", "#203860", "#FFD0C0", "#E890B0", zone=2)
tones(128, "#C8589C", "#FFD0B0", "#FFE8A0", "#FFC050", "#F09028", "#D06420", "#A84024", "#782830", "#501C34", zone=2)
tones(141, "#FFF4B0", "#FFD870", "#FFB850", "#7C3C98", "#F08090", "#FFC090", "#FFF0C0", zone=2)
tones(225, "#E8F880", "#A0DC40", "#58B830", "#2C9034", "#186840", "#104444", zone=2)
ZPAL[3].update(ZPAL[1])
tones(96, "#0C1030", "#101840", "#182454", "#20306C", "#284084", "#30549C", "#3C6CB0", "#5088C4", "#70A8D8", zone=3)
tones(107, "#8098D0", "#5870B0", "#405090", "#3C4C8C", "#2C3870", "#5870B0", zone=3)
tones(113, "#1C2458", "#141A44", "#2C3C78", "#0C1030", zone=3)
tones(141, "#F4F8FF", "#C8D8F0", zone=3)


def plate(u: int, d: int) -> int:
    """The deck's colour d pixels under its surface, u along it; 0 below: plating and its rivets, a hazard
    stripe, the four lights that run, a lip."""
    if d < 3:
        return DECK[0] if d == 0 else DECK[2] if d == 2 and u % 8 == 3 else DECK[1]
    if d < 5:
        return DECK[d - 1]
    if d < 10:
        return DECK[5] if (u + d) // 4 % 2 else HAZARD[0 if d == 5 else 2 if d == 9 else 1]
    if d in (11, 12) and 3 <= u % 16 <= 12:
        return CHASE[u // 16 % 4]
    return (DECK[5], DECK[4], DECK[4], DECK[5], DECK[4], PANEL[0], 0)[min(d, 16) - 10]


def span(kind: int, col: int, under: int = 0) -> Pic:
    """A deck tile, the deck's col-th 16 of 64; under: what of a slope's deck runs on into the tile below."""
    p = Pic(16, 16)
    for x in range(16):
        s = surface(kind, x) - under * 16
        for y in range(max(s, 0), 16):
            p.put(x, y, plate(col * 16 + x, y - s))
    return p


def shelf() -> Pic:
    """A floating shelf's 16 of deck."""
    p = span(1, 0)
    p.p[15] = [0] * 16
    return p


def wall(p: Pic, x: int, y: int) -> None:
    """The works' wall, 64 by 128 and meeting itself: panels 32 square, a vent, a fat pipe, a fan, a light, a
    girder, ribs."""
    u, v, n = x % 32, y % 32, y // 32 * 2 + x // 32
    val = 0.52 - v * 0.007
    if n in (2, 3):
        if 6 <= v <= 21:
            g = 1.0 - abs(v - 12) / 10 + (0.3 if v in (9, 10) else 0)
            if x >= 5 and v == 9:
                p.put(x, y, FLOW[x // 4 % 4])
            else:
                p.shade(x, y, CHROME if x < 5 else COPPER, g)
            return
        if x < 5 and v in (4, 5, 22, 23):
            p.put(x, y, CHROME[1 if v > 20 else 4])
            return
        if 22 <= v <= 27:
            val -= 0.3 - (v - 22) * 0.05
    if u == 0 or v == 0:
        val = 0.0
    elif u == 1 or v == 1:
        val += 0.25
    elif u == 31 or v == 31:
        val -= 0.18
    elif u in (4, 27) and v in (4, 27) and n not in (2, 3):
        val = 1.0
    elif n == 0 and 8 <= u <= 23 and 9 <= v <= 23:
        val = 0.75 if (v - 9) % 3 == 0 else 0.08
    elif n == 1 and v == 9 and u in (8, 11, 14):
        p.put(x, y, PINK if u == 14 else CYAN)
        return
    elif n == 1 and 16 <= u <= 25 and 19 <= v <= 24:
        p.put(x, y, GOLD[3] if (u + v) // 3 % 2 else DARK[0])
        return
    elif n == 4:
        r = math.hypot(u - 15.5, v - 15.5)
        if r < 2.5:
            val = 0.9
        elif r <= 9:
            p.put(x, y, FAN[int(math.atan2(v - 15.5, u - 15.5) / math.pi * 4 + 8) % 4])
            return
        elif r <= 11.5:
            p.shade(x, y, CHROME, 0.55 - (u + v - 31) / 30)
            return
    elif n == 5 and 6 <= u <= 25 and 13 <= v <= 18:
        p.put(x, y, CHROME[4] if u in (6, 25) else GLOWC[(0, 1, 2, 2, 1, 0)[v - 13]])
        return
    elif n == 6 and 4 <= u <= 27 and 4 <= v <= 27:
        val = 0.1 if v > 5 else 0.0
        if abs(u - v) < 2 or abs(u + v - 31) < 2:
            val = 0.5
    elif n == 7 and 6 <= u <= 25 and 6 <= v <= 25:
        val += 0.14 if u % 4 < 2 else -0.14
    p.shade(x, y, PANEL, val)


def mural() -> Pic:
    p = Pic(64, 128)
    for y in range(128):
        for x in range(64):
            wall(p, x, y)
    return p


def cut(p: Pic, x: int, y: int, w: int, h: int) -> Pic:
    q = Pic(w, h)
    for j in range(h):
        for i in range(w):
            q.put(i, j, p.get(x + i, y + j))
    return q


def mover() -> Pic:
    """A moving platform: a deck's slab, jets under it."""
    p = Pic(48, 12)
    for x in range(1, 47):
        for y in range(10):
            p.put(x, y, plate(x, y))
    for x in range(6, 48, 12):
        p.box(x - 1, 9, x, 10, CYAN)
    return p.outline()


def chute() -> Pic:
    """The works' quarter pipe: a block of plating, the deck up its curve."""
    p = Pic(48, 48)
    for y in range(48):
        for x in range(48):
            d = int(math.hypot(x + 0.5, y + 0.5) - 48.5)
            if d < 0:
                continue
            c = plate(int(math.atan2(x + 0.5, y + 0.5) * 48.5) % 64, d) if d < 11 else 0
            rivet = (x == 44 and y % 8 == 4) or (y == 44 and x % 8 == 4)
            p.put(x, y, c or PANEL[0 if d < 13 or x == 47 or y == 47 else 4 if rivet else 2])
    return p


def column(p: Pic, x0: int, x1: int, y0: int, y1: int, r: list[int]) -> None:
    """An upright cylinder in r's tones, dark to light, lit from the left: a rim, a glint, a long shade."""
    for x in range(x0, x1 + 1):
        f = (x - x0) / max(x1 - x0, 1)
        t = 3 if f < 0.1 else 5 if f < 0.24 else 4 if f < 0.4 else 3 if f < 0.6 else 2 if f < 0.9 else 3
        p.box(x, y0, x, y1, r[min(t, len(r) - 1) if x1 - x0 > 3 else (4, 5, 3, 2)[x - x0]])


def post() -> Pic:
    """A lamp post."""
    p = Pic(16, 48)
    p.box(7, 9, 8, 44, CHROME[2])
    p.box(7, 9, 7, 44, CHROME[4])
    p.box(4, 44, 11, 46, CHROME[2])
    p.box(4, 44, 11, 44, CHROME[4])
    p.box(6, 9, 9, 10, CHROME[3])
    p.ball(7.5, 4.5, 4.4, 4.4, [LAMP[1], HAZARD[2], HAZARD[1], LAMP[0], WHITE], 0.45, 1.0)
    return p.outline()


def tank() -> Pic:
    """A tank on legs: a dome, hoops, a gauge, a hazard band."""
    p = Pic(32, 40)
    for x in (5, 6, 25, 26):
        p.box(x, 34, x, 38, CHROME[1 + (x in (5, 25))])
    column(p, 3, 28, 9, 34, CHROME)
    for y in range(2, 9):  # the dome, a tone lighter up
        half = round(13 * math.sqrt(1 - ((9 - y) / 7.5) ** 2))
        column(p, 16 - half, 15 + half, y, y, CHROME)
    for y in (13, 24):  # hoops
        for x in range(3, 29):
            p.put(x, y, CHROME[max(CHROME.index(p.get(x, y)) - 1, 0)])
    for y in range(29, 33):
        for x in range(3, 29):
            p.put(x, y, DECK[5] if (x + y) // 3 % 2 else HAZARD[1 if 6 < x < 22 else 2])
    p.ball(11.5, 18.5, 3.4, 3.4, [CHROME[1], CHROME[5], CHROME[5]], 0.6, 1.0)
    p.put(11, 18, LAMP[1])
    p.put(12, 17, LAMP[1])
    p.box(14, 0, 17, 1, CHROME[2])
    return p.outline()


def vent() -> Pic:
    """Two stacks, the taller ringed red."""
    p = Pic(16, 28)
    column(p, 2, 6, 3, 25, CHROME)
    column(p, 9, 13, 10, 25, CHROME)
    p.box(1, 1, 7, 2, CHROME[4])
    p.box(8, 8, 14, 9, CHROME[3])
    p.box(2, 7, 6, 8, LAMP[1])
    p.box(1, 26, 14, 27, CHROME[2])
    p.box(1, 26, 14, 26, CHROME[4])
    return p.outline()


def skyline(p: Pic, base: int, seed: int, low: int, high: int, r: list[int], panes: bool) -> None:
    """A row of buildings standing on base, of period 320: each a lit left edge, a face and a shaded right; some
    stepped, some masted, some sloped; windows in rows, a few lit."""
    x = i = 0
    while x < 320:
        w = 12 + int(noise(i, 1, seed) * 16)
        if 320 - x - w < 12:
            w = 320 - x
        gap = 2 if noise(i, 4, seed) > 0.7 else 0
        top, style, x1 = base - low - int(noise(i, 2, seed) * (high - low)), int(noise(i, 3, seed) * 4), x + w - gap - 1

        def slab(xa: int, xb: int, ya: int, yb: int) -> None:
            for xx in range(xa, xb + 1):
                p.box(xx, ya, xx, yb, r[2] if xx == xa else r[1] if xx > xb - 3 else r[0])

        slab(x, x1, top, base)
        if style == 1 and w > 16:
            slab(x + 3, x1 - 4, top - 6, top - 1)
        elif style == 2:
            p.box(x + w // 2, top - 9, x + w // 2, top - 1, r[1])
            p.put(x + w // 2, top - 10, LAMP[1])
        elif style == 3:
            for k in range(1, 5):
                slab(x, x1 - k * 2, top - k, top - k)
        if panes:
            for wy in range(top + 4, base - 3, 5):
                for wx in range(x + 3, x1 - 3, 4):
                    v = noise(wx, wy, seed)
                    p.box(wx, wy, wx + 1, wy + 2, PANE[0] if v > 0.72 else PANE[1] if v > 0.6 else PANE[2])
        x, i = x + w, i + 1


def works() -> Pic:
    """The works' far backdrop, of period 320: a banded dusk, stars, a sun half set, streaks of cloud, two rows of
    city, a lamplit road."""
    p = Pic(320, 200)
    edges = [0, 16, 32, 48, 64, 80, 96, 110, 124, 200]
    for y in range(160):
        k = max(i for i in range(9) if edges[i] <= y)
        if k < 8 and y == edges[k + 1] - 2:
            k += 1
        p.box(0, y, 319, y, DUSK[k])
    for i in range(40):
        x, y = int(noise(i, 1, 9) * 320), int(noise(i, 2, 9) * 60)
        p.put(x, y, STAR[i % 3 > 0])
        if i % 9 == 0:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                p.put(x + dx, y + dy, STAR[1])
    for y in range(86, 136):  # the sun, in slices as it sets
        for x in range(50, 143):
            d = math.hypot(x - 96 + 0.5, y - 136 + 0.5)
            if d < 38 and (136 - y) not in (3, 4, 9, 10, 15, 20):
                p.put(x, y, SUNSET[0] if d < 30 else SUNSET[1])
            elif 38 <= d < 44:
                p.put(x, y, DUSK[min(DUSK.index(p.get(x, y)) + 1, 8)])
    for cx, cy, w in ((60, 30, 34), (190, 22, 26), (250, 58, 40), (130, 70, 22), (20, 84, 28), (296, 96, 20)):
        for row, half in enumerate((w * 0.5, w, w * 0.8, w * 0.35)):
            for x in range(int(cx - half + row * 3), int(cx + half + row * 5)):
                for dx in (-320, 0, 320):
                    p.put(x + dx, cy + row, WISP[2 if row == 0 else 0 if row == 3 else 1])
    skyline(p, 160, 5, 34, 78, TOWN, False)
    skyline(p, 160, 8, 16, 52, CITY, True)
    p.box(0, 156, 319, 159, CITY[3])
    for x in range(4, 320, 12):
        p.put(x, 155, PANE[1])
        p.put(x, 156, PANE[0])
    p.box(0, 160, 319, 199, WORKS[1])
    return p


def plant() -> Pic:
    """The works' near band, masked, of period 320: tanks, chimneys, a pipe rack, a sphere, a shed, a mast; lit from
    the left, lamps on."""
    p = Pic(320, 80)
    p.box(0, 56, 319, 79, WORKS[1])
    p.box(0, 56, 319, 56, WORKS[3])
    column(p, 14, 54, 20, 55, WORKS)  # a tank and its dome
    for y in range(12, 20):
        half = round(20 * math.sqrt(1 - ((20 - y) / 8.5) ** 2))
        column(p, 34 - half, 34 + half, y, y, WORKS)
    p.box(14, 30, 54, 30, WORKS[1])
    p.box(14, 44, 54, 44, WORKS[1])
    for x0, y0 in ((70, 4), (82, 14)):  # chimneys, banded red and white, a lamp on each
        column(p, x0, x0 + 5, y0, 55, WORKS)
        for k in range(3):
            p.box(x0, y0 + 2 + k * 4, x0 + 5, y0 + 3 + k * 4, LAMP[1 + k % 2])
        p.put(x0 + 2, y0 - 1, LAMP[0])
        p.put(x0 + 3, y0 - 1, LAMP[0])
    for y0 in (38, 47):  # a rack of pipes on posts
        p.box(96, y0, 150, y0 + 2, WORKS[2])
        p.box(96, y0, 150, y0, WORKS[4])
        p.box(96, y0 + 3, 150, y0 + 3, WORKS[0])
    for x in range(100, 150, 16):
        p.box(x, 38, x + 1, 55, WORKS[1])
    for x in (166, 186):
        p.box(x, 40, x + 1, 55, WORKS[1])
    p.ball(176.5, 28.5, 17, 17, WORKS[1:], 0.0, 1.0)  # a sphere on legs, belted
    p.box(160, 29, 193, 29, WORKS[1])
    for i in range(4):  # a shed, its roof in teeth, its windows lit
        for k in range(12):
            p.box(204 + i * 12 + k, 44 - k * 10 // 12, 204 + i * 12 + k, 44, WORKS[4 if k == 0 else 2])
    p.box(204, 45, 251, 55, WORKS[2])
    p.box(204, 45, 205, 55, WORKS[4])
    p.box(244, 45, 251, 55, WORKS[1])
    for x in range(210, 242, 7):
        p.box(x, 48, x + 3, 50, LAMP[0])
    for y in range(6, 56):  # a lattice mast, a lamp at its top
        p.put(264, y, WORKS[4])
        p.put(276, y, WORKS[2])
        p.put(264 + (y % 12), y, WORKS[3])
        p.put(276 - (y % 12), y, WORKS[2])
        if y % 12 == 0:
            p.box(264, y, 276, y, WORKS[3])
    p.box(269, 3, 271, 5, LAMP[1])
    column(p, 290, 312, 36, 55, WORKS)  # a low tank
    p.box(290, 34, 312, 35, WORKS[4])
    return p


def button(down: bool) -> Pic:
    """A floor button: a red dome on a plate; trodden, flat and green."""
    p = Pic(16, 8)
    p.box(1, 5, 14, 7, CHROME[2])
    p.box(1, 5, 14, 5, CHROME[4])
    if down:
        p.box(4, 4, 11, 4, GRASS[3])
    else:
        p.ball(7.5, 4.5, 4.4, 3.4, SCARF, 0.3, 1.0)
        p.box(3, 5, 12, 7, CHROME[2])
        p.box(3, 5, 12, 5, CHROME[4])
    return p.outline()


def nozzle() -> Pic:
    """A jet's mouth in the ground."""
    p = Pic(16, 6)
    p.box(2, 3, 13, 5, CHROME[2])
    p.box(4, 1, 11, 2, CHROME[4])
    p.box(6, 1, 9, 1, DARK[0])
    return p.outline()


def steam(f: int) -> Pic:
    """A jet's column, wider up, frame f."""
    p = Pic(12, 64)
    for y in range(64):
        w = 1.5 + (63 - y) * 0.06
        for x in range(12):
            d = abs(x - 5.5) / w
            if d < 1 and noise(x, (y + f * 5) // 2, 3 + f) > 0.25 + d * 0.3:
                p.put(x, y, WHITE if d < 0.4 else CLOUD[2 if (x + y + f) % 2 else 1])
    return p


def bumper(lit: bool) -> Pic:
    """A bumper: a ball that throws off what touches it; lit as it does."""
    p = Pic(24, 24)
    p.ball(11.5, 11.5, 10.5, 10.5, GOLD if lit else SCARF, 0.2, 1.0)
    p.ball(11.5, 11.5, 5.5, 5.5, CLOUD if lit else GOLD, 0.5, 1.0)
    p.put(8, 7, WHITE)
    p.put(9, 7, WHITE)
    return p.outline()


def mace() -> Pic:
    """A spiked ball."""
    p = Pic(16, 16)
    for i in range(8):
        a = i * math.pi / 4
        p.line(7.5, 7.5, 7.5 + 6.5 * math.cos(a), 7.5 + 6.5 * math.sin(a), STONE[3], 1.4)
    p.ball(7.5, 7.5, 4.6, 4.6, METAL, 0.1, 1.0)
    return p.outline()


def hub() -> Pic:
    p = Pic(8, 8)
    p.ball(3.5, 3.5, 2.6, 2.6, METAL, 0.2, 1.0)
    return p.outline()


def hoop(lit: bool) -> Pic:
    """A hoop seen edge on: through it is faster."""
    p = Pic(12, 32)
    for y in range(32):
        for x in range(12):
            e = ((x - 5.5) / 5) ** 2 + ((y - 15.5) / 15) ** 2
            if 0.5 < e <= 1:
                p.shade(x, y, CLOUD if lit else GOLD, 0.9 - 0.1 * x)
    return p.outline()


OWN: list[set[int]] = [set(), set()]  # the pictures only one zone shows: they may use that zone's own colours


def pictures() -> list[tuple[int, Pic, bool, int]]:
    """Each picture's index in the game's Img table, whether it is masked, and its mirror's index or -1."""
    out: list[tuple[int, Pic, bool, int]] = []
    itile, ihero, nf = 10, 34, 23
    out.append((itile + 18, ledge(), True, -1))
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
    # the Chrome Works' own
    z = k + 490 + 9
    out.append((z + 18, shelf(), True, -1))
    m, steel = mural(), PANEL[::-1]
    out += [(z + 54 + c, span(1, c), False, -1) for c in range(4)]
    out += [(z + 58 + c * 8 + r, cut(m, c * 16, r * 16, 16, 16), False, -1) for c in range(4) for r in range(8)]
    out += [(z + 90 + c, cut(m, c * 16, 0, 16, 128), False, -1) for c in range(4)]
    out += [
        (z + 94, nozzle(), True, -1),
        (z + 96, steam(0), True, -1),
        (z + 98, steam(1), True, -1),
        (z + 100, bumper(False), True, -1),
        (z + 102, bumper(True), True, -1),
        (z + 104, mace(), True, -1),
        (z + 106, hub(), True, -1),
        (z + 108, hoop(False), True, -1),
        (z + 110, hoop(True), True, -1),
        (z + 120, button(False), True, -1),
        (z + 122, button(True), True, -1),
    ]
    out += [
        (z + 24, mover(), True, -1),
        (z + 26, post(), True, -1),
        (z + 28, tank(), True, -1),
        (z + 30, vent(), True, -1),
        (z + 32, words("CHROME WORKS", 178, 18, 1.65, 3.0), True, -1),
        (z - 9, works(), False, -1),
        (z - 8, plant(), True, -1),
        (z - 4, ring(188, 72, 93, lambda u, d: plate(u % 64, d) if d < 11 else 0, steel, INK), True, -1),
        (z - 2, chute(), True, -1),
    ]
    out += [(z + 116 + h * 2, ribbon(h, [DECK[0], DECK[1], *steel[1:4], steel[5]], INK), True, -1) for h in range(2)]
    # a slope of deck, by the deck's four 16s: its tile, then what runs on into the tile under it
    b = z + 402
    for kind in range(2, 8):
        for c in range(4):
            out.append((b + ((kind - 2) * 4 + c) * 2, span(kind, c), True, -1))
            out.append((b + 48 + ((kind - 2) * 4 + c) * 2, span(kind, c, 1), True, -1))
    OWN[1].update({*range(z - 9, z + 94), z + 116, z + 118, *range(b, b + 96)})
    # Amber Downs' own: its backdrop, hills, loop and ribbon where the game once built them, then its ground by
    # the comb's column of 4 and row of 3: flats, each slope and the tile under it, the comb alone, pipes, a raft
    out += [(0, downs(), False, -1), (1, hills(), True, -1)]
    out += [(z + 112 + h * 2, ribbon(h, AMBER, PLUM), True, -1) for h in range(2)]
    a = z + 124
    for c in range(4):
        out.append((a + 240 + c, deep(c, 0, 192), False, -1))
        for r in range(3):
            v = c * 3 + r
            out += [(a + v, top(1, c, r), False, -1), (a + 228 + v, deep(c, r, 16), False, -1)]
            out.append((a + 244 + v * 2, pipe(c, r), True, -1))
            for kind in range(2, 8):
                out.append((a + 12 + ((kind - 2) * 12 + v) * 2, top(kind, c, r), True, -1))
                out.append((a + 156 + (kind - 2) * 12 + v, top(kind, c, r, 1), False, -1))
    out.append((a + 268, raft(), True, -1))
    out.append((z + 594, startled(), True, z + 596))
    # Charlie round a twist: 12 turns of two strides, their mirrors facing left
    out += [
        (z + 498 + (n * 2 + f) * 2, turned(frames[5 + f * 3], n, f), True, z + 546 + (n * 2 + f) * 2)
        for n in range(12)
        for f in range(2)
    ]
    out.append((5, ring(188, 72, 93, turf, AMBER, PLUM), True, -1))
    out += [(a + 270 + i * 2, bare(i > 1, bool(i & 1)), True, -1) for i in range(4)]
    OWN[0].update({0, 1, 5, itile + 18, z + 112, z + 114, *range(a, a + 278), *range(k + 36, k + 50)})
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


def song() -> bytes:
    """SONG's DATA lines in order: a number a LONG, a string its length and its characters."""
    out = b""
    for line in SONG.read_text().splitlines():
        if line.startswith("DATA "):
            for num, text in re.findall(r'(-?\d+)|"([^"]*)"', line[5:]):
                out += struct.pack("<i", int(num)) if num else struct.pack("<i", len(text)) + text.encode("ascii")
    return out


def audit(pics: list[tuple[int, Pic, bool, int]]) -> None:
    """A zone's own colours are only in its own pictures: elsewhere they would show as another zone's."""
    # the game's store ran out, an Illegal function call as it loaded: these with their masks and mirrors, the 160
    # turned runs it flops and its own glyphs and HUD (6000 at most) must fit it
    room = int(re.search(r"(?i)const IMGSZ = (\d+)", DAT.with_suffix(".BAS").read_text())[1])
    need = sum(len(p.longs()) * (2 if m else 1) * (2 if k2 >= 0 else 1) for _, p, m, k2 in pics) + 160 * 258 + 6000
    if need > room:
        sys.exit(f"charlieart: the pictures need {need} LONGs of the game's IMGSZ, {room}")
    # turned a sixth of the way round a twist he was his side pressed to half its height, flat: he keeps his bulk
    run = hero()
    for turn in range(1, 12):
        for f in range(2):
            if turned(run[5 + f * 3], turn, f).count() < 0.6 * run[5 + f * 3].count():
                sys.exit(f"charlieart: Charlie is flat, turned {turn} twelfths round a twist")
    # the game draws the comb from 12 tiles a kind: it must repeat every 64 across and 48 down
    if any(comb(x, y) != comb(x + 64, y) or comb(x, y) != comb(x, y + 48) for x in range(64) for y in range(48)):
        sys.exit("charlieart: the comb does not repeat every 64 by 48")
    # a slope's turf was cut off at its tile's foot: the tile and the one under it must hold it down to its hem
    for kind in range(2, 8):
        both = [top(kind, 0, 0).p[y] for y in range(16)] + [top(kind, 0, 0, 1).p[y] for y in range(16)]
        if any(TURF[5] not in [row[x] for row in both] for x in range(16)):
            sys.exit(f"charlieart: slope {kind}'s turf is cut short")
    for k, p, _, _ in pics:
        used = {v for row in p.p for v in row}
        for z in ZPAL:
            if k not in OWN[0] | OWN[1] and used & ZPAL[z].keys():
                sys.exit(f"charlieart: picture {k} uses zone {z}'s own colours {sorted(used & ZPAL[z].keys())}")


def blob() -> bytes:
    """CHARLIE.DAT: the 1-based byte its tunes start at, the palette, each zone's own, the pictures, the tunes;
    each value a little-endian LONG."""
    pics = pictures()
    audit(pics)
    vals = [len(PAL)]
    for i, (r, g, b) in sorted(PAL.items()):
        vals += [i, r + 256 * g + 65536 * b]
    for z in sorted(ZPAL):
        vals.append(len(ZPAL[z]))
        for i, (r, g, b) in sorted(ZPAL[z].items()):
            vals += [i, r + 256 * g + 65536 * b]
    vals.append(len(pics))
    for k, p, masked, k2 in pics:
        vals += [k, p.w, p.h, int(masked), k2, *encode(p.longs())]
        if masked:
            vals += encode(p.longs(mask=True))
    return struct.pack(f"<{len(vals) + 1}i", len(vals) * 4 + 5, *vals) + song()


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
    ap.add_argument("--check", action="store_true", help="fail when CHARLIE.DAT is stale")
    ap.add_argument("--sheet", type=Path, help="the pictures to this PNG")
    a = ap.parse_args()
    if a.sheet:
        sheet(a.sheet)
        return
    new = blob()
    if a.check:
        fresh = DAT.exists() and DAT.read_bytes() == new
        sys.exit(0 if fresh else "charlieart: CHARLIE.DAT is stale; run tools/charlieart.py")
    DAT.write_bytes(new)
    print(f"charlieart: {len(pictures())} pictures, {len(new)} bytes")


if __name__ == "__main__":
    main()
