#!/usr/bin/env python3
"""highvale_test.py [EXE]: HIGHVALE.BAS's terrain, missions and play on the host build.

Flat land at a known height must fill each column from the row the farthest step projects to, down, in
the fogged tone that step's distance gives, the horizon turned by the roll. A step whose top rounded up onto
the open row once divided by zero, easing its light over no rows. A slanting ramp's top edge must be where
reading heights between texels puts it: read at the nearest texel, it stepped up to 12 rows a column, the
blocky near ground first drawn. Each mission's land must rise from its sea without a cliff and place its
foes on land, clear of the start; an autopilot must clear the first; a craft with no hull left by flak
must be downed; and every screen must draw.
"""

import sys
import math
import tempfile
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEMOS = ROOT / "src/plat/wasm/web/demos"
GAME = DEMOS / "HIGHVALE.BAS"

# The view, the terrain's steps and the palette, derived from the game's constants by hand.
VIEW_W, VIEW_H = 320, 152
FOCAL = 160 / math.tan(0.7)
SCALE = 0.5
FAR, FOG_NEAR, FOG_STEPS = 420, 150, 3
GROUND, LIGHTS, GROUNDS, GRASS = 8, 24, 3, 1
# The most the land may rise from the sea to its next texel, in 16ths of a step: the sea once lay at 0 while the
# land rose from the sea's line, every shore a cliff of up to 506.
SHORE = 48

HEAD = """option explicit
import gfx.display, gfx.surface, gfx.draw
from highvale.model.land import Land, STEP_PARTS, LEVELS
from highvale.model.game import Game, TITLE, BRIEFING, FLYING
from highvale.model.game import CLEARED, DOWNED, VICTORY
from highvale.model.foes import FLAK, DRONE
from highvale.land.palette import set_colours
from highvale.land.maps import allot_land, ground_at, wrapped
from highvale.land.maps import level_side, level_base
from highvale.play.mission import start_mission, foes_left
from highvale.play.combat import atan2_of, turned
from highvale.view.eye import Camera, aim_camera, aim_fall
from highvale.view.eye import VIEW_W, VIEW_H
from highvale.view.terrain import Terrain, make_terrain, draw_terrain
from highvale.view.scene import draw_scene
from highvale.view.art import Art, make_art
from highvale.main import step_state, draw_overlay
dim canvas as long
dim cockpit as long
dim tv as Terrain
dim k as Camera
dim ld as Land
dim i as long
dim x as integer
dim y as integer
canvas = gfx.display.open(320, 200, 8, 0)
set_colours
cockpit = gfx.surface.new_view(canvas, 0, 0, VIEW_W, VIEW_H)
make_terrain tv
allot_land ld
"""

# Each column's first drawn row, then its colours at that row and at the bottom.
TOPS = """
sub tops (byval cockpit as long)
    dim x as integer
    dim y as integer
    for x = 0 to VIEW_W - 1
        for y = 0 to VIEW_H - 1
            if gfx.surface.point(cockpit, x, y) <> 0 then exit for
        next
        print "top"; x; y;
        if y < VIEW_H then print gfx.surface.point(cockpit, x, y);
        print gfx.surface.point(cockpit, x, VIEW_H - 1)
    next
end sub
"""

FLAT = """for i = 0 to ubound(ld.heights)
    ld.heights(i) = 40 * STEP_PARTS
    ld.lights(i) = 5
next
k.x = 128
k.y = 128
k.height = 60
k.horizon = 50
k.roll = {roll}
gfx.draw.clear cockpit, 0
draw_terrain cockpit, tv, ld, k
tops cockpit
"""

# A ramp rising 30 steps over 4 texels, its top edge running across the view at a slant 12 texels ahead,
# 16 steps over the eye, at every level of the land: its edge is the outline.
RAMP = """dim m as integer
dim n as long
dim r as single
for m = 0 to LEVELS - 1
    n = level_side(m) - 1
    for y = 0 to n
        for x = 0 to n
            i = level_base(m) + y * (n + 1) + x
            r = (x * 2 ^ m - 136 - (y * 2 ^ m - 128) * 0.7) / 4
            if r < 0 then r = 0
            if r > 1 then r = 1
            ld.heights(i) = (10 + 30 * r) * STEP_PARTS
            ld.lights(i) = 5
        next
    next
next
k.x = 128
k.y = 128
k.height = 24
k.horizon = 140
gfx.draw.clear cockpit, 0
draw_terrain cockpit, tv, ld, k
tops cockpit
"""

MISSIONS = """dim g as Game
allot_land g.vale
dim n as integer
dim d as single
dim most as long
dim sea as long
for n = 1 to 3
    start_mission g, n
    most = 0
    sea = 32767
    for i = 0 to 256& * 257 - 1
        if g.vale.heights(i) < sea then sea = g.vale.heights(i)
    next
    for i = 0 to 256& * 257 - 1
        if g.vale.heights(i) <> sea then continue for
        d = g.vale.heights(i + 1) - sea
        if d > most then most = d
        d = g.vale.heights(i + 257) - sea
        if d > most then most = d
    next
    print "land"; n; most; g.vale.sea; sea
    for i = 0 to ubound(g.foes)
        if g.foes(i).kind = 0 then continue for
        d = abs(wrapped(g.foes(i).place.x - 128))
        d = d + abs(wrapped(g.foes(i).place.y - 128))
        print "foe"; n; g.foes(i).kind; d;
        print g.foes(i).place.z - ground_at(g.vale, g.foes(i).place.x, g.foes(i).place.y);
        print ground_at(g.vale, g.foes(i).place.x, g.foes(i).place.y) - g.vale.sea
    next
next
"""

# The pilot turns to the nearest foe, keeps the sight's ray on it by its height, closing in while the ground
# holds it too high, and fires when lined up.
PILOT = """dim g as Game
dim a as Art
dim t as single
dim near as integer
dim best as single
dim d as single
dim dx as single
dim dy as single
dim want as single
dim low as integer
dim frames as long
make_art a
gfx.draw.set_font a.font
allot_land g.vale
start_mission g, 1
g.state = FLYING
g.heli.hull = 30000
g.heli.rounds = 30000
do while g.state = FLYING and t < 400
    best = 1e9
    for i = 0 to ubound(g.foes)
        if g.foes(i).kind = 0 or g.foes(i).hp <= 0 then continue for
        dx = wrapped(g.foes(i).place.x - g.heli.place.x)
        dy = wrapped(g.foes(i).place.y - g.heli.place.y)
        d = sqr(dx * dx + dy * dy)
        if d < best then best = d: near = i
    next
    dx = wrapped(g.foes(near).place.x - g.heli.place.x)
    dy = wrapped(g.foes(near).place.y - g.heli.place.y)
    want = turned(atan2_of(dy, dx) - g.heli.heading)
    g.asked.turn = sgn(want) * (abs(want) > 0.01)
    g.asked.turn = -g.asked.turn
    aim_camera k, g.heli
    d = g.foes(near).place.z + 1 + aim_fall(k) * best - g.heli.place.z
    low = g.heli.place.z < ground_at(g.vale, g.heli.place.x, g.heli.place.y) + 4
    g.asked.pitch = -(best > 90 or (low and d < -1 and best > 12))
    g.asked.climb = -sgn(d) * (abs(d) > 0.5)
    g.asked.guns = abs(want) < 0.04 and best < 200
    step_state g, k, canvas, 0, 1 / 30
    if frames mod 15 = 0 then
        draw_scene cockpit, k, tv, g.vale, g.foes(), g.shots(), g.blasts(), a, t
        draw_overlay canvas, g, t
    end if
    frames = frames + 1
    t = t + 1 / 30
loop
print "pilot"; g.state; foes_left(g.foes()); int(t); " "; g.note
"""

# A craft hanging still in a flak site's sight, its hull nearly gone.
DOWN = """dim g as Game
allot_land g.vale
start_mission g, 1
for i = 0 to ubound(g.foes)
    if g.foes(i).kind = DRONE then g.foes(i).kind = 0
next
g.state = FLYING
g.heli.place = g.foes(0).place
g.heli.place.x = g.heli.place.x + 60
g.heli.place.z = ground_at(g.vale, g.heli.place.x, g.heli.place.y) + 60
g.heli.hull = 5
dim t as single
do while g.state = FLYING and t < 60
    step_state g, k, canvas, 0, 1 / 30
    t = t + 1 / 30
loop
print "down"; g.state; " "; g.note
g.state = FLYING
g.heli.hull = 50
g.heli.rounds = 0
g.heli.rockets = 0
step_state g, k, canvas, 0, 1 / 30
print "dry"; g.state; " "; g.note
"""

SCREENS = """dim g as Game
dim a as Art
dim s as integer
make_art a
gfx.draw.set_font a.font
allot_land g.vale
start_mission g, 1
for s = TITLE to VICTORY
    g.state = s
    g.note = "MISSION CLEAR"
    aim_camera k, g.heli
    draw_scene cockpit, k, tv, g.vale, g.foes(), g.shots(), g.blasts(), a, 1
    draw_overlay canvas, g, 1
    gfx.display.flip
    print "screen"; s
next
"""


def run(exe: str, tmp: Path, name: str, body: str, subs: str = "") -> list[list[str]]:
    """body run after HEAD in tmp, beside the game's modules: its lines' fields."""
    (tmp / f"{name}.BAS").write_text(HEAD + body + "gfx.display.close\nend\n" + subs)
    r = subprocess.run([exe, f"{name}.BAS"], cwd=tmp, capture_output=True, timeout=600)
    out = r.stdout.decode(errors="replace")
    if r.returncode:
        sys.exit(f"highvale FAIL: {name}\n{(out + r.stderr.decode(errors='replace'))[-800:]}")
    return [line.split() for line in out.splitlines() if line.strip()]


def steps() -> list[float]:
    """The terrain's step distances: from 1 by a quarter, the stride doubling past 64 strides."""
    z, stride, out = 1.0, 0.25, []
    while z < FAR:
        if z >= 64 * stride:
            stride *= 2
        out.append(z)
        z += stride
    return out


def flat(exe: str, tmp: Path, roll: float) -> list[str]:
    """Flat land 20 steps under the eye, lit 5: each column's top row and tones against the hand's."""
    z = steps()[-1]
    fog = min(1 + int((z - FOG_NEAR) * (FOG_STEPS - 1) / (FAR - FOG_NEAR)), FOG_STEPS - 1)
    far = GROUND + (fog * GROUNDS + GRASS) * LIGHTS + 5
    near = GROUND + GRASS * LIGHTS + 5
    bad = []
    for f in run(exe, tmp, "FLAT", FLAT.replace("{roll}", repr(roll)), TOPS):
        x, top, colour, bottom = (int(v) for v in f[1:5])
        want = max(0, math.ceil(50 + 20 * SCALE * FOCAL / z + (x - 159.5) * math.tan(roll) - 1e-4))
        if (top, colour, bottom) != (want, far, near):
            bad.append(f"x {x}: top {top} {colour} bottom {bottom}, want {want} {far} {near}")
    return bad


def ramp_height(x: float, y: float) -> float:
    """RAMP's height at texel x, y, in steps."""
    return 10 + 30 * min(1.0, max(0.0, (x - 136 - (y - 128) * 0.7) / 4))


def ramp_edge(column: int) -> int:
    """The row a column's ramp edge is drawn from: the highest any step projects to, its height read
    between the four texels round it."""
    across, best = (column - 159.5) / FOCAL, float("inf")
    for z in steps():
        x, y = 128 + z, 128 + z * across
        ix, iy, fx, fy = math.floor(x), math.floor(y), x % 1, y % 1
        a = ramp_height(ix, iy) + (ramp_height(ix + 1, iy) - ramp_height(ix, iy)) * fx
        b = ramp_height(ix, iy + 1) + (ramp_height(ix + 1, iy + 1) - ramp_height(ix, iy + 1)) * fx
        best = min(best, 140 + (24 - (a + (b - a) * fy)) * SCALE * FOCAL / z)
    return max(0, math.ceil(best - 1e-4))


def ramp(exe: str, tmp: Path) -> list[str]:
    """The columns whose ramp edge is not where reading between texels puts it."""
    return [
        f"ramp x {x}: edge {int(f[2])}, want {ramp_edge(x)}"
        for x, f in enumerate(run(exe, tmp, "RAMP", RAMP, TOPS))
        if abs(int(f[2]) - ramp_edge(x)) > 1
    ]


def main() -> None:
    exe = str(Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build/host/basic").resolve())
    check = subprocess.run([exe, "--check", str(GAME)], capture_output=True, text=True, timeout=60)
    if check.returncode:
        sys.exit("highvale FAIL\n" + (check.stdout + check.stderr)[-600:])
    bad = []
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d)
        for p in sorted(p for s in ("highvale", "engine") for p in (DEMOS / s).rglob("*.bas")):
            (tmp / p.relative_to(DEMOS)).parent.mkdir(parents=True, exist_ok=True)
            (tmp / p.relative_to(DEMOS)).write_bytes(p.read_bytes())
        for roll in (0.0, math.atan(0.25)):
            bad += flat(exe, tmp, roll)[:4]
        bad += ramp(exe, tmp)[:4]
        lands = run(exe, tmp, "MISSIONS", MISSIONS)
        foes = [f for f in lands if f[0] == "foe"]
        for f in lands:
            if f[0] == "land" and (int(f[2]) > SHORE or int(f[3]) != int(f[4]) // 16):
                bad.append(f"mission {f[1]}: shore rises {f[2]} parts, sea {f[3]} steps, its texels {f[4]} parts")
        counts = [sum(1 for f in foes if f[1] == str(n)) for n in (1, 2, 3)]
        if counts != [4, 7, 10]:
            bad.append(f"missions: foes {counts}, want [4, 7, 10]")
        for f in foes:
            if float(f[3]) < 70 or float(f[5]) < 6 or not -0.01 < float(f[4]) - 25 * (f[2] == "2") < 0.01:
                bad.append("foe misplaced: " + " ".join(f))
        pilot = run(exe, tmp, "PILOT", PILOT)[-1]
        if pilot[1:3] != ["3", "0"]:
            bad.append("pilot: state, foes left, seconds: " + " ".join(pilot[1:]))
        down = {f[0]: f[1:] for f in run(exe, tmp, "DOWN", DOWN)}
        if down["down"] != ["4", "SHOT", "DOWN"] or down["dry"] != ["4", "OUT", "OF", "ROUNDS"]:
            bad.append(f"downed: {down}")
        screens = run(exe, tmp, "SCREENS", SCREENS)
        if [f[1] for f in screens] != ["0", "1", "2", "3", "4", "5"]:
            bad.append(f"screens: {screens}")
    print("highvale " + ("ok" if not bad else "FAIL\n" + "\n".join(bad[:12])))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
