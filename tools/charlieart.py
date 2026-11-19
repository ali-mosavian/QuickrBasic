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
from pathlib import Path

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
    """Three of the run and the two of the dash, down 45 and 27 degrees, then up 27 and 45."""
    h = hero()
    return [tilted(h[f], deg) for deg in (-45, -26.57, 26.57, 45) for f in (4, 6, 8, 10, 11)]


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
    out += [(k + 50 + i * 2, t, True, k + 90 + i * 2) for i, t in enumerate(tilts())]
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
