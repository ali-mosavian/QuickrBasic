"""blender -b -P tools/charlierender.py [-- NAME ...]: CHARLIE.BAS's rendered pictures. Each is a small scene built
here, a unit a pixel, lit from the upper left, rendered at eight times its size, averaged down and matched to the
game's palette with a dither; written to tools/charlie_px/NAME.txt, which tools/charlieart.py reads. The files are
kept, so drawing the game's art needs no Blender."""

import sys
import math
import tempfile
from pathlib import Path

import bpy
import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import charlieart as art  # noqa: E402

OUT = ROOT / "tools/charlie_px"
OVER = 8  # rendered pixels a picture's pixel, each way
Mat, Ob = bpy.types.Material, bpy.types.Object


def reset(w: int, h: int) -> None:
    """An empty scene looked at from the front, w by h units in view, a sun over the left shoulder."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.samples = 32
    sc.cycles.use_denoising = False
    sc.render.film_transparent = True
    sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = w * OVER, h * OVER, 100
    sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    back = world.node_tree.nodes["Background"]
    back.inputs[0].default_value, back.inputs[1].default_value = (0.75, 0.85, 1.0, 1.0), 0.55
    sc.world = world
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.type, cam.data.ortho_scale = "ORTHO", max(w, h)
    cam.location, cam.rotation_euler = (w / 2, -200, h / 2), (math.pi / 2, 0, 0)
    sc.collection.objects.link(cam)
    sc.camera = cam
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy, sun.data.angle = 3.2, 0.3
    sun.rotation_euler = (math.radians(58), 0, math.radians(-38))
    sc.collection.objects.link(sun)


def mat(rgb: tuple[int, int, int], metal: float = 0.0, rough: float = 0.5, glow: float = 0.0) -> Mat:
    """A material of a palette colour's hue (6 bits a part)."""
    m = bpy.data.materials.new("m")
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    c = (*((v / 63) ** 2.2 for v in rgb), 1.0)
    b.inputs["Base Color"].default_value = c
    b.inputs["Metallic"].default_value, b.inputs["Roughness"].default_value = metal, rough
    if glow:
        b.inputs["Emission Color"].default_value, b.inputs["Emission Strength"].default_value = c, glow
    return m


def done(m: Mat, smooth: bool = False) -> Ob:
    ob = bpy.context.object
    ob.data.materials.append(m)
    if smooth:
        bpy.ops.object.shade_smooth()
    return ob


def box(
    x0: float, z0: float, x1: float, z1: float, m: Mat, y: float = 0.0, deep: float = 4.0, bevel: float = 0.0
) -> Ob:
    """A slab facing the camera, its front at y."""
    bpy.ops.mesh.primitive_cube_add(size=1, location=((x0 + x1) / 2, y + deep / 2, (z0 + z1) / 2))
    ob = done(m)
    ob.scale = (x1 - x0, deep, z1 - z0)
    if bevel:
        bpy.ops.object.transform_apply(scale=True)
        mod = ob.modifiers.new("b", "BEVEL")
        mod.width, mod.segments = bevel, 3
    return ob


def rod(x0: float, z0: float, x1: float, z1: float, r: float, m: Mat, y: float = 0.0) -> Ob:
    """A round bar from one point to another, in the plane at y."""
    n = math.hypot(x1 - x0, z1 - z0)
    bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=r, depth=n, location=((x0 + x1) / 2, y, (z0 + z1) / 2))
    ob = done(m, True)
    ob.rotation_euler = (0, math.atan2(x1 - x0, z1 - z0), 0)
    return ob


def disc(x: float, z: float, r: float, m: Mat, y: float = 0.0, deep: float = 2.0) -> Ob:
    """A cylinder end on."""
    bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=r, depth=deep, location=(x, y, z))
    ob = done(m, True)
    ob.rotation_euler = (math.pi / 2, 0, 0)
    return ob


def ball(x: float, z: float, r: float, m: Mat, y: float = 0.0, squash: tuple[float, float, float] = (1, 1, 1)) -> Ob:
    bpy.ops.mesh.primitive_uv_sphere_add(segments=64, ring_count=32, radius=r, location=(x, y, z))
    ob = done(m, True)
    ob.scale = squash
    return ob


def take(w: int, h: int, ramps: list[list[int]], spread: float = 5.0) -> list[list[int]]:
    """The scene rendered and brought down to w by h palette indices of ramps, 0 where it is clear."""
    with tempfile.TemporaryDirectory() as tmp:
        bpy.context.scene.render.filepath = str(Path(tmp) / "r.png")
        bpy.ops.render.render(write_still=True)
        img = bpy.data.images.load(str(Path(tmp) / "r.png"))
        px = np.array(img.pixels[:], dtype=np.float32).reshape(h * OVER, w * OVER, 4)[::-1]
    px = px.reshape(h, OVER, w, OVER, 4)
    alpha = px[..., 3].mean(axis=(1, 3))
    rgb = (px[..., :3] * px[..., 3:]).mean(axis=(1, 3)) / np.maximum(alpha, 1e-6)[..., None] * 63
    allowed = sorted({c for r in ramps for c in r})
    table = np.array([art.PAL[c] for c in allowed], dtype=np.float32)
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            if alpha[y, x] < 0.5:
                row.append(0)
                continue
            c = rgb[y, x] + ((art.BAYER[y & 3][x & 3] + 0.5) / 16 - 0.5) * spread
            row.append(allowed[int(((table - c) ** 2 * (2, 4, 3)).sum(axis=1).argmin())])
        rows.append(row)
    return rows


def save(name: str, rows: list[list[int]]) -> None:
    OUT.mkdir(exist_ok=True)
    (OUT / f"{name}.txt").write_text("".join("".join(f"{v:02x}" for v in r) + "\n" for r in rows))
    print("charlierender:", name, len(rows[0]), "by", len(rows))


def chrome() -> Mat:
    return mat((54, 58, 62), 1.0, 0.32)


def tank() -> None:
    """A storage tank on legs: a drum under a dome, a warning band, a gauge, a lamp."""
    reset(32, 40)
    steel, dark = chrome(), mat((12, 14, 20), 0.6, 0.5)
    rod(15.5, 7, 15.5, 30, 12.5, steel)
    ball(15.5, 30, 12.5, steel, squash=(1, 1, 0.42))
    for z in (12, 21):
        rod(15.5, z, 15.5, z + 1.2, 12.9, dark)
    rod(15.5, 16, 15.5, 18.4, 12.8, mat((63, 50, 10), 0.0, 0.5))
    for x in (5.5, 25.5):
        box(x - 1.2, 0, x + 1.2, 8, dark, y=-2)
    disc(10, 25, 3.2, mat((60, 60, 58), 0, 0.4), y=-12.2)
    disc(10, 25, 0.9, dark, y=-13.4)
    ball(15.5, 37.2, 1.9, mat((63, 12, 8), 0, 0.3, 2.0))
    save("tank", take(32, 40, [art.CHROME, art.GOLD, art.DARK, art.SCARF, art.STEEL, [art.INK, art.WHITE]]))


def sphere() -> None:
    """A ball tank on a ring of legs, a walkway round it."""
    reset(44, 46)
    steel, dark = chrome(), mat((12, 14, 20), 0.6, 0.5)
    ball(21.5, 27, 18.5, steel)
    bpy.ops.mesh.primitive_torus_add(major_radius=19.2, minor_radius=1.3, location=(21.5, 0, 24))
    done(mat((63, 50, 10), 0, 0.5), True)
    for x in (8, 15, 28, 35):
        rod(x, 0, x, 16, 1.1, dark, y=-6 if x in (15, 28) else 4)
    rod(8, 2, 35, 12, 0.7, dark, y=-6)
    save("sphere", take(44, 46, [art.CHROME, art.GOLD, art.DARK, art.STEEL, [art.INK, art.WHITE]]))


def bumper(lit: bool) -> None:
    """A bumper: a red ring round a gold dome; lit, it glows."""
    reset(24, 24)
    turn = (math.pi / 2, 0, 0)
    bpy.ops.mesh.primitive_torus_add(major_radius=8.2, minor_radius=3.2, location=(12, 0, 12), rotation=turn)
    done(mat((63, 56, 20) if lit else (60, 10, 10), 0.1, 0.25, 1.5 if lit else 0), True)
    dome = mat((63, 63, 60) if lit else (63, 52, 12), 0.3, 0.25, 2.0 if lit else 0)
    ball(12, 12, 6.2, dome, y=-1, squash=(1, 0.6, 1))
    ramps = [art.SCARF, art.GOLD, art.CLOUD, art.DARK, [art.INK, art.WHITE]]
    save("bumper1" if lit else "bumper0", take(24, 24, ramps))


def mace() -> None:
    """A spiked ball."""
    reset(16, 16)
    iron = mat((40, 42, 48), 1.0, 0.35)
    ball(8, 8, 4.8, iron)
    for i in range(8):
        a = i * math.pi / 4
        bpy.ops.mesh.primitive_cone_add(vertices=16, radius1=1.7, depth=4.2,
                                        location=(8 + 5.6 * math.cos(a), 0, 8 + 5.6 * math.sin(a)))
        done(iron, True).rotation_euler = (0, math.pi / 2 - a, 0)
    save("mace", take(16, 16, [art.METAL, art.STONE, art.DARK, [art.INK, art.WHITE]]))


def mural() -> None:
    """The works' wall, 64 by 128 and meeting itself: bevelled panels 32 square, a vent, a fat pipe on brackets, a
    fan's housing, a light's fitting, a girder's cross in a recess, ribs. The game's art tool lays the lights, the
    fan's blades and the pipe's glints over it, as they move."""
    reset(64, 128)
    panel, dark, steel = mat((14, 24, 40), 0.5, 0.45), mat((5, 8, 16), 0.3, 0.6), chrome()
    box(-8, -8, 72, 136, dark, y=3)
    for j in range(4):
        for i in range(2):
            x, z, n = i * 32, 96 - j * 32, j * 2 + i
            if n == 6:
                box(x + 1, z + 1, x + 31, z + 31, panel, y=2.2, bevel=0.8)
                for k in (-1, 1):
                    rod(x + 5, z + 16 - 11 * k, x + 27, z + 16 + 11 * k, 1.3, steel, y=1.4)
                continue
            box(x + 0.7, z + 0.7, x + 31.3, z + 31.3, panel, bevel=1.4)
            if n not in (2, 3):
                for cx, cz in ((4.5, 4.5), (27.5, 4.5), (4.5, 27.5), (27.5, 27.5)):
                    ball(x + cx, z + cz, 1.1, steel, y=-0.3)
            if n == 0:
                box(x + 8, z + 8, x + 24, z + 23, dark, y=-0.4, deep=1)
                for k in range(5):
                    slat = box(x + 8.5, z + 9 + k * 3, x + 23.5, z + 10.2 + k * 3, steel, y=-1.2, deep=1.4)
                    slat.rotation_euler = (0.5, 0, 0)
            if n == 1:
                box(x + 16, z + 7, x + 26, z + 13, mat((63, 50, 10), 0, 0.5), y=-0.5, deep=0.6)
            if n == 4:
                bpy.ops.mesh.primitive_torus_add(major_radius=10.4, minor_radius=1.5, location=(x + 16, -0.6, z + 16),
                                                 rotation=(math.pi / 2, 0, 0))
                done(steel, True)
                disc(x + 16, z + 16, 9.4, dark, y=0.4, deep=1)
            if n == 5:
                box(x + 5, z + 12.5, x + 27, z + 19.5, steel, y=-0.8, deep=1, bevel=0.6)
            if n == 7:
                for k in range(5):
                    box(x + 6.5 + k * 4, z + 6, x + 8.5 + k * 4, z + 26, panel, y=-1.4, deep=1.6, bevel=0.5)
    rod(-8, 82, 72, 82, 7.6, mat((60, 34, 12), 1.0, 0.3), y=-9)
    for x in (2.5, 66.5):
        rod(x - 2.5, 82, x + 2.5, 82, 9.4, steel, y=-9)
    rows = take(64, 128, [art.PANEL, art.CHROME, art.COPPER, art.GOLD, art.DARK, [art.INK]], spread=4.0)
    save("mural", rows)


JOBS = {"tank": tank, "sphere": sphere, "bumper0": lambda: bumper(False), "bumper1": lambda: bumper(True),
        "mace": mace, "mural": mural}

if __name__ == "__main__":
    names = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else list(JOBS)
    for name in names:
        JOBS[name]()
