"""gfx's architecture diagrams: python3 tools/diagrams/gfx.py stdlib/gfx/docs/img"""

import sys
from itertools import pairwise

from svg import MUTED, THEME, Svg

OUT = sys.argv[1]
W = 1000
L, R = 32, W - 32


def legend(s: Svg, y: int) -> None:
    s.arrow([(L, y), (L + 40, y)])
    s.text(L + 48, y + 4, "calls (depends on)", size=12, fill=MUTED)
    s.arrow([(L + 200, y), (L + 240, y)], color=THEME["warn"][1], dash=True)
    s.text(L + 248, y + 4, "at display.flip", size=12, fill=MUTED)


def layers() -> None:
    s = Svg(W, 880, "Layers", "Dependencies point down only. An entry draws through one backend table.")
    full = R - L

    s.panel(L, 90, full, 70, "front", "program", "BASIC, through gfx.qbx's glue")
    s.chips(L + 16, 120, ["canvas& = display.open(320, 200, 8, 0)", "draw.sprite canvas&, x, y, hero, 0",
                          "display.flip"], "front")

    s.panel(L, 186, full, 84, "front", "src/api", "arguments checked, a command made")
    s.chips(L + 16, 226, ["display.bas · .c", "surface.bas · .c", "draw.bas · .c", "style.c",
                          "scene.bas (declared)"], "front")

    s.panel(L, 296, full, 84, "engine", "src/core", "one state, Gx; nothing static")
    s.chips(L + 16, 336, ["gfx: gx_check · gx_begin", "surface", "command", "style", "palette", "font",
                          "shape", "handle", "mem"], "engine", maxw=full - 32)

    s.panel(L, 406, full, 176, "engine", "src/backend", "backend.h: one table, its state its own")
    half = (full - 48) / 2
    s.box(L + 16, 446, half, 120, "engine", r=8, dash=True)
    s.text(L + 32, 468, "soft/  every platform, the reference", size=12, mono=True)
    s.chips(L + 32, 480, ["backend", "ink", "shapes", "sprite", "poly", "scanner", "span", "steps",
                          "fill8 · 16 · 32"], "engine", maxw=half - 32)
    s.box(L + 32 + half, 446, half, 120, "engine", r=8, dash=True)
    s.text(L + 48 + half, 468, "gpu/  a frame recorded as a list", size=12, mono=True)
    s.chips(L + 48 + half, 480, ["backend", "gpu", "list", "vertex", "shaders"], "engine", maxw=half - 32)

    s.panel(L, 608, full, 70, "base", "src/base · src/image", "no state")
    s.chips(L + 16, 638, ["pixels", "geom", "cover", "disc", "depth", "gmath", "bmp · png · inflate"], "base")

    s.panel(L, 704, full, 120, "base", "the host (QxHost)", "the interpreter's; knows nothing of gfx")
    s.chips(L + 16, 742, ["gfx_page · gfx_show", "gpu_play · gpu_read", "clock_us", "file_read"], "base")
    for i, name in enumerate(["DOS: VGA · VBE", "SDL_GPU", "WebGL2", "terminal · tests"]):
        x = L + 16 + i * 160
        s.box(x, 780, 148, 32, "base", r=6, dash=True)
        s.text(x + 74, 801, name, size=12, anchor="middle", mono=True)
    s.box(R - 236, 780, 220, 32, "base", r=6, dash=True)
    s.text(R - 126, 801, "DOS: the runtime's mem.asm", size=12, anchor="middle", mono=True)

    cx = W / 2
    for y0, y1 in [(160, 186), (270, 296), (380, 406), (582, 608)]:
        s.arrow([(cx, y0), (cx, y1 - 2)])
    s.arrow([(R - 120, 582), (R - 120, 702)], color=THEME["warn"][1], dash=True)
    legend(s, 856)
    s.save(f"{OUT}/layers.svg")


def step(s: Svg, x: int, y: int, w: int, theme: str, title: str, note: str) -> None:
    s.box(x, y, w, 64, theme, r=8)
    s.text(x + 12, y + 26, title, size=13, weight=700, fill=THEME[theme][2], mono=True)
    s.text(x + 12, y + 47, note, size=12, fill=MUTED)


def draw_call() -> None:
    s = Svg(W, 470, "A draw call", "Checked and logged once; drawn by the screen's backend.")
    w, gap = 220, 18
    xs = [L + i * (w + gap) for i in range(4)]
    top = [
        ("front", "draw.sprite", "dst, x, y, src, flags"),
        ("front", "api/draw.c", "its arguments checked"),
        ("engine", "gx_check", "dst, src apart, flags, colour"),
        ("engine", "gx_begin", "placed by dst's camera, logged"),
    ]
    for x, (theme, title, note) in zip(xs, top, strict=True):
        step(s, x, 100, w, theme, title, note)
    for a, b in pairwise(xs):
        s.arrow([(a + w, 132), (b - 2, 132)])

    soft = [
        ("engine", "soft: begin", "the ink made once"),
        ("engine", "sprite · poly", "rows, or spans walked"),
        ("engine", "fill8 · 16 · 32", "pixels into the page"),
        ("base", "gfx_show", "the host shows the page"),
    ]
    gpu = [
        ("engine", "gpu: begin", "target, texture, blend"),
        ("engine", "backend.c", "vertices with their texels"),
        ("engine", "list.c", "batched while they match"),
        ("base", "gpu_play", "SDL_GPU or WebGL2 plays"),
    ]
    for row, y in ((soft, 230), (gpu, 350)):
        for i, (x, (theme, title, note)) in enumerate(zip(xs, row, strict=True)):
            step(s, x, y, w, theme, title, note)
            if i == 2:
                s.arrow([(x + w, y + 32), (xs[3] - 2, y + 32)], color=THEME["warn"][1], dash=True)
            elif i < 2:
                s.arrow([(x + w, y + 32), (xs[i + 1] - 2, y + 32)])
    trunk = L - 14
    s.line([(xs[3] + w / 2, 164), (xs[3] + w / 2, 196), (trunk, 196), (trunk, 382)])
    s.arrow([(trunk, 262), (xs[0] - 2, 262)])
    s.arrow([(trunk, 382), (xs[0] - 2, 382)])
    legend(s, 446)
    s.save(f"{OUT}/draw-call.svg")


layers()
draw_call()
