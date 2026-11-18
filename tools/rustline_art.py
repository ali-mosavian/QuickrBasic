"""rustline_art.py: RUSTLINE's pre-drawn art, ray-marched from shapes and written into the game.

    rustline_art.py [--preview DIR]

Renders the land, buildings and units isometric, picks the 72 colours the game's own 32 lack, and
rewrites the blocks after PalData: and ArtData: in src/plat/wasm/web/demos/RUSTLINE.BAS. --preview also saves
each picture as a PNG at 4 times its size.

The world is in pixels: a tile is 16 on a side; the screen's x is x - y, its y (x + y) / 2 - z.
"""

import re
import argparse
from pathlib import Path
from dataclasses import dataclass
from collections.abc import Callable

import numpy as np
from numpy.typing import NDArray

BAS = Path(__file__).resolve().parent.parent / "src/plat/wasm/web/demos/RUSTLINE.BAS"
F = NDArray[np.float64]
# a shape: points (n, 3) to their distance outside it (n,) and material (n,)
Shape = Callable[[F], tuple[F, NDArray[np.int64]]]

# the game's colour names (RUSTLINE.BAS's Setup)
TEAM = ((10, 11, 25), (12, 13, 26))  # mid, light, dark

LIGHT = np.array([-0.35, 0.75, 1.0]) / np.linalg.norm([-0.35, 0.75, 1.0])
VIEW = np.array([1.0, 1.0, 1.0]) / np.sqrt(3.0)


# --- shapes: each a distance and a material number


def box(c: tuple[float, float, float], h: tuple[float, float, float], mat: int, r: float = 0.0) -> Shape:
    cc, hh = np.array(c), np.array(h) - r

    def f(p: F) -> tuple[F, NDArray[np.int64]]:
        q = np.abs(p - cc) - hh
        d = np.linalg.norm(np.maximum(q, 0), axis=1) + np.minimum(q.max(axis=1), 0) - r
        return d, np.full(len(p), mat)

    return f


def cyl(c: tuple[float, float, float], r: float, h: float, mat: int, axis: int = 2) -> Shape:
    """A cylinder r round, standing on c (along axis 2) or lying along axis 0 or 1 centred at c."""
    cc = np.array(c)

    def f(p: F) -> tuple[F, NDArray[np.int64]]:
        q = p - cc
        if axis == 2:
            rad, along = np.hypot(q[:, 0], q[:, 1]), q[:, 2] - h / 2
        elif axis == 0:
            rad, along = np.hypot(q[:, 1], q[:, 2]), q[:, 0]
        else:
            rad, along = np.hypot(q[:, 0], q[:, 2]), q[:, 1]
        dx, dy = rad - r, np.abs(along) - h / 2
        d = np.minimum(np.maximum(dx, dy), 0) + np.hypot(np.maximum(dx, 0), np.maximum(dy, 0))
        return d, np.full(len(p), mat)

    return f


def ball(c: tuple[float, float, float], r: tuple[float, float, float], mat: int) -> Shape:
    cc, rr = np.array(c), np.array(r)

    def f(p: F) -> tuple[F, NDArray[np.int64]]:
        q = (p - cc) / rr
        k = np.linalg.norm(q, axis=1)
        return (k - 1) * rr.min(), np.full(len(p), mat)

    return f


def union(*s: Shape) -> Shape:
    def f(p: F) -> tuple[F, NDArray[np.int64]]:
        d, m = s[0](p)
        for t in s[1:]:
            e, n = t(p)
            k = e < d
            d, m = np.where(k, e, d), np.where(k, n, m)
        return d, m

    return f


def cut(a: Shape, b: Shape) -> Shape:
    """a less b; the cut's walls a's."""

    def f(p: F) -> tuple[F, NDArray[np.int64]]:
        d, m = a(p)
        e, _ = b(p)
        return np.maximum(d, -e), m

    return f


def above(a: Shape, z: float) -> Shape:
    """a's part above height z."""

    def f(p: F) -> tuple[F, NDArray[np.int64]]:
        d, m = a(p)
        return np.maximum(d, z - p[:, 2]), m

    return f


# --- materials: a colour lit, and how it shines; some by where on the shape


@dataclass(frozen=True)
class Mat:
    rgb: tuple[int, int, int]
    shine: float = 0.0
    pattern: str = ""


def materials(team: int) -> dict[int, Mat]:
    tc = ((0, 44, 46), (60, 26, 4))[team]
    tl = ((26, 60, 58), (63, 50, 14))[team]
    return {
        1: Mat((30, 24, 16), pattern="earth"),
        2: Mat((40, 41, 46), 0.2),  # concrete
        3: Mat((46, 47, 54), 0.5, "panels"),  # metal
        4: Mat(tc, 0.4),  # the side's
        5: Mat(tl, 0.6, "ribs"),
        6: Mat((22, 40, 58), 1.0),  # glass
        7: Mat((58, 50, 8), 0.2, "stripes"),  # hazard
        8: Mat((14, 14, 18), 0.1),  # dark metal, openings
        9: Mat((24, 63, 44), 0.0),  # moss's glow, unlit
        10: Mat((26, 28, 34), 0.3, "panels"),
        11: Mat((20, 20, 24), 0.2, "treads"),
        12: Mat((10, 52, 30), 0.0, "moss"),
        13: Mat((30, 25, 30), 0.15, "crack"),  # rock
        14: Mat((20, 14, 10), 0.0),  # stem
        15: Mat((46, 26, 50), 0.3, "spots"),  # the fungus's cap
        16: Mat((26, 14, 30), 0.0),  # its gills
    }


# --- the buildings, on their footprint 0 to 16 * n square


def hall() -> Shape:
    return union(
        box((16, 16, 1), (15, 15, 1), 1),
        box((16, 16, 4), (12, 12, 2.5), 2, 1),
        box((17, 17, 10), (9, 9, 5), 3, 1),
        above(ball((17, 17, 14), (9.5, 9.5, 10), 4), 14),
        cyl((17, 17, 14), 10, 1.2, 10),
        box((7, 23, 8), (3.5, 4, 4.5), 4, 0.5),
        box((7, 27.2, 8), (2, 0.6, 2.5), 6),
        cyl((26, 9, 4), 0.6, 22, 8),
        ball((26, 9, 26.5), (1.2, 1.2, 1.2), 9),
        box((22, 27, 7), (3, 0.6, 1.2), 9),
    )


def mill() -> Shape:
    silo = lambda x, y: union(  # noqa: E731
        cyl((x, y, 5), 6, 17, 3),
        above(ball((x, y, 22), (6, 6, 4), 4), 22),
        cyl((x, y, 13), 6.3, 1, 10),
    )
    return union(
        box((16, 16, 1), (15, 15, 1), 1),
        box((16, 17, 3), (13, 12, 2.5), 2, 0.5),
        silo(10, 11),
        silo(22, 11),
        box((16, 25, 6), (12, 3, 2), 7),
        box((16, 22, 9), (2, 6, 1), 10),
        cyl((16, 17, 5), 2, 12, 8),
        ball((16, 17, 18), (1.5, 1.5, 1.5), 9),
    )


def den() -> Shape:
    vault = cut(
        above(cyl((16, 15, 4), 11, 24, 5, axis=0), 4),
        box((28.5, 15, 7), (1, 6, 5), 8),
    )
    return union(
        box((16, 16, 1), (15, 15, 1), 1),
        box((16, 16, 3), (13, 13, 2), 2, 0.5),
        vault,
        box((28.7, 15, 7), (0.3, 6, 5), 8),
        box((8, 27, 6), (3, 2.5, 4), 3, 0.5),
        cyl((8, 27, 9), 1.2, 6, 8),
        box((24, 27, 6), (4, 1.5, 1), 9),
    )


def spire() -> Shape:
    return union(
        box((8, 8, 1), (7.5, 7.5, 1), 1),
        box((8, 8, 3), (6, 6, 2), 2, 1),
        cyl((8, 8, 5), 4, 9, 3),
        cyl((8, 8, 9), 4.4, 1, 10),
        above(ball((8, 8, 14), (4.2, 4.2, 4), 4), 14),
    )


BUILDINGS: list[tuple[str, Callable[[], Shape], int, int]] = [  # name, shape, tiles, height
    ("hall", hall, 2, 28),
    ("mill", mill, 2, 28),
    ("den", den, 2, 28),
    ("spire", spire, 1, 22),
]


# --- the units, in their own frame: u ahead, v to the left, z up; the feet at 0, 0, 0


def turned(local: Shape, hx: float, hy: float) -> Shape:
    """local facing world heading hx, hy."""

    def f(p: F) -> tuple[F, NDArray[np.int64]]:
        q = np.stack([p[:, 0] * hx + p[:, 1] * hy, -p[:, 0] * hy + p[:, 1] * hx, p[:, 2]], axis=1)
        return local(q)

    return f


def tracks(half: float, w: float, len_: float, h: float) -> Shape:
    return union(
        box((0, half, h / 2), (len_ / 2, w / 2, h / 2), 11, 0.8),
        box((0, -half, h / 2), (len_ / 2, w / 2, h / 2), 11, 0.8),
    )


def crawler(fr: int) -> Shape:
    return union(
        tracks(3.2, 2.0, 10, 2.6),
        box((-1, 0, 3.4), (3.5, 2.4, 1.4), 4, 0.5),
        box((-1.5, 0, 4.6), (2.4, 1.8, 0.4), 12),
        box((3.4, 0, 4.0), (1.4, 2.2, 1.8), 5, 0.5),
        box((4.6, 0, 4.4), (0.3, 1.4, 0.8), 6),
        box((5.4, 0, 1.6), (0.6, 2.8, 0.9), 3),
    )


def warden(fr: int) -> Shape:
    st = 1.2 if fr else -1.2
    return union(
        cyl((st, 1.3, 0), 0.7, 4.2, 8),
        cyl((-st, -1.3, 0), 0.7, 4.2, 8),
        ball((0, 0, 5.6), (1.8, 2.2, 2.0), 4),
        ball((0.4, 0, 8.0), (1.3, 1.3, 1.3), 5),
        box((0.9, 0, 8.1), (0.5, 0.8, 0.35), 6),
        box((2.4, -2.1, 5.4), (2.6, 0.5, 0.5), 8),
        box((0, 1.6, 6.4), (0.9, 0.6, 0.9), 3),
    )


def ram(fr: int) -> Shape:
    return union(
        tracks(3.3, 2.2, 10.5, 2.6),
        box((0, 0, 3.6), (4.6, 2.6, 1.3), 4, 0.6),
        cyl((-0.6, 0, 4.4), 2.5, 2.2, 5),
        cyl((3.6, 0, 5.6), 0.55, 6.6, 3, axis=0),
        box((-4.6, 0, 3.7), (0.3, 2.0, 0.8), 10),
    )


UNITS: list[tuple[str, Callable[[int], Shape]]] = [("crawler", crawler), ("warden", warden), ("ram", ram)]


def heading(d: int) -> tuple[float, float]:
    """The world heading the screen shows as direction d (0 east, 2 south)."""
    a = d * np.pi / 4
    hx, hy = (np.cos(a) + 2 * np.sin(a)) / 2, (2 * np.sin(a) - np.cos(a)) / 2
    k = np.hypot(hx, hy)
    return hx / k, hy / k


# --- the land: a tile's diamond, 32 by 16; u east and v south on the tile, 0 to 16

ROCKH, STEPH = 6, 6  # RUSTLINE.BAS's
BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16


def diamond(h: int = 16, top: int = 0) -> tuple[F, F, NDArray[np.bool_]]:
    """u, v of each pixel of a 32 by h picture whose diamond's top is at row top, and which lie on it."""
    iy, ix = np.mgrid[0:h, 0:32]
    ex, ey = (ix + 0.5 - 16) / 16, (iy - top + 0.5) / 8
    u, v = (ey + ex) / 2 * 16, (ey - ex) / 2 * 16
    return u, v, (u >= 0) & (u < 16) & (v >= 0) & (v < 16)


def perlin(w: int, cells: int, rng: np.random.Generator) -> F:
    """Gradient noise w square, repeating, cells a side."""
    ang = rng.random((cells, cells)) * 2 * np.pi
    gx, gy = np.cos(ang), np.sin(ang)
    y, x = (np.mgrid[0:w, 0:w] + 0.5) * cells / w
    i, j = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = x - i, y - j
    i1, j1 = (i + 1) % cells, (j + 1) % cells
    dot = lambda a, b, ox, oy: gx[b, a] * (fx - ox) + gy[b, a] * (fy - oy)  # noqa: E731
    s = lambda t: t * t * t * (t * (t * 6 - 15) + 10)  # noqa: E731
    a = dot(i, j, 0, 0) + s(fx) * (dot(i1, j, 1, 0) - dot(i, j, 0, 0))
    b = dot(i, j1, 0, 1) + s(fx) * (dot(i1, j1, 1, 1) - dot(i, j1, 0, 1))
    return a + s(fy) * (b - a)


def ramp(stops: list[tuple[float, tuple[int, int, int]]], t: F) -> F:
    xs = [a for a, _ in stops]
    return np.stack([np.interp(t, xs, [c[k] for _, c in stops]) for k in range(3)], axis=-1)


GREEN = [(-0.8, (8, 15, 7)), (-0.3, (10, 19, 8)), (0.1, (13, 23, 9)), (0.5, (17, 27, 10)), (0.9, (21, 31, 12))]


def grass() -> list[tuple[F, NDArray[np.bool_]]]:
    """The 64 square field cut into 16 diamonds, at each of 3 levels, the higher the sunnier."""
    rng = np.random.default_rng(11)
    n = perlin(64, 4, rng) + 0.5 * perlin(64, 8, rng) + 0.25 * perlin(64, 16, rng)
    field = ramp(GREEN, n)
    out = []
    for lvl in range(3):
        for k in range(16):
            u, v, a = diamond()
            fx = np.clip((k % 4) * 16 + u.astype(int), 0, 63)
            fy = np.clip((k // 4) * 16 + v.astype(int), 0, 63)
            rgb = field[fy, fx] * (1 + 0.1 * lvl) + np.array([1.5, 1.0, 0.0]) * lvl
            out.append((np.where(a[..., None], rgb, 0), a))
    return out


def sides_edge(u: F, v: F, same: int, inset: float, wob: float) -> F:
    """Distance inside the land's ragged edge on the sides that do not go on (bits: 1 north, 2 east,
    4 south, 8 west); two edges meeting round their corner."""
    big = np.full_like(u, 99.0)
    soft = lambda a, b: (a + b - np.sqrt((a - b) ** 2 + 9)) / 2  # noqa: E731
    e, es = big.copy(), big.copy()
    if not same & 1:
        e = v - (inset + wob * np.sin(u * 1.1 + 1))
    if not same & 8:
        e = soft(e, u - (inset + wob * np.sin(v * 1.3 + 2)))
    if not same & 4:
        es = (16 - v) - (inset + wob * np.sin(u * 0.9 + 1))
    if not same & 2:
        es = soft(es, (16 - u) - (inset + wob * np.sin(v * 1.2)))
    return soft(e, es)


WATERC, NWATER = 104, 8  # the colours the game turns round to move the water (RUSTLINE.BAS's)


def water_colour(k: int, t: int = 0) -> tuple[float, float, float]:
    """Colour WATERC + k at turn t: a swell's crest every NWATER, as RenderGame turns them."""
    c = ((1 + np.cos((k + t) * 2 * np.pi / NWATER)) / 2) ** 2
    return (5 + 11 * c, 11 + 19 * c, 30 + 22 * c)


def water(same: int) -> tuple[F, NDArray[np.bool_], NDArray[np.int64]]:
    """Water whose land goes on at the sides same: its foam and bank in colours, its body in the
    turning colours by where on a swell each pixel is."""
    u, v, a = diamond()
    d = sides_edge(u, v, same, 1.5, 1.0)
    tau = 2 * np.pi / 16
    phase = ((2 * v + u) / 8 + 0.3 * np.sin(u * tau) + 0.15 * np.sin(v * 2 * tau)) % 1
    force = np.where(a & (d >= 1), WATERC + (phase * NWATER).astype(int) % NWATER, -1)
    rgb = np.zeros((16, 32, 3))
    foam = (d >= 0) & (d < 1)
    rgb[foam] = np.where(((u + v).astype(int) % 4 > 0)[foam, None], (40, 50, 60), (20, 32, 54))
    bank = (d >= -1.2) & (d < 0)
    rgb[bank] = (20, 14, 8)
    return rgb, a & (d >= -1.2), force


def dirt() -> tuple[F, NDArray[np.bool_]]:
    u, v, a = diamond()
    fx, fy = u - 8, v - 8
    d = np.hypot(fx, fy) + np.sin(np.arctan2(fy, fx) * 3 + 1) * 0.9
    rng = np.random.default_rng(5)
    rgb = ramp([(0, (34, 26, 16)), (1, (22, 16, 10))], rng.random(u.shape)) * 1.0
    th = BAYER4[np.arange(16)[:, None] % 4, np.arange(32)[None] % 4]
    on = a & ((d < 5.5) | ((d < 7.5) & ((7.5 - d) / 2 > th)))
    pebbles = rng.random(u.shape) < 0.06
    rgb[pebbles] = (40, 36, 34)
    return np.where(on[..., None], rgb, 0), on


def flowers() -> tuple[F, NDArray[np.bool_]]:
    rgb, a = np.zeros((16, 32, 3)), np.zeros((16, 32), bool)
    rng = np.random.default_rng(9)
    for c in ((60, 52, 20), (52, 30, 56), (40, 36, 36)):
        uu, vv = 3 + rng.random() * 10, 3 + rng.random() * 10
        x, y = int(16 + uu - vv), int((uu + vv) / 2)
        rgb[y, x], a[y, x] = c, True
        if c[2] < 50:
            rgb[y + 1, x], a[y + 1, x] = (8, 16, 6), True
    return rgb, a


def moss(k: int) -> NDArray[np.int64]:
    """Crystals out of the grass, k 1 to 3 the thicker: in the game's own colours, 8 the glow that
    breathes, 9 and 22 the crystal, 31 the tip, 1 its foot."""
    img = np.full((16, 32), -1)
    rng = np.random.default_rng(20 + k)
    for _ in range(k * 3 + 1):
        uu, vv = 2.5 + rng.random() * 11, 2.5 + rng.random() * 11
        x, y = int(16 + uu - vv), int((uu + vv) / 2)
        h = 2 + int(rng.random() * 2.99)
        img[min(y + 1, 15), x : x + 2] = 1
        for j in range(h + 1):
            if y - j >= 0:
                img[y - j, x] = 8 if j == h else 22 if j > h // 2 else 9
        if y - h + 1 >= 0:
            img[y - h + 1, x + 1] = 22
        if y - h - 1 >= 0:
            img[y - h - 1, x] = 31
    return img


def rock(same: int) -> Shape:
    """Boulders in a tile, four a tile as if the rock went on, their sides open where it does not."""
    parts = []
    for j in range(-1, 3):
        for i in range(-1, 3):
            r = int("31524031"[(i % 2) * 2 + (j % 2) * 4])
            cx, cy = i * 8 + 4 + r - 2, j * 8 + 4 + (r * 3 % 5) - 2
            parts.append(ball((cx, cy, 0), (5.2, 5.0, 4.0 + r * 0.8), 13))
    lumps = union(*parts)

    def f(p: F) -> tuple[F, NDArray[np.int64]]:
        d, m = lumps(p)
        u, v = p[:, 0], p[:, 1]
        edge = sides_edge(u, v, same, 2.0, 0.8)
        inside = np.maximum.reduce([-u, -v, u - 16, v - 16])
        top = p[:, 2] - ((u + v) / 2 + ROCKH - 0.6)
        return np.maximum.reduce([d, -edge, inside, top, -p[:, 2]]), m

    return f


def tree() -> Shape:
    return union(
        cyl((0, 0, 0), 1.1, 10, 14),
        ball((0, 0, 10.5), (5.0, 5.0, 3.0), 15),
        ball((0, 0, 9.0), (4.2, 4.2, 1.2), 16),
    )


def face(k: int) -> tuple[F, NDArray[np.bool_]]:
    """A cliff's step, STEPH high, under a diamond's lower left (k 0) or right (1) edge: earth and
    stones, the left lit."""
    h = 8 + STEPH
    rgb, a = np.zeros((h, 16, 3)), np.zeros((h, 16), bool)
    rng = np.random.default_rng(30 + k)
    for x in range(16):
        e = x // 2 if k == 0 else (15 - x) // 2
        for y in range(STEPH):
            c = np.array((30, 22, 14) if k == 0 else (19, 13, 8), float) * (1.05 - 0.06 * y)
            if rng.random() < 0.12:
                c = np.array((38, 34, 34) if k == 0 else (22, 20, 22), float)
            rgb[e + y, x], a[e + y, x] = c, True
    return rgb, a


# --- the camera


def normal(s: Shape, p: F) -> F:
    e = 0.05
    n = np.zeros_like(p)
    for i in range(3):
        o = np.zeros(3)
        o[i] = e
        n[:, i] = s(p + o)[0] - s(p - o)[0]
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-9)


def march(s: Shape, start: F, d: F, far: float, steps: int = 160) -> tuple[F, NDArray[np.bool_]]:
    t = np.zeros(len(start))
    hit = np.zeros(len(start), bool)
    for _ in range(steps):
        live = ~hit & (t < far)
        if not live.any():
            break
        e, _ = s(start[live] + t[live, None] * d)
        idx = np.flatnonzero(live)
        hit[idx[e < 0.02]] = True
        t[idx] += np.maximum(e, 0.02) * 0.9
    return t, hit


def shade(
    s: Shape, mats: dict[int, Mat], w: int, h: int, ox: float, oy: float, hd: tuple[float, float] = (1, 0), fr: int = 0
) -> tuple[F, NDArray[np.bool_], NDArray[np.bool_]]:
    """The picture w by h whose (ox, oy) is the world's 0, 0, 0: RGB 0 to 63, where it is, and
    where it shades the ground. Patterns that move go by heading hd and frame fr."""
    iy, ix = np.mgrid[0:h, 0:w]
    sx, sy = (ix + 0.5 - ox).ravel(), (iy + 0.5 - oy).ravel()
    z0 = 80.0
    start = np.stack([(sx + 2 * (sy + z0)) / 2, (2 * (sy + z0) - sx) / 2, np.full_like(sx, z0)], axis=1)
    t, hit = march(s, start, -VIEW, 400)
    p = start + t[:, None] * -VIEW
    rgb = np.zeros((len(p), 3))
    if hit.any():
        ph = p[hit]
        n = normal(s, ph)
        _, mat = s(ph)
        lam = np.clip(n @ LIGHT, 0, 1)
        # shadow: toward the light, from just off the surface
        _, sh = march(s, ph + n * 0.3, LIGHT, 60, 60)
        lit = np.where(sh, 0.25, 1.0)
        # ambient occlusion: how open the surface is along its normal
        ao = np.ones(len(ph))
        for k, dist in enumerate((1.0, 2.0, 4.0)):
            e, _ = s(ph + n * dist)
            ao -= (dist - np.clip(e, 0, dist)) / dist * (0.18 / (k + 1))
        base = np.array([mats[m].rgb for m in mat], float)
        shine = np.array([mats[m].shine for m in mat])
        pat = np.array([mats[m].pattern for m in mat])
        # patterns
        q = ph
        f = np.ones(len(ph))
        f = np.where(
            pat == "panels",
            f * (1 - 0.18 * ((np.floor(q[:, 0] / 4) + np.floor(q[:, 1] / 4) + np.floor(q[:, 2] / 4)) % 2)),
            f,
        )
        f = np.where(pat == "ribs", f * np.where(q[:, 0] % 4 < 1, 0.6, 1.0), f)
        f = np.where(pat == "stripes", np.where(((q[:, 0] + q[:, 2]) // 2.5) % 2 == 0, 0.25, 1.0), f)
        rnd = np.random.default_rng(3).random(len(ph))
        f = np.where(pat == "earth", f * (0.85 + 0.3 * rnd), f)
        along = q[:, 0] * hd[0] + q[:, 1] * hd[1] + fr * 1.0
        f = np.where(pat == "treads", f * np.where(np.floor(along) % 2 == 0, 0.55, 1.15), f)
        f = np.where(pat == "moss", f * (0.8 + 0.5 * (rnd > 0.7)), f)
        f = np.where(pat == "crack", f * (0.9 + 0.2 * rnd), f)
        spot = (np.sin(q[:, 0] * 2.1) * np.sin(q[:, 1] * 1.7 + 1) * np.sin(q[:, 2] * 2.3)) > 0.45
        f = np.where((pat == "spots") & spot, f * 1.35, f)
        diffuse = 0.42 + 0.75 * lam * lit
        half = (LIGHT + VIEW) / np.linalg.norm(LIGHT + VIEW)
        spec = np.clip(n @ half, 0, 1) ** 24 * shine * lit * 40
        col = base * (diffuse * ao * f)[:, None] + spec[:, None]
        glow = mat == 9
        col[glow] = base[glow]
        rgb[hit] = col
    # the ground's shade: ground points that see no light
    g = np.stack([(sx + 2 * sy) / 2, (2 * sy - sx) / 2, np.zeros_like(sx)], axis=1)
    _, gs = march(s, g + LIGHT * 0.3, LIGHT, 40, 60)
    return np.clip(rgb, 0, 63).reshape(h, w, 3), hit.reshape(h, w), (gs & ~hit).reshape(h, w)


def outline(rgb: F, a: NDArray[np.bool_]) -> F:
    """Darken the shape's edge pixels: those beside the background."""
    out = rgb.copy()
    pad = np.pad(a, 1)
    edge = a & ~(pad[:-2, 1:-1] & pad[2:, 1:-1] & pad[1:-1, :-2] & pad[1:-1, 2:])
    out[edge] *= 0.55
    return out


def ring(rgb: F, a: NDArray[np.bool_]) -> tuple[F, NDArray[np.bool_]]:
    """A dark line round the shape, outside it: small things stand out from the grass."""
    pad = np.pad(a, 1)
    near = pad[:-2, 1:-1] | pad[2:, 1:-1] | pad[1:-1, :-2] | pad[1:-1, 2:]
    r = near & ~a
    out = rgb.copy()
    out[r] = (6, 6, 8)
    return out, a | r


# --- the palette and the data


def base_palette(src: str) -> F:
    pal = np.zeros((32, 3))
    for c, r, g, b in re.findall(r"^\s*Shade (\d+), (\d+), (\d+), (\d+)", src, re.M):
        if int(c) < 32 and not pal[int(c)].any():
            pal[int(c)] = (int(r), int(g), int(b))
    return pal


def fit(base: F, px: F, n: int = 32, rounds: int = 25) -> F:
    """n colours that with base cover the pixels px best (k-means, base held)."""
    rng = np.random.default_rng(1)
    free = px[rng.choice(len(px), n, replace=False)].copy()
    for _ in range(rounds):
        pal = np.vstack([base, free])
        near = ((px[:, None, :] - pal[None]) ** 2).sum(2).argmin(1)
        for i in range(n):
            m = px[near == 32 + i]
            free[i] = m.mean(0) if len(m) else px[rng.integers(len(px))]
    return np.round(free)


BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16 - 0.47


def quantize(rgb: F, a: NDArray[np.bool_], pal: F) -> NDArray[np.int64]:
    """Ordered dithering between two colours: the nearest, and the one across the pixel from it,
    mixed by the Bayer matrix in the share that puts their blend at the pixel's colour."""
    h, w, _ = rgb.shape
    x = rgb.reshape(-1, 3)
    c1 = ((x[:, None] - pal[None]) ** 2).sum(2).argmin(1)
    c2 = ((2 * x - pal[c1])[:, None] - pal[None]) ** 2
    c2[np.arange(len(x)), c1] = np.inf
    c2 = c2.sum(2).argmin(1)
    v = pal[c2] - pal[c1]
    t = ((x - pal[c1]) * v).sum(1) / np.maximum((v * v).sum(1), 1e-9)
    # a second colour too far off is no blend: keep the first
    far = np.linalg.norm(x - (pal[c1] + v * np.clip(t, 0, 1)[:, None]), axis=1) > 6
    th = (BAYER + 0.47).ravel()[(np.arange(h)[:, None] % 4 * 4 + np.arange(w)[None] % 4).ravel()]
    pick = np.where((np.clip(t, 0, 1) > th) & ~far, c2, c1).reshape(h, w)
    return np.where(a, pick, -1)


def encode(img: NDArray[np.int64]) -> list[str]:
    """Rows of symbols: a colour CHR$(35 + c) below 64, from 64 "!" then CHR$(35 + c - 64); 99
    nothing, 32 nothing but shade; from 100 a run of the next symbol, CODE - 97 long."""

    def sym(v: int) -> str:
        return "c" if v == -1 else " " if v == -2 else chr(35 + v) if v < 64 else "!" + chr(35 + v - 64)

    flat = [int(v) for v in img.ravel()]
    tokens, i = [], 0
    while i < len(flat):
        j = i
        while j < len(flat) and flat[j] == flat[i] and j - i < 29:
            j += 1
        tokens += [chr(97 + j - i) + sym(flat[i])] if j - i >= 3 else [sym(flat[i])] * (j - i)
        i = j
    # a run and its symbol stay on one line
    rows = [""]
    for t in tokens:
        if len(rows[-1]) + len(t) > 100:
            rows.append("")
        rows[-1] += t
    return rows


def block(label: str, comment: str, lines: list[str]) -> str:
    return f"{label}:\n' {comment}\n" + "".join(f"DATA {x}\n" for x in lines)


def replace_block(src: str, label: str, text: str) -> str:
    m = re.search(rf"^{label}:\n(?:'.*\n)*(?:DATA .*\n)*", src, re.M)
    if m:
        return src[: m.start()] + text + src[m.end() :]
    i = src.index("\nMapData:\n") + 1
    return src[:i] + text + "\n" + src[i:]


def png(path: Path, img: NDArray[np.int64], pal: F, k: int = 4) -> None:
    from PIL import Image

    bg = np.where(img[..., None] == -2, np.array([16, 20, 16]), np.array([30, 40, 24]))
    rgb = np.where(img[..., None] < 0, bg, pal[np.maximum(img, 0)]) * 255 / 63
    Image.fromarray(rgb.astype(np.uint8)).resize((img.shape[1] * k, img.shape[0] * k), Image.NEAREST).save(path)


@dataclass
class Pic:
    name: str
    rgb: F | None  # to dither; or None, img given
    a: NDArray[np.bool_]
    sh: NDArray[np.bool_]
    img: NDArray[np.int64] | None = None
    force: NDArray[np.int64] | None = None  # colours fixed where 0 or more


def pictures() -> list[Pic]:
    """All of them, in the order BuildArt reads them."""
    pics: list[Pic] = []
    none = lambda a: np.zeros_like(a)  # noqa: E731
    for k, (rgb, a) in enumerate(grass()):
        pics.append(Pic(f"grass{k}", rgb, a, none(a)))
    rgb, a = dirt()
    pics.append(Pic("dirt", rgb, a, none(a)))
    rgb, a = flowers()
    pics.append(Pic("flowers", rgb, a, none(a)))
    for k in (1, 2, 3):
        img = moss(k)
        pics.append(Pic(f"moss{k}", None, img >= 0, none(img >= 0), img))
    for same in range(16):
        rgb, a, force = water(same)
        pics.append(Pic(f"water{same}", rgb, a, none(a), force=force))
    mats = materials(0)
    for same in range(16):
        rgb, a, _ = shade(rock(same), mats, 32, 16 + ROCKH, 16, ROCKH)
        pics.append(Pic(f"rock{same}", outline(rgb, a), a, none(a)))
    rgb, a, sh = shade(tree(), mats, 16, 24, 8, 21)
    rgb, a = ring(rgb, a)
    pics.append(Pic("tree", rgb, a, sh & ~a))
    for k in (0, 1):
        rgb, a = face(k)
        pics.append(Pic(f"face{k}", rgb, a, none(a)))
    for name, make, n, bh in BUILDINGS:
        for team in (0, 1):
            rgb, a, _ = shade(make(), materials(team), 32 * n, bh + 16 * n, 16 * n, bh)
            pics.append(Pic(f"{name}{team}", outline(rgb, a), a, none(a)))
    # the units: kind, side, the screen's 8 headings, 2 frames; 16 by 16, the feet at 8, 12
    for name, model in UNITS:
        for team in (0, 1):
            for d in range(8):
                hd = heading(d)
                for fr in (0, 1):
                    rgb, a, sh = shade(turned(model(fr), *hd), materials(team), 16, 16, 8, 12, hd, fr)
                    rgb, a = ring(rgb, a)
                    pics.append(Pic(f"{name}{team}{d}{fr}", rgb, a, sh & ~a))
    return pics


NFIT = 72  # the colours the art tool picks: 32 to 103


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--preview", type=Path)
    args = ap.parse_args()
    src = BAS.read_text()
    base = base_palette(src)
    pics = pictures()
    px = np.vstack([p.rgb[p.a & (p.force < 0 if p.force is not None else True)] for p in pics if p.rgb is not None])
    fitted = fit(base, px, NFIT)
    pal = np.vstack([base, fitted, [water_colour(k) for k in range(NWATER)]])
    lines = []
    for p in pics:
        img = p.img if p.img is not None else quantize(p.rgb, p.a, pal[: 32 + NFIT])  # type: ignore[arg-type]
        if p.force is not None:
            img = np.where(p.force >= 0, p.force, img)
        img = np.where(p.sh, -2, img)
        lines.append(f"{img.shape[1]}, {img.shape[0]}")
        lines += [f'"{r}"' for r in encode(img)]
        lines.append('""')
        if args.preview:
            args.preview.mkdir(parents=True, exist_ok=True)
            png(args.preview / f"{p.name}.png", img, pal)

    rows = [", ".join(str(int(v)) for v in c) for c in fitted]
    src = replace_block(src, "PalData", block("PalData", f"colours 32 to {31 + NFIT}: red, green, blue", rows))
    src = replace_block(
        src,
        "ArtData",
        block(
            "ArtData",
            "grass by level, what lies on it, rock, the tree, a cliff's faces, the buildings, the units:\n"
            "' width, height, then rows of symbols (Unpack); tools/rustline_art.py writes these",
            lines,
        ),
    )
    BAS.write_text(src)
    print(f"{len(pics)} pictures, {sum(len(x) for x in lines)} bytes of data")


if __name__ == "__main__":
    main()
