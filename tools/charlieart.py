#!/usr/bin/env python3
"""charlieart.py [--check] [--sheet PNG]: CHARLIE.BAS's palette and pictures (Charlie, tiles, things, props),
drawn here in flat tones and written to CHARLIE.DAT beside the game, a little-endian LONG a value:
drawn by the game they would cost its start-up a pixel at a time. --check: fail when the file is
stale. --sheet: every picture to a PNG, to look at. The game's tunes, tools/charliesong.txt, go in after them,
then the kinds' rows.

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
from typing import NamedTuple
from dataclasses import dataclass
from collections.abc import Callable

ROOT = Path(__file__).resolve().parents[1]
DAT = ROOT / "src/plat/wasm/web/demos/CHARLIE.DAT"
SONG = ROOT / "tools/charliesong.txt"
LEVELS = ROOT / "tools/charlielevels.txt"
GAME = ROOT / "src/plat/wasm/web/demos/barkley"  # the game's package
ENGINE = GAME.with_name("engine")  # the engine it runs on
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
ZPAL: dict[int, dict[int, tuple[int, int, int]]] = {stage: {} for stage in range(12)}


def pal(z: int) -> dict[int, tuple[int, int, int]]:
    """The colours on screen in stage z."""
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
    snout(p, 22.5 + lean + stretch * 0.5, 12 + bob + squat * 1.2 + lean * 0.3 + stretch * 0.6, ear, eye, pant)
    return p.outline()


def snout(p: Pic, hx: float, hy: float, ear: float, eye: str, pant: bool) -> None:
    """Charlie's head about hx, hy, facing right: its muzzle, its ears hung at `ear`, its nose, mouth and eye."""
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


# ---- shapes as masks: what the Baron and the family are built of ----

Pt = tuple[float, float]
Mask = set[tuple[int, int]]
FALL = (0.55, 0.83)  # the way a shadow falls: down and to the right


def spline(pts: list[Pt], n: int = 10, closed: bool = True) -> list[Pt]:
    """A smooth curve through pts, n steps a span."""
    out: list[Pt] = []
    m = len(pts)
    for i in range(m if closed else m - 1):
        p0 = pts[(i - 1) % m] if closed or i > 0 else pts[0]
        p1, p2 = pts[i % m], pts[(i + 1) % m]
        p3 = pts[(i + 2) % m] if closed or i + 2 < m else pts[m - 1]
        for k in range(n):
            t = k / n
            x, y = (
                0.5
                * (
                    2 * p1[a]
                    + (p2[a] - p0[a]) * t
                    + (2 * p0[a] - 5 * p1[a] + 4 * p2[a] - p3[a]) * t * t
                    + (3 * p1[a] - p0[a] - 3 * p2[a] + p3[a]) * t**3
                )
                for a in (0, 1)
            )
            out.append((x, y))
    return out if closed else [*out, pts[-1]]


def poly(pts: list[Pt]) -> Mask:
    """The dots whose middles are inside a polygon."""
    out: Mask = set()
    ys = [y for _, y in pts]
    for y in range(math.floor(min(ys)), math.ceil(max(ys)) + 1):
        cy = y + 0.5
        xs = sorted(
            x0 + (cy - y0) * (x1 - x0) / (y1 - y0)
            for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1], strict=True)
            if (y0 <= cy) != (y1 <= cy)
        )
        for a, b in zip(xs[::2], xs[1::2], strict=False):
            out |= {(x, y) for x in range(math.ceil(a - 0.5), math.floor(b - 0.5) + 1)}
    return out


def curved(pts: list[Pt], n: int = 10) -> Mask:
    """What a closed curve through pts holds."""
    return poly(spline(pts, n))


def oval(cx: float, cy: float, rx: float, ry: float, rot: float = 0) -> Mask:
    """An ellipse, turned rot."""
    c, s, r = math.cos(rot), math.sin(rot), max(rx, ry)
    out: Mask = set()
    for y in range(math.floor(cy - r) - 1, math.ceil(cy + r) + 1):
        for x in range(math.floor(cx - r) - 1, math.ceil(cx + r) + 1):
            dx, dy = x + 0.5 - cx, y + 0.5 - cy
            if ((dx * c + dy * s) / rx) ** 2 + ((dy * c - dx * s) / ry) ** 2 <= 1:
                out.add((x, y))
    return out


def stroke(pts: list[Pt], w0: float, w1: float | None = None) -> Mask:
    """A line through pts, w0 wide at its start to w1 at its end, its ends round."""
    w1 = w0 if w1 is None else w1
    spans = list(itertools.pairwise(pts)) or [(pts[0], pts[0])]
    total = sum(math.dist(a, b) for a, b in spans) or 1
    out: Mask = set()
    done = 0.0
    for a, b in spans:
        d = math.dist(a, b)
        n = max(1, int(d * 3))
        for i in range(n + 1):
            t = i / n
            x, y = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
            r = (w0 + (w1 - w0) * (done + d * t) / total) / 2
            out |= oval(x, y, r, r)
        done += d
    return out


def rect(x0: int, y0: int, x1: int, y1: int) -> Mask:
    return {(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)}


def near_edge(m: Mask, q: tuple[int, int], way: Pt, d: float) -> bool:
    """Whether q is within d of m's edge, going way."""
    return any((round(q[0] + way[0] * k / 2), round(q[1] + way[1] * k / 2)) not in m for k in range(1, int(d * 2) + 1))


def part(
    p: Pic, m: Mask, r: list[int], sh: float = 1.5, hl: float = 0, line: int = 0, within: Mask | None = None
) -> Mask:
    """m painted in r's tones, shadow then base then, if it has one, light: its shadow within sh of its lower right
    edge, its light within hl of its upper left; line: a line of that tone round it where it lies on what is drawn.
    within: only there."""
    if within is not None:
        m = m & within
    if line:
        for x, y in m:
            for i, j in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if (x + i, y + j) not in m and p.get(x + i, y + j):
                    p.put(x + i, y + j, line)
    for q in m:
        lit = hl and len(r) > 2 and near_edge(m, q, (-FALL[0], -FALL[1]), hl)
        p.put(*q, r[0] if sh and near_edge(m, q, FALL, sh) else r[2] if lit else r[1])
    return m


def fill(p: Pic, m: Mask, c: int, within: Mask | None = None) -> None:
    for x, y in m if within is None else m & within:
        p.put(x, y, c)


def dots(p: Pic, c: int, *at: tuple[int, int]) -> None:
    for x, y in at:
        p.put(x, y, c)


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
CLOTH = [CLOUD[1], CLOUD[2]]  # white cloth: its shadow, itself
BLACK, GREY, JEANS = DARK[:3], STONE[1:], [DENIM[1], DENIM[2], WHITE]
# a tone in something's shadow
SHADE = {TAN[2]: TAN[1], TAN[1]: TAN[0], PALE[2]: PALE[1], PALE[1]: PALE[0], CLOUD[2]: CLOUD[1], CLOUD[1]: CLOUD[0]}
SHADE |= {DARK[3]: DARK[2], DARK[2]: DARK[1], DARK[1]: DARK[0], STONE[3]: STONE[2], STONE[2]: STONE[1]}
SHADE |= {DENIM[2]: DENIM[1], DENIM[1]: DENIM[0], NAVY[2]: NAVY[1], NAVY[1]: NAVY[0], STONE[1]: STONE[0]}
SHADE |= {FUR[5]: FUR[4], FUR[4]: FUR[3], PETAL[2]: CLOUD[1], WHITE: DENIM[2]}
# Each is drawn on a stage, its middle column MID and its feet on row FEET, then cut to what it fills.
STAGE, MID, FEET = (97, 116), 48.5, 104


@dataclass
class Pose:
    """How someone stands: bob drops all over the hips, squat bends the knees; left and right the arms at our left
    and right, or the Pomchi's front and back legs; wave moves a raised hand, or tips the Pomchi's nose up; lift:
    how far off the ground; f: a count for what else moves."""

    bob: int = 0
    squat: int = 0
    left: str = "down"
    right: str = "down"
    blink: bool = False
    joy: bool = False  # the mouth open
    wave: float = 0
    lift: int = 0
    f: int = 0


ARMS: dict[str, tuple[Pt, Pt]] = {  # an arm's elbow and wrist from its shoulder, in arm lengths, out and down
    "down": ((0.2, 0.98), (0.22, 1.9)),
    "back": ((0.36, 0.92), (0.62, 1.74)),
    "hip": ((0.7, 0.7), (0.0, 1.2)),
    "half": ((0.84, 0.36), (1.3, -0.4)),
    "up": ((0.56, -0.78), (0.5, -1.76)),
    "high": ((0.32, -0.92), (0.2, -1.9)),
    "out": ((0.9, -0.36), (1.42, -1.2)),
}
LOW = ("down", "back")  # the arms drawn before what they hang beside
Arm = tuple[Pt, Pt, Pt]  # a shoulder, an elbow, a wrist


def cast(p: Pic, m: Mask, rows: int = 1) -> None:
    """m's shadow on what is drawn under its lower edge."""
    for x, y in m:
        for k in range(1, rows + 1) if (x, y + 1) not in m else ():
            if (x, y + k) not in m and p.get(x, y + k) in SHADE:
                p.put(x, y + k, SHADE[p.get(x, y + k)])


def joints(sx: float, sy: float, side: int, pose: str, length: float, wave: float = 0) -> Arm:
    (ex, ey), (wx, wy) = ARMS[pose]
    return (sx, sy), (sx + side * ex * length, sy + ey * length), (sx + side * wx * length + wave, sy + wy * length)


def hand(p: Pic, j: Arm, side: int, skin: list[int]) -> None:
    """A hand past a wrist: a mitt, its thumb a dot to the body's side."""
    _, el, wr = j
    d = max(math.dist(el, wr), 1)
    hx, hy = wr[0] + (wr[0] - el[0]) / d * 1.6, wr[1] + (wr[1] - el[1]) / d * 1.6
    part(p, oval(hx, hy, 2.6, 2.6), skin[1:], sh=1.0, line=skin[1])
    dots(p, skin[1], (round(hx - side * 0.9 - 0.5), round(hy + (0.8 if wr[1] > el[1] else -0.8) - 0.5)))


def arm(p: Pic, j: Arm, side: int, w: float, top: list[int], skin: list[int], sleeve: float, line: int) -> None:
    """An arm by its joints, w wide: sleeved in top for `sleeve` of its upper arm (2: to the wrist), then a hand."""
    sh, el, wr = j
    limb = stroke([sh, el], w + 0.6, w) | stroke([el, wr], w - 0.1, w - 1.0)
    if sleeve >= 2:
        part(p, limb, top, sh=1.3, line=line)
        d = max(math.dist(el, wr), 1)
        cuff = (wr[0] - (wr[0] - el[0]) / d * 1.2, wr[1] - (wr[1] - el[1]) / d * 1.2)
        fill(p, stroke([cuff], w - 0.6), top[0], limb)
        hand(p, j, side, skin)
        return
    part(p, limb, skin[1:], sh=1.3, line=skin[0])
    hand(p, j, side, skin)
    end = (sh[0] + (el[0] - sh[0]) * sleeve, sh[1] + (el[1] - sh[1]) * sleeve)
    cast(p, part(p, stroke([sh, end], w + 2.0), top, sh=1.3, line=line))


def bare_legs(p: Pic, hip: float, stance: float, squat: int, skin: list[int], w: float) -> float:
    """Two bare legs to the ankles, w half a thigh: a knee, a calf's swell on its outer side. The knee's row."""
    knee = (hip + FEET - 5) / 2 + 1
    for side in (-1, 1):
        x, k = MID + side * stance, MID + side * (stance + squat * 1.2)
        out = [(x + side * (w + 0.2), hip), (k + side * w, knee - 3), (k + side * (w - 0.6), knee + 1)]
        out += [(k + side * (w - 0.3), knee + 5), (x + side * (w - 0.9), FEET - 6), (x + side * (w - 1.2), FEET - 3)]
        back = [(x - side * (w - 1.5), FEET - 3), (x - side * (w - 1.3), FEET - 7), (k - side * (w - 0.8), knee + 4)]
        back += [(k - side * (w - 0.9), knee), (x - side * (w - 0.3), hip)]
        part(p, poly(spline(out + back, 6)), skin[1:], sh=1.4)
        dots(
            p, skin[1], (round(k - 1.5), round(knee)), (round(k - 0.5), round(knee) + 1), (round(k + 0.5), round(knee))
        )
    return knee


def trousers(p: Pic, hip: float, stance: float, squat: int, r: list[int], w0: float, w1: float) -> float:
    """Two legs in cloth from the hips to the ankles, joined at the seat. The knee's row."""
    knee, mid = (hip + FEET - 4) / 2, (w0 + w1) / 2 + 0.3
    for side in (-1, 1):
        h, a = (MID + side * stance, hip), (MID + side * (stance + 0.5), FEET - 3.5)
        k = (MID + side * (stance + 0.3 + squat * 1.2), knee)
        part(p, stroke([h, k], w0, mid) | stroke([k, a], mid, w1), r, sh=1.4, line=r[0])
    half = round(stance + w0 / 2 - 0.5)
    part(p, rect(int(MID) - half, round(hip) - 3, int(MID) + half, round(hip) + 1), r, sh=1.4)
    return knee


SHOE = "..uuuu.. .uuuuuu. uuuuuuud uuuuuudd ssssssss"
SMALL = ".uuuu. uuuuud uuuudd ssssss"


def sneakers(p: Pic, stance: float, upper: list[int], sole: int, sock: int = 0, rows: str = SHOE) -> None:
    """Shoes from in front, toes a little out; sock rows of white rib over them."""
    n, w = len(rows.split()), len(rows.split()[0])
    for side in (-1, 1):
        x, y = int(MID) + side * round(stance), FEET - n + 1
        if sock:
            part(p, rect(x - 2, y - sock, x + 2, y), CLOTH, sh=1.0)
            fill(p, rect(x - 2, y - sock, x + 2, y - sock), CLOUD[1])
        grid = rows if side > 0 else " ".join(r[::-1] for r in rows.split())
        stamp(p, grid, x - (w // 2 - 1 if side > 0 else w // 2), y, {"u": upper[1], "d": upper[0], "s": sole})
        dots(p, upper[0], (x - 1 + (side > 0), y + 1), (x + (side > 0), y + 2))


def shorts(p: Pic, top: float, hem: float, w0: float, w1: float, r: list[int], squat: int = 0) -> None:
    """Loose shorts from their waist to their hems, parted under the crotch; r: shadow, tone, light."""
    crotch = top + (hem - top) * 0.55
    left = [(MID - w0, top), (MID - w1 - squat, hem), (MID - 1.4 - squat * 0.5, hem)]
    m = poly([*left, (MID, crotch), *((2 * MID - x, y) for x, y in left[::-1])])
    part(p, m, r, sh=2.0)
    fill(p, stroke([(MID + 0.5, crotch - 1), (MID + 4.5, crotch - 4)], 1.0), r[0], m)  # folds from the crotch
    fill(p, stroke([(MID - 1.5, crotch - 1), (MID - 5, crotch - 3.5)], 1.0), r[0], m)
    fill(p, {(x, y - 1) for x, y in m if (x, y + 1) not in m and y > crotch}, r[2], m)  # the hems' turn
    cast(p, m, 2)


def torso(p: Pic, sy: float, hem: float, half: float, low: float, r: list[int], line: int, slope: float) -> Mask:
    """A top from its shoulders to its hem: half the shoulders across, `low` half its hem."""
    left = [(MID - 3.6, sy - 1.2), (MID - half, sy + slope - 1), (MID - half - 0.8, sy + 5)]
    left += [(MID - (half + low) / 2 + 0.6, (sy + hem) / 2 + 1), (MID - low, hem - 0.6)]
    m = poly(spline([*left, (MID, hem), *((2 * MID - x, y) for x, y in left[::-1])], 6))
    part(p, m, r, sh=2.2, line=line)
    cast(p, m, 2)
    return m


def neck(p: Pic, sy: float, skin: list[int], top: Mask, collar: int, deep: float = 2.2) -> None:
    """A neck into a top's round collar."""
    hole, throat = oval(MID, sy + 0.2, 3.9, deep), rect(int(MID) - 2, round(sy) - 5, int(MID) + 2, round(sy))
    fill(p, throat, skin[1])
    fill(p, hole, skin[1], top | throat)
    fill(p, {(x, y + 1) for x, y in hole if (x, y + 1) not in hole}, collar, top)


def face(grid: str, pose: Pose, eye: int, mouth: int, deep: int = 1) -> str:
    """A head's rows, its eyes shut or its mouth open: eye the lash line's row, deep the rows of eye under it,
    mouth the lower lip's row."""
    rows = grid.split()
    if pose.blink:
        rows[eye] = rows[eye].replace("K", "s")
        for k in range(1, deep):
            rows[eye + k] = rows[eye + k].replace("Kbw", "sss")
        rows[eye + deep] = rows[eye + deep].replace("Kbw", "KKK")
    if pose.joy:
        a, b = rows[mouth].index("l"), rows[mouth].rindex("l")
        rows[mouth] = rows[mouth][:a] + "R" + "t" * (b - a - 1) + "R" + rows[mouth][b + 1 :]
        rows[mouth + 1] = rows[mouth + 1][: a + 1] + "l" * (b - a - 1) + rows[mouth + 1][b:]
    return " ".join(rows)


FACE = {"K": INK, "w": WHITE, "l": LIP, "u": LIP, "R": SCARF[0], "t": TONGUE[1], "p": BLUSH}


def sides(pose: Pose) -> tuple[tuple[int, str], tuple[int, str]]:
    """Each arm's side, -1 ours left, and how it is held."""
    return (-1, pose.left), (1, pose.right)


# Hair in tufts leaning one way over sides cut close, a brow's hair high at the temples, a short beard going grey.
DAD = """
.........h.hh.h....
.....h.hhhHhhHhh...
....hhhhHhhHhhHhh..
...hhHhhHhhHhhHhhn.
..hhHhhHhhHhhHhhhn.
.vhhhhhhhhhhhhhhhv.
.vhsshhhhhhhhhssnv.
.vsssssssssssssssv.
.vsssssssssssssssv.
.vsssssssssssssssv.
.vsBBBBBsssBBBBBsv.
essssssssssssssssse
edssKKKsssssKKKssde
edssKbwsssssKbwssde
.ssssssssdsssssssd.
.NssssssdddssssssN.
.NNsssssssssssssNx.
.NNNsmmmmmmmmmsNNx.
.NNNmmuuuuuuummNNx.
..NNNmuwwwwwumNNx..
..NNNNmlllllmNNNx..
...NNNGgggggGNNx...
....NNNGgggGNNx....
......NNNNNNx......
"""


def bearded(pose: Pose) -> Pic:
    """A grown up: tanned, dark hair up in tufts over short sides, heavy brows, a short beard going grey; a big
    white shirt, its sleeves to the elbow, loose dark shorts to the knee, white socks and shoes."""
    p = Pic(*STAGE)
    half, top = 10.8, FEET - 77 + pose.bob + pose.squat * 2  # the hair's top row
    sy, hip = top + 24, top + 46
    knee = bare_legs(p, hip, 4.8, pose.squat, TAN, 3.8)
    sneakers(p, 4.8, CLOTH, FUR[1], sock=3)
    shorts(p, hip - 6, knee - 2, 10.0, 10.6, BLACK, pose.squat)
    arms = [(joints(MID + s * (half + 0.4), sy + 3, s, how, 11, s * pose.wave), s, how) for s, how in sides(pose)]
    for j, side, _ in (a for a in arms if a[2] in LOW):
        arm(p, j, side, 5.6, CLOTH, TAN, 0.66, CLOUD[0])
    shirt = torso(p, sy, hip - 1, half, half + 0.4, CLOTH, CLOUD[0], 1.6)
    for a, b in (((-6, 12), (-4.4, 17)), ((4.2, 9), (2.4, 15)), ((-2, 15), (-0.5, 19))):  # its folds
        fill(p, stroke([(MID + a[0], sy + a[1]), (MID + b[0], sy + b[1])], 1.0), CLOUD[1], shirt)
    hem = round(half + 1)
    fill(p, rect(int(MID) - hem, round(hip) - 4, int(MID) + hem, round(hip) - 4), CLOUD[1], shirt)
    neck(p, sy, TAN, shirt, CLOUD[1])
    for j, side, _ in (a for a in arms if a[2] not in LOW):
        arm(p, j, side, 5.6, CLOTH, TAN, 0.66, CLOUD[0])
    key = FACE | {"h": DARK[1], "H": DARK[2], "n": DARK[0], "v": DARK[2], "s": TAN[2], "d": TAN[1], "e": TAN[2]}
    key |= {"B": DARK[0], "b": BROWN, "N": DARK[2], "m": DARK[2], "x": DARK[1], "g": ASH, "G": DARK[3]}
    stamp(p, face(DAD, pose, 12, 20), int(MID) - 9, top, key | {"u": SCARF[0]})
    return p.outline()


# A fair bob parted at one side and swept across, darker at its roots, down to the jaw.
MUM = """
.......oooooo........
.....ooyyYYYyoo......
....oyyOyYYYYyyoo....
...oyyOOyyYYYYyyyo...
..oyyyOyyyyyYYYyyyo..
..oyyysssyyyyyyYyyo..
.oyyyssssssyyyyyYyyo.
.oyysssssssssoyyyyyo.
.oyyssssssssssoyyyyo.
.oyysssssssssssoyyyo.
.oyysBBBsssssBBBsyyo.
.oYyKsssssssssssKyyo.
.oYysKKKsssssKKKsyyo.
.oYysKbwsssssKbwsyyo.
.oyysssssssssssssyyo.
.oyyspssssdsssspdyyo.
.oyysssuuuuuuusssyyo.
.oyysssuwwwwwusssyyo.
..oyyssslllllssdyyo..
..ooyysssssssssdyoo..
...ooyysssssssdyoo...
.....oo.dddddd.oo....
"""


def fair(pose: Pose) -> Pic:
    """A grown up: blue eyed, tanned, short fair hair parted at one side, darker at its roots; a big grey hoodie,
    its hood behind and a pocket in front, dark shorts, white socks and shoes."""
    p = Pic(*STAGE)
    half, top = 8.8, FEET - 72 + pose.bob + pose.squat * 2
    sy, hip = top + 22, top + 42
    bare_legs(p, hip, 4.1, pose.squat, TAN, 3.9)
    sneakers(p, 4.1, CLOTH, FUR[1], sock=3)
    shorts(p, hip - 4, hip + 7, 8.8, 9.4, NAVY, pose.squat)
    arms = [(joints(MID + s * (half + 0.4), sy + 3, s, how, 10, s * pose.wave), s, how) for s, how in sides(pose)]
    for j, side, _ in (a for a in arms if a[2] in LOW):
        arm(p, j, side, 5.0, GREY, TAN, 2, STONE[0])
    part(p, oval(MID, sy + 0.6, 9.2, 3.6), GREY, sh=1.4)  # the hood, behind
    top_ = torso(p, sy, hip + 3, half, half + 1.2, GREY, STONE[0], 2.0)
    band = round(half + 2)
    fill(p, rect(int(MID) - band, round(hip) + 1, int(MID) + band, round(hip) + 1), STONE[1], top_)
    pocket = poly([(MID - 5, sy + 11), (MID + 5, sy + 11), (MID + 7.4, sy + 18), (MID - 7.4, sy + 18)])
    rim = {(x, y) for x, y in pocket if any((x + i, y + j) not in pocket for i, j in ((1, 0), (-1, 0), (0, -1)))}
    fill(p, rim, STONE[1], top_)
    fill(p, {(x, y) for x, y in pocket if (x - 1, y - 1) not in pocket and (x, y - 1) in pocket}, STONE[3], top_)
    neck(p, sy, TAN, top_, STONE[1], 2.8)
    for dx in (-2, 2):  # the hood's cords
        fill(p, rect(int(MID) + dx, round(sy) + 3, int(MID) + dx, round(sy) + 9), CLOUD[2], top_)
        dots(p, CLOUD[0], (int(MID) + dx, round(sy) + 10))
    for j, side, _ in (a for a in arms if a[2] not in LOW):
        arm(p, j, side, 5.0, GREY, TAN, 2, STONE[0])
    key = FACE | {"y": BLOND[2], "Y": BLOND[3], "o": BLOND[1], "O": BLOND[0], "s": TAN[2], "d": TAN[1]}
    stamp(p, face(MUM, pose, 12, 18), int(MID) - 10, top, key | {"B": BLOND[0], "b": BLUE})
    return p.outline()


# Hair pulled back to a knot on top, headphones over it: their band, a cup an ear.
TEEN = """
........nhhhn........
.......nhHHhhn.......
.......nhhhhhn.......
......WWWWWWWWW......
....WWGhhHHhhhGWW....
...WGhhhHHhhhhhhGW...
..WGhhhHHhnhhhhhhGW..
.WGhhhssssssssshhhGW.
.WGhsssssssssssssnGW.
WWWhsssssssssssssnWWW
WGWhsBBBsssssBBBsnWGW
WGWssssssssssssssdWGW
WGWssKKKsssssKKKsdWGW
WGWssKbwsssssKbwsdWGW
WGWssssssssssssssdWGW
WWWsspssssdsssspsdWWW
.WWsssssssssssssdWW..
...ssssusssssussd....
....sssslllllssd.....
.....sssssssssd......
......ssssssdd.......
........dddd.........
"""


def guitar(p: Pic, bx: float, by: float, nx: float, ny: float) -> None:
    """A guitar, its body about bx, by, its neck up to nx, ny: amber going dark at its edge, two pickups."""
    part(p, stroke([(bx + 5, by - 2.4), (nx, ny)], 3.2, 2.8), [HAIR[1], BLOND[3]], sh=0.9, line=INK)
    for i in range(1, 7):  # its frets
        dots(p, ASH, (round(bx + 4.5 + (nx - bx - 5) * i / 7), round(by - 2.9 + (ny - by + 2.4) * i / 7)))
    part(p, oval(nx + 1.8, ny - 1.6, 2.6, 3.2, 0.6), HAIR[:3], sh=1.0, hl=0.8, line=INK)
    dots(p, WHITE, (round(nx + 0.5), round(ny - 3)), (round(nx + 2.5), round(ny - 1)))
    body = oval(bx - 2, by + 2, 9.2, 6.8) | oval(bx + 5, by - 1.6, 5.2, 4.8)
    edge = {(x + i, y + j) for x, y in body for i, j in ((1, 0), (-1, 0), (0, 1), (0, -1)) if p.get(x + i, y + j)}
    fill(p, edge - body, INK)
    for x, y in body:  # the burst: by how much of the body is about a dot
        near = sum((x + i, y + j) in body for i in range(-2, 3) for j in range(-2, 3))
        p.put(x, y, EARTH[1] if near < 19 else EARTH[4] if near < 25 else GOLD[2])
    fill(p, oval(bx - 2.6, by + 1.2, 4.6, 3.2), GOLD[3], body)
    for x in (round(bx) - 5, round(bx)):
        fill(p, rect(x, round(by) - 1, x + 1, round(by) + 4), DARK[1])
        fill(p, rect(x, round(by), x, round(by) + 3), ASH)
    fill(p, rect(round(bx) - 9, round(by), round(bx) - 8, round(by) + 3), METAL[3])
    fill(p, stroke([(bx - 8, by + 1.6), (bx + 6, by - 1.6)], 1.0), METAL[3], body)


STRUM = {"s0": -1.0, "s1": 1.0, "s2": 3.0}  # a strumming hand's row over the strings


def big(pose: Pose) -> Pic:
    """The big one: brown eyed, light brown hair pulled back and tied up, white headphones, a black hoodie, pale
    wide jeans worn through at a knee, black shoes on white soles; a guitar on a strap, its long pale neck up.
    left: its strumming hand's stroke, or that arm up."""
    p = Pic(*STAGE)
    half, top, c = 8.4, FEET - 62 + pose.bob + pose.squat * 2, int(MID)
    sy, hip = top + 21, top + 38
    knee = round(trousers(p, hip, 3.8, pose.squat, JEANS, 6.8, 6.2))
    fill(p, rect(c - 6, knee - 1, c - 3, knee + 1), PALE[1])  # a knee worn through
    fill(p, rect(c - 6, knee - 2, c - 2, knee - 2), WHITE)
    fill(p, rect(c - 5, knee + 2, c - 2, knee + 2), WHITE)
    sneakers(p, 4.2, BLACK, CLOUD[2])
    part(p, oval(MID, sy + 0.6, 8.8, 3.4), DARK[1:], sh=1.4)  # the hood
    hood = torso(p, sy, hip + 1, half, half + 0.8, BLACK, DARK[0], 2.0)
    neck(p, sy, PALE, hood, DARK[2], 2.4)
    for dx in (-2, 2):
        fill(p, rect(c + dx, round(sy) + 3, c + dx, round(sy) + 7), CLOUD[1], hood)
    gx, gy = MID - 1, hip - 3 + (pose.f % 2)
    fill(p, stroke([(MID - 7, sy + 1), (MID + 7, gy - 4)], 2.0), ASH, hood)  # the strap
    fill(p, stroke([(MID - 7, sy + 2), (MID + 7, gy - 3)], 1.0), STONE[1], hood)
    guitar(p, gx, gy, MID + 25, gy - 17)
    # the hand on its neck, the hand at its strings: or that one up
    sr, sl = (MID + half + 0.4, sy + 3), (MID - half - 0.4, sy + 3)
    arm(p, (sr, (sr[0] + 3.6, sr[1] + 6.5), (MID + 13.5, gy - 8.5)), 1, 4.8, BLACK, PALE, 2, DARK[2])
    if pose.left in STRUM:
        arm(p, (sl, (sl[0] - 3.4, sl[1] + 7), (gx - 3.5, gy + STRUM[pose.left])), -1, 4.8, BLACK, PALE, 2, DARK[2])
    else:
        arm(p, joints(*sl, -1, pose.left, 9.5), -1, 4.8, BLACK, PALE, 2, DARK[2])
    key = FACE | {"h": HAIR[2], "H": HAIR[3], "n": HAIR[1], "W": CLOUD[2], "G": CLOUD[0], "s": PALE[2], "d": PALE[1]}
    stamp(p, face(TEEN, pose, 12, 18), c - 10, top, key | {"B": HAIR[1], "b": BROWN})
    return p.outline()


# A peaked cap spotted with paint over wavy hair to the shoulders.
KID = """
........WWWWWWW........
......WWWW5WWWWWG......
.....WW3W45WWWWWGG.....
.....WWWW4WWWW33GG.....
.....WWWWWWWWW33GG.....
.WWWW2WWWWWWWWWWWGG....
..GGGGGGGGGGGGGGGG.....
..hhhhsssssssssssnhhh..
.hHhhsssssssssssssnhHh.
.hHhhsBBBsssssBBBsnhhh.
hhHhhsssssssssssssnhhhh
hHhhhsKKKsssssKKKsnhHhh
hHhhnsKbwsssssKbwsnhhHh
.hhhnsKbwsssssKbwsnhhh.
.hHhhspssssdsssspsnhHh.
hhHhhsssssssssssssnhhHh
hHhhnsssusssssusssnhhHh
.hhhhnssslllllssdnnhhh.
.hHhhhnsssssssssdnhhHh.
hhHhhhhnddddddddnhhhHhh
hHhhhhn.........nhhHhhh
.hhHhhn.........nhHhhh.
.hHhhn...........nhhHh.
..hhhn...........nhhh..
..nhn.............nhn..
"""


def little(pose: Pose) -> Pic:
    """The little one: brown eyed, wavy light brown hair to her shoulders under a white peaked cap spotted with
    paint; a pale zipped hoodie with ruffled shoulders, pale leggings spotted as a leopard, white shoes."""
    p = Pic(*STAGE)
    half, top, c = 7.0, FEET - 50 + pose.bob + pose.squat * 2, int(MID)
    sy, hip = top + 19, top + 34
    pale, pink = [FUR[3], FUR[4]], [CLOUD[1], PETAL[2]]
    trousers(p, hip, 3.0, pose.squat, pink, 5.6, 4.6)
    for x, y in ((-5, 0), (-2, 2), (-4, 5), (2, 0), (5, 2), (3, 5), (-2, -3), (4, -3), (-5, 8), (3, 9)):
        q = (c + x, round(hip + 3 + y))  # a leopard's spot, a paler dot beside it
        if p.get(*q) in pink and q[1] < FEET - 4:
            p.put(*q, DARK[1])
            if p.get(q[0] - 1, q[1]) in pink:
                p.put(q[0] - 1, q[1], DARK[3])
    sneakers(p, 3.2, CLOTH, FUR[1], rows=SMALL)
    arms = [(joints(MID + s * (half + 0.2), sy + 2.6, s, how, 7.2, s * pose.wave), s, how) for s, how in sides(pose)]
    for j, side, _ in (a for a in arms if a[2] in LOW):
        arm(p, j, side, 4.2, pale, PALE, 2, FUR[2])
    top_ = torso(p, sy, hip + 1, half, half + 0.6, pale, FUR[2], 1.4)
    fill(p, rect(c, round(sy) + 2, c, round(hip)), STONE[2], top_)  # the zip
    dots(p, GOLD[3], (c, round(sy) + 2))
    neck(p, sy, PALE, top_, FUR[3], 1.8)
    for j, side, _ in (a for a in arms if a[2] not in LOW):
        arm(p, j, side, 4.2, pale, PALE, 2, FUR[2])
    for side in (-1, 1):  # the ruffles on her shoulders
        for i in range(3):
            ruffle = oval(MID + side * (4.6 + i * 1.8), sy + 1.2 + i * 0.7, 1.6, 1.5)
            part(p, ruffle, [FUR[4], WHITE], sh=0.8, line=FUR[1])
    key = FACE | {"W": CLOUD[2], "G": CLOUD[1], "1": LIP, "2": GRASS[3], "3": PETAL[0], "4": TEAL[3], "5": GOLD[3]}
    key |= {"h": HAIR[2], "H": HAIR[3], "n": HAIR[1], "s": PALE[2], "d": PALE[1], "B": HAIR[1], "b": BROWN}
    stamp(p, face(KID, pose, 11, 17, deep=2), c - 11, top, key)
    return p.outline()


LEGS = {"down": (0.0, 6.0), "fwd": (-0.95, 6.2), "up": (-1.45, 5.2), "back": (0.95, 6.2), "tuck": (0.35, 3.2)}


def pomchi(pose: Pose) -> Pic:
    """The Pomchi: small, black and fluffy, a fox's face going grey at the muzzle, big pricked ears, a ruff, a white
    fleck on its chest, a plume of a tail curled over its back; stood side on, facing left. left and right: how
    its front and its back legs are held, each a lean from straight down and a length; wave: how far its nose is
    tipped up; squat: crouched; f: its tail's wag."""
    p = Pic(*STAGE)
    ground = FEET + 1.0
    fur, fluff, far = DARK[:3], DARK[1:], DARK[:2]
    wag = (0, 1, 0, -1)[pose.f % 4]
    by = ground - 9.6 + pose.squat * 2
    c, s = math.cos(pose.wave), math.sin(pose.wave)

    def at(dx: float, dy: float) -> Pt:
        """A place on its body, turned with it."""
        return (MID + dx * c - dy * s, by + dx * s + dy * c)

    def leg(hip: Pt, how: str, r: list[int]) -> None:
        a, n = (0.0, 4.0) if pose.squat and how == "down" else LEGS[how]
        y = hip[1] + math.cos(a) * n
        paw = (hip[0] + math.sin(a) * n, min(y, ground - 1.4) if how == "down" else y)
        part(p, stroke([hip, paw], 3.2, 2.6), r, sh=0.9, line=DARK[0])
        part(p, oval(paw[0] - 0.6, paw[1] + 0.2, 2.0, 1.4), r, sh=0.7, line=DARK[0])

    front = pose.left if pose.left in LEGS else "down"
    back = pose.right if pose.right in LEGS else "down"
    leg(at(-3.6, 2.4), front, far)  # the far pair
    leg(at(6.2, 2.4), back, far)
    rump = at(7.0, -2.6)
    for x, y, r in (
        (1.2, -1.6, 3.2),
        (0.6 + wag * 0.6, -5.4, 3.5),
        (-2.2 + wag, -7.4, 3.1),
        (-4.8 + wag * 1.4, -6.2, 2.4),
    ):
        part(p, oval(rump[0] + x, rump[1] + y, r, r), fluff, sh=1.2, hl=0.9)  # the plume
    part(p, oval(MID, by, 7.8, 5.0, pose.wave), fur, sh=1.6, hl=1.0, line=DARK[0])
    leg(at(-5.4, 3.0), front, DARK[1:3])  # the near pair
    leg(at(4.4, 3.0), back, DARK[1:3])
    for dx, dy, r in ((-5.2, -1.8, 3.7), (-6.4, 1.2, 3.1), (-3.4, 1.6, 2.6)):  # the ruff
        part(p, oval(*at(dx, dy), r, r), fluff, sh=1.0, hl=0.9)
    part(p, oval(*at(-7.4, 2.0), 1.3, 1.7), CLOTH, sh=0.7)
    hx, hy = at(-8.6, -7.2)
    hy += pose.bob
    flat = (1.2, 1) if pose.squat else (0, 0)  # its ears laid back as it crouches
    for ex, tip, r in ((-3.0, -3.2, far), (1.6, 2.2, fur)):  # the far ear, the near one
        ear = poly([(hx + ex - 2.0, hy - 3.2), (hx + ex + 2.4, hy - 3.6), (hx + tip + flat[0], hy - 10.2 + flat[1])])
        part(p, ear, r, sh=0.8, line=DARK[0])
    fill(p, poly([(hx + 0.6, hy - 4.0), (hx + 2.8, hy - 4.4), (hx + 2.2 + flat[0], hy - 8.2 + flat[1])]), STONE[1])
    head = oval(hx, hy, 5.4, 5.0) | oval(hx + 3.6, hy + 2.4, 2.6, 2.6)
    part(p, head, fur, sh=1.3, hl=1.0, line=DARK[0])
    part(p, oval(hx - 4.6, hy + 1.9, 3.0, 2.1), [DARK[3], ASH], sh=0.9, line=DARK[0])
    nx, ny = round(hx - 7.6), round(hy + 0.4)
    fill(p, rect(nx, ny, nx + 1, ny + 1), INK)
    ex, ey = round(hx - 2.6), round(hy - 2.0)
    fill(p, rect(ex - 1, ey - 1, ex + 2, ey + 2), DARK[2], head)
    if pose.blink:
        dots(p, INK, (ex, ey + 1), (ex + 1, ey + 1))
    else:
        fill(p, rect(ex, ey, ex + 1, ey + 1), INK)
        dots(p, WHITE, (ex, ey))
    dots(p, DARK[1], (nx + 2, ny + 3), (nx + 3, ny + 3), (nx + 4, ny + 3))
    if pose.joy:
        fill(p, rect(nx + 2, ny + 3, nx + 3, ny + 5), TONGUE[1])
        dots(p, TONGUE[2], (nx + 2, ny + 4))
    return p.outline()


NFAM, FAMF = 5, 9  # who meets Charlie at the level's end, and the pictures each has: the game's own counts
# Who meets Charlie at the level's end, left to right, and how far right of its place in the line each is drawn.
WHO: list[tuple[Callable[[Pose], Pic], int]] = [(pomchi, 4), (little, 1), (big, -5), (fair, 0), (bearded, 5)]
WAIT = [Pose(), Pose(bob=1), Pose(bob=1, blink=True)]
CHEER: dict[Callable[[Pose], Pic], list[Pose]] = {
    pomchi: [  # a hop: crouched, reared, stretched out, gathered, coming down, landed
        Pose(squat=1),
        Pose(left="up", wave=0.5, joy=True, lift=1),
        Pose(left="fwd", right="back", wave=0.3, joy=True, lift=6, f=1),
        Pose(left="tuck", right="tuck", joy=True, lift=9, f=2),
        Pose(left="fwd", right="back", wave=-0.25, joy=True, lift=5, f=3),
        Pose(squat=1, joy=True, f=1),
    ],
    little: [  # a jump, her arms thrown up
        Pose(squat=1, left="back", right="back"),
        Pose(left="half", right="half", joy=True, lift=3),
        Pose(left="up", right="up", joy=True, lift=7),
        Pose(left="high", right="high", joy=True, lift=9),
        Pose(left="up", right="up", joy=True, lift=6),
        Pose(squat=1, left="half", right="half", joy=True),
    ],
    big: [  # a few strokes of the strings, then a fist up
        Pose(left="s0", joy=True),
        Pose(left="s1", joy=True, bob=1, f=1),
        Pose(left="s2", joy=True),
        Pose(left="s1", joy=True, bob=1, f=1, blink=True),
        Pose(left="up", joy=True),
        Pose(left="high", joy=True, lift=1),
    ],
    fair: [  # a wave, a hand on her hip
        Pose(left="hip", right="out", wave=-2, joy=True),
        Pose(left="hip", right="out", wave=0, joy=True, bob=1),
        Pose(left="hip", right="out", wave=2, joy=True),
        Pose(left="hip", right="out", wave=3, joy=True, blink=True),
        Pose(left="hip", right="out", wave=2, joy=True, bob=1),
        Pose(left="hip", right="out", wave=0, joy=True),
    ],
    bearded: [  # both fists up and up again
        Pose(left="half", right="half", joy=True, bob=1),
        Pose(left="up", right="up", joy=True),
        Pose(left="high", right="high", joy=True, lift=1),
        Pose(left="high", right="high", joy=True, lift=2, blink=True),
        Pose(left="up", right="up", joy=True, lift=1),
        Pose(left="half", right="half", joy=True, bob=1),
    ],
}
WAITING: dict[Callable[[Pose], Pic], list[Pose]] = {
    pomchi: [Pose(), Pose(f=1, bob=1), Pose(f=3, bob=1, blink=True)],
    big: [Pose(left="s1"), Pose(left="s1", bob=1), Pose(left="s1", bob=1, blink=True)],
}


@functools.cache
def staged(who: Callable[[Pose], Pic]) -> list[Pic]:
    """who on the stage, frame by frame: three of the wait, six of the cheer, each as high as its jump has it."""
    out = []
    for pose in WAITING.get(who, WAIT) + CHEER[who]:
        src, p = who(pose), Pic(*STAGE)
        for y in range(pose.lift, src.h):
            p.p[y - pose.lift] = list(src.p[y])
        out.append(p)
    return out


@functools.cache
def family() -> list[list[Pic]]:
    """Who meets Charlie, each their own pictures, FAMF of them: the wait's, then the cheer's. One's are all one
    size: their foot the picture's last row, and their middle column its place in the line, the game told neither."""
    out = []
    for who, off in WHO:
        frames = staged(who)
        drawn = [(x, y) for p in frames for y in range(p.h) for x in range(p.w) if p.p[y][x]]
        place = int(MID) - off
        half = max(abs(x - place) for x, _ in drawn)
        top = min(y for _, y in drawn)
        cut = []
        for p in frames:
            q = Pic(2 * half + 1, FEET + 2 - top)
            q.p = [row[place - half : place + half + 1] for row in p.p[top : FEET + 2]]
            cut.append(q)
        out.append(cut)
    return out


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
    "U": (4, [[(0, 0), (0, 5), (1, 6), (3, 6), (4, 5), (4, 0)]]),
    "D": (4, [[(0, 0), (0, 6)], [(0, 0), (2.4, 0), (4, 1.5), (4, 4.5), (2.4, 6), (0, 6)]]),
    "E": (3.6, [[(3.6, 0), (0, 0), (0, 6), (3.6, 6)], [(0, 3), (2.8, 3)]]),
    "H": (4, [[(0, 0), (0, 6)], [(4, 0), (4, 6)], [(0, 3), (4, 3)]]),
    "I": (2.4, [[(1.2, 0), (1.2, 6)], [(0, 0), (2.4, 0)], [(0, 6), (2.4, 6)]]),
    "K": (4, [[(0, 0), (0, 6)], [(4, 0), (0, 3.6)], [(1.5, 2.5), (4, 6)]]),
    "L": (3.6, [[(0, 0), (0, 6), (3.6, 6)]]),
    "M": (5, [[(0, 6), (0, 0), (2.5, 3.6), (5, 0), (5, 6)]]),
    "N": (4, [[(0, 6), (0, 0), (4, 6), (4, 0)]]),
    "O": (4, [[(1, 0), (3, 0), (4, 1), (4, 5), (3, 6), (1, 6), (0, 5), (0, 1), (1, 0)]]),
    "P": (4, [[(0, 0), (0, 6)], [(0, 0), (2.4, 0), (3.8, 0.8), (3.8, 2.4), (2.4, 3.2), (0, 3.2)]]),
    "Q": (4, [[(1, 0), (3, 0), (4, 1), (4, 5), (3, 6), (1, 6), (0, 5), (0, 1), (1, 0)], [(2.5, 4.2), (4.2, 6.3)]]),
    "R": (4, [[(0, 0), (0, 6)], [(0, 0), (2.4, 0), (3.8, 0.8), (3.8, 2.4), (2.4, 3.2), (0, 3.2)], [(2, 3.2), (4, 6)]]),
    "S": (
        4,
        [[(4, 1.1), (3, 0), (1, 0), (0, 1), (0, 2.2), (1, 3), (3, 3), (4, 3.8), (4, 5), (3, 6), (1, 6), (0, 4.9)]],
    ),
    "T": (4, [[(0, 0), (4, 0)], [(2, 0), (2, 6)]]),
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


def goldbone(turn: float) -> Pic:
    """A golden bone, turned: a bone in gold, a glint on its upper knob."""
    p = Pic(10, 10)
    c, s = math.cos(turn), math.sin(turn)
    p.line(4.5 - c * 2.4, 4.5 - s * 2.4, 4.5 + c * 2.4, 4.5 + s * 2.4, GOLD[3], 2)
    for end in (-2.6, 2.6):
        for side in (-1.2, 1.2):
            p.ball(4.5 + c * end - s * side, 4.5 + s * end + c * side, 1.3, 1.3, GOLD[1:], 0.5, 1.2)
    top = min((y, x) for y in range(10) for x in range(10) if p.p[y][x])
    p.put(top[1], top[0], WHITE)
    return p.outline()


CATFUR = EARTH[2:]


def startled(fur: list[int] = CATFUR) -> Pic:
    """The cat barked at: its back arched, its legs stiff, its tail a brush, an eye wide."""
    p = Pic(20, 14)
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


def cat(f: int, fur: list[int] = CATFUR) -> Pic:
    """A cat, ginger unless told, prowling right: its tail up, its back striped."""
    p = Pic(20, 14)
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


def tones(first: int, *hexes: str, stage: int = -1) -> list[int]:
    """Palette entries from first, a colour each; their indices. stage: only that stage's, over the shared ones:
    a stage is a zone at an hour, the zone's first act even and its second odd."""
    for i, h in enumerate(hexes):
        rgb = (int(h[1:3], 16) * 63 // 255, int(h[3:5], 16) * 63 // 255, int(h[5:7], 16) * 63 // 255)
        (PAL if stage < 0 else ZPAL[stage])[first + i] = rgb
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


def cloud(p: Pic, cx: int, cy: int, kind: str, r: list[int] | None = None) -> None:
    """A cloud of puffs on a flat base: white where the sun reaches, a tone where the cloud itself is in the way.
    r: its three tones, the Downs' unless given."""
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
                p.put(cx + x + dx, cy + y, (r or PUFF)[t])


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


class Skin(NamedTuple):
    """What a zone's earth is made of, repeating every 64 across and 48 down: its colour at a world pixel d under
    the surface, the surface's column hem; its colour far under it; its outline; on a bare side the crust's end,
    the body's lit side and its dark one."""

    soil: Callable[[int, int, int, int], int]
    far: Callable[[int, int], int]
    ink: int
    edge: tuple[int, int, int]


TURFED = Skin(soil, lambda x, y: AMBER[comb(x, y)], PLUM, (TURF[5], AMBER[1], AMBER[5]))


def top(kind: int, col: int, row: int, under: int = 0, skin: Skin = TURFED) -> Pic:
    """A surface tile of the earth's col-th column of 4 and row-th of 3; under: the tile below a slope's."""
    p = Pic(16, 16)
    for x in range(16):
        s = surface(kind, x) - under * 16
        for y in range(max(s, 0), 16):
            p.put(x, y, skin.soil(col * 16 + x, row * 16 + under * 16 + y, y - s, col * 16 + x))
    return p


def deep(col: int, row: int, h: int, skin: Skin = TURFED) -> Pic:
    """The earth far under its surface."""
    p = Pic(16, h)
    for x in range(16):
        for y in range(h):
            p.put(x, y, skin.far(col * 16 + x, row * 16 + y))
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


def bare(right: bool, body: bool, skin: Skin = TURFED) -> Pic:
    """A tile of the ground's bare side: the crust's end, then the body's, lit on the left and dark on the right."""
    p = Pic(16, 16)
    x, into = (15, -1) if right else (0, 1)
    for y in range(16):
        p.put(x, y, skin.ink if body or y > 9 else skin.edge[0])
        if body or y > 12:
            p.put(x + into, y, skin.edge[2 if right else 1])
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


def pipe(col: int, row: int, skin: Skin = TURFED) -> Pic:
    """A quarter pipe and the row of ground under it, the earth's col-th column and row-th row at its top left: the
    crust follows the curve, from the floor's on its left up to the wall's top."""
    p = Pic(48, 64)
    for y in range(64):
        for x in range(48):
            r = math.hypot(x + 0.5, y + 0.5)
            if y >= 48 or r >= 48.5:
                hem = col * 16 + int(math.atan2(x + 0.5, y + 0.5) * 48.5)
                p.put(x, y, skin.soil(col * 16 + x, row * 16 + y, int(r - 48.5), hem))
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


def tree(bark: list[int] = BARK, crown: list[int] = CROWN) -> Pic:
    """A tree: bark's four tones, light to dark, and a crown of leaf clumps in crown's six."""
    p = Pic(64, 80)
    for y in range(40, 80):  # the trunk, flaring to its roots
        half = 4 + (y > 70) + 2 * (y > 75) + (y > 77)
        for x in range(-half, half + 1):
            t = 0 if x <= -half + 1 else 1 if x < 1 else 2
            if -half + 1 < x < half and (x + y // 5) % 4 == 0 and y % 5:
                t += 1  # the bark's furrows
            p.put(32 + x, y, bark[min(t + 1, 2) if y < 47 else t])  # darker in the crown's shadow
    back = [(44, 34, 10, 3), (32, 38, 9, 3), (20, 35, 9, 2), (50, 24, 9, 2), (38, 28, 11, 2), (14, 26, 9, 1)]
    clumps(p, [*back, (26, 26, 11, 1), (42, 15, 10, 1), (30, 13, 11, 0), (19, 16, 9, 0), (34, 22, 8, 1)], crown)
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


# ---- the intro's: the van and the old house, the Mob's float and the Baron, what falls off ----

CREAM = FUR[2:]
CARTON = [EARTH[1], EARTH[2], EARTH[4], EARTH[6]]


def slab(p: Pic, x0: int, y0: int, x1: int, y1: int, r: list[int]) -> None:
    """A box side on, of r's four tones, dark to lit: its face, a lit top edge, a shaded foot and right side."""
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            p.put(x, y, r[3] if y - y0 < 2 else r[1] if y1 - y < 2 or x1 - x < 2 else r[2])


def tyre(turn: int) -> Pic:
    """A wheel of the van's or the float's, 18 across, its three spokes a sixth of a turn on at each `turn`."""
    p = Pic(18, 18)
    for y in range(18):
        for x in range(18):
            dx, dy = x - 8.5, y - 8.5
            d = math.hypot(dx, dy)
            if d > 8.6:
                continue
            spoke = (math.degrees(math.atan2(dy, dx)) + turn * 60) % 120 < 34
            p.put(x, y, INK if d > 5.4 else STONE[3] if d < 1.8 else STONE[1] if spoke else STONE[2])
    return p


def van() -> Pic:
    """The family's van, bound right, less its wheels: a cream box, a red band along its foot, its roof piled with
    boxes roped down, a rolled rug and a lamp among them; its back door at the left."""
    p = Pic(140, 80)
    slab(p, 16, 10, 40, 27, WOOD)
    slab(p, 42, 14, 66, 27, CARTON)
    slab(p, 68, 6, 90, 27, WOOD)
    slab(p, 92, 16, 110, 27, CARTON)
    for x in range(20, 64):  # the rug, rolled
        for y in range(4, 10):
            p.put(x, y, SCARF[1] if y > 7 else SCARF[2] if (x // 4) % 2 else SCARF[3])
    for x, y0 in ((28, 10), (54, 14), (79, 6), (101, 16)):  # the ropes
        p.line(x, y0, x, 27, FUR[1])
    for y in range(8):  # the lamp's shade
        for x in range(96 - y // 2, 105 + y // 2):
            p.put(x, y + 8, GOLD[2] if y < 6 else GOLD[1])
    p.line(100, 4, 100, 8, STONE[1])
    p.box(12, 26, 114, 28, STONE[1])  # the rack
    for y in range(29, 70):  # the body, its nose stepped down
        for x in range(4, 136):
            if x > 116 and y < 47 - (136 - x) // 3:
                continue
            if (x < 6 or x > 133) and (y < 31 or y > 67):
                continue
            t = CREAM[3] if y < 31 else SCARF[1] if y > 66 else SCARF[2] if y > 56 else CREAM[2]
            p.put(x, y, CREAM[1] if 54 <= y <= 56 else t)
    for y in range(34, 50):  # the cab's window, slanted, then two along the side
        for x in range(118, 131):
            if x - 118 < (y - 34) * 0.9 + 9:
                p.put(x, y, TEAL[4] if y < 37 else TEAL[3])
    for x0 in (56, 86):
        for y in range(34, 50):
            for x in range(x0, x0 + 24):
                p.put(x, y, TEAL[4] if (x - x0) + (y - 34) < 9 else TEAL[3])
        p.box(x0 - 1, 33, x0 + 24, 33, CREAM[1])
        p.box(x0 - 1, 50, x0 + 24, 50, CREAM[1])
    for x, y0, y1 in ((8, 33, 64), (9, 33, 64), (22, 33, 64), (50, 33, 64), (114, 46, 64)):  # the doors' seams
        p.box(x, y0, x, y1, CREAM[1])
    p.box(11, 47, 13, 49, STONE[1])
    p.box(108, 52, 111, 53, STONE[1])
    p.box(132, 52, 135, 57, GOLD[4])  # its lamps and bumpers
    p.box(4, 50, 5, 56, SCARF[3])
    p.box(2, 66, 12, 69, STONE[2])
    p.box(128, 66, 138, 69, STONE[2])
    for cx in (32, 108):  # the arches
        for y in range(58, 70):
            for x in range(cx - 11, cx + 12):
                if math.hypot(x - cx, y - 70) < 11.5:
                    p.put(x, y, INK)
    return p.outline()


def door() -> Pic:
    """The van's back door swung open, edge on."""
    p = Pic(12, 36)
    for y in range(34):
        for x in range(9 - y // 12):
            p.put(x + 1, y + 1, CREAM[3] if x == 0 else CREAM[1] if x > 5 else CREAM[2])
    p.box(3, 15, 5, 17, STONE[1])
    return p.outline()


def peek() -> Pic:
    """A window of the van's, open, and Charlie leaning from it into the wind: his chest on its sill, his ears
    flying, his tongue out. The window's 24 by 16 with 3 over at its sides, 2 over and under."""
    dog = Pic(30, 20)
    dog.ball(13, 19, 7, 7.5, FUR, 0.3, 1.1)
    snout(dog, 18, 9, 1.5, "happy", True)
    dog.outline()
    p = Pic(30, 20)
    p.box(3, 2, 26, 17, DARK[1])
    p.box(3, 2, 26, 2, DARK[0])
    for y in range(18):  # under the sill he is inside
        for x in range(30):
            if dog.p[y][x]:
                p.put(x, y, dog.p[y][x])
    return p


def boxes() -> Pic:
    """The last of the boxes by the gate: three, a plant in the top one."""
    p = Pic(44, 40)
    slab(p, 2, 22, 22, 38, WOOD)
    slab(p, 23, 18, 41, 38, CARTON)
    slab(p, 10, 8, 30, 21, WOOD)
    p.box(11, 14, 29, 14, FUR[1])
    p.box(3, 29, 21, 29, FUR[1])
    for dx, dy in ((0, 0), (-3, -3), (3, -2), (-1, -6), (2, -5)):
        p.ball(20 + dx, 7 + dy, 2.6, 2.6, GRASS, 0.3, 1.1)
    return p.outline()


def crate() -> Pic:
    """One box, thrown."""
    p = Pic(20, 16)
    slab(p, 1, 1, 18, 14, CARTON)
    p.box(2, 7, 17, 7, FUR[1])
    return p.outline()


def basket() -> Pic:
    """The picnic basket: wicker, a checked cloth out of its side, a hooped handle."""
    p = Pic(18, 15)
    for x in range(4, 14):
        p.put(x, 4 - round(3.5 * math.sin((x - 4) / 9 * math.pi)), WOOD[2])
    for y in range(6, 14):
        for x in range(2 + (y > 11), 16 - (y > 11)):
            p.put(x, y, WOOD[3] if y == 6 else WOOD[1] if (x + y // 2) % 3 == 0 else WOOD[2])
    for y in range(5, 10):
        for x in range(9, 15):
            if x - 9 >= (y - 5) - 1:
                p.put(x, y, SCARF[2] if (x + y) % 2 else FUR[5])
    return p.outline()


GINGER, FARSIDE, BIB = [EARTH[4], EARTH[6], EARTH[7]], [EARTH[3], EARTH[5], EARTH[6]], [CLOUD[1], CLOUD[2]]
SEAM, TABBY = EARTH[2], EARTH[3]  # a line where ginger lies on ginger; a stripe
GLASS = ((25, 13), (26, 13), (27, 13), (24, 14), (28, 14), (23, 15), (29, 15), (23, 16), (29, 16), (23, 17), (29, 17))
GLASS += ((24, 18), (28, 18), (25, 19), (26, 19), (27, 19))  # the ring of his eyeglass


def baron(point: bool = False, f: int = 0, blink: bool = False) -> Pic:
    """Baron Marmalade: a great ginger tabby sat as a cat sits, cream fronted and socked, in a top hat and a bow
    tie, a glass at one eye on its chain; his tail thin, curled up beside him and tipped with cream, f its sway.
    point: a paw out at what he wants."""
    p = Pic(40, 44)
    sway = (0, 1, 2, 1)[f % 4]
    curl = [(11, 41), (6, 41), (2.8 - sway * 0.3, 37.5), (3.2 + sway * 0.4, 32), (6 + sway, 28.2)]
    path = spline([*curl, (8.5 + sway * 1.5, 29.5)], 8, closed=False)
    tail = part(p, stroke(path, 3.6, 2.6), FARSIDE, sh=1.0)
    for i in range(6, len(path) - 8, 7):
        fill(p, stroke(path[i : i + 2], 3.4), TABBY, tail)
    fill(p, stroke(path[-5:], 2.8, 2.6), CLOUD[2], tail)
    fill(p, stroke(path[-2:], 2.6), CLOUD[1], tail)
    # the body, a pear; a haunch a side, its paw out in front
    left = [(15, 21.5), (11.6, 27), (9.2, 33), (8.8, 39), (11.5, 43.4)]
    body = curved([*left, *((41 - x, y) for x, y in left[::-1])])
    part(p, body, GINGER, sh=2.0, hl=1.0, line=SEAM)
    for x, y in ((10.8, 28.2), (9.8, 31.6)):  # his sides' stripes
        fill(p, stroke([(x, y), (x + 2.6, y + 0.8)], 1.4), TABBY, body)
        fill(p, stroke([(41 - x, y), (41 - x - 2.2, y + 0.8)], 1.4), TABBY, body)
    part(p, oval(12.6, 38.4, 4.3, 4.8), FARSIDE, sh=1.4, line=SEAM, within=body)
    part(p, oval(28.4, 38.4, 4.5, 4.8), GINGER, sh=1.6, hl=1.0, line=SEAM, within=body)
    for cx in (11.8, 29.2):  # hind paws, socked in cream
        part(p, oval(cx, 42.4, 3.4, 2.0), BIB, sh=0.8, line=SEAM)
    # his front legs, together between his haunches, socked too; or one out
    part(p, stroke([(18.9, 32), (19.0, 40.6)], 3.8), FARSIDE, sh=0.8, line=SEAM)
    part(p, oval(18.8, 42.4, 2.6, 2.0), BIB, sh=0.8, line=SEAM)
    if not point:
        part(p, stroke([(23.1, 32), (23.0, 40.6)], 3.8), GINGER, sh=0.8, hl=0.8, line=SEAM)
        part(p, oval(23.2, 42.4, 2.6, 2.0), BIB, sh=0.8, line=SEAM)
    chest = [(17.5, 22.5), (15.2, 27), (15, 31.5), (17.4, 35), (21, 36.6), (24.6, 35), (27, 31.5), (26.8, 27)]
    part(p, curved([*chest, (24.5, 22.5)]), BIB, sh=1.6, line=SEAM)  # cream, over where they start
    if point:
        part(p, stroke([(26.4, 27.6), (31, 26.6), (35.6, 23)], 4.8, 4.2), GINGER, sh=1.2, hl=0.8, line=SEAM)
        part(p, oval(36.4, 22.4, 2.8, 2.5), GINGER, sh=1.0, hl=0.8, line=SEAM)
    for side in (-1, 1):  # ears, behind the head
        cx = 20.5 + side * 8.2
        part(p, poly([(cx - side * 3.6, 10.5), (cx + side * 2.4, 15), (cx + side * 2.6, 5.2)]), GINGER, 0.8, line=SEAM)
        fill(p, poly([(cx - side * 1.2, 10.8), (cx + side * 1.6, 13), (cx + side * 1.8, 7.4)]), BLUSH)
    # the head, its cheeks tufted
    left = [(20.5, 9.2), (13.5, 10.8), (10.6, 15.5), (9.4, 19.4), (11.6, 20), (10.6, 22), (14.5, 23), (20.5, 24.2)]
    head = curved([*left, *((41 - x, y) for x, y in left[-2:0:-1])], 8)
    part(p, head, GINGER, sh=1.6, hl=1.0, line=SEAM)
    for x in (19, 21, 23):
        fill(p, rect(x, 11, x, 13 - (x != 21)), TABBY, head)
    for side in (-1, 1):
        for y, out in ((17, 8.6), (19, 9.2)):
            x = 20.5 + side * out
            fill(p, stroke([(x, y + 0.5), (x - side * 1.8, y + 0.5)], 1.0), TABBY, head)
    part(p, oval(22, 20.1, 4.8, 3.0), BIB, sh=1.2, within=head)
    fill(p, rect(21, 18, 23, 18), TONGUE[1])
    dots(p, TONGUE[1], (22, 19))
    dots(p, SEAM, (22, 20), (21, 21), (20, 21), (23, 21), (24, 21), (19, 20), (25, 20))
    for ex in (16, 25):  # eyes, half shut: a lid, a green iris, a slit
        fill(p, rect(ex, 15, ex + 2, 15), INK)
        if not blink:
            fill(p, rect(ex, 16, ex + 2, 17), GRASS[4])
            fill(p, rect(ex + 1, 16, ex + 1, 17), INK)
            dots(p, GRASS[2], (ex, 17), (ex + 2, 17))
    for side in (-1, 1):  # the bow tie
        wing = poly([(21.5 + side * 4.4, 22.2), (21.5 + side * 4.4, 26.8), (21.5 + side * 0.4, 24.5)])
        part(p, wing, SCARF[1:3], sh=1.0, line=INK)
    fill(p, rect(21, 24, 22, 25), SCARF[3])
    # the top hat: silk, a red band, a curled brim
    crown = poly([(14.6, 0.6), (27.4, 0.6), (26.4, 9), (15.6, 9)])
    part(p, crown, DARK[:3], sh=2.2, line=INK)
    fill(p, rect(17, 2, 18, 5) | rect(15, 1, 26, 1), DARK[2])
    fill(p, rect(15, 6, 26, 8), SCARF[2], crown)
    fill(p, rect(15, 8, 26, 8) | rect(24, 6, 26, 7), SCARF[1], crown)
    fill(p, rect(17, 6, 18, 6), SCARF[3], crown)
    fill(p, rect(11, 9, 30, 10) | {(10, 9), (31, 9), (10, 8), (31, 8)}, DARK[1])
    fill(p, rect(11, 10, 30, 10), DARK[0])
    fill(p, rect(12, 9, 19, 9), DARK[2])
    p.outline()
    # over his outline: his glass and its chain, his whiskers
    dots(p, GOLD[3], *GLASS)
    dots(p, GOLD[4], (25, 13), (24, 14), (23, 15))
    dots(p, WHITE, (25, 14))
    for i, at in enumerate(((28, 19), (29, 20), (29, 21), (29, 22), (28, 23), (27, 24), (26, 25))):
        dots(p, GOLD[3] if i % 2 else GOLD[2], at)
    for side in (-1, 1):
        for i in range(1, 5):
            dots(p, WHITE, (22 + side * (5 + i), 20 if i < 3 else 19), (22 + side * (5 + i), 22 if i < 3 else 23))
    return p


def tail_clear(p: Pic) -> int:
    """How many rows on end the Baron's tail, at the picture's left, stands clear of his body."""
    best = run = 0
    for row in p.p:
        x = next((i for i, c in enumerate(row) if c), len(row))
        clear = x < 8 and any(row[i] and not row[i - 1] for i in range(x + 1, 20))
        run = run + 1 if clear else 0
        best = max(best, run)
    return best


def barons() -> list[Pic]:
    """The Baron's pictures: three of his tail's sway, a blink, two of him pointing."""
    return [baron(f=0), baron(f=1), baron(f=2), baron(f=1, blink=True), baron(True, 0), baron(True, 2)]


def milkfloat() -> Pic:
    """The Mob's ride, a milk float bound right, less its wheels: a flat cart, crates of bottles at its back, a
    bench at its front, a lamp on its nose."""
    p = Pic(88, 40)
    for x0 in (8, 23):  # the crates, a row of bottles in each
        slab(p, x0, 9, x0 + 14, 18, WOOD)
        for x in range(x0 + 2, x0 + 14, 4):
            p.box(x, 4, x + 1, 8, FUR[5])
            p.box(x, 3, x + 1, 3, TEAL[3])
    slab(p, 60, 11, 76, 18, STONE)  # the bench
    p.box(74, 4, 76, 10, STONE[2])
    slab(p, 4, 19, 84, 29, [TEAL[0], TEAL[1], TEAL[2], TEAL[3]])
    p.box(84, 22, 86, 25, GOLD[4])
    for cx in (20, 68):  # the arches
        for y in range(24, 30):
            for x in range(cx - 10, cx + 11):
                if math.hypot(x - cx, y - 30) < 10.5:
                    p.put(x, y, INK)
    return p.outline()


def hedge() -> Pic:
    """The hedge the Mob waits in: deep, clipped flat along its foot."""
    p = Pic(160, 52)
    rows = ((14, 10, 30, 7, 5, 12, 2), (13, 16, 20, 5, 6, 11, 1), (12, 22, 12, 3, 5, 9, 0))
    clumps(
        p,
        [(x0 + i * 11, y + (i * m) % d, rad, lv) for n, x0, y, m, d, rad, lv in rows for i in range(n)],
        SHRUB,
        flat=50,
    )
    return p


def house() -> Pic:
    """The old house's front, left behind: a plastered wall under thatch, a door stood open on an empty hall, a
    window with a box of flowers."""
    p = Pic(132, 124)
    for y in range(34, 122):
        for x in range(6, 126):
            p.put(x, y, AMBER[0] if x < 9 else AMBER[2] if x > 122 or y > 118 else AMBER[1])
    for y in range(4, 38):  # the thatch, in courses
        half = round(66 - (38 - y) * 0.2)
        for x in range(max(66 - half - 4, 0), min(66 + half + 5, 132)):
            under = (y - 4) % 8 > 5
            p.put(x, y, BARK[0] if y < 7 else BARK[3] if under and (x + y) % 3 == 0 else BARK[2] if under else BARK[1])
    for x in range(0, 132, 5):
        p.box(x, 38, x + 2, 39, BARK[2])
    slab(p, 92, 0, 104, 10, BOULDER[3::-1])  # the chimney
    p.box(22, 62, 50, 121, BARK[3])  # the door, open: the dark of the hall, the door's edge
    p.box(24, 64, 48, 121, PLUM)
    p.box(44, 66, 48, 121, BARK[1])
    p.box(20, 60, 52, 61, BARK[2])
    p.box(18, 120, 54, 122, BOULDER[1])
    p.box(72, 62, 108, 92, BARK[3])  # the window, its panes and its box
    for x0, y0 in ((74, 64), (91, 64), (74, 79), (91, 79)):
        for y in range(y0, y0 + 13):
            for x in range(x0, x0 + 15):
                p.put(x, y, TEAL[4] if (x - x0) + (y - y0) < 8 else TEAL[2])
    slab(p, 70, 93, 110, 99, BARK[::-1])
    for x in range(73, 108, 5):
        p.ball(x, 91, 2.2, 2.2, BLOOM[1::-1] if (x // 5) % 2 else BLOOM[:1:-1], 0.2, 1.0)
        p.put(x, 94, SHRUB[2])
    return p.outline()


def cart(hit: bool, lean: int) -> Pic:
    """Biscuit's honey cart, bound right, less its wheels: planks, a pot of honey slung behind, a pennant, the picnic
    basket aboard and Biscuit at its front; hit: bumped, the cat off its feet. lean: its front wheel that many
    sevenths of a gentle slope's fall under its back one: each column slid down its length about its middle, the
    picture 7 higher over and under a seventh."""
    body = Pic(56, 34)
    body.box(27, 2, 27, 13, STONE[1])  # the pennant on its pole
    for y in range(5):
        for x in range(5 - y):
            body.put(28 + x, 2 + y, SCARF[2])
    paste(body, basket(), 8, 10)
    paste(body, startled() if hit else cat(0), 31, 6 if hit else 11)
    slab(body, 6, 25, 50, 32, WOOD)
    for x in (16, 28, 40):
        body.box(x, 27, x, 30, WOOD[1])
    body.line(6, 26, 1, 21, WOOD[2])  # the pot on its hook, a drip from it
    body.ball(3, 17, 4.2, 4.6, GOLD, 0.2, 1.1)
    body.box(1, 12, 5, 13, EARTH[2])
    body.put(4, 22, GOLD[3])
    p = Pic(56, 34 + 14 * lean)
    for y in range(34):
        for x in range(56):
            if body.p[y][x]:
                p.put(x, y + 7 * lean + round((x - 28) * lean / 4), body.p[y][x])
    return p.outline()


def cartwheel(f: int) -> Pic:
    """A wheel of the cart's, 13 across, its spokes an eighth of a turn on at f."""
    p = Pic(13, 13)
    for y in range(13):
        for x in range(13):
            d = math.hypot(x - 6, y - 6)
            if d <= 6.3:
                spoke = (math.degrees(math.atan2(y - 6, x - 6)) + f * 45) % 90 < 30
                p.put(x, y, INK if d > 4.2 else STONE[3] if d < 1.5 else STONE[1] if spoke else STONE[2])
    return p


def intro(k: int) -> list[tuple[int, Pic, bool, int]]:
    """The intro's pictures from index k: the Downs' own, as its road is theirs, but the van's: it bumps again on
    every zone's road."""
    OWN[0].update(range(k + 12, k + 28))
    return [
        (k, van(), True, -1),
        (k + 2, tyre(0), True, -1),
        (k + 4, tyre(1), True, -1),
        (k + 6, door(), True, -1),
        (k + 8, peek(), True, k + 10),
        (k + 12, house(), True, -1),
        (k + 14, boxes(), True, -1),
        (k + 16, crate(), True, -1),
        (k + 18, hedge(), True, -1),
        (k + 20, milkfloat(), True, -1),
        (k + 26, basket(), True, -1),
    ]


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
DUSK = tones(96, "#241858", "#3C2070", "#5C2C88", "#84389C", "#B048A4", "#D8609C", "#F08088", "#FFA878", stage=2)
DUSK += tones(104, "#FFD488", stage=2)
STAR = tones(105, "#FFF0FF", "#B894D8", stage=2)
WISP = tones(107, "#FFC8A0", "#E888A8", "#A85CA8", stage=2)  # a cloud's lit underside, its body, its top
TOWN = tones(110, "#B060A0", "#94488C", "#D888B0", stage=2)  # the far city: a face, its shade, its lit edge
CITY = tones(113, "#4C2C78", "#382060", "#6C4498", "#241448", stage=2)  # the near: and the dark under it
PANE = tones(117, "#FFE890", "#FFB060", "#7C54A8", stage=2)  # a window lit, warm, dark
WORKS = tones(120, "#0C1438", "#142454", "#1E3C74", "#2C5C98", "#58A4CC", "#A8E4F0", stage=2)
LAMP = tones(126, "#FFE478", "#F04858", "#F0E8F8", stage=2)
DECK = tones(129, "#F4FCFF", "#C4D8EC", "#8CA4CC", "#5C7098", "#343C68", "#1C1838", stage=2)
HAZARD = tones(135, "#FFE478", "#FFC428", "#C88818", stage=2)
SUNSET = tones(141, "#FFFCD8", "#FFE890", stage=2)

# each zone again at another hour: the Amber Downs at evening, the works by night, their pictures recoloured
tones(96, "#3A1C6C", "#5C2880", "#883490", "#B84890", "#E06080", "#F88468", "#FFAC60", "#FFD070", "#FFF0A0", stage=1)
tones(105, "#FFE8E0", "#E8B8D8", "#C898CC", "#A87CBC", "#8C64A8", "#A074B8", "#8860A8", "#704C98", "#5C3C84", stage=1)
tones(114, "#4C3074", "#7C6CA8", "#644C98", "#503C84", stage=1)
tones(118, "#2C4C74", "#38648C", "#4C7C98", "#6898A0", "#90B4A4", "#C0D0A8", "#203860", "#FFD0C0", "#E890B0", stage=1)
tones(128, "#C8589C", "#FFD0B0", "#FFE8A0", "#FFC050", "#F09028", "#D06420", "#A84024", "#782830", "#501C34", stage=1)
tones(141, "#FFF4B0", "#FFD870", "#FFB850", "#7C3C98", "#F08090", "#FFC090", "#FFF0C0", stage=1)
tones(225, "#E8F880", "#A0DC40", "#58B830", "#2C9034", "#186840", "#104444", stage=1)
ZPAL[3].update(ZPAL[2])
tones(96, "#0C1030", "#101840", "#182454", "#20306C", "#284084", "#30549C", "#3C6CB0", "#5088C4", "#70A8D8", stage=3)
tones(107, "#8098D0", "#5870B0", "#405090", "#3C4C8C", "#2C3870", "#5870B0", stage=3)
tones(113, "#1C2458", "#141A44", "#2C3C78", "#0C1030", stage=3)
tones(141, "#F4F8FF", "#C8D8F0", stage=3)


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


STEELBOX = [CHROME[0], CHROME[1], CHROME[2], CHROME[4]]
BLUEBOX = [PANEL[0], PANEL[1], PANEL[3], PANEL[5]]


def stripes(p: Pic, x0: int, y0: int, x1: int, y1: int) -> None:
    """A hazard band: yellow and dark, slanted."""
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            p.put(x, y, HAZARD[1] if (x + y) // 3 % 2 else DARK[0])


def gondola(hit: bool) -> Pic:
    """Sprocket's crane, 44 by 46: a trolley clamped on the rail, two cables, the gondola slung from them with the
    cat at its rail in a hard hat, a lamp at its bow and a hook under; hit: bumped, the cat off its feet, its hat
    off its head and the lamp out."""
    p = Pic(44, 46)
    slab(p, 8, 1, 35, 10, STEELBOX)
    stripes(p, 10, 5, 33, 7)
    for x in (12, 31):  # a wheel each end, on the rail's top
        p.ball(x, 2.5, 2.6, 2.6, METAL, 0.3, 1.0)
    for x in (14, 29):
        p.box(x, 11, x, 20, CHROME[3])
    p.box(8, 20, 35, 21, CHROME[3])  # the gondola's frame: a bar and a post each end
    for x in (9, 34):
        p.box(x, 22, x, 30, CHROME[2])
    paste(p, startled() if hit else cat(0), 12, 16 if hit else 19)
    hy = 13 if hit else 20  # its hard hat: on its head, or knocked off it
    p.box(25, hy, 29, hy + 1, HAZARD[1])
    p.box(26, hy, 27, hy, HAZARD[0])
    p.box(24, hy + 2, 31, hy + 2, HAZARD[2])
    slab(p, 6, 30, 37, 41, BLUEBOX)
    stripes(p, 8, 35, 35, 37)
    p.box(38, 32, 39, 33, DARK[1] if hit else LAMP[0])
    p.box(21, 42, 22, 44, CHROME[3])  # the hook
    p.put(20, 44, CHROME[3])
    return p.outline()


def girder() -> Pic:
    """A length of the crane's rail, 32 by 8: two flanges and a lattice between, open."""
    p = Pic(32, 8)
    p.box(0, 0, 31, 0, CHROME[4])
    p.box(0, 1, 31, 1, CHROME[2])
    p.box(0, 6, 31, 6, CHROME[2])
    p.box(0, 7, 31, 7, CHROME[0])
    for x in range(32):
        d = x % 8
        p.put(x, 2 + min(d, 7 - d), CHROME[1])
    return p


def woodbox() -> Pic:
    """A crate of the works, 16 square: planks, braced corner to corner, steel at its corners."""
    p = Pic(16, 16)
    slab(p, 1, 1, 14, 14, WOOD)
    for i in range(3, 13):
        p.put(i, i, WOOD[0])
        p.put(15 - i, i, WOOD[0])
    for x, y in ((1, 1), (13, 1), (1, 13), (13, 13)):
        p.box(x, y, x + 1, y + 1, CHROME[3])
    return p.outline()


def sweeper(f: int) -> Pic:
    """A floor polisher going left, its brush a sixteenth of a turn on at f: an armoured hood over the brush, a
    housing behind it with a cap on top to land on, a handle."""
    p = Pic(30, 20)
    p.ball(7, 15.0, 6.4, 4.6, COPPER[1:], 0.2, 0.8)
    for i in range(8):
        a = i * math.pi / 4 + f * math.pi / 8
        p.put(round(7 + 4.4 * math.cos(a)), round(15.0 + 3.0 * math.sin(a)), COPPER[4])
        p.put(round(7 + 2.2 * math.cos(a + 0.4)), round(15.0 + 1.5 * math.sin(a + 0.4)), COPPER[0])
    p.box(11, 7, 26, 15, PANEL[2])
    p.box(11, 7, 26, 7, PANEL[4])
    p.box(11, 14, 26, 15, PANEL[1])
    for y in (10, 12):
        p.box(19, y, 24, y, PANEL[0])
    p.ball(23.5, 16.5, 2.8, 2.8, CHROME, 0.1, 0.8)
    for y in range(3, 13):
        for x in range(15):
            dx, dy = (x - 9) / 9.0, (y - 14) / 11.0
            if dx * dx + dy * dy <= 1 and x <= 13:
                p.shade(x, y, CHROME[1:5], 0.3 - 0.45 * dx - 0.5 * dy)
    p.box(1, 12, 13, 12, CHROME[1])
    for i in range(3):
        p.box(3 + i * 3, 10, 4 + i * 3, 11, HAZARD[1])
    p.box(15, 9, 16, 10, LAMP[1])
    p.put(15, 9, WHITE)
    p.box(17, 5, 23, 6, HAZARD[1])
    p.box(17, 5, 23, 5, HAZARD[0])
    p.line(25, 7, 28, 2, CHROME[3], 1.5)
    p.ball(28, 1.6, 1.6, 1.6, CHROME, 0.4, 1.0)
    return p.outline()


def sparker(lit: bool, f: int = 0) -> Pic:
    """A spark's carriage on its floor rail: an insulator, dark and safe, or alight, its rays an eighth on at f."""
    p = Pic(22, 22)
    p.ball(10.5, 12.5, 5.2, 5.2, CHROME[2:] if lit else CHROME[:4], 0.1, 0.9)
    p.box(6, 12, 15, 13, GLOWC[1] if lit else CHROME[0])
    p.box(8, 17, 13, 19, CHROME[1])
    p.outline()
    if lit:
        p.ball(10.5, 12.5, 3.2, 3.2, [GLOWC[1], GLOWC[2], WHITE], 0.4, 1.0)
        for i in range(8):
            a = i * math.pi / 4 + f * math.pi / 8
            r1 = 9.5 if (i + f) % 2 else 7.5
            mx, my = 10.5 + 6.5 * math.cos(a + 0.25), 12.5 + 6.5 * math.sin(a + 0.25)
            p.line(10.5 + 5.6 * math.cos(a), 12.5 + 5.6 * math.sin(a), mx, my, GLOWC[2])
            p.line(mx, my, 10.5 + r1 * math.cos(a - 0.1), 12.5 + r1 * math.sin(a - 0.1), WHITE)
    return p


def floorrail() -> Pic:
    """A tile's length of the rail a spark runs on: a bar on ties."""
    p = Pic(16, 4)
    for x in range(16):
        p.put(x, 0, CHROME[4])
        p.put(x, 1, CHROME[2])
        if x % 8 < 2:
            p.put(x, 2, CHROME[1])
            p.put(x, 3, CHROME[0])
    return p


def lipstripes(p: Pic, x0: int, y0: int, x1: int, y1: int) -> None:
    """A hazard stripe over a box: lit along its top, shaded along its foot."""
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            p.put(x, y, DECK[5] if (x + y) // 4 % 2 else HAZARD[0 if y == y0 else 2 if y == y1 else 1])


def presshouse() -> Pic:
    """A press's housing, hung from a girder: two lamps, a striped lip."""
    p = Pic(36, 21)
    p.box(2, 0, 33, 19, PANEL[2])
    p.box(2, 0, 33, 1, PANEL[4])
    for x in (5, 30):
        for y in (4, 10):
            p.put(x, y, PANEL[5])
    lipstripes(p, 2, 15, 33, 19)
    p.box(12, 5, 23, 11, PANEL[0])
    p.box(14, 7, 15, 8, LAMP[0])
    p.box(20, 7, 21, 8, LAMP[1])
    return p.outline()


def pressrod() -> Pic:
    """A length of a press's rod, and of its gantry's legs."""
    p = Pic(8, 8)
    for y in range(8):
        for i, tone in enumerate((1, 3, 5, 5, 4, 3, 2, 1)):
            p.put(i, y, CHROME[tone])
    return p


def presshead() -> Pic:
    """A press's head: a plate, striped at its lip."""
    p = Pic(38, 16)
    p.box(1, 1, 36, 14, DECK[2])
    p.box(1, 1, 36, 2, DECK[0])
    p.box(1, 3, 36, 4, DECK[1])
    p.box(1, 9, 36, 9, DECK[3])
    lipstripes(p, 1, 10, 36, 14)
    return p.outline()


def toolbox() -> Pic:
    """The bearded one's toolbox: red, a lid's seam, a latch and a handle over it."""
    p = Pic(20, 14)
    p.box(7, 1, 12, 1, CHROME[3])
    for x in (6, 13):
        p.box(x, 2, x, 3, CHROME[3])
    slab(p, 1, 4, 18, 12, SCARF)
    p.box(1, 7, 18, 7, SCARF[0])
    p.box(9, 6, 10, 8, GOLD[3])
    return p.outline()


# ---- Saltwick Sands: sand in strata, a boardwalk, the sea; its colours its own, at noon and at sunset ----


def both(first: int, noon: str, dusk: str, zone: int = 2) -> list[int]:
    """A ramp of a zone's own colours from index first: its tones in its first act, and the same places in its
    second. The sands' unless told."""
    a, b = noon.split(), dusk.split()
    assert len(a) == len(b)
    tones(first, *a, stage=zone * 2)
    return tones(first, *b, stage=zone * 2 + 1)


# the backdrop: pale, and paler than the playfield
SEASKY = both(
    96,
    "#2F86D6 #469ADE #60AEE6 #80C2EC #A4D6F0 #C8E8F2 #E8F4EC",
    "#4A3A80 #6A4690 #9058A0 #BC70A4 #E08C98 #F8AC84 #FFD08C",
)
SEAPUFF = both(103, "#FFFFFF #E6F2FA #C2DCF2", "#FFDCB4 #F4A0A0 #B870A8")
FARSEA = both(106, "#E2F4EE #A4D8DE #88C8D6 #70B8CC", "#FFDC98 #F09C98 #C078A4 #9060A0")
NEARSEA = both(110, "#62ACC6 #569EBC #B4E0E4", "#74509C #5C4290 #F0A0A0")  # two bands, a crest's line
CHALK = both(113, "#D4E0E4 #B4C8D6 #98B0C8", "#8C68A0 #70508C #58407C")  # the headland's cliff: lit, mid, shade
CHALKTOP = both(116, "#C4E0B8 #A2CCA6", "#7C7498 #645C84")
# the lighthouse: white lit and shaded, red lit and shaded, its ironwork, its lamp, dark by day
TOWER = both(118, "#FFFFFF #D0DAE8 #EE8C80 #C8686C #665872 #D0DAE8", "#FFDCC8 #B89CC0 #D05C70 #983C60 #3C2C58 #FFFFD8")
# the playfield: cream sand, coral, teal
SAND = both(
    127,
    "#FFFBEA #FBEEC8 #F4DCA4 #E6C088 #D0A074 #B08068 #845C64",
    "#FFE8C4 #FCCC9C #F0AC7C #DC8C6C #BC7068 #94586C #683E5E",
)
ROSE = both(134, "#F6C4B0 #E8A090", "#EE9888 #CE7478")
SEAINK = both(136, "#3A1E36", "#2C1838")[0]  # the ground's own ink
LAGOON = both(
    141,
    "#F0FFFA #B4F4E4 #64DCCC #2CB8B8 #1C94A4 #16708C #124E70",
    "#FFF4D8 #FFC8A8 #60C0BC #3098A8 #247898 #1E5C88 #184470",
)
PLANK = both(156, "#F0E8F0 #C8BACE #9C8EAC #72648A #4A3C62", "#F0C8C4 #BC94A8 #8E6A90 #664A78 #40305C")
WEED = both(161, "#A4E070 #58B458 #247C50", "#8CB874 #4C8C60 #285C50")
CORAL = both(164, "#FFD8C8 #FFA890 #F4786C #D04C58 #94304C", "#FFC8B0 #FF9080 #EC6068 #B83C58 #7C284C")
# the sun on the horizon, and its path on the sea's two far bands: the sky's and the sea's colours by day, when
# neither shows
LOWSUN = both(169, "#E8F4EC #E8F4EC #E8F4EC", "#FFFFE0 #FFF0A0 #FFD070")
GLEAM = both(185, "#A4D8DE #88C8D6", "#FFF0A0 #FFD070")
SEALINE, LOWSUNX = 104, 296


def shore() -> Pic:
    """The sands' far backdrop, of period 320: a banded sky, the low sun, clouds, the sea to the picture's foot
    and its swells, the sun's path, three sails, the headland and its lighthouse."""
    p = Pic(320, 200)
    edges = [0, 14, 28, 42, 56, 70, 86, SEALINE]
    for y in range(SEALINE):
        k = max(i for i in range(7) if edges[i] <= y)
        if k < 6 and y == edges[k + 1] - 2:  # a line of the next band before it starts
            k += 1
        p.box(0, y, 319, y, SEASKY[k])
    for y in range(88, SEALINE):
        for x in range(LOWSUNX - 15, LOWSUNX + 16):
            d = math.hypot(x - LOWSUNX + 0.5, y - SEALINE + 3.5)
            if d < 14:
                p.put(x, y, LOWSUN[0] if d < 9 else LOWSUN[1] if d < 12 else LOWSUN[2])
    for x, y, kind in ((34, 20, "mid"), (150, 30, "big"), (236, 18, "small"), (286, 46, "mid"), (70, 58, "small")):
        cloud(p, x, y, kind, SEAPUFF)
    for y in range(SEALINE, 200):
        d = y - SEALINE
        tone = (
            FARSEA[0]
            if d == 0
            else FARSEA[1]
            if d < 7
            else FARSEA[2]
            if d < 20
            else FARSEA[3]
            if d < 40
            else NEARSEA[0]
        )
        p.box(0, y, 319, y, {5: FARSEA[2], 17: FARSEA[3], 36: NEARSEA[0]}.get(d, tone))
    # swells: long flat crests, longer and further apart nearer
    for row, (y, step, run) in enumerate(((112, 40, 9), (121, 64, 14), (133, 80, 20), (150, 160, 30), (172, 160, 40))):
        for x in range(320):
            u = (x + row * 37) % step
            if u < run:
                p.put(x, y, NEARSEA[2] if y > 126 else FARSEA[0])
                if y > 140 and 2 < u < run - 3:
                    p.put(x, y + 1, NEARSEA[1])
    for d in range(1, 20, 2):
        for x in range(320):
            if abs(x - LOWSUNX) <= 4 + d * 0.3 and (x + d * 3) % 7 < 5:
                p.put(x, SEALINE + d, GLEAM[0 if d < 7 else 1])
    for x, y, h, lean in ((196, 108, 9, 1), (232, 111, 12, -1), (262, 107, 7, 1)):
        sail(p, x, y, h, lean)
    headland(p)
    lighthouse(p, 100, 75)
    return p


def sail(p: Pic, x: int, y: int, h: int, lean: int) -> None:
    """A far boat: a white sail lit on one side, a jib, a sliver of hull on the water at y."""
    for i in range(h):
        w = max(1, round((i + 1) * 0.55))
        for j in range(w):
            p.put(x + lean * j, y - h + i, TOWER[0] if j < w - 1 or w == 1 else TOWER[1])
        if i > 2:
            for j in range(1, max(1, round((i - 2) * 0.4)) + 1):
                p.put(x - lean * (j + 1), y - h + i, TOWER[1])
    p.box(x - 4, y, x + 4, y, TOWER[4])
    p.box(x - 3, y + 1, x + 3, y + 1, FARSEA[0])


HEADLINE = [(8, SEALINE), (12, 92), (20, 80), (34, 73), (56, 70), (84, 73), (112, 77), (132, 83), (148, 92)]
HEADLINE += [(158, 100), (163, SEALINE)]
GULLY = {0: 14, 9: 22, 15: 9}  # a gully's line down the chalk, by its place in each 23 across: how far down


def headland(p: Pic) -> None:
    """The far point the lighthouse stands on, sea either side of it: a chalk cliff under a cap of grass, lit
    from the left."""
    for x in range(8, 164):
        i = max(k for k in range(len(HEADLINE) - 1) if HEADLINE[k][0] <= x)
        (xa, ya), (xb, yb) = HEADLINE[i], HEADLINE[i + 1]
        top = round(ya + (yb - ya) * (x - xa) / (xb - xa))
        cap = 3 + (1 if (x // 5) % 3 == 0 else 0)
        for y in range(top, SEALINE + 1):
            if y - top < cap and 16 < x < 150:
                p.put(x, y, CHALKTOP[0] if y == top else CHALKTOP[1])
                continue
            # chalk: lit under the grass, a gully's line here and there, shade at its foot
            d, u = y - top - cap, x % 23
            gully = d < GULLY.get(u, -9) + (x // 23 % 3) * 3
            lit = d < 3 or (u in (1, 10, 16) and d < 8)
            p.put(x, y, CHALK[2] if gully or y > SEALINE - 4 else CHALK[0] if lit else CHALK[1])
    p.box(4, SEALINE + 1, 168, SEALINE + 1, FARSEA[0])  # the surf at its foot


def lighthouse(p: Pic, x: int, base: int) -> None:
    """The landmark: a white tower banded red, a gallery, the lamp under a cap."""
    for i in range(30):
        half = 5.2 - i * 0.07
        for xx in range(round(x - half), round(x + half) + 1):
            red = 7 <= i < 13 or 19 <= i < 25
            shade = xx > x + half * 0.35
            p.put(xx, base - i, (TOWER[3] if shade else TOWER[2]) if red else (TOWER[1] if shade else TOWER[0]))
    p.box(x - 1, base - 3, x, base, TOWER[4])  # its door
    g = base - 30
    p.box(x - 6, g, x + 6, g, TOWER[4])  # the gallery and its rail
    p.box(x - 6, g - 2, x + 6, g - 2, TOWER[4])
    for xx in (x - 6, x - 3, x, x + 3, x + 6):
        p.put(xx, g - 1, TOWER[4])
    p.box(x - 3, g - 7, x + 3, g - 3, TOWER[5])  # the lamp room
    for xx in (x - 3, x, x + 3):
        p.box(xx, g - 7, xx, g - 3, TOWER[4])
    for i in range(4):  # the cap
        p.box(x - 4 + i, g - 8 - i, x + 4 - i, g - 8 - i, TOWER[3] if i else TOWER[4])
    p.box(x, g - 14, x, g - 12, TOWER[4])


def swell() -> Pic:
    """The nearer sea, a masked band of period 320 that goes by faster: clear over its first crest, then its
    water and three rows of swells."""
    p = Pic(320, 80)
    p.box(0, 37, 319, 79, NEARSEA[0])
    for x in range(320):
        p.put(x, 36, NEARSEA[2] if x % 40 < 31 else NEARSEA[0])
    for row, (y, step, run) in enumerate(((46, 64, 18), (59, 80, 28), (73, 160, 44))):
        for x in range(320):
            u = (x + row * 29) % step
            if u < run:
                p.put(x, y, NEARSEA[2])
                if 2 < u < run - 3:
                    p.put(x, y + 1, NEARSEA[1])
    return p


# the sand's cut face: its beds down a period of 48, each from a row to a row and its tone's slot
BEDS = [(0, 6, 2), (6, 9, 7), (9, 17, 3), (17, 23, 1), (23, 29, 2), (29, 33, 8), (33, 41, 3), (41, 44, 4), (44, 48, 2)]
SHELLS = {
    3: ("..www.", ".wWcw.", ".wcWw.", "..ww.."),  # a whorl
    19: ("......", ".wwww.", "wwswsw", ".wwww."),  # a cockle
    35: ("......", "..tT..", "..Tt..", "......"),  # a chip of sea glass
    51: ("......", "....ww", "..ww..", "ww...."),  # a razor shell
}
PEBBLES = {(5, 3), (6, 3), (5, 4), (21, 5), (22, 5), (40, 2), (41, 2), (41, 3), (55, 4), (56, 4), (56, 5), (30, 6)}
# a slot's colour dry, and under the tide: 0..6 sand light to dark, 7 and 8 the rose beds, 9 white, 10 coral, 11
# and 12 sea glass
DRY = [*SAND, *ROSE, WHITE, CORAL[2], LAGOON[2], LAGOON[3]]
WET = [LAGOON[4], LAGOON[4], LAGOON[5], LAGOON[5], LAGOON[6], LAGOON[6], LAGOON[6], LAGOON[5], LAGOON[6]]
WET += [LAGOON[2], LAGOON[3], LAGOON[3], LAGOON[4]]
CRUST = (6, 7, 8, 8, 7, 6, 5, 5, 6, 7, 7, 6, 5, 6, 7, 8)  # the crust's foot, by twos: a period of 32


def strata(x: int, y: int) -> int:
    """The slot of the sand's cut face at a world pixel, repeating every 64 across and 48 down: flat beds that
    rise and fall together in a long fold, a seam's line over each; one bed holds shells, one pebbles."""
    fold = round(3 * math.sin(2 * math.pi * x / 64) + 1.5 * math.sin(4 * math.pi * x / 64 + 1.1))
    v = (y - fold) % 48
    lo, hi, tone = next(b for b in BEDS if b[0] <= v < b[1])
    u = x % 64
    if v == lo:  # the seam over a bed: dark, but over the shells and the thin beds a soft one
        return 5 if tone in (2, 3) else 4
    if v == lo + 1 and tone in (2, 3):
        return tone - 1  # the light under a seam
    if tone == 1:  # the shell bed
        for x0, rows in SHELLS.items():
            if 0 <= u - x0 < 6 and 0 <= v - lo - 1 < 4 and rows[v - lo - 1][u - x0] != ".":
                return {"w": 9, "W": 0, "c": 10, "t": 11, "T": 12, "s": 3}[rows[v - lo - 1][u - x0]]
        return 1
    return 5 if (lo, hi) == (33, 41) and (u, v - lo) in PEBBLES else tone


def crust(hem: int, d: int) -> int:
    """The slot of the sand's crust d pixels under the surface at column hem, -1 under it: pale sand, a line where
    it ends, its shade on the beds."""
    f = CRUST[(hem // 2) % 16]
    return 0 if d < 2 else 1 if d < f - 1 else 3 if d < f else 5 if d == f else -1


def sandy(r: list[int]) -> Skin:
    """The sand's skin in a slot's colours r: dry, or under the tide."""

    def soil(x: int, y: int, d: int, hem: int) -> int:
        return r[crust(hem, d) if crust(hem, d) >= 0 else strata(x, y)]

    return Skin(soil, lambda x, y: r[strata(x, y)], SEAINK, (r[3], r[1], r[5]))


def boards() -> Pic:
    """A boardwalk's 16: planks on a beam."""
    p = Pic(16, 16)
    for x in range(16):
        seam = x % 8 == 7
        p.put(x, 0, PLANK[0])
        p.put(x, 1, PLANK[3] if seam else PLANK[0] if x % 8 == 0 else PLANK[1])
        p.put(x, 2, PLANK[3] if seam else PLANK[1])
        for y, tone in ((3, PLANK[2]), (4, SEAINK), (5, PLANK[3]), (6, PLANK[4]), (7, SEAINK)):
            p.put(x, y, tone)
    return p


def skiff() -> Pic:
    """The sands' moving platform: a rowing boat, coral strakes under a white gunwale, its bow to the right."""
    p = Pic(48, 12)
    for x in range(2, 46):
        top = 3 - round(1.6 * ((x - 23) / 22) ** 2)
        lo = 10 - (round((x - 39) * 1.2) if x > 39 else (4 - x) * 2 if x < 4 else 0)
        for y in range(top, lo + 1):
            d = y - top
            tone = (
                WHITE
                if d == 0
                else TOWER[1]
                if d == 1
                else CORAL[4]
                if d in (2, 6)
                else CORAL[1]
                if d == 3
                else CORAL[2]
                if d < 6
                else CORAL[3]
            )
            p.put(x, y, CORAL[3] if x > 36 and d >= 2 and d != 6 else tone)
    return p.outline(SEAINK)


def brolly() -> Pic:
    """A beach umbrella where the Downs have a tree: six gores, coral and white, a scalloped rim, a pole."""
    p = Pic(64, 80)
    cx, cy, rx, ry = 32, 51, 20, 14
    p.box(31, 48, 32, 79, PLANK[2])
    p.box(31, 48, 31, 79, PLANK[1])
    for y in range(cy - ry, cy + 2):
        v = (y - cy) / ry
        half = rx * math.sqrt(max(0.0, 1 - v * v)) if y <= cy else rx - 1
        for x in range(cx - rx, cx + rx + 1):
            if abs(x - cx + 0.5) > half:
                continue
            a = math.asin(max(-1.0, min(1.0, (x - cx + 0.5) / max(half, 1))))
            gore = int((a / math.pi + 0.5) * 6 - 1e-6)
            rim = y >= cy - 1
            if y > cy and int((a / math.pi + 0.5) * 12) % 2 == 0:
                continue  # the rim's scallops
            if gore % 2 == 0:
                tone = CORAL[3] if rim or gore >= 4 else CORAL[1] if gore == 0 and v < -0.3 else CORAL[2]
            else:
                tone = TOWER[1] if rim or gore >= 4 else WHITE
            p.put(x, y, tone)
    p.box(31, 35, 32, 36, CORAL[3])
    return p.outline()


def marram() -> Pic:
    """Marram grass where the Downs have a bush: three tufts, a few blades from each root."""
    p = Pic(40, 20)
    for x0, tall in ((5, 0), (16, 3), (28, 1)):
        for dx, h, lean in ((0, 7, -2), (1, 9, -1), (2, 10, 0), (3, 8, 2), (4, 6, 3)):
            for i in range(h + tall):
                p.put(
                    x0 + dx + round(lean * i / (h + tall)), 18 - i, WEED[2] if i < 2 else WEED[1] if dx % 2 else WEED[0]
                )
    return p.outline(SEAINK)


STARFISH = (
    "....o....",
    "...oco...",
    "...oco...",
    "ooooCoooo",
    "occCCCcco",
    ".oocCcoo.",
    "..ocCco..",
    ".oco.oco.",
    ".oo...oo.",
)


def starfish(f: int) -> Pic:
    """A starfish where the Downs have a flower: frame f, an arm lifted."""
    p = Pic(12, 16)
    for y, row in enumerate(STARFISH):
        for x, ch in enumerate(row if f == 0 else row[::-1]):
            if ch != "." and not (f and (x, y) in ((1, 8), (2, 8), (1, 7))):
                p.put(1 + x, 7 + y - (f and x < 3), {"o": INK, "c": CORAL[2], "C": CORAL[1]}[ch])
    return p


def urchin() -> Pic:
    """A sea urchin where the Downs have thorns: spines round a dark ball."""
    p = Pic(16, 12)
    for i in range(9):
        a = math.pi * i / 8
        p.line(7.5, 9, 7.5 - 7 * math.cos(a), 9 - 8 * math.sin(a), PLANK[4])
    p.ball(7.5, 8, 4.4, 3.6, [PLANK[4], PLANK[3], PLANK[2]], 0.1, 1.0)
    return p.outline()


def crab(f: int) -> Pic:
    """A crab, face on: frames 0 and 1 its scuttle, claws up and eyes on stalks; 2 in its shell, claws tucked."""
    p = Pic(24, 15)
    back = CORAL[::-1]
    if f == 2:
        for side in (-1, 1):
            p.ball(12 + side * 7, 11, 2.6, 2.4, back, 0.3, 1.1)
        p.ball(12, 10, 6.4, 3.6, back, 0.25, 1.1)
        p.line(9.5, 9.5, 14.5, 9.5, CORAL[4])
        p.put(10, 9, WHITE)
        p.put(14, 9, WHITE)
        return p.outline(SEAINK)
    for side in (-1, 1):
        for i in range(3):
            tip = 13 - ((i + f) % 3 == 2)
            p.line(12 + side * (4 + i * 1.5), 10, 12 + side * (7.5 + i * 1.6 - f * (i == 1)), tip, CORAL[3], 2)
        cy = 3.6 - f * (side > 0) + f * (side < 0)
        p.line(12 + side * 5, 8, 12 + side * 9, cy + 1.4, CORAL[3], 2)
        p.ball(12 + side * 9.2, cy, 2.8, 2.6, back, 0.3, 1.1)
        p.put(round(12 + side * 9.2), round(cy - 0.6), SEAINK)
        p.put(round(12 + side * 9.2), round(cy - 1.6), 0)
        p.put(round(12 + side * 9.2), round(cy - 2.6), 0)
        p.box(round(12 + side * 2.5), 3, round(12 + side * 2.5), 5, CORAL[3])
    p.ball(12, 9, 6.4, 3.6, back, 0.25, 1.1)
    p.line(9.5, 10.5, 14.5, 10.5, CORAL[4])
    for side in (-1, 1):
        ex = round(12 + side * 2.5)
        p.box(ex - (side < 0), 2, ex + (side > 0), 3, WHITE)
        p.put(ex, 3, SEAINK)
    return p.outline(SEAINK)


def jelly(f: int) -> Pic:
    """A jellyfish: a pink bell, light on its crown, four trailing arms. Frame 1 is trodden flat."""
    p = Pic(16, 18)
    top = 6 + 3 * f
    p.ball(8, top, 6.6 + f, 5.4 - 2 * f, list(PETAL), 0.35, 1.15)
    for y in range(top + 2, 18):
        for x in range(16):
            p.p[y][x] = 0
    for x in range(2 - f, 14 + f):
        p.put(x, top + 2, PETAL[0] if (x - 2) % 3 == 2 else PETAL[1])
    p.box(4, top - 3 + f, 6, top - 3 + f, WHITE)
    p.put(4, top - 2 + f, WHITE)
    for i, x in enumerate((3, 6, 9, 12)):
        for d in range((7 if i % 2 else 5) - 3 * f):
            p.put(x + (1 if (d + i) % 4 < 2 else 0), top + 3 + d, PETAL[1] if d < 3 else PETAL[0])
    return p.outline(SEAINK)


def buoy(f: int) -> Pic:
    """A buoy, its foot in the water: a coral cone banded white, a lamp on its neck, lit in frame 1 as it bumps."""
    p = Pic(18, 18)
    for r in range(11):
        half = 2.5 + r * 0.55
        for x in range(round(9 - half), round(9 + half) + 1):
            u = (x - 9) / half
            band = WHITE if u < 0.3 else TOWER[1]
            p.put(x, 7 + r, band if r in (4, 5, 6) else CORAL[1] if u < -0.45 else CORAL[2] if u < 0.3 else CORAL[3])
    p.box(8, 4, 9, 6, PLANK[3])
    p.box(7, 1, 10, 3, GOLD[4] if f else GOLD[3])
    p.box(7, 1, 8, 1, WHITE if f else GOLD[4])
    p.box(10, 2, 10, 3, GOLD[3] if f else GOLD[1])
    return p.outline(SEAINK)


def deckchair(f: int) -> Pic:
    """The sands' spring, a deckchair: striped canvas slung between a crossed frame's tops, thrown taut in
    frame 1."""
    p = Pic(30, 22)
    p.line(26, 8, 7, 20, PLANK[3], 2)
    p.line(4, 3, 23, 20, PLANK[1], 2)
    p.line(4, 4, 23, 21, PLANK[2], 1)
    p.box(3, 20, 9, 21, PLANK[3])
    p.box(20, 20, 26, 21, PLANK[3])
    for i in range(60):
        u = i / 59
        x, y = 4 + 22 * u, 3 + 5 * u + (-1.2 if f else 3.4) * math.sin(math.pi * u)
        stripe = int(u * 8) % 2
        for d in range(4):
            tone = (CORAL[2] if d < 3 else CORAL[3]) if stripe else (WHITE if d < 3 else TOWER[1])
            p.put(round(x), round(y) + d, CORAL[1] if d == 0 and stripe else tone)
    p.box(2, 1, 5, 3, PLANK[1])
    p.box(25, 6, 28, 8, PLANK[1])
    return p.outline(SEAINK)


def gull(f: int) -> Pic:
    """A gull on the wing, heading right: grey wings, up in frame 0 and down in 1, a yellow bill."""
    p = Pic(26, 20)
    up = 1 - 2 * f
    p.line(11, 10 - 2 * up, 15, 10 - 7 * up, STONE[1], 3)
    p.line(15, 10 - 7 * up, 17, 10 - 8 * up, STONE[0], 2)
    p.line(4, 10, 1, 11, STONE[2], 2)
    p.ball(11, 10, 6.5, 2.8, [STONE[2], STONE[3], WHITE, WHITE], 0.3, 1.2)
    p.ball(18.5, 9, 2.8, 2.5, [STONE[3], WHITE, WHITE], 0.4, 1.2)
    p.line(10, 10 - up, 4, 10 - 7 * up, STONE[2], 4)
    p.line(9, 10 - up, 5, 10 - 5 * up, STONE[3], 2)
    p.line(4, 10 - 7 * up, 2, 10 - 9 * up, STONE[0], 2)
    p.line(21, 9.5, 23.5, 10.5, GOLD[2], 2)
    p.put(24, 11, GOLD[1])
    p.put(19, 8, SEAINK)
    return p.outline(SEAINK)


def pedalo(hit: bool) -> Pic:
    """Captain Kipper's pedal boat, 52 by 34, its deck at row 22 and its waterline four rows from its foot: a
    paddle wheel at its stern, the cat at its helm in a captain's cap; hit: bumped, the cat off its feet and its
    cap off its head."""
    p = Pic(52, 34)
    for x in range(4, 50):
        lo = 33 - (round((x - 42) * 1.3) if x > 42 else 0)
        for y in range(22, lo + 1):
            d = y - 22
            tone = (
                WHITE
                if d == 0
                else TOWER[1]
                if d == 1
                else CORAL[4]
                if d in (2, 8)
                else CORAL[1]
                if d == 3
                else CORAL[2]
            )
            p.put(x, y, CORAL[3] if (x > 38 or d > 8) and d >= 2 and d != 8 else tone)
    for i in range(8):  # the wheel: a rim, and blades about its hub
        ang = math.pi * i / 4 + (0.4 if hit else 0)
        p.line(8, 25, 8 + 6.5 * math.cos(ang), 25 + 6.5 * math.sin(ang), PLANK[2 + i % 2], 2)
    p.ball(8, 25, 2.2, 2.2, [PLANK[4], PLANK[3], PLANK[1]], 0.3, 1.0)
    p.box(44, 14, 44, 21, PLANK[3])  # a staff at its bow, a pennant on it
    for i in range(3):
        p.box(45, 14 + i, 47 - i, 14 + i, LAGOON[2] if i < 2 else LAGOON[3])
    paste(p, startled() if hit else cat(0), 18, 5 if hit else 8)
    hy = 1 if hit else 8  # its cap: white, a dark peak, a gold badge
    p.box(30, hy, 35, hy + 1, WHITE)
    p.box(30, hy + 2, 37, hy + 2, SEAINK)
    p.put(33, hy + 1, GOLD[3])
    return p.outline(SEAINK)


def teddy() -> Pic:
    """The little one's teddy, afloat: round ears, a pale muzzle, arms out."""
    fur = EARTH[1:]
    p = Pic(14, 16)
    for x in (3.2, 10.8):
        p.ball(x, 2.6, 2.0, 2.0, fur, 0.3, 1.0)
    p.line(2, 10, 12, 10, fur[1], 3)
    p.ball(7, 11.5, 4.2, 4.2, fur, 0.3, 1.1)
    p.ball(7, 5.5, 4.4, 4.0, fur, 0.3, 1.1)
    p.box(6, 6, 8, 7, fur[-1])
    p.put(7, 6, SEAINK)
    p.put(5, 4, SEAINK)
    p.put(9, 4, SEAINK)
    p.box(6, 11, 8, 12, fur[-1])
    return p.outline(SEAINK)


def trough(p: Pic, x: int, y: int, f: int, batten: bool) -> None:
    """A column of a flume at x, the ground's line at y: its water over its near wall, planks and a coral band; the
    water's flecks 2 further on a frame, round every 8."""
    wall = (PLANK[0], PLANK[1], PLANK[1], CORAL[2], CORAL[3], PLANK[2], PLANK[2], PLANK[3], PLANK[4])
    for d, tone in enumerate(LAGOON[:4]):
        p.put(x, y - 8 + d, tone)
    if (x - f * 2) % 8 < 3:
        p.put(x, y - 7, WHITE)
    for d, tone in enumerate(wall):
        p.put(x, y - 4 + d, PLANK[4] if batten else tone)


def sluice(f: int) -> Pic:
    """A flume's fall over one column, a tile down: where a slope's tile is, 8 over it."""
    p = Pic(16, 29)
    for x in range(16):
        trough(p, x, x + 8, f, x == 8)
    return p


def spout(f: int) -> Pic:
    """A flume's level end over its last column, capped."""
    p = Pic(16, 13)
    for x in range(16):
        trough(p, x, 8, f, x in (8, 15))
    return p


def runnel(f: int) -> Pic:
    """A length of the water that falls from a flume's end, 8 down."""
    p = Pic(5, 8)
    for y in range(8):
        for x in range(5):
            fleck = x in (1, 2, 3) and (x * 2 + y - f * 2) % 8 < 3
            p.put(x, y, WHITE if fleck else LAGOON[1] if x in (0, 4) else LAGOON[2])
    return p


def cistern() -> Pic:
    """A flume's head: the tank that fills it, on legs, and the first of its trough."""
    p = Pic(30, 30)
    p.box(3, 4, 18, 19, PLANK[2])
    p.box(3, 4, 18, 5, PLANK[0])
    for x in (7, 11, 15):
        p.box(x, 6, x, 19, PLANK[3])
    p.box(2, 9, 19, 10, CORAL[3])
    p.box(2, 15, 19, 16, CORAL[3])
    p.box(4, 2, 17, 3, LAGOON[2])
    for x in (4, 17):
        p.box(x, 20, x + 1, 25, PLANK[4])
    p.outline(SEAINK)
    for x in range(24, 30):
        trough(p, x, 25, 0, x == 24)
    return p


def lifepost() -> Pic:
    """A post with a life ring where the Downs have a lantern plant."""
    p = Pic(20, 44)
    p.box(9, 14, 10, 43, PLANK[2])
    p.box(9, 14, 9, 43, PLANK[1])
    for y in range(1, 15):
        for x in range(3, 17):
            d = math.hypot(x - 9.5, y - 7.5)
            if 3 <= d <= 6.6:
                quarter = (x > 9) ^ (y > 7)
                p.put(x, y, (CORAL[2] if x + y < 18 else CORAL[3]) if quarter else (WHITE if x + y < 18 else TOWER[1]))
    return p.outline()


def surf() -> Pic:
    """The water's edge, 16 along: foam, broken where a wavelet dips, over the first of the water under it."""
    p = Pic(16, 8)
    for x in range(16):
        w = (x // 2) % 8
        if w in (2, 3):
            p.put(x, 0, LAGOON[0])
        p.put(x, 1, LAGOON[1] if w == 7 else LAGOON[0])
        p.put(x, 2, LAGOON[1])
        p.box(x, 3, x, 7, LAGOON[2])
    return p


def sands() -> list[tuple[int, Pic, bool, int]]:
    """Saltwick Sands' own pictures, at the indices the Downs' own have: the game draws a zone of this kind from
    one layout."""
    k, z = 126, 625
    a, dry, planks = z + 124, sandy(DRY), [*PLANK, PLANK[4]]
    out = [(0, shore(), False, -1), (1, swell(), True, -1), (28, boards(), True, -1)]
    out.append(
        (5, ring(188, 72, 93, lambda hem, d: DRY[crust(hem, d)] if crust(hem, d) >= 0 else 0, SAND, SEAINK), True, -1)
    )
    out += [(z + 112 + h * 2, ribbon(h, planks, SEAINK), True, -1) for h in range(2)]
    out += earth(a, dry)
    out.append((a + 268, skiff(), True, -1))
    # its ground again under the tide, where no other zone has pictures, and the water's edge
    out += earth(z + 652, sandy(WET))
    out.append((z + 930, surf(), True, -1))
    out += [(z + 932 + f * 2, crab(f), True, -1) for f in range(3)]
    for i, thing in enumerate((jelly, buoy, deckchair)):
        out += [(z + 938 + i * 4 + f * 2, thing(f), True, -1) for f in range(2)]
    out += [(z + 950 + f * 2, gull(f), True, z + 954 + f * 2) for f in range(2)]
    out += [(z + 958 + h * 2, pedalo(bool(h)), True, -1) for h in range(2)]
    out.append((z + 962, teddy(), True, -1))
    out += [(z + 964 + f * 2, sluice(f), True, -1) for f in range(4)]
    out += [(z + 972 + f * 2, spout(f), True, -1) for f in range(4)]
    out += [(z + 980 + f * 2, runnel(f), True, -1) for f in range(4)]
    out.append((z + 988, cistern(), True, -1))
    out += [
        (k + 36, brolly(), True, -1),
        (k + 38, marram(), True, -1),
        (k + 40, starfish(0), True, -1),
        (k + 42, starfish(1), True, -1),
        (k + 44, rock(), True, -1),
        (k + 46, urchin(), True, -1),
        (k + 48, lifepost(), True, -1),
        (k + 376, words("SALTWICK SANDS", 178, 18, 1.42, 2.7), True, -1),
    ]
    return out


# ---- Whitecap Pass: a snow cap over faceted ice, peaks and pines; its colours its own, a clear morning and dusk ----

ALPSKY = both(
    96,
    "#2C5CC4 #3C74D4 #5490E0 #78ACE8 #A4C8EE #D0DCEC #F8DCD8",
    "#262448 #34305C #483C74 #62508C #846498 #AC7CA0 #D89CA4",
    3,
)
FARPEAK = both(103, "#FFF0EC #F8C4CC #C4B0DC #9890C8", "#E0B8CC #B088B4 #6C5894 #4A3E74", 3)  # snow, rock: lit, shade
NEARPEAK = both(107, "#FFFFFF #D0E2F8 #98B4E4 #7090CC #5070B0", "#C8C0E0 #9A94C4 #6A68A4 #4A4A84 #363664", 3)
PINE = both(112, "#3C7C74 #285C60 #1A4250 #EAF4FC", "#34446C #263458 #1A2644 #B4B0D8", 3)  # lit, mid, shade, its snow
SNOW = both(116, "#FFFFFF #EAF4FC #C6DCF4 #A0C0E8 #7CA0D4", "#EEE4F4 #CCC2E4 #A69CCC #8278B4 #5E5A98", 3)
ICE = both(121, "#DCF6FC #AEE2F4 #78C6E8 #4CA2D8 #2E7AC0 #1E5298", "#C0BCEC #9894D8 #7270C4 #5252A6 #3C3C88 #2A2A66", 3)
FROSTINK = both(127, "#1A2248", "#141432", 3)[0]  # the ground's own ink
ALPGLOW = both(128, "#FFC8D0 #F4A0B4", "#F090A8 #C4648C", 3)
HEARTH = both(130, "#FFD468 #FFF4B0", "#FFFFC8 #FFE070", 3)  # a window, a lantern: its glass, its flame
LOG = both(132, "#B87850 #8C5438 #643828", "#845878 #5E3C60 #3E2848", 3)
SKYLINE = 106
FARLINE = [(-10, 70), (22, 40), (40, 58), (66, 28), (84, 50), (104, 44), (132, 66), (160, 36), (176, 52), (204, 24)]
FARLINE += [(226, 50), (250, 42), (274, 64), (300, 38), (330, 72)]
NEARLINE = [(-10, 92), (30, 72), (52, 88), (88, 62), (118, 90), (150, 78), (186, 96), (224, 68), (252, 92), (286, 74)]
NEARLINE += [(330, 91)]


def crags(p: Pic, pts: list[tuple[int, int]], r: list[int], cap: int) -> None:
    """A range along pts, of period 320: rock, lit on the flanks that face left, and a snow cap down from its ridge,
    its foot in fingers. r: snow lit and shaded, rock lit and shaded."""
    for x in range(320):
        (x0, y0), (x1, y1) = next(seg for seg in itertools.pairwise(pts) if seg[0][0] <= x <= seg[1][0])
        ridge, lit = y0 + (y1 - y0) * (x - x0) / (x1 - x0), y1 < y0
        foot = cap + (3 if x % 7 < 3 else 0) + (4 if x % 11 < 2 else 0)
        for y in range(round(ridge), SKYLINE + 30):
            snowy = y - ridge < foot * (1.0 if lit else 0.7)
            p.put(x, y, r[(0 if snowy else 2) + (0 if lit else 1)])


def spruce(p: Pic, x: int, foot: int, h: int, snowy: bool = True) -> None:
    """A pine: three tiers, each a triangle lit to the left of its trunk; snow on each tier's upper edge."""
    for t in range(3):
        y0 = foot - h + t * h // 4
        y1 = foot - (2 - t) * h // 5 - 2
        half = 2 + (t + 1) * h // 7
        for y in range(y0, y1 + 1):
            w = round(half * (y - y0 + 1) / (y1 - y0 + 1))
            for dx in range(-w, w + 1):
                tone = PINE[0] if dx < -w // 3 else PINE[1] if dx <= w // 3 else PINE[2]
                if snowy and dx <= 0 and y - y0 < 3 and y - y0 <= 1 + abs(dx) * (y1 - y0) // max(1, half) // 3:
                    tone = PINE[3]
                p.put(x + dx, y, tone)
        if snowy:  # a line of snow down the lit edge
            for dx in range(-half, 1):
                p.put(x + dx, y0 + round((y1 - y0) * abs(dx) / half), PINE[3])
    p.box(x, foot - 2, x, foot, WOOD[1])


def cabin(p: Pic, x: int, foot: int) -> None:
    """A log cabin: snow on its roof, a window, smoke from its chimney."""
    for y in range(foot - 12, foot + 1):
        p.box(x, y, x + 25, y, LOG[0] if (foot - y) % 4 else LOG[1])
    p.box(x + 20, foot - 12, x + 25, foot, LOG[2])
    for i in range(9):  # the roof: a gable, white on top, a shade line under its eave
        p.box(x - 3 + i, foot - 13 - i, x + 28 - i * 2, foot - 13 - i, SNOW[0] if i else SNOW[2])
    p.box(x + 5, foot - 9, x + 10, foot - 4, HEARTH[0])
    p.box(x + 5, foot - 7, x + 10, foot - 7, LOG[2])
    p.box(x + 8, foot - 9, x + 8, foot - 4, LOG[2])
    p.box(x + 14, foot - 8, x + 18, foot, LOG[2])
    p.box(x + 19, foot - 24, x + 22, foot - 17, LOG[1])
    p.box(x + 18, foot - 25, x + 23, foot - 25, SNOW[0])
    for dx, dy, r in ((22, -29, 3), (26, -33, 4)):
        p.ball(x + dx, foot + dy, r, r * 0.8, [SNOW[2], SNOW[1], SNOW[0]], 0.3, 1.0)


def skilift(p: Pic) -> None:
    """A ski lift, of period 320: a cable from pylon to pylon, sagging between them, and two chairs hung from it."""
    posts = ((96, 46), (256, 56), (416, 46))

    def cable(x: int) -> int:
        (x0, y0), (x1, y1) = next(seg for seg in itertools.pairwise(posts) if seg[0][0] <= x < seg[1][0])
        u = (x - x0) / (x1 - x0)
        return round(y0 + (y1 - y0) * u + 8 * (1 - (2 * u - 1) ** 2))

    for x in range(320):
        p.put(x, cable(x if x >= 96 else x + 320), FROSTINK)
    for x, y in posts[:2]:
        p.box(x, y, x + 1, SKYLINE + 30, NEARPEAK[4])
        p.box(x - 3, y, x + 4, y + 1, NEARPEAK[4])
    for x in (40, 178):
        y = cable(x if x >= 96 else x + 320)
        p.box(x, y, x, y + 8, FROSTINK)
        p.box(x - 4, y + 8, x + 4, y + 9, ALPGLOW[1])
        p.box(x - 4, y + 9, x - 4, y + 13, FROSTINK)
        p.box(x - 4, y + 13, x + 4, y + 13, FROSTINK)


def alps() -> Pic:
    """The pass's far backdrop, of period 320: a banded sky, two ranges, a ski lift before them, snow to its foot."""
    p = Pic(320, 200)
    edges = [0, 14, 30, 46, 62, 78, 92, SKYLINE + 30]
    for y in range(SKYLINE + 30):
        k = max(i for i in range(7) if edges[i] <= y)
        if k < 6 and y == edges[k + 1] - 2:  # a line of the next band before it starts
            k += 1
        p.box(0, y, 319, y, ALPSKY[k])
    crags(p, FARLINE, FARPEAK, 12)
    crags(p, NEARLINE, NEARPEAK[:4], 9)
    skilift(p)
    p.box(0, SKYLINE + 30, 319, 199, NEARPEAK[2])
    return p


def firwood() -> Pic:
    """The pass's nearer band, masked, of period 320, which goes by faster: two ranks of pine on a snowfield, its
    drifts' crests across it, a cabin among them."""
    p = Pic(320, 80)
    for y in range(24, 80):
        p.box(0, y, 319, y, NEARPEAK[1] if y < 68 or y == 70 else NEARPEAK[2])
    for row, (y, step, run) in enumerate(((52, 64, 26), (64, 80, 38), (76, 160, 50))):
        for x in range(320):
            if (x + row * 41) % step < run:
                p.put(x, y, NEARPEAK[0])
    for i in range(32):  # the far rank
        for wrap in (-320, 0, 320):
            spruce(p, i * 10 + (i * 5) % 4 + wrap, 26 + (i * 3) % 3, 14 + (i * 7) % 5, i % 3 == 0)
    for i in range(20):  # the near rank
        for wrap in (-320, 0, 320):
            spruce(p, i * 16 + 2 + (i * 7) % 6 + wrap, 42 + (i * 5) % 4, 22 + (i * 11) % 7)
    cabin(p, 64, 38)
    return p


CAPHEM = (5, 6, 7, 7, 6, 5, 4, 4, 5, 6, 6, 5, 4, 5, 6, 7)  # the snow cap's foot, by twos: a period of 32


def snowcap(hem: int, d: int, thin: int = 1) -> int:
    """The slot of the snow cap d pixels under the surface at column hem, -1 under it: white, its shade under its
    lip, a hem that drips. thin: that many times thinner."""
    foot = (5 + CAPHEM[(hem // 2) % 16]) // thin
    return 0 if d <= 0 else 1 if d < foot - 2 else 2 if d < foot else 3 if d == foot else -1


def facet(x: int, y: int, deep: int = 0) -> int:
    """The ice's tone at a world pixel, repeating every 64 across and 48 down: a lattice of triangles, 16 across
    and 12 down, each flat, a pale line along some of their edges; deep: a tone darker."""
    cx, cy = (x // 16) % 4, (y // 12) % 4
    if (x % 16 == 0 and (cx + cy) % 3 == 0) or (y % 12 == 0 and (cx + cy) % 4 == 1):
        return ICE[0]
    u, v = (x % 16) / 16, (y % 12) / 12
    upper = (u + v < 1) if (cx + cy) % 2 else (u > v)
    return ICE[1 + (cx * 5 + cy * 3 + (2 if upper else 0)) % 4 // 2 + upper + deep]


def icy() -> Skin:
    """The pass's ground: a snow cap, a fleck of light on it here and there, over ice paler than it is far down."""

    def soil(x: int, y: int, d: int, hem: int) -> int:
        slot = snowcap(hem, d)
        if slot < 0:
            return facet(x, y)
        return SNOW[0 if slot == 1 and d == 1 and (x + y) % 9 == 0 else slot]

    return Skin(soil, lambda x, y: facet(x, y, 1), FROSTINK, (SNOW[2], ICE[1], ICE[4]))


def glassy() -> Skin:
    """The pass's bare ice: a bright lip, a band of sheen with glints across it, then the facets."""

    def soil(x: int, y: int, d: int, hem: int) -> int:
        if d <= 0:
            return SNOW[0] if hem % 16 in (3, 4, 11) else ICE[0]
        if d == 1:
            return ICE[1]
        if d < 6:
            return ICE[0] if (hem - d) % 16 in (2, 3, 7) else ICE[2]
        return facet(x, y)

    return Skin(soil, lambda x, y: facet(x, y, 1), FROSTINK, (ICE[1], ICE[1], ICE[4]))


def iceledge() -> Pic:
    """A floating shelf's 16: snow on a slab of ice, icicles under it."""
    p = Pic(16, 16)
    for x in range(16):
        for y, tone in enumerate((SNOW[0], SNOW[1], SNOW[1], SNOW[2], SNOW[3])):
            p.put(x, y, tone)
        p.box(x, 5, x, 8, ICE[2] if (x // 4) % 2 else ICE[3])
        p.put(x, 5, ICE[1])
        p.put(x, 9, FROSTINK)
        n = (0, 3, 0, 0, 5, 0, 2, 0, 0, 4, 0, 0, 3, 0, 5, 0)[x]
        for d in range(n):
            p.put(x, 10 + d, ICE[0] if d < 2 else ICE[1])
    return p


def floe() -> Pic:
    """The pass's moving platform: a deck of logs, snow on it."""
    p = Pic(48, 12)
    for x in range(1, 47):
        for y, tone in enumerate(
            (SNOW[0], SNOW[1], SNOW[2] if x % 6 < 4 else SNOW[1], SNOW[3] if x % 6 < 2 else LOG[0])
        ):
            p.put(x, 1 + y, tone)
        p.box(x, 5, x, 8, LOG[2] if x % 12 == 0 else LOG[0] if x % 12 < 7 else LOG[1])
        p.put(x, 9, LOG[2])
    return p.outline(FROSTINK)


def fir() -> Pic:
    """A pine where the Downs have a tree, by the backdrop's rule, bigger."""
    p = Pic(64, 80)
    spruce(p, 32, 75, 66)
    p.box(31, 70, 33, 78, WOOD[1])
    p.box(31, 70, 31, 78, WOOD[2])
    return p.outline(FROSTINK)


def drift() -> Pic:
    """A drift where the Downs have a bush: a mound of snow over a low pine, its twigs out of it."""
    p = Pic(40, 20)
    for x0, y0, lean in ((6, 12, -3), (14, 9, -1), (26, 10, 2), (33, 13, 3)):
        p.line(x0, 17, x0 + lean, y0 - 3, PINE[1], 2)
        p.put(x0 + lean, y0 - 4, PINE[0])
    p.ball(20, 16, 16, 8, [SNOW[3], SNOW[2], SNOW[1], SNOW[0]], 0.35, 1.2)
    p.box(0, 19, 39, 19, 0)
    return p.outline(FROSTINK)


def pennant(f: int) -> Pic:
    """A trail's pennant where the Downs have a flower: frame f, its tip lifted."""
    p = Pic(12, 16)
    p.box(3, 1, 3, 15, LOG[1])
    for i in range(6):
        y = 2 + i // 2 - (f and i > 3)
        p.box(4 + i, y, 4 + i, y + 3 - i // 2, ALPGLOW[1] if i % 3 else ALPGLOW[0])
    return p.outline(FROSTINK)


def boulder(cap: list[int] = SNOW, ink: int = FROSTINK) -> Pic:
    """A rock under snow, or what else lies on it: cap's first three tones, down from its top."""
    p = rock()
    for x in range(p.w):
        ys = [y for y in range(p.h) if p.p[y][x]]
        for d, y in enumerate(ys[: 2 + (x % 5 < 2)]):
            p.put(x, y, cap[min(d, 2)])
    return p.outline(ink)


def shards() -> Pic:
    """Shards of ice where the Downs have thorns: spikes, lit on their left."""
    p = Pic(16, 12)
    for x0, h in ((2, 6), (5, 10), (9, 8), (12, 5)):
        for d in range(h):
            w = 1 + d * 3 // h
            for dx in range(-(w // 2), w - w // 2):
                p.put(
                    x0 + dx,
                    11 - h + d + 1 - 1,
                    ICE[0] if dx == -(w // 2) else ICE[4] if dx == w - w // 2 - 1 and w > 2 else ICE[2],
                )
    return p.outline(FROSTINK)


def lantern(
    post: tuple[int, int] = (LOG[0], LOG[1]),
    light: list[int] = HEARTH,
    cap: tuple[int, int] = (SNOW[0], SNOW[2]),
    ink: int = FROSTINK,
) -> Pic:
    """A lantern on a post where the Downs have a lantern plant: its glass lit, snow on its cap. post: its lit side
    and its body; light: the glass and what burns in it; cap: what lies on it and that's shade."""
    p = Pic(20, 44)
    p.box(9, 14, 10, 43, post[1])
    p.box(9, 14, 9, 43, post[0])
    p.box(6, 4, 13, 5, METAL[1])
    p.box(7, 6, 12, 12, light[0])
    p.box(9, 8, 10, 11, light[1])
    for x in (6, 13):
        p.box(x, 6, x, 12, METAL[1])
    p.box(6, 13, 13, 13, METAL[1])
    p.box(5, 2, 14, 3, cap[0])
    p.box(7, 1, 12, 1, cap[0])
    p.box(5, 3, 14, 3, cap[1])
    return p.outline(ink)


def snapshot() -> Pic:
    """The fair one's camera: a black box, its lens, a pink button, a strap's loop."""
    p = Pic(18, 14)
    p.box(1, 4, 16, 12, METAL[0])
    p.box(1, 4, 16, 4, METAL[2])
    p.box(1, 11, 16, 12, STONE[0])
    p.box(4, 2, 8, 3, METAL[1])
    p.box(12, 2, 14, 3, ALPGLOW[1])
    p.ball(9, 8, 3.6, 3.6, [METAL[1], METAL[2], METAL[3]], 0.4, 1.0)
    p.ball(9, 8, 2.2, 2.2, [ICE[5], ICE[3], ICE[1]], 0.2, 1.0)
    p.put(8, 7, ICE[0])
    p.box(2, 6, 3, 7, HEARTH[1])
    return p.outline(FROSTINK)


def leant(o: Pic, lean: int) -> Pic:
    """o as the ground lies under it: lean halves of a pixel's fall for each along, about its middle; room over and
    under it for the steepest."""
    pad = o.w // 2
    p = Pic(o.w, o.h + 2 * pad)
    for x in range(o.w):
        for y in range(o.h):
            if o.p[y][x]:
                p.put(x, pad + y + round(lean * (x - pad) / 2), o.p[y][x])
    return p


def toboggan() -> Pic:
    """A toboggan on the level, its nose to the right: slats curled up at it, a runner under."""
    o = Pic(30, 12)
    o.box(2, 5, 24, 6, WOOD[3])
    o.box(2, 7, 24, 7, WOOD[1])
    for x in (6, 11, 16, 21):
        o.put(x, 5, WOOD[1])
    for i, (x, y) in enumerate(((25, 5), (26, 4), (27, 3), (27, 2), (26, 1), (25, 1))):
        o.box(x, y, x + 1, y, WOOD[3] if i < 3 else WOOD[2])
    o.box(3, 9, 23, 9, METAL[2])
    o.put(24, 8, METAL[2])
    for x in (5, 20):
        o.put(x, 8, METAL[1])
    return o.outline(FROSTINK)


def sledge(lean: int) -> Pic:
    """A toboggan as the ground lies, its runner's middle at the picture's row 24."""
    return leant(toboggan(), lean)


def frostcat() -> Pic:
    """Frost: a white cat, sat, goggles on, a pink scarf out behind her."""
    fur = [ICE[3], SNOW[3], SNOW[1], SNOW[0]]
    q = Pic(26, 20)
    q.line(7, 16, 3, 13, fur[2], 2)
    q.line(3, 13, 4, 9, fur[3], 2)
    q.ball(12, 13, 5.5, 5.5, fur, 0.25, 1.1)
    q.ball(16, 6.5, 4.2, 3.8, fur, 0.3, 1.1)
    for x, y in ((13, 1), (13, 2), (14, 2), (19, 1), (18, 2), (19, 2)):
        q.put(x, y, fur[3])
    q.box(13, 5, 20, 6, FROSTINK)
    q.box(14, 5, 15, 6, ICE[1])
    q.box(18, 5, 19, 6, ICE[1])
    q.put(20, 8, PETAL[0])
    q.box(12, 10, 17, 11, ALPGLOW[1])
    for i in range(7):
        q.box(11 - i, 10 - (i // 3), 11 - i, 11 - (i // 3), ALPGLOW[0] if i % 3 == 0 else ALPGLOW[1])
    q.box(15, 14, 18, 15, fur[3])
    return q.outline(FROSTINK)


def frost(lean: int) -> Pic:
    """Frost on her toboggan, as the ground lies."""
    p = Pic(34, 31)
    paste(p, frostcat(), 4, 5)
    paste(p, toboggan(), 2, 19)
    return leant(p, lean)


def dunked() -> Pic:
    """Frost knocked into deep snow: her head and her scarf out of the heap."""
    p = Pic(56, 22)
    q = frostcat()
    for y in range(13):
        for x in range(q.w):
            if q.p[y][x]:
                p.put(15 + x, y, q.p[y][x])
    paste(p, deepsnow(), 0, 10)
    return p


def deepsnow() -> Pic:
    """Deep snow: a long soft mound, flecks of shade on it."""
    p = Pic(56, 12)
    for x in range(56):
        h = round(9 * (1 - ((x - 27.5) / 28) ** 2) + (1 if x % 9 < 4 else 0))
        for d in range(h):
            p.put(x, 11 - d, SNOW[0] if d >= h - 2 else SNOW[1] if d > 1 else SNOW[2])
        if x % 7 == 3 and h > 4:
            p.put(x, 11 - h + 3, SNOW[2])
    return p.outline(FROSTINK)


def geyservent() -> Pic:
    """A geyser's mouth in the snow, where the works have a jet's: a rim of ice round a dark hole."""
    p = Pic(16, 6)
    p.box(1, 3, 14, 5, ICE[3])
    p.box(3, 1, 12, 2, ICE[1])
    p.box(3, 1, 12, 1, ICE[0])
    p.box(6, 1, 9, 2, FROSTINK)
    return p.outline(FROSTINK)


def geyserjet(f: int) -> Pic:
    """A geyser's column, frame f: water in pulses up it, darker at its edges, a head of spray, drops thrown off."""
    p = Pic(12, 64)
    for y in range(9, 64):
        half = 4 if ((y + f * 3) // 6) % 2 else 3
        for dx in range(-half, half + 1):
            tone = ICE[3] if abs(dx) == half else SNOW[0] if dx < -1 else ICE[0] if dx < 2 else ICE[1]
            p.put(5 + dx + (dx >= 0), y, tone)
    for cx, cy, r in ((5.5, 6, 5.4), (2.5 + f, 9, 2.6), (9.5 - f, 10, 2.6), (5.5, 2 + f, 2.4)):
        p.ball(cx, cy, r, r * 0.8, [ICE[3], ICE[1], ICE[0], SNOW[0]], 0.3, 1.1)
    for x, y in ((0, 20), (11, 25), (0, 34), (11, 41), (0, 48)):
        p.put(x, y + f * 4, ICE[1])
    return p


def snowball(f: int) -> Pic:
    """The snowball Charlie is in, rolling: frame f, a crease in it gone half round."""
    p = Pic(28, 28)
    p.ball(13.5, 13.5, 13, 13, [SNOW[3], SNOW[2], SNOW[1], SNOW[0]], 0.35, 1.2)
    for i in range(5):
        a = math.pi * (0.25 + f + i * 0.09)
        p.put(round(13.5 + 8 * math.cos(a)), round(13.5 - 8 * math.sin(a)), SNOW[3])
    return p.outline(FROSTINK)


def fort(up: bool) -> Pic:
    """A snow-thrower, facing left: a cat in a bobble hat behind a wall of snow, ducked; or up, a snowball held."""
    p = Pic(28, 26)
    if up:
        q = cat(0)
        for y in range(q.h):
            for x in range(q.w):
                if q.p[y][x]:
                    p.put(8 + q.w - 1 - x, 9 + y, q.p[y][x])
        p.box(10, 8, 14, 9, ALPGLOW[1])
        p.box(9, 10, 15, 10, SNOW[0])
        p.box(11, 6, 12, 7, SNOW[0])
        p.ball(7.5, 4, 2.6, 2.6, [SNOW[2], SNOW[1], SNOW[0]], 0.3, 1.0)
    else:
        p.box(10, 13, 14, 14, ALPGLOW[1])
        p.box(11, 11, 12, 12, SNOW[0])
    p.outline(FROSTINK)
    for x in range(1, 17):
        h = 10 - (1 if x in (1, 16) else 0) - (1 if x % 5 == 2 else 0)
        for d in range(h):
            brick = d % 4 == 0 or (x + (d // 4) * 3) % 6 == 0
            p.put(x, 24 - d, SNOW[0] if d >= h - 2 else SNOW[2] if brick else SNOW[1])
    return p.outline(FROSTINK)


def lobbed() -> Pic:
    """A thrown snowball."""
    p = Pic(8, 8)
    p.ball(3.5, 3.5, 2.8, 2.8, [SNOW[2], SNOW[1], SNOW[0]], 0.3, 1.0)
    return p.outline(FROSTINK)


def icicle() -> Pic:
    """An icicle, hung by its head."""
    p = Pic(7, 16)
    for d in range(15):
        half = 2 if d < 5 else 1 if d < 11 else 0
        for dx in range(-half, half + 1):
            p.put(3 + dx, d, ICE[0] if dx == -half and half else ICE[3] if dx == half and half else ICE[1])
    return p.outline(FROSTINK)


def liftchair() -> Pic:
    """A ski lift's chair, a platform wide: a seat for him to stand on, a frame over it for its hanger."""
    p = Pic(48, 38)
    p.box(3, 1, 44, 2, METAL[1])
    p.box(3, 1, 4, 31, METAL[1])
    p.box(43, 1, 44, 31, METAL[1])
    p.box(3, 1, 44, 1, METAL[3])
    p.box(1, 31, 46, 36, ALPGLOW[1])
    p.box(1, 31, 46, 31, ALPGLOW[0])
    for x in range(8, 44, 8):
        p.box(x, 32, x, 36, LOG[1])
    return p.outline(FROSTINK)


GOATHEAD = """
....HHHH.......
..HHhhhhH......
.Hhh....hH.....
Hh......hbb....
H......bbaab...
.......baakaw..
......bbaaawwp.
.....bbbaaaww..
.....bbbbaa.w..
.....bbbb...w..
.....bbb.......
......b........
"""
GOATBODY = """
.c....aaaaaaaa.....
cbbaaaaaaaaaaaaab..
cbaaaaaaaaaaaaaaabb
.bbaaaaaaaaaaaaaabb
.bbbaaaaaaaaaaaabbb
.cbbbaaaaaaaaaabbbc
..ccbbbbbbbbbbbbcc.
...c.cb.bc.cb.bc...
"""


def stamp(p: Pic, rows: str, x0: int, y0: int, tones: dict[str, int]) -> None:
    """A hand-placed grid's dots onto p: a letter a tone, a dot clear."""
    for y, row in enumerate(rows.split()):
        for x, ch in enumerate(row):
            if ch != ".":
                p.put(x0 + x, y0 + y, tones[ch])


def goat(f: int, down: bool) -> Pic:
    """A mountain goat, facing right: horns swept back, a beard, a shaggy coat; down: its head low, at a charge,
    its legs flung out (f 0) or gathered."""
    p = Pic(34, 24)
    tones = {"a": LOG[0], "b": LOG[1], "c": LOG[2], "w": SNOW[0], "h": SNOW[2], "H": SNOW[4], "k": FROSTINK}
    tones["p"] = ALPGLOW[1]
    for i, x in enumerate((5, 8, 16, 19)):
        fore = 1 if i > 1 else -1
        s = (fore * 3 if f == 0 else -fore) if down else (1 if (i + f) % 2 else -1)
        p.line(x, 15, x + s, 20, LOG[2 - i % 2], 2)
        p.box(x + s, 21, x + s + 1, 21, FROSTINK)
    stamp(p, GOATBODY, 2, 8, tones)
    if down:
        stamp(p, GOATHEAD, 17, 6, tones)
    else:
        stamp(p, GOATHEAD, 14, 1, tones)
    return p.outline(FROSTINK)


def icewall(broken: bool) -> Pic:
    """A wall of ice across the road, five tiles tall, snow on its head; broken: its stump."""
    h = 22 if broken else 82
    p = Pic(18, h)
    for y in range(1, h - 1):
        for x in range(1, 17):
            if broken and y < 8 and (x * 7 + y * 3) % 11 < 5:
                continue
            p.put(x, y, SNOW[0] if y < 3 and not broken else ICE[0] if x == 1 else facet(x + 5, y + 7 + 82 - h))
    if not broken:
        p.line(5, 22, 9, 30, ICE[0])
        p.line(9, 30, 7, 40, ICE[0])
    return p.outline(FROSTINK)


def snowload() -> Pic:
    """A ledge's load of snow, the ledge's top under its row 16: three mounds along it, a lip hung over its right
    end."""
    p = Pic(76, 22)
    tones = [SNOW[2], SNOW[1], SNOW[0]]
    over = rect(0, 0, 75, 15)
    part(p, oval(52, 14, 17, 11) & over, tones, 3, 2)
    part(p, curved([(62, 16), (62, 7), (69, 7), (74, 13), (73, 20), (68, 21), (64, 19)]), tones, 2.5, 1.5, SNOW[2])
    part(p, oval(15, 15, 15, 9) & over, tones, 3, 2, SNOW[2])
    part(p, oval(33, 14, 15, 13) & over, tones, 3, 2, SNOW[2])
    fill(p, rect(64, 19, 72, 21), SNOW[3], {q for q in rect(64, 19, 72, 21) if p.get(*q)})
    return p.outline(FROSTINK)


def snowheap() -> Pic:
    """A slid load across the road: a heap of snow, a mound at either foot."""
    p = Pic(60, 34)
    tones = [SNOW[2], SNOW[1], SNOW[0]]
    here = rect(0, 0, 59, 33)
    part(p, curved([(8, 34), (12, 22), (20, 9), (30, 1), (40, 6), (47, 18), (52, 34)]) & here, tones, 4, 2.5)
    part(p, oval(12, 33, 12, 10) & here, tones, 2.5, 2, SNOW[2])
    part(p, oval(46, 33, 13, 12) & here, tones, 3, 2, SNOW[2])
    fill(p, stroke([(24, 12), (27, 17), (26, 22)], 1), SNOW[2])
    return p.outline(FROSTINK)


def whitecap() -> list[tuple[int, Pic, bool, int]]:
    """Whitecap Pass's own pictures, at the indices the Downs' own have; its keepsake after the works' last."""
    k, z = 126, 625
    out = [(0, alps(), False, -1), (1, firwood(), True, -1), (28, iceledge(), True, -1)]
    cap = lambda hem, d: SNOW[snowcap(hem, d, 2)] if snowcap(hem, d, 2) >= 0 else 0  # noqa: E731
    out.append((5, ring(188, 72, 93, cap, ICE, FROSTINK), True, -1))
    road = [SNOW[0], SNOW[1], ICE[1], ICE[2], ICE[4], ICE[5]]
    out += [(z + 112 + h * 2, ribbon(h, road, FROSTINK), True, -1) for h in range(2)]
    out += earth(z + 124, icy())
    out.append((z + 124 + 268, floe(), True, -1))
    # its ground again as bare ice, where the sands have theirs under the tide
    out += earth(z + 652, glassy())
    out += [
        (k + 36, fir(), True, -1),
        (k + 38, drift(), True, -1),
        (k + 40, pennant(0), True, -1),
        (k + 42, pennant(1), True, -1),
        (k + 44, boulder(), True, -1),
        (k + 46, shards(), True, -1),
        (k + 48, lantern(), True, -1),
        (k + 376, words("WHITECAP PASS", 178, 18, 1.53, 2.85), True, -1),
        (z + 1012, snapshot(), True, -1),
        (z + 1024, deepsnow(), True, -1),
        (z + 1026, geyservent(), True, -1),
        (z + 1028, geyserjet(0), True, -1),
        (z + 1030, geyserjet(1), True, -1),
        (z + 1032, snowball(0), True, -1),
        (z + 1034, snowball(1), True, -1),
        (z + 1036, fort(False), True, z + 1040),
        (z + 1038, fort(True), True, z + 1042),
        (z + 1044, lobbed(), True, -1),
        (z + 1046, icicle(), True, -1),
        (z + 1048, liftchair(), True, -1),
        (z + 1050, goat(0, False), True, z + 1052),
        (z + 1094, icewall(False), True, -1),
        (z + 1096, icewall(True), True, -1),
        (z + 1108, dunked(), True, -1),
        (z + 1212, snowload(), True, -1),
        (z + 1214, snowheap(), True, -1),
    ]
    out += [(z + 1098 + (lean + 2) * 2, frost(-lean), True, -1) for lean in range(-2, 3)]
    # a goat at a charge as the ground lies, like the sled: each stride, and each mirrored for the fall the other way
    for lean in range(-2, 3):
        for f in range(2):
            at = z + 1054 + ((lean + 2) * 2 + f) * 4
            out.append((at, leant(goat(f, True), -lean), True, z + 1054 + ((2 - lean) * 2 + f) * 4 + 2))
    # a sled as the ground lies: falling steeply to the right, gently, level, rising gently, steeply
    out += [(z + 1014 + (lean + 2) * 2, sledge(-lean), True, -1) for lean in range(-2, 3)]
    return out


# ---- Russet Wood: leaf litter over loam, roots woven through it, trunks in mist; its colours its own, a misty noon
# and the firefly night ----

# the mist: the canopy's shade at its top down to the floor's pale haze
MIST = both(
    96,
    "#B4C690 #C2CF9A #CFD8A4 #DCE0AE #E7E7B8 #F0ECC4 #F7F1D0",
    "#1C1E42 #22244A #282A52 #2E325A #343A64 #3A426E #424C78",
    4,
)
# the trunks' ranks, far, mid and near: each its lit edge, its body
TRUNK = both(
    103, "#B9C59A #A7B68A #A0A584 #8A9072 #857C68 #6C6456", "#363E6A #2E3460 #2E345E #262A52 #24284C #1C1E40", 4
)
CANOPY = both(109, "#8C6C44 #AC8850 #C8A460 #E4C878", "#1A1638 #221E44 #2A2850 #3A3A64", 4)  # shade to a gold leaf
FERN = both(113, "#A9C084 #8EAA76 #76946C", "#2C3C5C #243254 #1C2848", 4)  # far to near
LITTER = both(116, "#F6CC54 #E48E30 #BC5C28 #7E3828 #4A2030", "#C0B080 #A0806C #7C5858 #563A4C #32203C", 4)
LOAM = both(121, "#2A1620 #3A2028 #4E2E2C #6A4234", "#141024 #1E162C #2A1E36 #3A2A42", 4)
# a root: lit, body, shade, the gap under a crossing
ROOTS = both(125, "#DCAC74 #AC7248 #74462C #20101A", "#94889C #6A5C7C #483E60 #0E0A1E", 4)
OAKBARK = both(129, "#CC9058 #A46E44 #7E4E34 #5A362C #381E24", "#7E7490 #5E5478 #463E62 #322A4E #1E1838", 4)
MOSS = both(134, "#BCDC64 #7CB444 #46843C", "#74A488 #487C6C #2E5658", 4)
WOODINK = both(137, "#22101C", "#0C0A1C", 4)[0]  # the ground's own ink
FOLIAGE = both(
    141, "#FFE278 #F8BA40 #E88628 #C45624 #8C3028 #4A1C2C", "#BCB890 #9C9478 #7E6E68 #5E4E60 #42324E #20183A", 4
)
BERRY = both(157, "#EC5078 #A42858", "#A85088 #6C2C60", 4)
VINE = both(147, "#8CB44C", "#4C7C6C", 4) + both(156, "#4C7C3C", "#2C5054", 4)  # the game draws its vines in these
FIREFLY = both(163, "#B8E850 #F8FFA0", "#B8E850 #F8FFA0", 4)  # a jar's glass, what glows in it
WOODFLOOR = 104  # where the far ferns begin, down the backdrop
FROND = (2, 4, 6, 7, 6, 4, 3, 5, 7, 8, 6, 3, 2, 3, 5, 4)  # a rank of ferns' tops, by twos: a period of 32
LITTERHEM = (0, 1, 2, 2, 1, 0, 0, 1, 2, 1, 0, 0, 1, 2, 2, 1)  # the litter's foot, by twos: a period of 32


def trunks(p: Pic, x0: int, step: int, w0: int, rank: int, sway: int) -> None:
    """A rank of trunks down to the wood's floor: each a flat tone, a lit edge to its left, a bough's stub on the
    nearer ones. None stands across the picture's edge."""
    lit, tone = TRUNK[rank * 2], TRUNK[rank * 2 + 1]
    for i, x in enumerate(range(x0, 320 - step // 3 - w0 - 16, step)):
        x += (i * sway) % (step // 3)
        w = w0 + (i * 5) % (2 + rank)
        p.box(x, 0, x + w - 1, WOODFLOOR + 4, tone)
        p.box(x, 0, x + rank // 2, WOODFLOOR + 4, lit)
        if rank:
            y = 24 + (i * 37) % 44
            p.line(x + w, y, x + w + 9 + rank * 3, y - 8 - rank * 2, tone, 2)


def shafts(p: Pic) -> None:
    """Shafts of light down from the left: the mist and the far trunks they cross are lighter."""
    lift = {MIST[i]: MIST[min(i + 2, 6)] for i in range(7)} | {TRUNK[1]: MIST[2], TRUNK[0]: MIST[5]}
    for x0, w in ((34, 20), (128, 26), (236, 16)):
        for y in range(10, WOODFLOOR + 4):
            for x in range(x0 + y * 2 // 5, x0 + y * 2 // 5 + w):
                p.put(x, y, lift.get(p.get(x, y), p.get(x, y)))


def canopy(p: Pic) -> None:
    """The far canopy, of period 320: a band along the top, its hem in clumps, a gold leaf here and there."""
    p.box(0, 0, 319, 8, CANOPY[1])
    for i in range(20):
        for wrap in (-320, 0, 320):
            p.ball(i * 16 + 4 + wrap, 4 + (i * 5) % 7, 10 + (i * 3) % 5, 9 + (i * 3) % 4, CANOPY[:3])
    for i in range(40):
        x, y = (i * 67 + 11) % 320, (i * 29) % 14
        if p.get(x, y) in CANOPY:
            p.put(x, y, CANOPY[3])


def bracken(p: Pic, y0: int, tone: int, shift: int) -> None:
    """A rank of ferns across p from y0 down, its tops in fronds."""
    for x in range(p.w):
        p.box(x, y0 - FROND[((x + shift) // 2) % 16], x, p.h - 1, tone)


def mistwood() -> Pic:
    """The wood's far backdrop, of period 320: mist in bands, three ranks of trunks into it, shafts of light across
    the far one, the canopy over them, ferns at their feet."""
    p = Pic(320, 200)
    edges = [0, 21, 39, 56, 72, 86, 98, 200]
    for y in range(200):
        p.box(0, y, 319, y, MIST[max(i for i in range(7) if edges[i] <= y)])
    trunks(p, 4, 19, 3, 0, 5)
    shafts(p)
    canopy(p)
    trunks(p, 18, 43, 7, 1, 11)
    bracken(p, WOODFLOOR, FERN[0], 0)
    trunks(p, 52, 131, 13, 2, 29)
    bracken(p, WOODFLOOR + 8, FERN[1], 7)
    return p


def bent(p: Pic, a: Pt, b: Pt, d: Pt, tone: int, w: int) -> list[Pt]:
    """A bent line on p from a to d, pulled toward b; its points."""
    pts = [
        (
            (1 - t) ** 2 * a[0] + 2 * t * (1 - t) * b[0] + t * t * d[0],
            (1 - t) ** 2 * a[1] + 2 * t * (1 - t) * b[1] + t * t * d[1],
        )
        for t in (i / 40 for i in range(41))
    ]
    for (xa, ya), (xb, yb) in itertools.pairwise(pts):
        p.line(xa, ya, xb, yb, tone, w)
    return pts


def spray(p: Pic, x: int, foot: int, h: int, r: list[int]) -> None:
    """A fern at x, its foot at foot: five fronds arched out h high, r's dark for each and its light along its rib."""
    for lean, lift in ((-1.0, 0.5), (-0.55, 0.9), (0.0, 1.0), (0.55, 0.9), (1.0, 0.5)):
        tip = (x + lean * h * 0.9, foot - lift * h + abs(lean) * h * 0.35)
        mid = (x + lean * h * 0.45, foot - lift * h * 1.25)
        bent(p, (x, foot), mid, tip, r[1], 3)
        bent(p, (x, foot), mid, tip, r[0], 1)


def undergrowth() -> Pic:
    """The wood's nearer band, masked, of period 320, which goes by faster: the nearest ferns, taller ones among
    them."""
    p = Pic(320, 80)
    bracken(p, 24, FERN[2], 15)
    for i, x in enumerate((22, 74, 139, 191, 248, 297)):
        for wrap in (-320, 0, 320):
            spray(p, x + wrap, 26, 15 + (i * 7) % 6, [FERN[1], FERN[2]])
    return p


def litter(hem: int, d: int) -> int:
    """The leaf litter's tone d pixels under the surface at column hem, of period 64; 0 under it: bright leaves on
    top, russet under them, a dark hem."""
    hem %= 64
    foot = 7 + LITTERHEM[(hem // 2) % 16]
    if d > foot:
        return 0
    if d <= 0:
        return LITTER[0] if (hem // 2) % 3 == 0 else LITTER[1]
    if d < 4:
        return LITTER[0] if (hem * 7 + d * 5) % 11 == 0 else LITTER[2] if (hem * 3 + d * 7) % 8 == 0 else LITTER[1]
    if d < foot:
        return LITTER[3] if (hem + d) % 6 == 0 or d == foot - 1 else LITTER[2]
    return LITTER[4]


def loam(x: int, y: int) -> int:
    """The loam's tone at a world pixel, repeating every 64 across and 48 down: a braid of two roots every 24 down,
    one over the other turn about, a rootlet hung from it, specks between."""
    x, y = x % 64, y % 48
    k = y // 24
    yc = k * 24 + 12
    u = x + 16 * (k % 2)
    over = (u + 8) // 16 % 2
    off = [y - (yc + 5 * math.sin(2 * math.pi * u / 32 + s * math.pi)) for s in (0, 1)]
    for s in (over, 1 - over):
        if -2 <= off[s] < 2:
            if s != over and -3.4 <= off[over] < 3.4:
                return ROOTS[3]
            return ROOTS[(0, 1, 1, 2)[math.floor(off[s] + 2)]]
    if u % 32 == 8 and 5 <= y - yc <= 10:
        return ROOTS[2]
    if (x * 13 + y * 7) % 53 == 0:
        return LOAM[3]
    if (x * 5 + y * 11) % 37 < 2:
        return LOAM[2]
    return LOAM[1]


def loamy() -> Skin:
    """The wood's ground: leaf litter over loam."""
    return Skin(lambda x, y, d, hem: litter(hem, d) or loam(x, y), loam, WOODINK, (LITTER[3], LOAM[3], LOAM[0]))


def mossledge() -> Pic:
    """A floating shelf's 16: moss on a slab of bark, rootlets hung under it."""
    p = Pic(16, 16)
    for x in range(16):
        for y, tone in enumerate((MOSS[0], MOSS[1], MOSS[1], MOSS[2])):
            p.put(x, y, tone)
        p.box(x, 4, x, 8, OAKBARK[1] if (x // 4) % 2 else OAKBARK[2])
        if x % 5 == 2:
            p.put(x, 4, MOSS[2])
        p.put(x, 9, WOODINK)
        n = (0, 2, 0, 0, 4, 0, 1, 0, 0, 3, 0, 0, 2, 0, 4, 0)[x]
        for d in range(n):
            p.put(x, 10 + d, ROOTS[1] if d < 2 else ROOTS[2])
    return p


def bough() -> Pic:
    """The wood's moving platform: a length of bough, moss along its top, a furrow down it."""
    p = Pic(48, 12)
    for x in range(1, 47):
        mossy = x % 11 < 8
        p.put(x, 1, MOSS[0] if mossy else OAKBARK[0])
        p.put(x, 2, MOSS[1] if mossy else OAKBARK[1])
        p.put(x, 3, MOSS[2] if mossy and x % 5 == 0 else OAKBARK[1])
        p.box(x, 4, x, 8, OAKBARK[2])
        if x % 9 < 6:
            p.put(x, 6, OAKBARK[4])
        p.put(x, 9, OAKBARK[3])
    return p.outline(WOODINK)


def fern() -> Pic:
    """A fern where the Downs have a bush."""
    p = Pic(40, 20)
    spray(p, 20, 18, 17, [MOSS[0], MOSS[1]])
    return p.outline(WOODINK)


CAP = """
...RRRR...
..RRwRRr..
.RRRRRwRr.
.RwRRRRRr.
RRRRwRRRrr
rrrrrrrrrr
"""
STALK = """
...sSS...
....sS...
....sS...
...ssS...
...sSS...
..gsSSg..
"""


def toadstool(f: int) -> Pic:
    """A toadstool where the Downs have a flower: frame f, its cap a pixel aside."""
    p = Pic(12, 16)
    tones = {"R": BERRY[0], "r": BERRY[1], "w": WHITE, "s": FUR[4], "S": FUR[1], "g": MOSS[1]}
    stamp(p, STALK, 1, 9, tones)
    stamp(p, CAP, 1 + f, 3, tones)
    return p.outline(WOODINK)


def sapling(sprung: bool) -> Pic:
    """A sapling, its foot at the picture's left: bowed over to the right, its crown down on the ground, a
    catapult; sprung: upright, as it stands when it has thrown."""
    p = Pic(60, 50)
    tip = (9.0, 8.0) if sprung else (44.0, 41.0)
    pts = bent(p, (6, 49), (6, 22) if sprung else (10, -14), tip, OAKBARK[2], 2)
    for x, y in pts[::2]:
        p.put(round(x), round(y), OAKBARK[0])
    p.outline(WOODINK)
    cx, cy = round(tip[0]), round(tip[1])
    crown = Pic(60, 48)
    leafy(crown, [(cx + 5, cy - 1, 7), (cx - 3, cy - 4, 6)], [*MOSS, WOODINK])
    paste(p, crown, 0, 0)
    return p


def bramble(cut: bool) -> Pic:
    """Bramble: three canes arched over, thorns along them, leaves and berries; cut: their stubs."""
    p = Pic(38, 26)
    canes = [((3, 25), (9, -2), (23, 25)), ((13, 25), (24, -6), (35, 22)), ((7, 25), (20, 8), (31, 25))]
    for n, (a, b, d) in enumerate(canes):
        pts = bent(Pic(38, 26) if cut else p, a, b, d, LITTER[3], 2)
        if cut:
            for (xa, ya), (xb, yb) in itertools.pairwise(pts[: 5 + n]):
                p.line(xa, ya, xb, yb, LITTER[3], 2)
            p.put(round(pts[4 + n][0]), round(pts[4 + n][1]), ROOTS[0])
            continue
        for i in range(3, 38, 5):
            p.put(round(pts[i][0]) + (1 if i % 2 else -1), round(pts[i][1]) - 2, ROOTS[0])
        for i in range(6 + n * 2, 36, 11):
            x, y = round(pts[i][0]), round(pts[i][1])
            p.box(x - 2, y + 1, x - 1, y + 2, MOSS[2])
            p.put(x - 2, y + 1, MOSS[1])
        for i in range(11 + n * 3, 36, 13):
            x, y = round(pts[i][0]), round(pts[i][1])
            p.box(x + 1, y + 1, x + 2, y + 2, BERRY[1])
            p.put(x + 1, y + 1, BERRY[0])
    return p.outline(WOODINK)


def hollowlog() -> Pic:
    """A hollow log lying along the ground, wide enough for him curled, its near end open, moss along its top."""
    p = Pic(64, 32)
    for x in range(10, 62):
        for d in range(28):
            if x > 59 and d in (0, 27):
                continue
            mossy = x % 11 < 7 and x > 18
            tone = (MOSS[d] if mossy else OAKBARK[d]) if d < 2 else OAKBARK[1 if d < 9 else 2 if d < 21 else 3]
            if d in (8, 14, 20) and (x * 3) % 17 > 4:
                tone = OAKBARK[4]
            if mossy and d == 2 and x % 5 == 0:
                tone = MOSS[1]
            p.put(x, 2 + d, tone)
    for y in range(1, 31):  # its open end: a ring of pale wood round the dark
        for x in range(1, 19):
            d = ((x - 9.5) / 8.5) ** 2 + ((y - 15.5) / 14.5) ** 2
            if d <= 1:
                p.put(x, y, WOODINK if d < 0.5 else OAKBARK[3] if d < 0.64 else OAKBARK[0])
    return p.outline(WOODINK)


SQUIRREL = both(159, "#F4A454 #D46E2C #8E3C24 #FFF0D0", "#AC8870 #845C5C #563648 #C8C4D0", 4)  # light to dark, belly
SQUIRRELSAT = (
    "............RRR... ...........RrrrR.. ..d.d.....RrrrrrR. .drdrd....Rrrrrrr. .RRrrr....rrrrrrr. "
    "RRkrrrr....rrrrrd. nrrrrrr.....rrrrd. .wwrrrrr....rrrdd. ..wwrrrrr...rrrd.. ..awwrrrrr.rrrdd.. "
    ".aaawrrrrrrrrdd... .AAawwrrrrrrdd.... ..A.wwrrrrrrd..... ....wrrrrrrrd..... ...ddrr..drrd..... "
    "..ddd....dddd....."
)


def squirrel(held: bool, right: bool) -> Pic:
    """A squirrel sat up, its tail over its back: an acorn held out before it or not, turned left or right."""
    rows = SQUIRRELSAT if held else SQUIRRELSAT.replace("a", ".").replace("A", ".")
    if right:
        rows = " ".join(r[::-1] for r in rows.split())
    a, b, c, d = SQUIRREL
    p = Pic(20, 18)
    stamp(p, rows, 1, 1, {"R": a, "r": b, "d": c, "w": d, "k": WOODINK, "n": WOODINK, "a": LITTER[1], "A": LITTER[0]})
    return p.outline(WOODINK)


def acorn() -> Pic:
    """An acorn in the air: its cup over it."""
    p = Pic(8, 8)
    stamp(p, ".ddd. ddddd .AAa. .AAa. ..a..", 1, 1, {"d": SQUIRREL[2], "A": LITTER[0], "a": LITTER[1]})
    return p.outline(WOODINK)


def mound() -> Pic:
    """A burrow's mound: thrown loam, the hole in its face, leaves on it."""
    p = Pic(34, 14)
    for y in range(1, 13):
        for x in range(1, 33):
            d = ((x - 16.5) / 15.5) ** 2 + ((y - 12) / 11) ** 2
            if d <= 1:
                p.put(x, y, LOAM[3] if d > 0.62 and x < 16 and y < 9 else LOAM[2])
    for y in range(4, 13):
        for x in range(9, 24):
            if ((x - 16.5) / 6.5) ** 2 + ((y - 9) / 4.5) ** 2 <= 1:
                p.put(x, y, WOODINK)
    for x, y in ((5, 8), (27, 7), (24, 3), (12, 2)):
        p.box(x, y, x + 1, y, LITTER[(x + y) % 2])
    return p.outline(WOODINK)


# ---- the wood's chase: its leaves, the great oak, the Bramble twins ----
LEAFDOTS = ((1, 0, 0), (2, 0, 0), (0, 1, 0), (1, 1, 1), (2, 1, 1), (3, 1, 1), (1, 2, 1), (2, 2, 1))  # lit 0, body 1
MOSSHEM = (2, 3, 4, 3, 2, 2, 3, 5, 4, 2, 1, 2, 3, 3, 2, 4, 5, 3, 2, 2, 3, 4, 2, 1)  # moss hung down, by its column
CAPLIT = both(165, "#FF9CB4", "#D279B2", 4)[0]  # a berry cap's light


def mirrored(p: Pic) -> Pic:
    q = Pic(p.w, p.h)
    q.p = [row[::-1] for row in p.p]
    return q


def leafy(p: Pic, parts: list[tuple[int, int, int]], r: list[int]) -> None:
    """Leaves in masses (x, y, radius): the dark between leaves, and leaves over it in rows, each a lit top and a
    body, those to a mass's upper left the lighter, those at its edge standing out of it. r: a leaf's light, its
    body, the dark between, the line round the lot."""
    mass: Mask = set()
    for px, py, rad in parts:
        mass |= oval(px, py, rad, rad * 0.9)
    fill(p, mass, r[2])
    xs, ys = [x for x, _ in mass], [y for _, y in mass]
    for j in range(min(ys) // 3 - 1, max(ys) // 3 + 2):
        for i in range(min(xs) // 5 - 1, max(xs) // 5 + 2):
            x, y = i * 5 + (j % 2) * 2 + (j * 7 + i * 3) % 3 - 1, j * 3 + (i * 5 + j) % 2
            if (x + 1, y + 1) not in mass:
                continue
            lit = any((x - px + 2) ** 2 + (y - py + 3) ** 2 < (rad * 0.8) ** 2 for px, py, rad in parts)
            for dx, dy, tone in LEAFDOTS:
                p.put(x + dx, y + dy, r[tone + (0 if lit else 1)])
    p.outline(r[3])


def sprig(p: Pic, pts: list[Pt], n: int, r: list[int]) -> None:
    """A twig's leaves: n along pts, a side each in turn, and one at its end; each lit to its upper left, the line
    round it its own. r: a leaf's light, its body, its line."""
    path = spline(pts, 6, closed=False)
    for i in range(n + 1):
        j = min(len(path) - 1, (i + 1) * (len(path) - 1) // n)
        (xa, ya), (xb, yb) = path[j - 1], path[j]
        way = math.atan2(yb - ya, xb - xa) + (0 if i == n else 0.95 if i % 2 else -0.95)
        cx, cy = xb + math.cos(way) * 2.8, yb + math.sin(way) * 2.8
        q = Pic(p.w, p.h)
        for x, y in oval(cx, cy, 3.4, 1.8, way):
            q.put(x, y, r[0] if (x + 0.5 - cx) * 0.55 + (y + 0.5 - cy) * 0.83 < 0.3 else r[1])
        paste(p, q.outline(r[2]), 0, 0)


def mossed(p: Pic, blobs: list[tuple[float, float, float, float]]) -> None:
    """A patch of moss on what is drawn: blobs (x, y, half width, half height) run together, hung down a little
    by its column, its top lit and its hem dark."""
    m: Mask = set()
    for cx, cy, rx, ry in blobs:
        m |= oval(cx, cy, rx, ry)
    for x in {x for x, _ in m}:
        foot = max(y for xx, y in m if xx == x)
        m |= {(x, foot + d) for d in range(1, MOSSHEM[x % len(MOSSHEM)] - 1)}
    m = {q for q in m if p.get(*q) and p.get(*q) != WOODINK}
    for x, y in m:
        p.put(x, y, MOSS[0] if (x, y - 1) not in m else MOSS[2] if (x, y + 1) not in m else MOSS[1])


SPRINGCAP = {
    False: """
......hhRR......
....hhhRRRRR....
...hhwwRRRRRr...
..hRRwwRRRRwRr..
.hRRRRRRRRRRRrr.
.RRwRRRRRwwRRrr.
RRRRRRRRRwwRRrrr
.rrrrrrrrrrrrrr.
...gGgGGgGGgg...
""",
    True: """
.....hhhRRR.....
..hhhwwRRRRwRr..
hhRRRwwRRRRRRRrr
RRwRRRRRRwwRRrrr
.rrrrrrrrrrrrrr.
...gGgGGgGGgg...
""",
}
SPRINGSTALK = {
    False: """
...SSSs...
...SSSs...
..SSSSs...
..SSSSss..
m.SSSSssm.
mmSSSSssmm
""",
    True: """
..SSSSss..
m.SSSSssm.
mmSSSSssmm
""",
}


def toadspring(pressed: bool) -> Pic:
    """The wood's spring, 16 by 16: a broad toadstool, its cap down on its stalk when trodden."""
    p = Pic(16, 16)
    tones = {"R": BERRY[0], "r": BERRY[1], "h": CAPLIT, "w": WHITE, "S": FUR[5], "s": FUR[2]}
    tones |= {"g": FUR[1], "G": FUR[3], "m": MOSS[1]}
    cap, stalk = SPRINGCAP[pressed].split(), SPRINGSTALK[pressed].split()
    stamp(p, SPRINGSTALK[pressed], 3, 16 - len(stalk), tones)
    stamp(p, SPRINGCAP[pressed], 0, 16 - len(stalk) - len(cap), tones)
    return p.outline(WOODINK)


TAU = 2 * math.pi
OAKW, OAKMID, OAKFLARE = 152, 76, 44  # the oak's foot's picture: its width, its middle, the rows its foot flares over
OAKTILE, OAKFOOT = 64, 48  # the rows of a length of its trunk, which repeats, and of its foot
PLATE = 0.24  # a plate of bark's width, round the trunk
LIMBTOP, LIMBFLAT, LIMBHID = 14, 48, 8  # a limb's picture: the row stood on, its flat's length, what the trunk hides


def girth(y: int, foot: int) -> tuple[float, float]:
    """The trunk's edges at row y, from its middle: a period of 64 down it, flared over its last rows to foot."""
    t = max(0.0, (y - (foot - OAKFLARE)) / OAKFLARE)
    left = -30 - 1.5 * math.sin(TAU * y / 64 + 0.5) - 24 * t**2.6
    return left, 30 + 1.5 * math.sin(TAU * y / 64 + 2.4) + 21 * t**2.6


def furrows(y: int, m: float, half: float) -> list[int]:
    """Where row y's furrows fall: each wanders with a period of 32 and of 64, its neighbours the other way, so the
    plates between them pinch and swell. The first is plate -5's right edge."""
    out = []
    for k in range(-5, 5):
        way = 0.21 * (-1) ** k * math.sin(TAU * y / 32 + 1.7 * k) + 0.17 * math.sin(TAU * y / 64 + 2.9 * k)
        out.append(math.floor(m + half * math.sin(PLATE * (k + 0.5 + way))))
    return out


def oak(w: int, mid: int, h: int, foot: int) -> Pic:
    """The great oak's trunk, h rows, its foot at row foot: plates of bark between furrows, each plate one tone
    by where it lies round the trunk, lit from the upper left, a rim of light on its far edge."""
    p = Pic(w, h)
    for y in range(h):
        left, right = girth(y, foot)
        m, half = mid + (left + right) / 2, (right - left) / 2
        fx = furrows(y, m, half)
        x0, x1 = math.ceil(m - half - 0.5), math.floor(m + half - 0.5)
        for x in range(x0, x1 + 1):
            k = next((i - 5 for i, f in enumerate(fx) if x <= f), 5)
            base = 2 if k <= -5 else 1 if k <= -2 else 2 if k <= 1 else 3
            tone = base
            if x in fx:
                tone = min(base + 2, 4)
            elif x - 1 in fx and base < 3:
                tone = base - 1
            elif (y + 11 * k) % 32 == 0 or (y + 7 * k + 17) % 64 == 0:
                tone = base + 1
            if x >= x1 - 1 and tone == 3:
                tone = 2
            p.put(x, y, OAKBARK[tone])
    return p


def oaktrunk() -> Pic:
    """A length of the great oak's trunk, 66 by 64: it repeats up it."""
    return oak(66, 33, OAKTILE, 10**9).outline(WOODINK)


def oakfoot() -> Pic:
    """The great oak's foot, 152 by 48, under a length of its trunk: flared into roots, moss on their backs and
    up its lit side, the dark under them."""
    h = OAKTILE + OAKFOOT
    p = oak(OAKW, OAKMID, h, h)
    lit, shaded = [OAKBARK[2], OAKBARK[1], OAKBARK[0]], [OAKBARK[4], OAKBARK[3], OAKBARK[2]]
    for x, y, rx, ry in ((OAKMID - 27, h, 9, 5), (OAKMID + 3, h, 8, 4), (OAKMID + 33, h, 8, 5)):
        fill(p, oval(x, y, rx, ry), WOODINK)
    roots = [
        ([(-30, -22), (-46, -9), (-66, -2.5)], 10, 4, lit),
        ([(31, -21), (45, -9), (63, -2.5)], 10, 4, shaded),
        ([(-12, -24), (-14, -10), (-22, -2.5)], 9, 5, lit),
        ([(13, -22), (17, -9), (27, -2.5)], 9, 5, shaded),
    ]
    for pts, w0, w1, tones in roots:
        m = stroke(spline([(OAKMID + x, h + y) for x, y in pts], 8, closed=False), w0, w1)
        part(p, m, tones, sh=1.6, hl=1.2, line=WOODINK)
        for x, y in m:
            if (x, y - 1) not in m and x % 9 < 5 and y < h - 3:
                p.put(x, y, MOSS[0] if x % 9 < 3 else MOSS[1])
    p.outline(WOODINK)
    mossed(p, [(OAKMID - 36, h - 33, 6, 4), (OAKMID - 28, h - 29, 7, 5), (OAKMID - 40, h - 25, 5, 3)])
    return cut(p, 0, OAKTILE, OAKW, OAKFOOT)


def limb(reach: int) -> Pic:
    """A limb of the great oak, out to the right from behind the trunk: reach rows of rise, then 48 flat to
    stand on, mossed, then twigs in leaf. Its flat's top is row LIMBTOP, and begins LIMBHID + 4 + reach along."""
    a = LIMBHID + 4 + reach
    w, e = a + LIMBFLAT + 28, a + LIMBFLAT
    p = Pic(w, 50 if reach < 30 else 58)
    mid = LIMBTOP + 4.5
    spine = [(0.0, mid + 5 + reach * 0.35), (6 + reach * 0.5, mid + 2.5 + reach * 0.14), (a + 2.0, mid), (e, mid)]
    m = stroke(spline([*spine, (e + 9, mid - 2.5), (e + 17, mid - 7.5)], 8, closed=False), 23, 3.2)
    m |= rect(a, LIMBTOP, e, LIMBTOP + 8)
    m |= stroke(spline([(e - 6, mid + 2), (e + 4, mid + 7), (e + 12, mid + 8)], 6, closed=False), 3.4, 2)
    m = {(x, y) for x, y in m if 0 <= x < w and not (a <= x <= e and y < LIMBTOP)}
    for x in range(w):
        col = sorted(y for xx, y in m if xx == x)
        if not col:
            continue
        top, bot = col[0], col[-1]
        for y in col:
            d, u = y - top, bot - y
            tone = 1 if d < 2 else 4 if u < 1 else 3 if u < 3 else 2
            if tone == 2 and (x + y * 5) % 13 < 7 and (y - top) % 4 == 3:
                tone = 3  # the bark's grain along it
            if tone == 1 and d == 0 and x % 7 < 3:
                tone = 0
            p.put(x, y, OAKBARK[tone])
        if a <= x <= e + 2:
            hem = MOSSHEM[x % len(MOSSHEM)] if x <= e else 2
            for d in range(min(hem, bot - top)):
                p.put(x, top + d, MOSS[0] if d == 0 else MOSS[1] if d < hem - 1 else MOSS[2])
    p.outline(WOODINK)
    leaves = [FOLIAGE[1], FOLIAGE[3], FOLIAGE[5]]
    sprig(p, [(e - 2, mid + 4), (e + 4, mid + 7), (e + 13, mid + 8.5)], 3, leaves)
    sprig(p, [(e + 3, mid - 1), (e + 9, mid - 2.5), (e + 17, mid - 7.5)], 4, leaves)
    return p


def knot() -> Pic:
    """A hollow in the oak: a lip of bark round it, light on its far wall, two eyes in its dark."""
    p = Pic(17, 20)
    part(p, oval(8.5, 10, 8, 9.5), [OAKBARK[4], OAKBARK[2], OAKBARK[0]], sh=2.0, hl=1.6)
    part(p, oval(8.5, 10.3, 5.2, 6.6), [OAKBARK[3], WOODINK], sh=1.2)
    dots(p, FIREFLY[1], (6, 10), (10, 10))
    return p


BRACKET = """
..LLLLLLL...
.LLlllllllL.
llllllllllll
.dddddddddd.
...DDDDDD...
"""


def bracket() -> Pic:
    """A bracket fungus, a shelf out from the bark."""
    p = Pic(12, 5)
    stamp(p, BRACKET, 0, 0, {"L": LITTER[0], "l": LITTER[1], "d": LITTER[2], "D": LITTER[3]})
    return p.outline(WOODINK)


TWINZ = 25 / 33  # the twins are drawn in a room of 33 by 34, then to this: three quarters the size
TWINEDGE, TWINHEAD = 3, 2  # the room their paws reach into: at their picture's sides, over their heads
TCOAT, TFAR = [HAIR[1], HAIR[2], HAIR[3]], [HAIR[0], HAIR[1], HAIR[2]]  # a brown tabby: its near side, its far
TSTRIPE, TSEAM = WOOD[1], HAIR[0]
HOOD = [DARK[1], DARK[2]]  # a burglar's mask, and its top edge
KERCHIEF = ([PETAL[0], PETAL[1], PETAL[2]], [GRASS[1], GRASS[2], GRASS[3]])  # berry, leaf
TWINPOSES = ("grin", "reach", "throw", "shock", "sulk")
TWINFACE = {
    "grin": """
mmmmmmmmmmmmm
MKKKMMMMMKKKM
MwwKMMMMMwwKM
.MMMMnnnMMMM.
K...........K
.KKKKKKKKKKK.
..wKwKwKwKw..
...KKKKKKK...
""",
    "reach": """
mmmmmmmmmmmmm
MwKwMMMMMwKwM
MwwwMMMMMwwwM
.MMMMnnnMMMM.
....KKKKK....
....KRRRK....
.....KtK.....
......K......
""",
    "throw": """
mmmmmmmmmmmmm
MMwMMMMMMwwKM
MwMwMMMMMwwKM
.MMMMnnnMMMM.
K............
.KKKKKKKKKKK.
..wwwwwwwtt..
...KKKKKKttK.
""",
    "shock": """
mwwwmmmmmwwwm
MwKwMMMMMwKwM
MwwwMMMMMwwwM
.MMMMnnnMMMM.
...KKKKKKK...
...KRRRRRK...
...KRtttRK...
....KKKKK....
""",
    "sulk": """
mmmmmmmmmmmmm
MKKMMMMMMMKKM
MwwKMMMMMKwwM
.MMMMnnnMMMM.
.............
....KKKKK....
...K.....K...
.............
""",
}


def along(pts: list[Pt]) -> list[Pt]:
    """Points of a twin as it is drawn, where they fall in its picture."""
    return [(x * TWINZ + TWINEDGE, y * TWINZ + TWINHEAD) for x, y in pts]


def lump(cx: float, cy: float, rx: float, ry: float) -> Mask:
    """An oval of a twin as it is drawn, in its picture: never thinner than a dot or two."""
    return oval(cx * TWINZ + TWINEDGE, cy * TWINZ + TWINHEAD, max(1.2, rx * TWINZ), max(1.0, ry * TWINZ))


def girt(w: float) -> float:
    """A line of a twin as it is drawn, as wide in its picture: never under two dots."""
    return max(1.9, w * TWINZ)


def foreleg(p: Pic, pts: list[Pt], tones: list[int], paw: float = 2.2) -> None:
    """A twin's foreleg, thin as a noodle, from its shoulder through pts, a cream mitt at its end."""
    path = spline(along(pts), 6, closed=False)
    part(p, stroke(path, girt(3.4), girt(3.0)), tones, sh=0.9, hl=0.7 if len(tones) > 2 else 0, line=TSEAM)
    (xa, ya), (xb, yb) = path[-4], path[-1]
    d = math.hypot(xb - xa, yb - ya) or 1
    mitt = oval(xb + (xb - xa) / d * paw / 2, yb + (yb - ya) / d * paw / 2, paw, paw - 0.2)
    part(p, mitt, BIB, sh=0.8, line=TSEAM)


def twin(pose: str, who: int) -> Pic:
    """A Bramble twin, 31 by 28, its feet on row 26: a brown tabby burglar sat on a limb, a mask over its eyes
    tied behind its head, a kerchief of its own colour, a head too big for it, forelegs thin as noodles, big cream
    feet, a kink in its tail. who: 0 in berry and facing right, 1 in leaf and facing left. pose: grin, its paws
    on its hips; reach, both up for the ball; throw, one flung out the way it faces, its tongue out; shock, the
    ball caught from it, its fur and tail on end; sulk, its ears flat and its forelegs folded."""
    p = Pic(25 + 2 * TWINEDGE, 26 + TWINHEAD)
    z, shock, flat = TWINZ, pose == "shock", pose == "sulk"
    curl = [(10.8, 31.2), (6.2, 31.4), (3.4, 28.6), (3.6, 25), (5.8, 22.6), (3.8, 20.4)]
    if shock:
        curl = [(10.8, 31.2), (6.0, 31.2), (3.6, 27.5), (3.6, 19.5)]
    path = spline(along(curl), 8, closed=False)
    tail = part(p, stroke(path, girt(5.6 if shock else 3.4), girt(5.0 if shock else 2.4)), TFAR, sh=1.0)
    for i in range(5, len(path) - 5, 6):
        fill(p, stroke(path[i : i + 2], 6), TSTRIPE, tail)
    if not shock:
        fill(p, stroke(path[-4:], girt(2.6), girt(2.4)), TSEAM, tail)
    left = [(14.2, 18.5), (11.4, 22), (9.6, 26), (9.3, 29.6), (11.3, 32.4)]
    body = curved(along([*left, *((33 - x, y) for x, y in left[::-1])]))
    part(p, body, TCOAT, sh=1.8 * z, hl=1.0 * z, line=TSEAM)
    for x, y in ((10.4, 23.4), (9.8, 26.2)):
        fill(p, stroke(along([(x, y), (x + 2.2, y + 0.7)]), 1.3 * z), TSTRIPE, body)
        fill(p, stroke(along([(33 - x, y), (33 - x - 2.0, y + 0.7)]), 1.3 * z), TSTRIPE, body)
    part(p, lump(11.7, 29.4, 3.3, 3.5), TFAR, sh=1.2 * z, line=TSEAM, within=body)
    part(p, lump(21.3, 29.4, 3.4, 3.5), TCOAT, sh=1.4 * z, hl=0.9 * z, line=TSEAM, within=body)
    for cx in (10.4, 22.6):
        part(p, lump(cx, 31.7, 3.5, 1.8), BIB, sh=0.7, line=TSEAM)
    if pose == "throw":
        part(p, stroke(along([(15.0, 24), (15.0, 30.4)]), girt(3.0)), TFAR, 0.7, line=TSEAM)
        part(p, lump(15.0, 31.8, 2.1, 1.6), BIB, sh=0.7, line=TSEAM)
    chest = [(14.3, 19.5), (13.1, 22), (13.3, 24.6), (14.9, 26.6), (16.5, 27.2), (18.1, 26.6), (19.7, 24.6), (19.9, 22)]
    part(p, curved(along([*chest, (18.7, 19.5)])), BIB, sh=1.2 * z, line=TSEAM)
    for s in (-1, 1):
        tones = TFAR if s < 0 else TCOAT
        if pose == "grin":
            foreleg(p, [(16.5 + s * 5.2, 21.6), (16.5 + s * 9.6, 24.4), (16.5 + s * 7.2, 27.0)], tones, 1.8)
        if pose == "reach":
            foreleg(p, [(16.5 + s * 5.4, 22.2), (16.5 + s * 11.6, 20.0), (16.5 + s * 13.0, 7.0)], tones)
        if shock:
            foreleg(p, [(16.5 + s * 5.4, 22.4), (16.5 + s * 9.6, 22.6), (16.5 + s * 12.0, 18.6)], tones)
    if pose == "throw":
        foreleg(p, [(21.9, 22.2), (26.4, 20.6), (28.0, 13.2)], TCOAT)
    k = KERCHIEF[who]
    scarf = rect(11, 16, 19, 17) | poly(along([(11.6, 19.2), (21.4, 19.2), (16.5, 25.6)]))
    part(p, scarf, k, sh=1.0 * z, hl=0.8 * z, line=INK)
    part(p, rect(9, 16, 10, 17) | rect(8, 18, 9, 18), k[:2], sh=0.6, line=INK)
    for end in ((2.6, 7.4), (3.4, 12.8)):  # the mask's ends, tied behind its head
        fill(p, stroke(along([(8.0, 10.0), end]), girt(2.6), girt(2.0)), HOOD[1])
    fill(p, lump(6.4, 10.0, 1.6, 1.6), HOOD[0])
    for s in (-1, 1):
        cx = 16.5 + s * 7.4
        ear = [(cx - s * 3.4, 6.2), (cx + s * 2.4, 10.4), (cx + s * 2.8, 0.4 if shock else 0.8)]
        inner = [(cx - s * 1.2, 6.4), (cx + s * 1.5, 8.4), (cx + s * 1.8, 2.8)]
        if flat:
            ear = [(cx - s * 3.0, 5.4), (cx + s * 1.8, 10.0), (cx + s * 5.6, 5.4)]
            inner = [(cx - s * 0.6, 6.2), (cx + s * 1.5, 8.2), (cx + s * 3.6, 6.2)]
        part(p, poly(along(ear)), TCOAT, 0.8, line=TSEAM)
        fill(p, poly(along(inner)), BLUSH)
    if not flat:
        p.put(8, 5, 0)  # a bite out of one ear
    lh = [(16.5, 4.6), (10.0, 6.0), (7.2, 10.4), (6.2, 14.4), (8.2, 15.0), (7.4, 17.0), (11.0, 18.4), (16.5, 19.3)]
    head = curved(along([*lh, *((33 - x, y) for x, y in lh[-2:0:-1])]), 8)
    part(p, head, TCOAT, sh=1.4 * z, hl=1.0 * z, line=TSEAM)
    if shock:
        for x in (12.5, 16.5, 20.5):  # its fur on end
            part(p, poly(along([(x - 1.6, 5.6), (x + 1.6, 5.6), (x, 2.2)])), TCOAT, 0.6)
    for x in (13, 15, 17):
        fill(p, rect(x, 6, x, 6), TSTRIPE, head)
    part(p, lump(16.5, 16.0, 6.6, 3.5), BIB, sh=1.0 * z, within=head)
    fill(p, rect(0, 8, p.w - 1, 10), HOOD[0], head)
    tones = {"m": HOOD[1], "M": HOOD[0], "w": WHITE, "K": INK, "n": TONGUE[1], "t": TONGUE[2], "R": TONGUE[0]}
    stamp(p, TWINFACE[pose], 9, 8, tones)
    if flat:
        part(p, stroke(along([(11.6, 25.0), (21.4, 25.0)]), girt(3.4)), TCOAT, sh=0.9, hl=0.7, line=TSEAM)
        part(p, lump(11.6, 24.8, 2.2, 2.0), BIB, sh=0.6, line=TSEAM)
        part(p, lump(21.4, 25.2, 2.2, 2.0), BIB, sh=0.6, line=TSEAM)
    if shock:
        for y in (21, 24, 27):  # its tail a brush
            part(p, poly(along([(1.4, y + 1.6), (1.4, y - 1.6), (-1.4, y - 0.4)])), TFAR, 0.6)
    p.outline(INK)
    for s in (-1, 1):
        dots(p, WHITE, (15 + s * 8, 12), (15 + s * 9, 11), (15 + s * 8, 14), (15 + s * 9, 14))
    return mirrored(p) if who else p


def pomball() -> Pic:
    """The Pomchi's ball: red, a white band round it."""
    p = Pic(14, 14)
    p.ball(7, 7, 5.5, 5.5, SCARF, 0.2, 1.0)
    for x in range(14):
        for y in range(14):
            if p.get(x, y) and abs((y - 7) + (x - 7) * 0.35) < 1.3:
                p.put(x, y, WHITE if x < 9 else FUR[3])
    return p.outline(WOODINK)


def russet() -> list[tuple[int, Pic, bool, int]]:
    """Russet Wood's own pictures, at the indices the Downs' own have; its keepsake after the pass's last."""
    k, z = 126, 625
    out = [(0, mistwood(), False, -1), (1, undergrowth(), True, -1), (28, mossledge(), True, -1)]
    out.append((5, ring(188, 72, 93, litter, [ROOTS[0], *OAKBARK], WOODINK), True, -1))
    out += [(z + 112 + h * 2, ribbon(h, [LITTER[0], *OAKBARK], WOODINK), True, -1) for h in range(2)]
    out += earth(z + 124, loamy())
    out += [
        (z + 124 + 268, bough(), True, -1),
        (k + 36, tree(OAKBARK[:4], FOLIAGE), True, -1),
        (k + 38, fern(), True, -1),
        (k + 40, toadstool(0), True, -1),
        (k + 42, toadstool(1), True, -1),
        (k + 44, boulder(MOSS, WOODINK), True, -1),
        (k + 46, thorn(), True, -1),
        (k + 48, lantern((OAKBARK[0], OAKBARK[2]), FIREFLY, (MOSS[0], MOSS[2]), WOODINK), True, -1),
        (k + 376, words("RUSSET WOOD", 178, 18, 1.8, 3.2), True, -1),
        (z + 1216, pomball(), True, -1),
        (z + 1218, sapling(False), True, -1),
        (z + 1220, sapling(True), True, -1),
        (z + 1222, bramble(False), True, -1),
        (z + 1224, bramble(True), True, -1),
        (z + 1226, hollowlog(), True, -1),
        (z + 1228, mound(), True, -1),
        *[(z + 1230 + f * 2, squirrel(f % 2 == 1, f > 1), True, -1) for f in range(4)],
        (z + 1238, acorn(), True, -1),
        (z + 1240, toadspring(False), True, -1),
        (z + 1242, toadspring(True), True, -1),
        (z + 1244, oaktrunk(), True, -1),
        (z + 1246, oakfoot(), True, -1),
        (z + 1248, limb(6), True, z + 1252),
        (z + 1250, limb(54), True, z + 1254),
        (z + 1256, knot(), True, -1),
        (z + 1258, bracket(), True, z + 1260),
    ]
    out += [(z + 1262 + (who * 5 + f) * 2, twin(q, who), True, -1) for who in (0, 1) for f, q in enumerate(TWINPOSES)]
    return out


# ---- Lantern Quarter: fish-scale roofs over brick, strings of lanterns across the houses behind; its colours its
# own, the blue hour and the festival night ----

# the backdrop: the sky down to its afterglow, the moon, three terraces each with its wall, roof and lit window
QSKY = both(
    96,
    "#232A6C #2F3A84 #40509C #5868B0 #7C84C0 #B09CC4 #EDB8A4",
    "#0A0A26 #0E0E30 #14143C #1A1A48 #222056 #2C2662 #3C2E6E",
    5,
)
QMOON = both(103, "#FFF8D8 #F0E4B8 #D4C4AC", "#FFFCE8 #F4ECC8 #D4CCB8", 5)
QFAR = both(106, "#9A94C6 #8A86BC #FFE0A0", "#2A2A5E #222252 #FFD080", 5)
QMID = both(109, "#7074B0 #5E60A2 #FFC870 #62669E", "#20204E #181842 #FFB850 #1A1A44", 5)  # and a window unlit
QNEAR = both(113, "#4C4E92 #404286 #FFB050", "#15153E #111136 #FFA840", 5)
# the playfield: indigo scales, a cream eave, brick red, lantern orange
SCALES = both(116, "#8C98F8 #4C54C8 #343AA0 #1E1C6C", "#7C80E0 #3A3EA8 #282884 #16144C", 5)  # lit, body, rim, gap
EAVE = both(120, "#FFE8C0 #C89878", "#E8C8A0 #A07868", 5)
QBRICK = both(122, "#E8785C #C04C44 #963A44 #5A2440", "#B85C5C #8E3C48 #6C2C44 #3A183C", 5)  # to its mortar
LITPANE = both(126, "#FFF4B0 #FFB840 #4A2038 #3C8C8C", "#FFF8C0 #FFA830 #30142C #2C6C74", 5)  # frame, shutter
QWALL = both(130, "#8C5068 #74405C #A46474", "#4C2C50 #3C2244 #5E3858", 5)  # a wall behind: body, mortar, lit
QINK = both(133, "#1C1030", "#0C0820", 5)[0]
STRIPE = both(134, "#FFF0D0 #FF9838 #C86428", "#F0D8B8 #F08830 #A85028", 5)
PAPER = both(141, "#FFF8C0 #FFA028 #D84828", "#FFF8C0 #FFA028 #D84828", 5)  # a lantern: its light, itself, its shade
QPOLE = both(161, "#D8A070 #A06848 #6C4038", "#A07C78 #74546C #4C3458", 5)
TIN = both(164, "#D8E0F0 #98A4C8 #6870A0 #3C3C70", "#A0A8D0 #6C74A8 #484C84 #282858", 5)
CLAY = both(190, "#F09860 #C06840 #58B458", "#C07C60 #8C5048 #3C8858", 5)  # a pot lit and shaded, its plant
FIRE = both(144, "#FF78B8 #FFE070 #70F0E0 #FFFFFF", "#FF78B8 #FFE070 #70F0E0 #FFFFFF", 5)  # a firework's
# a tram: its roof lit and shaded, its side red to dark, its windows
TRAMWAY = both(185, "#FFF0C8 #D8C098 #E05040 #A83440 #FFD878", "#F0DCB0 #B8A088 #C84040 #8C2C40 #FFD060", 5)
# washing: white and its shade, blue, red, yellow
LINEN = both(156, "#FFFFFF #C8D0F0 #58A0E8 #F05868 #FFD860", "#D8DCF8 #9CA4D8 #4878C8 #C84868 #E0B860", 5)


def terrace(
    p: Pic, base: int, widths: tuple[int, ...], h0: int, hv: int, r: list[int], win: tuple[int, int], lit: int
) -> None:
    """A row of houses side by side across p, of period 320: walls of r[0] under roofs of r[1], gabled or flat, a
    stack on every other; their windows win wide and high, one in lit of them alight, r[2], the rest r[3] where
    there is one."""
    assert 320 % sum(widths) == 0
    x = 0
    for i in range(len(widths) * (320 // sum(widths))):
        w = widths[i % len(widths)]
        top = base - h0 - (i * 11 + base * 3) % hv
        p.box(x, top, x + w - 1, p.h - 1, r[0])
        if i % 3 == 0:
            p.box(x, top - 2, x + w - 1, top, r[1])
        else:
            for k in range(w // 2 + 1):
                p.box(x + k, top - (k * 2 + 2) // 3, x + w - 1 - k, top, r[1])
        if i % 2:
            p.box(x + w * 2 // 3, top - w // 3 - 5, x + w * 2 // 3 + max(2, w // 9), top, r[1])
        for n, wy in enumerate(range(top + 5, p.h, win[1] * 2 + 2)):
            for m, wx in enumerate(range(x + 3, x + w - win[0] - 1, win[0] * 2 + 1)):
                on = (i * 5 + n * 3 + m * 7 + base) % lit == 0
                if on or len(r) > 3:
                    p.box(wx, wy, wx + win[0] - 1, wy + win[1] - 1, r[2] if on else r[3])
        x += w


def festoon(p: Pic, a: Pt, b: Pt, drop: float, line: int, every: int, lamp: int) -> None:
    """A far string of lanterns from a to b, hung drop at its middle: a dot of light along it every so often."""
    n = int(abs(b[0] - a[0]))
    for i in range(n + 1):
        t = i / n
        x, y = round(a[0] + (b[0] - a[0]) * t), round(a[1] + (b[1] - a[1]) * t + drop * 4 * t * (1 - t))
        p.put(x, y, line)
        if i % every == every // 2:
            p.box(x, y + 1, x + 1, y + 2, lamp)


def quartersky() -> Pic:
    """The quarter's far backdrop, of period 320: the sky in bands down to its afterglow, a few stars, the moon; a
    far terrace, lanterns strung across it, and a nearer one."""
    p = Pic(320, 200)
    edges = [0, 18, 36, 52, 66, 78, 90, 200]
    for y in range(200):
        p.box(0, y, 319, y, QSKY[max(i for i in range(7) if edges[i] <= y)])
    for i in range(12):
        p.put((i * 53 + 17) % 320, (i * 31 + 5) % 60, QMOON[1])
    fill(p, oval(214, 36, 17, 17), QMOON[1])
    fill(p, oval(214, 36, 17, 17) & oval(211, 33, 14.5, 14.5), QMOON[0])
    for cx, cy, r in ((208, 30, 3), (220, 41, 4), (213, 46, 2), (224, 28, 2)):
        fill(p, oval(cx, cy, r, r * 0.8), QMOON[2])
    terrace(p, 126, (12, 18, 14, 20, 16), 14, 22, QFAR, (1, 2), 4)
    festoon(p, (0, 96), (160, 92), 9, QFAR[1], 12, PAPER[1])
    festoon(p, (160, 92), (320, 96), 11, QFAR[1], 12, PAPER[1])
    terrace(p, 150, (34, 28, 40, 30, 28), 22, 26, QMID, (3, 4), 5)
    return p


def roofline() -> Pic:
    """The quarter's nearer band, masked, of period 320, which goes by faster: the roofs across the street, their
    stacks, a window alight here and there."""
    p = Pic(320, 80)
    terrace(p, 66, (36, 44, 32, 48), 30, 14, QNEAR, (3, 4), 7)
    return p


SCALLOP = ("11111111", "10001111", "10111111", "21111112", "32111123", "33222233")


def scales(hem: int, d: int) -> int:
    """The roof's tone d pixels under the surface at column hem, of period 8; 0 under it: its ridge, two rows of
    fish-scale tiles, each row over the next, and the eave's board."""
    if d > 15:
        return 0
    if d <= 0:
        return SCALES[0]
    if d == 1:
        return SCALES[2]
    if d >= 14:
        return EAVE[d - 14]
    return SCALES[int(SCALLOP[(d - 2) % 6][(hem + 4 * ((d - 2) // 6 % 2)) % 8])]


def casement(u: int, v: int) -> int:
    """A lit window's tone, 28 by 16 with its shutters back and its sill; 0 where the wall shows."""
    if v >= 14:
        return EAVE[v - 14] if v - 12 <= u <= 39 - v else 0
    if u < 4 or u > 23:
        if not 1 <= v <= 12:
            return 0
        return LITPANE[2] if v % 3 == 2 and u % 24 in (1, 2) else LITPANE[3]
    if u in (4, 13, 14, 23) or v in (0, 6, 13):
        return LITPANE[2]
    return LITPANE[0] if u < 13 and v < 6 else LITPANE[1]


def brickwork(x: int, y: int) -> int:
    """The wall's tone at a world pixel, repeating every 64 across and 48 down: brick in courses of eight, every
    other half a brick along, a few lighter and darker; in each a lit window, in the one tile row a roof at that
    height hides whole, so that none is cut by an eave."""
    x, y = x % 64, y % 48
    if y < 16 and 18 <= x < 46 and casement(x - 18, y):
        return casement(x - 18, y)
    k = y // 8
    u = (x + 8 * (k % 2)) % 64
    if y % 8 == 7 or u % 16 == 15:
        return QBRICK[3]
    h = (u // 16) * 7 + k * 13
    return QBRICK[2] if h % 5 == 0 else QBRICK[0] if h % 7 == 3 else QBRICK[1]


def roofed() -> Skin:
    """The quarter's ground: a roof's scales over its house's brick."""
    return Skin(
        lambda x, y, d, hem: scales(hem, d) or brickwork(x, y), brickwork, QINK, (EAVE[1], QBRICK[0], QBRICK[2])
    )


def balcony() -> Pic:
    """A floating shelf's 16: a stone slab on a course of brick, iron brackets under it."""
    p = Pic(16, 16)
    for x in range(16):
        for y, tone in enumerate((EAVE[0], EAVE[0], EAVE[1])):
            p.put(x, y, tone)
        for y in range(3, 8):
            p.put(x, y, QBRICK[3] if x % 8 == 7 else QBRICK[0] if y == 3 else QBRICK[1])
        p.put(x, 8, QINK)
    for x in (2, 13):
        p.box(x, 9, x, 13, TIN[3])
        p.put(x + (1 if x < 8 else -1), 9, TIN[3])
        p.put(x + (1 if x < 8 else -1), 10, TIN[2])
    return p


def gangplank() -> Pic:
    """The quarter's moving platform: a plank on two iron shoes."""
    p = Pic(48, 12)
    for x in range(1, 47):
        p.put(x, 1, QPOLE[0])
        for y in range(2, 5):
            p.put(x, y, QPOLE[2] if y == 3 and (x * 3) % 17 < 2 else QPOLE[1])
        p.put(x, 5, QPOLE[2])
    for x in (5, 39):
        p.box(x, 6, x + 3, 8, TIN[2])
        p.box(x, 6, x, 8, TIN[1])
    return p.outline(QINK)


def flowerpot(f: int) -> Pic:
    """A pot where the Downs have a flower: its plant in bloom, red or orange."""
    p = Pic(12, 16)
    p.box(3, 10, 8, 14, CLAY[1])
    p.box(3, 10, 4, 14, CLAY[0])
    p.box(2, 9, 9, 9, CLAY[0])
    p.box(5, 5, 6, 8, CLAY[2])
    dots(p, CLAY[2], (4, 7), (7, 6), (3, 6), (8, 5))
    dots(p, PAPER[2 - f], (4, 3), (7, 3), (5, 2), (6, 2), (5, 4), (6, 4), (4, 2), (7, 4))
    dots(p, PAPER[0], (5, 3), (6, 3))
    return p.outline(QINK)


def housefront(storeys: int = 1, pot: bool = True) -> Pic:
    """Where the Downs have a tree: the gable of a house behind the way, a window alight in it, a pot on its sill,
    a round one under its ridge. storeys: that many windows, one 48 under another; pot: the top one's pot."""
    h = 32 + 48 * storeys
    p = Pic(64, h)
    for y in range(24, h):
        for x in range(8, 56):
            mortar = y % 8 == 7 or (x + 8 * (y // 8 % 2)) % 16 == 15
            p.put(x, y, QWALL[2] if x == 8 else QWALL[1] if mortar else QWALL[0])
    for y in range(2, 25):
        half = 2 + (y - 2) * 30 // 22
        for x in range(32 - half, 32 + half):
            p.put(x, y, SCALES[int(SCALLOP[(y - 2) % 6][(x + 4 * ((y - 2) // 6 % 2)) % 8])])
    p.box(1, 25, 62, 25, EAVE[0])
    p.box(2, 26, 61, 26, EAVE[1])
    fill(p, oval(32, 15, 4.5, 4.5), LITPANE[2])
    fill(p, oval(32, 15, 3, 3), LITPANE[1])
    for v in range(16):
        for u in range(28):
            for y in range(42 + v, h, 48):
                if casement(u, v):
                    p.put(18 + u, y, casement(u, v))
    if pot:
        paste(p, flowerpot(0), 33, 41)
    return p.outline(QINK)


TIPPER = (
    "..E....E.......",
    ".EeE..EeE......",
    ".EEEEEEEE......",
    "EEEEEEEEEE.....",
    "EgkEEEEgkE.....",
    "EEEEnnEEEE.....",
    ".EEwwwwEE......",
    "..EwwwwEdd.....",
    ".dEEEEEEEEEE...",
    "dEEEEEdEEEEEEww",
    "dEEEEEdEEEEEEww",
)


def tipper(f: int) -> Pic:
    """A cat sat at a window, looking out over its sill; f 1: a paw out to the pot it is about to tip."""
    p = Pic(17, 13)
    key = {"E": CATFUR[2], "e": PETAL[0], "d": CATFUR[0], "k": INK, "g": GRASS[4], "n": PETAL[0], "w": CLOUD[2]}
    for y, r in enumerate(TIPPER):
        for x, ch in enumerate(r if f or y < 8 else r[:10]):
            if ch != ".":
                p.put(x + 1, y + 1, key[ch])
    return p.outline()


def skylight() -> Pic:
    """Where the Downs have a bush: a skylight, its panes lit from under."""
    p = Pic(40, 20)
    for y in range(4, 18):
        inset = max(0, 9 - (y - 4) * 2)
        for x in range(3 + inset, 37 - inset):
            bar = (x - 4) % 8 == 0 or y in (4, 17) or x in (3 + inset, 36 - inset)
            p.put(x, y, LITPANE[2] if bar else LITPANE[0] if x < 20 and y < 10 else LITPANE[1])
    p.box(1, 18, 38, 18, EAVE[0])
    p.box(2, 19, 37, 19, EAVE[1])
    return p.outline(QINK)


def teachest() -> Pic:
    """Where the Downs have a rock: a crate left on the roof."""
    p = Pic(20, 12)
    p.box(1, 1, 18, 10, QPOLE[1])
    p.box(1, 1, 18, 1, QPOLE[0])
    p.box(1, 10, 18, 10, QPOLE[2])
    for x in (1, 6, 13, 18):
        p.box(x, 2, x, 9, QPOLE[2] if x in (6, 13) else QPOLE[0] if x == 1 else QPOLE[2])
    p.line(7, 9, 12, 2, QPOLE[0])
    return p.outline(QINK)


def birdspikes() -> Pic:
    """Where the Downs have a thorn: a strip of spikes set against the pigeons."""
    p = Pic(16, 12)
    p.box(1, 9, 14, 10, TIN[2])
    p.box(1, 9, 14, 9, TIN[1])
    for x, lean in ((2, -1), (5, 0), (8, 1), (11, 0), (13, 1)):
        p.line(x, 8, x + lean, 2, TIN[1])
        p.put(x + lean, 1, TIN[0])
    return p.outline(QINK)


def washing(what: int) -> Pic:
    """What hangs on a clothesline, its peg at the middle of its top: a striped shirt, trousers, a sock."""
    if what == 0:
        p = Pic(18, 15)
        sleeves = poly([(5, 1), (1, 5), (3, 8), (5, 6)]) | poly([(13, 1), (17, 5), (15, 8), (13, 6)])
        part(p, rect(5, 1, 12, 13) | sleeves, [LINEN[1], LINEN[0]], 1.5)
        for y in (4, 7, 10):
            for x in range(5, 13):
                if p.get(x, y):
                    p.put(x, y, LINEN[3])
        p.box(8, 1, 9, 1, LINEN[1])
    elif what == 1:
        p = Pic(13, 17)
        legs = rect(1, 1, 11, 5) | rect(1, 5, 4, 15) | rect(8, 5, 11, 15)
        part(p, legs, [NAVY[1], LINEN[2], LINEN[1]], 1.5, 1.2)
    else:
        p = Pic(8, 11)
        part(p, rect(1, 1, 4, 7) | rect(1, 6, 6, 9), [QPOLE[0], LINEN[4]], 1.2)
        p.box(1, 1, 4, 2, LINEN[3])
    return p.outline(QINK)


def awning(trodden: bool) -> Pic:
    """An awning out from a wall, its rail along its top: striped canvas down to a scalloped hem; trodden, the
    canvas given under him."""
    p = Pic(46, 22)
    p.box(1, 1, 44, 2, QPOLE[2])
    for x in range(1, 45):
        dip = min(2, (x - 1) // 3, (44 - x) // 3) if trodden else 0
        stripe = ((x + 2) // 5) % 2
        p.box(x, 3, x, 3 + dip, STRIPE[2] if dip else STRIPE[0])
        for y in range(4, 13):
            p.put(x, y + dip, STRIPE[2] if y == 12 else STRIPE[stripe])
        for y in range(13, 14 + (2, 4, 4, 4, 2)[(x + 2) % 5]):
            p.put(x, y + dip, STRIPE[stripe] if y < 16 else STRIPE[2] if stripe else CLOUD[1])
    return p.outline(QINK)


def chimney() -> Pic:
    """A chimney stack on a roof: brick up to its cap, a clay pot on it."""
    p = Pic(18, 32)
    for y in range(9, 32):
        for x in range(2, 16):
            joint = y % 4 == 3 or (x + 4 * (y // 4 % 2)) % 8 == 7
            p.put(x, y, QBRICK[3] if joint else QBRICK[0] if x < 4 else QBRICK[1] if x < 12 else QBRICK[2])
    p.box(1, 7, 16, 7, EAVE[0])
    p.box(1, 8, 16, 8, EAVE[1])
    p.box(5, 2, 12, 6, CLAY[1])
    p.box(5, 2, 6, 6, CLAY[0])
    p.box(4, 1, 13, 2, CLAY[0])
    p.box(11, 1, 13, 2, CLAY[1])
    return p.outline(QINK)


def tram() -> Pic:
    """A tram seen from its side: a cream roof to ride, lit windows, a red side with a cream band, its wheels."""
    p = Pic(66, 33)
    part(p, poly([(2, 7), (4, 3), (10, 1), (55, 1), (61, 3), (63, 7)]), [TRAMWAY[1], TRAMWAY[0]], 1.5)
    p.box(3, 7, 62, 18, TRAMWAY[0])
    p.box(3, 18, 62, 18, TRAMWAY[1])
    for i in range(6):
        x = 6 + i * 9 + (i > 2) * 2
        p.box(x, 9, x + 6, 16, TRAMWAY[4])
        p.box(x, 9, x + 6, 9, LITPANE[0])
        p.box(x, 17, x + 6, 17, TRAMWAY[1])
    p.box(3, 19, 62, 27, TRAMWAY[2])
    p.box(3, 19, 62, 19, QBRICK[0])
    p.box(3, 22, 62, 22, TRAMWAY[0])
    p.box(3, 26, 62, 27, TRAMWAY[3])
    p.box(5, 28, 60, 30, TIN[3])
    for cx in (15, 50):
        fill(p, oval(cx, 30, 3.4, 2.6), TIN[2])
    p.box(1, 21, 2, 22, LITPANE[0])
    return p.outline(QINK)


def paperlantern() -> Pic:
    """A paper lantern hung from a line: lit from inside, a cap over it and under it, its tassel."""
    p = Pic(9, 14)
    part(p, oval(4.5, 6.5, 3.6, 4.4), [PAPER[2], PAPER[1], PAPER[0]], 1.3, 1.7)
    p.box(3, 1, 5, 1, QPOLE[2])
    p.box(3, 11, 5, 11, QPOLE[2])
    p.put(4, 12, PAPER[2])
    return p.outline(QINK)


def dustbin(eyes: bool) -> Pic:
    """A dustbin: its lid lifted from inside and two eyes in the gap, or shut."""
    p = Pic(18, 25)
    part(p, rect(2, 9, 15, 23), [TIN[2], TIN[1], TIN[0]], 2, 1.5)
    for x in (5, 9, 12):
        p.box(x, 11, x, 22, TIN[2])
    p.box(1, 8, 16, 9, TIN[0])
    p.box(13, 8, 16, 9, TIN[1])
    lid = 2 if eyes else 5
    if eyes:
        p.box(2, 5, 15, 7, QINK)
        for x in (5, 10):
            p.box(x, 5, x + 1, 6, PAPER[0])
            p.put(x + 1, 6, GRASS[4])
    p.box(1, lid, 16, lid + 2, TIN[1])
    p.box(1, lid, 12, lid, TIN[0])
    p.box(8, lid - 2, 9, lid - 1, TIN[2])
    return p.outline(QINK)


def snatcher(f: int) -> Pic:
    """A snatcher, an alley cat in grey with a mask across its eyes: f 0 and 1 its strides, 2 it startled."""
    fur = [TIN[2], TIN[2], TIN[1], TIN[1], TIN[0], TIN[0]]
    p = startled(fur) if f == 2 else cat(f, fur)
    for y in (4, 5):
        for x in range(13, 19):
            if p.p[y][x] in fur:
                p.put(x, y, QINK)
    return p


def keepguitar() -> Pic:
    """The big one's guitar as a cat carries it, lying, its neck out behind: an amber body dark at its edge, two
    pickups, a pale neck."""
    p = Pic(26, 13)
    body = oval(17, 7, 6.6, 5.2) | oval(10.5, 6.5, 4.2, 3.8)
    for x, y in body:  # the burst: by how much of the body is about a dot
        near = sum((x + i, y + j) in body for i in range(-2, 3) for j in range(-2, 3))
        p.put(x, y, EARTH[1] if near < 19 else EARTH[4] if near < 25 else GOLD[2])
    for x in (14, 18):
        p.box(x, 5, x, 9, DARK[1])
    p.box(2, 6, 7, 6, BLOND[3])
    p.box(2, 7, 7, 7, HAIR[1])
    p.box(0, 5, 2, 8, HAIR[1])
    dots(p, WHITE, (1, 5), (1, 8))
    return p.outline()


TWINFUR = [HAIR[0], HAIR[0], HAIR[1], HAIR[2], HAIR[2], HAIR[3]]


def relaycat(leg: int, f: int) -> Pic:
    """Who carries the guitar on the relay's leg, 20 by 18, its feet on the last row: Biscuit, ginger, a red
    kerchief; Sprocket in a hard hat; Kipper in his captain's cap; Frost, white, in goggles and a pink scarf; a
    Bramble twin, a brown tabby in a mask. f: 0 and 1 its strides, 2 it bumped, its hat off its head."""
    fur = [CATFUR, CATFUR, CATFUR, FUR, TWINFUR][leg]
    p = Pic(20, 18)
    paste(p, startled(fur) if f == 2 else cat(f, fur), 0, 4)
    hy = 1 if f == 2 else 5
    if leg == 0:
        p.box(11, 11, 13, 12, SCARF[2])
        p.put(10, 12, SCARF[1])
    elif leg == 1:
        p.box(13, hy, 17, hy + 1, GOLD[3])
        p.box(14, hy, 15, hy, GOLD[4])
        p.box(12, hy + 2, 19, hy + 2, GOLD[1])
    elif leg == 2:
        p.box(12, hy - 1, 17, hy, WHITE)
        p.box(12, hy + 1, 19, hy + 1, DARK[0])
        p.put(15, hy, GOLD[3])
    elif leg == 3:
        p.box(13, 8, 18, 9, DARK[0])
        p.box(16, 8, 17, 9, TEAL[4])
        p.box(11, 11, 13, 12, PETAL[1])
        p.box(8, 10, 10, 10, PETAL[1])
        p.box(6, 9, 7, 9, PETAL[0])
    else:
        for y in (8, 9):
            for x in range(13, 19):
                if p.p[y][x] in fur:
                    p.put(x, y, DARK[0])
    return p.outline()


def mortar() -> Pic:
    """A firework's mortar: a short tube leant back on its foot, a band round it."""
    p = Pic(13, 15)
    m = poly([(4, 13), (2, 3), (7, 1), (10, 12)])
    part(p, m, [TIN[2], TIN[1], TIN[0]], 1.5, 1.2)
    for x, y in m:
        if y in (6, 7):
            p.put(x, y, PAPER[2])
    p.box(1, 13, 11, 14, QPOLE[2])
    return p.outline(QINK)


def burst(f: int, a: int, b: int) -> Pic:
    """A firework burst, f 0 to 2 as it opens: sixteen rays of dots out from a white middle, by turns in a and b, a
    dot of a at each ray's end. Opened, its middle and its inmost dots are gone."""
    p = Pic(48, 48)
    r = (8, 15, 22)[f]
    for k in range(16):
        ang = 2 * math.pi * k / 16
        for d in (0.38, 0.62, 0.84)[f // 2 :]:
            p.put(round(24 + r * d * math.cos(ang)), round(24 + r * d * math.sin(ang) * 0.92), a if k % 2 else b)
        p.put(round(24 + r * math.cos(ang)), round(24 + r * math.sin(ang) * 0.92), a)
    if f < 2:
        p.box(23, 23, 24, 24, FIRE[3])
    return p


def lanternq() -> list[tuple[int, Pic, bool, int]]:
    """The Lantern Quarter's own pictures, at the indices the Downs' own have."""
    k, z = 126, 625
    brick = [EAVE[0], *QBRICK, QBRICK[3]]
    out = [(0, quartersky(), False, -1), (1, roofline(), True, -1), (28, balcony(), True, -1)]
    out.append((5, ring(188, 72, 93, scales, brick, QINK), True, -1))
    out += [(z + 112 + h * 2, ribbon(h, [SCALES[0], *QBRICK, QBRICK[3]], QINK), True, -1) for h in range(2)]
    out += earth(z + 124, roofed())
    out += [
        (z + 124 + 268, gangplank(), True, -1),
        (k + 36, housefront(), True, -1),
        (k + 38, skylight(), True, -1),
        (k + 40, flowerpot(0), True, -1),
        (k + 42, flowerpot(1), True, -1),
        (k + 44, teachest(), True, -1),
        (k + 46, birdspikes(), True, -1),
        (k + 48, lantern((QPOLE[0], QPOLE[2]), [PAPER[0], PAPER[1]], (STRIPE[1], STRIPE[2]), QINK), True, -1),
        (k + 376, words("LANTERN QUARTER", 178, 18, 1.35, 2.6), True, -1),
        *[(z + 1290 + f * 2, washing(f), True, -1) for f in range(3)],
        (z + 1296, awning(False), True, -1),
        (z + 1298, awning(True), True, -1),
        (z + 1300, chimney(), True, -1),
        (z + 1302, tram(), True, -1),
        (z + 1304, paperlantern(), True, -1),
        (z + 1306, dustbin(True), True, -1),
        (z + 1308, dustbin(False), True, -1),
        *[(z + 1310 + f * 2, snatcher(f), True, z + 1314 + f * 2) for f in range(2)],
        (z + 1318, snatcher(2), True, z + 1320),
        (z + 1322, mortar(), True, -1),
        *[(z + 1324 + f * 2, burst(f, FIRE[0], FIRE[1]), True, -1) for f in range(3)],
        *[(z + 1330 + f * 2, burst(f, FIRE[2], FIRE[3]), True, -1) for f in range(3)],
        (z + 1336, keepguitar(), True, -1),
        *[(z + 1338 + (leg * 3 + f) * 2, relaycat(leg, f), True, -1) for leg in range(5) for f in range(3)],
        (z + 1368, housefront(2, False), True, -1),
        *[(z + 1370 + f * 2, tipper(f), True, -1) for f in range(2)],
    ]
    return out


# the pictures only one zone shows: they may use that zone's own colours, and are in the store only while it is
# played
OWN: list[set[int]] = [set(), set()]
ZONES = 6


def earth(a: int, skin: Skin) -> list[tuple[int, Pic, bool, int]]:
    """A zone's ground from index a, by its earth's column of 4 and row of 3: flats, each slope and the tile under
    it, the body alone and in tall strips, pipes, its bare sides."""
    out: list[tuple[int, Pic, bool, int]] = []
    for c in range(4):
        out.append((a + 240 + c, deep(c, 0, 192, skin), False, -1))
        for r in range(3):
            v = c * 3 + r
            out += [(a + v, top(1, c, r, 0, skin), False, -1), (a + 228 + v, deep(c, r, 16, skin), False, -1)]
            out.append((a + 244 + v * 2, pipe(c, r, skin), True, -1))
            for kind in range(2, 8):
                out.append((a + 12 + ((kind - 2) * 12 + v) * 2, top(kind, c, r, 0, skin), True, -1))
                out.append((a + 156 + (kind - 2) * 12 + v, top(kind, c, r, 1, skin), False, -1))
    return out + [(a + 270 + i * 2, bare(i > 1, bool(i & 1), skin), True, -1) for i in range(4)]


def pictures() -> list[tuple[int, Pic, bool, int]]:
    """Each picture's index in the game's img array, whether it is masked, and its mirror's index or -1."""
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
    out += [
        (7, words("CHARLIE", 170, 26, 2.8, 5), True, -1),
        (k + 374, words("BARKLEY", 170, 26, 2.8, 5), True, -1),
        (k + 376, words("AMBER DOWNS", 178, 18, 1.8, 3.2), True, -1),
    ]
    # the Chrome Works' own
    z = k + 490 + 9
    # who meets Charlie, each in FAMF pictures, after the pass's last; then the Baron's, the Downs' own
    out += [(z + 1110 + (n * FAMF + f) * 2, p, True, -1) for n, one in enumerate(family()) for f, p in enumerate(one)]
    out += [(z + 1200 + f * 2, p, True, -1) for f, p in enumerate(barons())]
    OWN[0].update(range(z + 1200, z + 1212))
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
    OWN[1].update({*range(z - 9, z + 94), z + 116, z + 118, *range(b, b + 96), *range(z + 642, z + 652)})
    # Amber Downs' own: its backdrop, hills, loop and ribbon where the game once built them, then its ground by
    # the comb's column of 4 and row of 3: flats, each slope and the tile under it, the comb alone, pipes, a raft
    out += [(0, downs(), False, -1), (1, hills(), True, -1)]
    out += [(z + 112 + h * 2, ribbon(h, AMBER, PLUM), True, -1) for h in range(2)]
    a = z + 124
    out += earth(a, TURFED)
    out.append((a + 268, raft(), True, -1))
    out.append((z + 594, startled(), True, z + 596))
    out += intro(z + 598)
    # the cart level, leaning half a gentle slope's fall and all of one's, each as it goes and bumped; its wheel
    out += [(z + 626 + (t * 2 + h) * 2, cart(bool(h), t), True, -1) for t in range(3) for h in range(2)]
    out += [(z + 638 + f * 2, cartwheel(f), True, -1) for f in range(2)]
    # the works' crane, as it goes and bumped; its rail, its crate, the toolbox
    out += [(z + 642 + h * 2, gondola(bool(h)), True, -1) for h in range(2)]
    out += [(z + 646, girder(), True, -1), (z + 648, woodbox(), True, -1), (z + 650, toolbox(), True, -1)]
    # the works' sweepers, going left and mirrored; a spark's carriage dark and alight, its rail; a press's parts
    w = z + 990
    out += [(w + f * 2, sweeper(f), True, w + 4 + f * 2) for f in range(2)]
    out += [
        (w + 8, sparker(False), True, -1),
        (w + 10, sparker(True, 0), True, -1),
        (w + 12, sparker(True, 1), True, -1),
    ]
    out += [(w + 14, floorrail(), True, -1)]
    out += [(w + 16, presshouse(), True, -1), (w + 18, pressrod(), True, -1), (w + 20, presshead(), True, -1)]
    OWN[1].update(range(w, w + 22))
    # Charlie round a twist: 12 turns of two strides, their mirrors facing left
    out += [
        (z + 498 + (n * 2 + f) * 2, turned(frames[5 + f * 3], n, f), True, z + 546 + (n * 2 + f) * 2)
        for n in range(12)
        for f in range(2)
    ]
    out.append((5, ring(188, 72, 93, turf, AMBER, PLUM), True, -1))
    out += [(z + 1282 + f * 2, goldbone(f * math.pi / 4), True, -1) for f in range(4)]
    OWN[0].update({0, 1, 5, itile + 18, z + 112, z + 114, *range(a, a + 278), *range(k + 36, k + 50)})
    OWN[0].update({*range(z + 626, z + 642), k + 376})
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
    """SONG's songs, each its DATA in order: a number a LONG, a string its length and its characters. `USE name`
    stands for `Part name:`'s DATA. Before them how many there are, and where each starts from the count."""
    parts: dict[str, list[int | str]] = {}
    songs: list[list[int | str]] = []
    cur: list[int | str] = []
    for line in SONG.read_text().splitlines():
        if m := re.match(r"(Part|Song) (\w+):$", line):
            cur = []
            if m[1] == "Part":
                parts[m[2]] = cur
            else:
                songs.append(cur)
        elif line.startswith("USE "):
            cur += parts[line[4:].strip()]
        elif line.startswith("DATA "):
            cur += [int(num) if num else text for num, text in re.findall(r'(-?\d+)|"([^"]*)"', line[5:])]
    for n, s in enumerate(songs):
        sound(n, s)
    blocks = [
        b"".join(
            struct.pack("<i", v) if isinstance(v, int) else struct.pack("<i", len(v)) + v.encode("ascii") for v in s
        )
        for s in songs
    ]
    at = [4 + 4 * len(blocks) + sum(len(b) for b in blocks[:n]) for n in range(len(blocks))]
    return struct.pack(f"<{len(at) + 1}i", len(at), *at) + b"".join(blocks)


def levels() -> bytes:
    """LEVELS' levels in order, each its commands to its end: a command its length and its characters, its count a
    LONG; a tune's count is its song's number. Before them how many there are, and where each starts from the count.
    A command the game's loader has no case for it would pass over, and a level without its end it would read past:
    either stops this."""
    tunes = re.findall(r"(?m)^Song (\w+):$", SONG.read_text())
    src = (GAME / "world/level.bas").read_text()
    loader = src[src.index("\nsub LoadLevel ") :]
    known = {*re.findall(r'case "(\w+)"', loader[: loader.index("\nend sub")]), "end"}
    found: list[list[tuple[str, str]]] = []
    for n, line in enumerate(LEVELS.read_text().splitlines(), 1):
        if line.startswith("Level "):
            found.append([])
        elif m := re.fullmatch(r"tune,(\w+)", line):
            if m[1] not in tunes:
                sys.exit(f"charlieart: {LEVELS.name} line {n} names a song there is not")
            found[-1].append(("tune", str(tunes.index(m[1]))))
        elif line and not line.startswith("'"):
            if not re.fullmatch(r"\w+,\d+(, \w+,\d+)*", line):
                sys.exit(f"charlieart: {LEVELS.name} line {n} is not commands and counts")
            found[-1] += re.findall(r"(\w+),(\d+)", line)
    for i, lv in enumerate(found):
        if unknown := {c for c, _ in lv} - known:
            sys.exit(f"charlieart: level {i} has commands the game does not know: {sorted(unknown)}")
        if [c for c, _ in lv].count("end") != 1 or lv[-1][0] != "end":
            sys.exit(f"charlieart: level {i} does not end with its end")
        ends = [v for c, v in lv if c == "burrow"]
        if any(ends.count(v) != 2 for v in ends):
            sys.exit(f"charlieart: level {i} has a burrow without its two mounds")
        if [c for c, _ in lv].count("tune") != 1:
            sys.exit(f"charlieart: level {i} does not name its one tune")
        if [c for c, _ in lv].count("gold") not in (0, game_const("NGOLD")):
            sys.exit(f"charlieart: level {i} has golden bones, and not the game's NGOLD")
    blocks = [
        b"".join(struct.pack("<i", len(c)) + c.encode("ascii") + struct.pack("<i", int(v)) for c, v in lv)
        for lv in found
    ]
    at = [4 + 4 * len(blocks) + sum(len(b) for b in blocks[:n]) for n in range(len(blocks))]
    return struct.pack(f"<{len(at) + 1}i", len(at), *at) + b"".join(blocks)


# A sound's commands, docs/sound.md: the numbers each takes at most and their ranges. H takes all four.
PATCH = {
    "V": [(0, 15)],
    "W": [(0, 4)],
    "Q": [(1, 99)],
    "H": [(0, 15)] * 4,
    "Y": [(0, 3), (30, 12000), (0, 15)],
    "Z": [(1, 15), (0, 15), (0, 15), (0, 15), (0, 7)],
    "U": [(0, 15)],
    "K": [(0, 15), (0, 15), (0, 24)],
    "S": [(1, 15)],
    "R": [(0, 15)],
    "I": [(0, 15)],
}


def refused_by_play(patch: str) -> bool:
    """Whether PLAY would refuse this sound: a command it has not, too many numbers or one out of its range."""
    found = re.findall(r"([A-Z])(\d+(?:,\d+)*)", patch)
    if "".join(c + nums for c, nums in found) != patch:
        return True
    for c, nums in found:
        vals = [int(v) for v in nums.split(",")]
        ranges = PATCH.get(c, [])
        if len(vals) > len(ranges) or (c == "H" and len(vals) != 4):
            return True
        if any(not lo <= v <= hi for v, (lo, hi) in zip(vals, ranges, strict=False)):
            return True
    return False


def sound(n: int, s: list[int | str]) -> None:
    """Song n's DATA read as the game reads it: a bar, voicing, riff or drum it names and has not would stop the
    game as it loaded, or play a rest; a sound PLAY refuses would stop it where the song first played."""
    it = iter(s)

    def texts(k: int) -> list[str]:
        return [str(next(it)) for _ in range(k)]

    def bad(why: str) -> None:
        sys.exit(f"charlieart: song {n}: {why}")

    order, voicing = texts(2)
    bars = int(next(it))
    for _ in range(13):
        next(it)
    patches = texts(4)
    voices, drums, notes = [], [], []
    for _ in range(int(next(it))):
        voices.append(str(next(it)))
        patches += texts(2)
        next(it)
    for _ in range(int(next(it))):
        drums.append(str(next(it)))
        patches.append(str(next(it)))
        notes.append(str((next(it), next(it), next(it))[2]))
    bass, chords, kits = (texts(int(next(it))) for _ in range(3))
    if wrong := [x for x in patches if refused_by_play(x)]:
        bad(f"PLAY would refuse its sound {wrong[0]!r}")
    if any(not re.fullmatch(r"O[0-6][A-G][#+-]?", x) for x in notes):
        bad(f"a drum's note is not an octave and a note: {notes}")
    if (
        len(voices) > game_const("NVOICE")
        or len(drums) > game_const("NDRUM")
        or max(map(len, (bass, chords, kits))) > 8
    ):
        bad("it has more voicings, drums, riffs or patterns than the game holds")
    if len(voicing) != bars or any(v not in voices for v in voicing):
        bad(f"its voicings {voicing!r} are not {bars} of {voices}")
    if any(ord(c) - 97 not in range(bars) for c in order) or bars > 30:
        bad("its order names a bar it has not, or it has more than 30")
    if any(len(k) != 16 or any(c not in [*drums, "."] for c in k) for k in kits):
        bad("a drum pattern is not 16 of its drums")
    for b in range(bars):
        nb, nd = (next(it), next(it), next(it))[1:]
        riffs = texts(4)
        if nb not in range(1, len(bass) + 1) or nd not in range(len(kits)):
            bad(f"bar {b} names a bass riff or drum pattern it has not")
        if len(riffs[0]) == 2 and int(riffs[0][0]) not in range(len(chords)):
            bad(f"bar {b} names a chord pattern it has not")
        if any(len(r) not in (0, 16) for r in riffs[1:]) or len(riffs[0]) not in (0, 2, 16):
            bad(f"bar {b} has a riff that is not 16 long")
        # the game's test plays no song through, a second a bar: a character that is no note would sound one below
        # the riff's lowest
        if any(not re.fullmatch(r"[0-9a-zA-Z.-]*", r) for r in [*riffs, *bass]):
            bad(f"bar {b} or a bass riff has a character that is no note, hold or rest")
    if next(it, None) is not None:
        bad("has DATA left over")


@functools.cache
def game_src() -> str:
    return "\n".join(p.read_text() for d in (GAME, ENGINE) for p in sorted(d.rglob("*.bas")))


def game_const(name: str) -> int:
    """A CONST of the game's, a whole number: an expression of numbers and the game's other CONSTs."""
    m = re.search(rf"(?im)^(?:private )?const {name} = ([^'\n]+)", game_src())
    if not m:
        sys.exit(f"charlieart: the game has no const {name}")
    expr = m[1].strip()
    if not re.fullmatch(r"[\w\s+*()\\-]+", expr):
        sys.exit(f"charlieart: the game's const {name} is not whole numbers: {expr}")
    py = re.sub(r"[A-Za-z_]\w*", lambda t: str(game_const(t[0])), expr).replace("\\", "//")
    return int(eval(py, {"__builtins__": {}}))


def audit(pics: list[tuple[int, Pic, bool, int]]) -> None:
    """A zone's own colours are only in its own pictures: elsewhere they would show as another zone's."""
    # the game's store ran out, an Illegal function call as it loaded: these with their masks and mirrors, the 160
    # turned runs it flops and its own glyphs and HUD (6000 at most) must fit it
    room = game_const("IMGSZ")
    need = sum(len(p.longs()) * (2 if m else 1) * (2 if k2 >= 0 else 1) for _, p, m, k2 in pics) + 160 * 258 + 6000
    if need > room:
        sys.exit(f"charlieart: the pictures need {need} LONGs of the game's IMGSZ, {room}")
    # turned a sixth of the way round a twist he was his side pressed to half its height, flat: he keeps his bulk
    run = hero()
    for turn in range(1, 12):
        for f in range(2):
            if turned(run[5 + f * 3], turn, f).count() < 0.6 * run[5 + f * 3].count():
                sys.exit(f"charlieart: Charlie is flat, turned {turn} twelfths round a twist")
    # Charlie at the van's window was a head alone on its glass, cut off under the chin: the window is open, all
    # of it the picture's, and he stands in it down to its sill
    pk = peek()
    if (
        any(not pk.get(x, y) for y in range(2, 18) for x in range(3, 27))
        or sum(pk.get(x, 17) in FUR for x in range(3, 27)) < 8
    ):
        sys.exit("charlieart: Charlie at the van's window is cut off, or its glass shows")
    # the Baron had two lumps on his back, his tail's end and a paw hard against him: his tail is one thing, clear
    # of his body five rows or more
    if any(tail_clear(b) < 5 for b in barons()):
        sys.exit("charlieart: the Baron's tail does not stand clear of his back")
    # the family were one picture waiting and one cheering: each of them has three and five or more, as many as
    # the game counts; the game stands each by its picture's middle column and last row
    fam = family()
    if len(fam) != game_const("NFAM") or {len(one) for one in fam} != {game_const("FAMF")}:
        sys.exit("charlieart: the family's pictures are not the game's NFAM by FAMF")
    for n, one in enumerate(fam):
        wait, cheer = ({tuple(map(tuple, p.p)) for p in some} for some in (one[:3], one[3:]))
        if len(wait) < 3 or len(cheer) < 5:
            sys.exit(f"charlieart: the family's {n} has {len(wait)} pictures waiting and {len(cheer)} cheering")
        if one[0].w % 2 == 0 or not all(any(p.p[-1]) for p in one[:3]):
            sys.exit(f"charlieart: the family's {n} has no middle column, or stands off its picture's last row")
    # an arm thrown up or out was cut off at its picture's edge: on the stage nothing reaches the edge
    if any(any(p.p[0]) or any(row[0] or row[-1] for row in p.p) for who, _ in WHO for p in staged(who)):
        sys.exit("charlieart: one of the family is cut off at the stage's edge")
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
                sys.exit(f"charlieart: picture {k} uses stage {z}'s own colours {sorted(used & ZPAL[z].keys())}")


def group(pics: list[tuple[int, Pic, bool, int]], zone: int) -> list[int]:
    """The pictures only zone shows, or with -1 those every zone does, as CHARLIE.DAT holds them: their count, then
    each with its mask. The sands', the pass's and the wood's are their own lists, at the Downs' indices."""
    mine = [q for q in pics if next((z for z, own in enumerate(OWN) if q[0] in own), -1) == zone]
    if zone >= 2:
        mine = (sands, whitecap, russet, lanternq)[zone - 2]()
    vals = [len(mine)]
    for k, p, masked, k2 in mine:
        vals += [k, p.w, p.h, int(masked), k2, *encode(p.longs())]
        if masked:
            vals += encode(p.longs(mask=True))
    return vals


SEATH = 30  # a seat's height over its rail
# A kind drawn by its row: its picture; its box, from the thing's place; how many frames it has, how one is picked (by
# FTICK, FSWAY, FSTATE or FPRESS, model/kinds.bas's) and the ticks a frame. A name is the game's CONST.
DRAWN: dict[str, tuple[int | str, ...]] = {
    "KDRIFT": ("IDRIFT", -28, -11, 56, 12),
    "KCISTERN": ("ICISTERN", -30, -25, 30, 30),
    "KSEAT": ("ISEAT", 0, -SEATH, "MOVEW", SEATH + 8),
    "KTRAM": ("ITRAM", -9, -1, 66, 33),
    "KPOST": ("IPOST", -7, -47, 16, 48),
    "KTANK": ("ITANK", -16, -39, 32, 40),
    "KVENT": ("IVENT", -8, -27, 16, 28),
    "KTREE": ("ITREE", -32, -78, 64, 80),
    "KBUSH": ("IBUSH", -20, -18, 40, 20),
    "KROCK": ("IROCK", -10, -10, 20, 12),
    "KTHORN": ("ITHORN", -8, -10, 16, 12),
    "KMOUND": ("IMOUND", -17, -12, 34, 14),
    "KLAMP": ("ILAMP", -10, -42, 20, 44),
    "KBOOST": ("IBOOST", -12, -9, 24, 10, 2, "FTICK", 4),
    "KFLUME": ("IFLUME", 0, -8, "TS", 29, 4, "FTICK", 3),
    "KFLOWER": ("IFLOWER", -6, -14, 12, 16, 2, "FSWAY", 30),
    "KSAPLING": ("ISAPLING", -6, -49, 60, 50, 2, "FPRESS"),
    "KAWNING": ("IAWNING", -23, -3, 46, 22, 2, "FPRESS"),
    "KBRAMBLE": ("IBRAMBLE", -19, -25, 38, 26, 2, "FPRESS"),
    "KBUMP": ("IBUMP", -12, -12, 24, 24, 2, "FPRESS"),
    "KBUOY": ("IBUOY", -9, -9, 18, 18, 2, "FPRESS"),
    "KHOOP": ("IHOOP", -6, -16, 12, 32, 2, "FPRESS"),
    "KMARK": ("IMARK", -6, -31, 12, 32, 2, "FSTATE"),
    "KBUTTON": ("IBTN", -8, -8, 16, 8, 2, "FSTATE"),
}

# The facts asked about kinds. The state a kind starts a stage in, else 0; the kinds that keep theirs: a burrow's mound
# its other end, a chimney too, a mortar its beat, a tipper its pot.
START: dict[str, int | str] = {
    "KBONE": 1,
    "KCAT": -1,
    "KCRAB": -1,
    "KSWEEP": -1,
    "KWALKER": -1,
    "KSNATCH": -1,
    "KJELLY": 1,
    "KTHROW": "THROWC",
    "KSQUIRREL": "SQWAIT",
    "KGOAT": 1,
    "KWALL": 1,
}
KEEPS = {"KMOUND", "KCHIMNEY", "KMORTAR", "KTIPPER"}
RIDES = {"KMOVE", "KSPAN", "KSEAT", "KTRAM"}  # carried along a track
HANGS = {"KICICLE", "KLOAD"}  # loosed by a bark from under it
CRUSHED = {"KCAT", "KTHROW"}  # beaten, awake, by a falling icicle or the avalanche
GROWS = {"KTREE", "KBUSH", "KFLOWER", "KROCK", "KTHORN", "KLAMP"}  # pulled up where the ground changes
# How each moves, model/kinds.bas's M consts; any other not at all.
MIND = {
    **dict.fromkeys(["KCAT", "KCRAB", "KSWEEP"], "MPROWL"),
    **dict.fromkeys(["KSPRING", "KBUMP", "KHOOP", "KSAPLING", "KAWNING"], "MREST"),
    **dict.fromkeys(["KSPARK", "KICICLE", "KLOAD", "KGOAT", "KTHROW", "KSQUIRREL", "KPOUNCE", "KSNATCH"], "MOWN"),
    **dict.fromkeys(["KMORTAR", "KTIPPER", "KBRAMBLE", "KVINE", "KBUOY", "KJELLY"], "MOWN"),
}
# What a touch does, model/kinds.bas's T consts: a kind not named does nothing. A row's touch: its reach each side, its
# top above and bottom below the thing's place, how fast it throws him up, the state it rests in, met.
TOUCH: dict[str, tuple[int | str, ...]] = {
    "KSPRING": ("TBOUNCE", 11, 12, 2, 560, 8),
    **dict.fromkeys(
        ["KBONE", "KGOLD", "KCAT", "KSQUIRREL", "KWALKER", "KSNATCH", "KCRAB", "KSWEEP", "KTHROW", "KGOAT", "KWALL"],
        ("TOWN",),
    ),
    **dict.fromkeys(
        ["KHEAP", "KICICLE", "KSLED", "KDRIFT", "KSPARK", "KPRESS", "KJELLY", "KSAPLING", "KAWNING", "KCHIMNEY"],
        ("TOWN",),
    ),
    **dict.fromkeys(
        ["KPOUNCE", "KMORTAR", "KTIPPER", "KVINE", "KBRAMBLE", "KLOG", "KMOUND", "KBOOST", "KFLUME", "KSPOUT"],
        ("TOWN",),
    ),
    **dict.fromkeys(["KMARK", "KGOAL", "KTHORN", "KBUTTON", "KJET", "KBUMP", "KBUOY", "KSPIN", "KHOOP"], ("TOWN",)),
}


def kinds() -> list[int]:
    """The kinds' rows as CHARLIE.DAT holds them: their count, then each kind's picture, box, frames, the way it picks
    one and its rate, naughts for a kind that draws itself; then the state it starts in, and whether it keeps its own,
    rides, hangs, is crushed and grows, -1 for yes; how it moves; what a touch does, and its box, speed and rest."""
    src = (GAME / "model/level.bas").read_text()
    ids = {k: int(v) for k, v in re.findall(r"(?m)^const (K[A-Z0-9]+) = (\d+)", src) if k != "KFRONT"}
    rows = [[0] * 8 for _ in range(max(ids.values()) + 1)]
    for kind, row in DRAWN.items():
        full = (*row, 1, "FNONE", 0) if len(row) == 5 else (*row, 0)[:8]
        rows[ids[kind]] = [v if isinstance(v, int) else game_const(v) for v in full]
    names = {k: kind for kind, k in ids.items()}
    for k, row in enumerate(rows):
        kind = names.get(k, "")
        start = START.get(kind, 0)
        row.append(start if isinstance(start, int) else game_const(start))
        row += [-(kind in fact) for fact in (KEEPS, RIDES, HANGS, CRUSHED, GROWS)]
        row.append(game_const(MIND.get(kind, "MNONE")))
        touch = (*TOUCH.get(kind, ("TNONE",)), 0, 0, 0, 0, 0)[:6]
        row += [v if isinstance(v, int) else game_const(v) for v in touch]
    for kind in [*START, *KEEPS, *RIDES, *HANGS, *CRUSHED, *GROWS, *MIND, *TOUCH]:
        if kind not in ids:
            sys.exit(f"charlieart: the game has no kind {kind}")
    # row 0, no kind's, once went without its facts, and every row after it was read six LONGs out
    if any(len(r) != len(rows[0]) for r in rows):
        sys.exit("charlieart: the kinds' rows are not all one length")
    return [len(rows), *(v for r in rows for v in r)]


def blob() -> bytes:
    """CHARLIE.DAT: the 1-based bytes its tunes, its levels and its kinds start at, the palette, each stage's own, the
    1-based byte each zone's own pictures start at, the pictures every zone shows, each zone's own, the levels, the
    tunes, the kinds; each value a little-endian LONG."""
    pics = pictures()
    audit(pics)
    vals = [len(PAL)]
    for i, (r, g, b) in sorted(PAL.items()):
        vals += [i, r + 256 * g + 65536 * b]
    for z in sorted(ZPAL):
        vals.append(len(ZPAL[z]))
        for i, (r, g, b) in sorted(ZPAL[z].items()):
            vals += [i, r + 256 * g + 65536 * b]
    groups = [group(pics, z) for z in range(-1, ZONES)]
    at = [4 * (3 + len(vals) + ZONES + sum(len(g) for g in groups[: z + 1])) + 1 for z in range(ZONES)]
    vals += at + [v for g in groups for v in g]
    lv, tunes = levels(), song()
    start = len(vals) * 4 + 13
    head = (start + len(lv), start, start + len(lv) + len(tunes))
    kd = kinds()
    return struct.pack(f"<{len(vals) + 3}i", *head, *vals) + lv + tunes + struct.pack(f"<{len(kd)}i", *kd)


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
