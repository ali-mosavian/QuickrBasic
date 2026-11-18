#!/usr/bin/env python3
"""pangart.py [--check] [--sheet PNG]: PANGDASH.BAS's palette and pictures (Pip, tiles, things, props),
drawn here as shaded, dithered pixels and written into the game as DATA between its PangArt markers:
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
DEMO = ROOT / "src/plat/wasm/web/demos/PANGDASH.BAS"
BEGIN, END = "' PangArt's begin.\n", "' PangArt's end.\n"
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


SCALE = ramp(8, (20, 10, 5), (60, 44, 20), 6)
SKIN = ramp(14, (38, 22, 18), (62, 48, 40), 4)
BELLY = ramp(18, (48, 38, 28), (63, 58, 46), 3)
SCARF = ramp(21, (30, 3, 8), (63, 20, 14), 4)
GRASS = ramp(25, (6, 24, 10), (44, 62, 22), 5)
EARTH = ramp(30, (13, 7, 6), (50, 32, 15), 8)
STONE = ramp(38, (22, 20, 24), (56, 54, 54), 4)
GOLD = ramp(42, (40, 20, 2), (63, 62, 34), 5)
TEAL = ramp(47, (2, 16, 24), (30, 60, 58), 5)
METAL = ramp(52, (16, 18, 24), (58, 60, 63), 4)
WOOD = ramp(56, (16, 9, 5), (42, 28, 14), 4)
LEAF = ramp(60, (34, 8, 4), (63, 50, 10), 6)
PETAL = ramp(66, (48, 16, 34), (63, 56, 60), 3)
SKY = ramp(96, (8, 16, 44), (62, 50, 42), 32)
FAR = ramp(128, (28, 26, 46), (50, 46, 58), 4)
MID = ramp(132, (8, 24, 22), (34, 50, 26), 6)
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


# ---- Pip ----


def leg(p: Pic, hx: float, hy: float, a: float, far: bool, lift: float = 0) -> None:
    """A leg from the hip at angle a from straight down, its clawed foot forward."""
    fx, fy = hx + math.sin(a) * 7, hy + math.cos(a) * 7 - lift
    p.line(hx, hy, fx, fy, SCALE[0] if far else SCALE[1], 3)
    p.ball(fx + 1, fy + 0.5, 2.6, 1.6, SKIN, 0.0 if far else 0.2, 0.6 if far else 1.0)
    p.put(round(fx + 3), round(fy + 1), WHITE if not far else SKIN[2])


def scarf(p: Pic, x: float, y: float, n: int, phase: float, rise: float, amp: float = 1.2) -> None:
    """The scarf's tail, n pixels back from the neck, waving; rise lifts its end."""
    for i in range(n):
        t = i / max(n - 1, 1)
        yy = y + math.sin(phase + i * 0.8) * amp * t - rise * t
        p.line(x - i, yy, x - i, yy + 1.6, SCARF[3] if i % 4 < 2 else SCARF[2], 1)
        p.put(round(x - i), round(yy + 2), SCARF[0])


def pip(
    bob: float = 0,
    lean: float = 0,
    front: float = 0.15,
    back: float = -0.15,
    eye: str = "open",
    tail: float = 0,
    sc: tuple[int, float, float] = (5, 0, -3),
    arm: float = 0.5,
    wheel: int = -1,
    lift: float = 0,
    squat: float = 0,
    gog: bool = False,
    cross: bool = False,
) -> Pic:
    """Pip standing, facing right, feet on row 30. bob drops the body, lean tips the head forward; front and back
    are the legs' angles; sc the scarf's length, phase and rise; wheel a blur of legs instead; squat folds them."""
    p = Pic(32, 32)
    by = 16 + bob + squat
    hip = 21.5 + bob * 0.5 + squat * 0.8
    for i, (tx, ty, tr) in enumerate(((7.5, 3.5, 3.4), (5, 5.6, 2.7), (3.2, 7.6, 2.0), (2, 9.2, 1.4))):
        p.ball(tx - tail * i * 0.3, by + ty - abs(tail) * i * 0.4, tr, tr, SCALE, 0.1, 0.8)
    if wheel >= 0:
        for i in range(7):
            a = (i + wheel * 0.5) * math.pi / 3.5
            p.line(
                13 + math.cos(a) * 2,
                26 + math.sin(a) * 1.5,
                13 + math.cos(a) * 8,
                26 + math.sin(a) * 4.5,
                (SKIN[2], SCALE[2], SCARF[3])[i % 3],
                1,
            )
        p.ball(13, 26, 3.5, 2.5, SKIN, 0.3, 0.9)
    else:
        leg(p, 11, hip, back, True)
        leg(p, 15, hip, -front * 0.7, True)
    p.ball(12.5, by, 7.8, 6.6, SCALE, 0.0, 0.75)
    for row in range(5):
        for col in range(6):
            sx, sy = 5.5 + col * 3.4 + (row % 2) * 1.7, by - 5.6 + row * 2.7
            inside = ((sx - 12.5) / 7.8) ** 2 + ((sy - by) / 6.6) ** 2 < 0.8
            if inside and not (sx > 14 and sy > by + 2.2):
                p.ball(sx, sy, 2.3, 2.0, SCALE, 0.25, 1.0, over=True)
                p.put(round(sx - 1), round(sy + 2), SCALE[0])
                p.put(round(sx), round(sy + 2), SCALE[0])
                p.put(round(sx + 1), round(sy + 2), SCALE[0])
    p.ball(16, by + 3.2, 4.6, 3.1, BELLY, 0.2, 1.0, over=True)
    if wheel < 0:
        leg(p, 12, hip, -back * 0.8, False)
        leg(p, 16, hip, front, False, lift)
    hx, hy = 22 + lean, 10.5 + bob + squat * 1.3 + lean * 0.4
    n, ph, rise = sc
    scarf(p, hx - 5, hy + 3.5, n, ph, rise)
    scarf(p, hx - 5, hy + 5, max(n - 2, 2), ph + 1.6, rise * 0.8)
    p.ball(hx - 3.5, hy - 3, 1.6, 2.2, SKIN, 0.0, 0.6)
    p.ball(hx, hy, 5.6, 5.0, SKIN)
    for cx, cy in ((-3.6, -3.2), (-1, -4.4), (1.8, -4.3)):
        p.ball(hx + cx, hy + cy, 2.2, 1.7, SCALE, 0.3, 1.0)
    p.ball(hx + 5, hy + 2.4, 4.2, 2.3, SKIN, 0.15, 1.0)
    p.put(round(hx + 8), round(hy + 2), INK)
    p.put(round(hx + 8), round(hy + 3), INK)
    p.line(hx + 3.5, hy + 4.6, hx + 5.5, hy + 4.4, INK)
    p.put(round(hx + 6.5), round(hy + 3.6), INK)
    p.ball(hx - 4.2, hy + 3.6, 2.6, 2.1, SCARF, 0.3, 1.0)
    ex, ey = hx + 1.5, hy - 1
    if gog:
        p.line(hx - 4.5, hy - 1.5, hx + 1, hy - 1, METAL[0])
        p.ball(ex, ey, 2.6, 2.5, TEAL, 0.3, 1.0)
        p.put(round(ex - 1), round(ey - 1), WHITE)
    elif eye == "open":
        p.ball(ex, ey + 0.4, 1.7, 1.9, [CLOUD[1], WHITE])
        p.put(round(ex + 1), round(ey + 1), INK)
        p.line(ex - 1.8, ey - 1.2, ex + 2, ey - 0.6, INK)
        p.line(ex - 2.4, ey - 3.3, ex + 2.2, ey - 1.8, INK)
    elif eye == "happy":
        p.line(ex - 1.5, ey + 0.5, ex, ey - 1, INK)
        p.line(ex, ey - 1, ex + 1.5, ey + 0.5, INK)
    elif eye == "hurt":
        p.line(ex - 1.5, ey - 1.5, ex + 1.5, ey + 1.5, INK)
        p.line(ex - 1.5, ey + 1.5, ex + 1.5, ey - 1.5, INK)
    else:
        p.line(ex - 1.5, ey + 0.5, ex + 1.6, ey + 0.5, INK)
        p.line(ex - 2.4, ey - 2.3, ex + 2, ey - 1.6, INK)
    if not gog:
        p.line(hx - 3.5, hy - 5.2, hx + 2.5, hy - 5.6, METAL[0])
        p.ball(hx - 0.5, hy - 5.6, 1.9, 1.5, TEAL, 0.4, 1.0)
    sx, sy = 18.5 + lean * 0.5, by + 0.5
    if cross:
        p.line(14.5, by + 1.5, 20, by + 2.5, SKIN[1], 2)
        p.line(15, by + 3.2, 19.5, by + 1.6, SKIN[2], 2)
        p.ball(14.5, by + 1.6, 1.6, 1.5, SKIN, 0.3, 1.0)
    else:
        ax, ay = sx + math.sin(arm) * 5, sy + math.cos(arm) * 5
        p.line(sx, sy, ax, ay, SKIN[1], 2)
        p.ball(ax + 0.5, ay + 0.5, 1.7, 1.6, SKIN, 0.3, 1.0)
    return p.outline()


def curled(turn: float) -> Pic:
    """Pip rolled up: a lit ball of plates in a spiral, the scarf's end round it, turned."""
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
                v = 0.14 + 0.8 * max(lit, 0) - (0.3 if band < 0.2 else 0) + (0.1 if 0.2 <= band < 0.4 else 0)
                red = 7.5 < d < 10 and (a % (2 * math.pi)) < 1.1
                p.shade(x, y, SCARF if red else SCALE, v + (0.25 if red else 0))
    p.ball(cx + math.cos(turn + 2.4) * 4, cy + math.sin(turn + 2.4) * 4, 2.5, 2.0, SKIN, 0.2, 0.9)
    return p.outline()


def hero() -> list[Pic]:
    """Idle, blink, two taps, six of the run, two of the dash, four curled, skid, crouch, hurt, sprung, two wins."""
    out = [pip(), pip(eye="blink"), pip(lift=2.5, front=0.5, cross=True), pip(front=0.5, cross=True)]
    for f in range(6):
        ph = 2 * math.pi * f / 6
        out.append(
            pip(
                bob=round(abs(math.sin(ph))),
                lean=1,
                front=0.95 * math.sin(ph),
                back=-0.95 * math.sin(ph),
                sc=(9, ph * 2, 0.5),
                arm=-0.7 * math.sin(ph),
                tail=math.sin(ph),
            )
        )
    for f in range(2):
        out.append(pip(bob=1, lean=2, wheel=f, sc=(12, f * 3.1, 1.5), arm=-1.2, tail=1.5, gog=True))
    out += [curled(i * math.pi / 14) for i in range(4)]
    out.append(pip(bob=1, lean=-2, front=1.0, back=0.7, sc=(7, 0, 5), arm=1.4, tail=-1))
    out.append(pip(squat=5, front=1.2, back=-1.2, sc=(5, 0, -2), arm=0.9))
    out.append(pip(bob=-1, lean=-1, front=-0.9, back=0.9, eye="hurt", sc=(8, 1, 6), arm=2.4, tail=2))
    out.append(pip(bob=-2, front=0.1, back=-0.1, sc=(8, 0, -7), arm=2.9, tail=-2))
    out.append(pip(eye="happy", arm=2.6, sc=(6, 0, -2)))
    out.append(pip(bob=-2, eye="happy", arm=3.0, front=0.4, back=-0.4, sc=(6, 2, 0)))
    return out


# ---- the ground ----


def surface(kind: int, i: int) -> int:
    """Tile kind's top row at column i, as the game's Prof: 1 full, 2 and 3 the 45s, 4 to 7 the gentle halves."""
    h = {1: 16, 2: i + 1, 3: 16 - i, 4: (i + 2) // 2, 5: 8 + (i + 2) // 2, 6: 16 - i // 2, 7: 8 - i // 2}[kind]
    return 16 - h


def soil(p: Pic, x: int, y: int, depth: float, seed: int) -> None:
    """Earth at depth pixels under the surface: lighter strata near the top, pebbles, darker down."""
    band = 0.09 * math.sin((depth + 2.2 * math.sin(x * math.pi / 8)) * math.pi / 5)
    v = 0.78 - min(depth, 70) * 0.0085 + band + (noise(x % 16, int(depth), seed) - 0.5) * 0.2
    p.shade(x, y, EARTH, v)
    if noise(x % 16 // 2, int(depth) // 2, seed + 7) > 0.955:
        p.put(x, y, STONE[2 if int(depth) % 2 == 0 else 0])


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


def bead(rx: float) -> Pic:
    p = Pic(10, 10)
    p.ball(4.5, 4.5, rx, 3.6, GOLD, 0.15, 1.0)
    if rx > 2:
        p.put(3, 3, WHITE)
    return p.outline()


def bug(f: int) -> Pic:
    p = Pic(20, 14)
    for i in range(3):
        x = 5 + i * 4
        p.line(x, 9, x + (1 if (i + f) % 2 else -1), 12, INK)
    p.ball(9, 6.5, 7, 5.2, TEAL)
    p.line(9, 2, 9, 11, TEAL[0])
    for x, y in ((6, 5), (12, 5), (5, 8), (13, 8)):
        p.put(x, y, TEAL[0])
    p.ball(15.5, 9, 2.8, 2.8, TEAL, 0.0, 0.5)
    p.put(16, 8, WHITE)
    p.put(17, 8, 4)
    p.line(15, 6, 17, 4, INK)
    p.line(14, 6.5, 17.5, 7, INK)
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
    itile, iearth, ihero, nf = 10, 30, 34, 22
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
    out += [(k + f * 2, bead(rx), True, -1) for f, rx in enumerate((3.6, 2.6, 1.0, 2.6))]
    out += [(k + 8 + f * 2, bug(f), True, k + 12 + f * 2) for f in range(2)]
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
    lines, cur = ["' The palette and pictures, as tools/pangart.py draws them.", "PangArt:"], ""
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
        sys.exit(0 if new == src else "pangart: PANGDASH.BAS's PangArt is stale; run tools/pangart.py")
    DEMO.write_text(new)
    print(f"pangart: {len(pictures())} pictures, {len(block())} characters")


if __name__ == "__main__":
    main()
