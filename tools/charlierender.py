"""blender -b -P tools/charlierender.py: CHARLIE.BAS's rendered pictures. Each is a small scene built here, a unit
a pixel, lit from the upper left and shaded in flat tones as a cel is, rendered at several times its size and
averaged down, so its edges are soft. A zone's pictures then share a palette made from their own colours, with no
dither: tools/charlie_px/zoneN.pal, and each picture's indices in NAME.txt, which tools/charlieart.py reads. The
files are kept, so drawing the game's art needs no Blender."""

import sys
import math
import random
import tempfile
from pathlib import Path

import bpy
import bmesh
import numpy as np
from mathutils import Matrix

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import charlieart as art  # noqa: E402

OUT = ROOT / "tools/charlie_px"
OVER = 8  # rendered pixels a picture's pixel, each way, unless said
Mat, Ob = bpy.types.Material, bpy.types.Object


def reset(w: int, h: int, over: int = OVER) -> None:
    """An empty scene looked at from the front, w by h units in view, a sun over the left shoulder; over rendered
    pixels a picture's, each way."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.film_transparent = True
    sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = w * over, h * over, 100
    sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    back = world.node_tree.nodes["Background"]
    back.inputs[0].default_value, back.inputs[1].default_value = (0.8, 0.8, 1.0, 1.0), 0.25
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


def lin(rgb: tuple[float, float, float], k: float = 1.0) -> tuple[float, float, float, float]:
    """A colour of 6 bits a part, k times as bright, as the renderer wants it."""
    return (*((min(v * k, 63) / 63) ** 2.2 for v in rgb), 1.0)


def mat(rgb: tuple[int, int, int], shine: float = 0.0, glow: bool = False) -> Mat:
    """A cel's material: rgb where the light falls, two flat tones darker away from it, a flat glint where shine
    says; glow: its own light, unshaded."""
    m = bpy.data.materials.new("m")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out, emit = nt.nodes.new("ShaderNodeOutputMaterial"), nt.nodes.new("ShaderNodeEmission")
    nt.links.new(emit.outputs[0], out.inputs[0])
    if glow:
        emit.inputs[0].default_value = lin(rgb)
        return m
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.interpolation = "CONSTANT"
    for i, (at, k) in enumerate(((0.0, 0.42), (0.22, 0.7), (0.62, 1.0))):
        e = ramp.color_ramp.elements[i] if i < 2 else ramp.color_ramp.elements.new(at)
        e.position, e.color = at, lin(rgb, k)
    lit = nt.nodes.new("ShaderNodeShaderToRGB")
    nt.links.new(nt.nodes.new("ShaderNodeBsdfDiffuse").outputs[0], lit.inputs[0])
    nt.links.new(lit.outputs[0], ramp.inputs[0])
    if not shine:
        nt.links.new(ramp.outputs[0], emit.inputs[0])
        return m
    gloss, seen = nt.nodes.new("ShaderNodeBsdfGlossy"), nt.nodes.new("ShaderNodeShaderToRGB")
    band = nt.nodes.new("ShaderNodeValToRGB")
    gloss.inputs["Roughness"].default_value = 0.3
    band.color_ramp.interpolation = "CONSTANT"
    band.color_ramp.elements[0].color = (0, 0, 0, 1)
    band.color_ramp.elements[1].position, band.color_ramp.elements[1].color = 0.5, (shine, shine, shine, 1)
    add = nt.nodes.new("ShaderNodeVectorMath")
    nt.links.new(gloss.outputs[0], seen.inputs[0])
    nt.links.new(seen.outputs[0], band.inputs[0])
    nt.links.new(ramp.outputs[0], add.inputs[0])
    nt.links.new(band.outputs[0], add.inputs[1])
    nt.links.new(add.outputs[0], emit.inputs[0])
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


def shoot(w: int, h: int) -> tuple[np.ndarray, np.ndarray]:
    """The scene rendered and averaged down to w by h: its colours, 6 bits a part, and how solid each pixel is."""
    over = bpy.context.scene.render.resolution_x // w
    with tempfile.TemporaryDirectory() as tmp:
        bpy.context.scene.render.filepath = str(Path(tmp) / "r.png")
        bpy.ops.render.render(write_still=True)
        img = bpy.data.images.load(str(Path(tmp) / "r.png"))
        px = np.array(img.pixels[:], dtype=np.float32).reshape(h * over, w * over, 4)[::-1]
    px = px.reshape(h, over, w, over, 4)
    alpha = px[..., 3].mean(axis=(1, 3))
    return (px[..., :3] * px[..., 3:]).mean(axis=(1, 3)) / np.maximum(alpha, 1e-6)[..., None] * 63, alpha


def palette(px: np.ndarray, k: int) -> np.ndarray:
    """k colours, 6 bits a part, that px's colours are nearest to: their means, settled over a few rounds."""
    rnd = np.random.default_rng(7)
    seen = np.unique(np.round(px * 2) / 2, axis=0)
    means = seen[rnd.choice(len(seen), k, replace=False)].astype(np.float32)
    for _ in range(24):
        near = nearest(px, means)
        for i in range(k):
            if (near == i).any():
                means[i] = px[near == i].mean(axis=0)
            else:
                means[i] = seen[rnd.integers(len(seen))]
    return np.unique(np.round(means), axis=0)


def nearest(px: np.ndarray, table: np.ndarray) -> np.ndarray:
    """Each of px's colours' nearest in table, by eye: green counts most."""
    out = np.empty(len(px), dtype=np.int64)
    for a in range(0, len(px), 20000):
        d = ((px[a : a + 20000, None, :] - table[None]) ** 2 * (2, 4, 3)).sum(axis=2)
        out[a : a + 20000] = d.argmin(axis=1)
    return out


def save(name: str, rows: list[list[int]]) -> None:
    OUT.mkdir(exist_ok=True)
    (OUT / f"{name}.txt").write_text("".join("".join(f"{v:02x}" for v in r) + "\n" for r in rows))
    print("charlierender:", name, len(rows[0]), "by", len(rows))


def mural() -> tuple[np.ndarray, np.ndarray]:
    """The works' wall, 64 by 128 and meeting itself: bevelled panels 32 square, a vent, a fat pipe on brackets, a
    fan's housing, a light's fitting, a girder's cross in a recess, ribs. The game's art tool lays the lights, the
    fan's blades and the pipe's glints over it, as they move."""
    reset(64, 128)
    panel, dark, steel = mat((17, 29, 47)), mat((6, 9, 19)), mat((54, 58, 62), 0.25)
    box(-8, -8, 72, 136, dark, y=3)
    for j in range(4):
        for i in range(2):
            x, z, n = i * 32, 96 - j * 32, j * 2 + i
            if n == 6:
                box(x + 1, z + 1, x + 31, z + 31, panel, y=2.2, bevel=0.8)
                for k in (-1, 1):
                    rod(x + 5, z + 16 - 11 * k, x + 27, z + 16 + 11 * k, 1.3, steel, y=1.4)
                continue
            box(x + 0.7, z + 0.7, x + 31.3, z + 31.3, panel, bevel=1.6)
            if n not in (2, 3):
                for cx, cz in ((4.5, 4.5), (27.5, 4.5), (4.5, 27.5), (27.5, 27.5)):
                    ball(x + cx, z + cz, 1.1, steel, y=-0.3)
            if n == 0:
                box(x + 8, z + 8, x + 24, z + 23, dark, y=-0.4, deep=1)
                for k in range(5):
                    slat = box(x + 8.5, z + 9 + k * 3, x + 23.5, z + 10.2 + k * 3, steel, y=-1.2, deep=1.4)
                    slat.rotation_euler = (0.5, 0, 0)
            if n == 1:
                box(x + 16, z + 7, x + 26, z + 13, mat((63, 50, 10)), y=-0.5, deep=0.6)
            if n == 4:
                turn = (math.pi / 2, 0, 0)
                at = (x + 16, -0.6, z + 16)
                bpy.ops.mesh.primitive_torus_add(major_radius=10.4, minor_radius=1.5, location=at, rotation=turn)
                done(steel, True)
                disc(x + 16, z + 16, 9.4, dark, y=0.4, deep=1)
            if n == 5:
                box(x + 5, z + 12.5, x + 27, z + 19.5, steel, y=-0.8, deep=1, bevel=0.6)
            if n == 7:
                for k in range(5):
                    box(x + 6.5 + k * 4, z + 6, x + 8.5 + k * 4, z + 26, panel, y=-1.4, deep=1.6, bevel=0.5)
    rod(-8, 82, 72, 82, 7.6, mat((60, 34, 12), 0.3), y=-9)
    for x in (2.5, 66.5):
        rod(x - 2.5, 82, x + 2.5, 82, 9.4, steel, y=-9)
    return shoot(64, 128)


def windows(towers: list[tuple[float, float, float, float, float]], turn: float, lit: bool, seed: int) -> None:
    """The towers' windows, lit ones or dark, as one mesh: rows of them on the two faces seen."""
    rnd = random.Random(seed)
    bm = bmesh.new()
    for x, y, w, d, h in towers:
        base = Matrix.Translation((x, y, 0)) @ Matrix.Rotation(turn, 4, "Z")
        for face in range(2):
            wide = w if face == 0 else d
            for i in range(int((wide - 3) // 5)):
                for j in range(int((h - 6) // 6)):
                    if (rnd.random() < 0.55) != lit:
                        continue
                    u, v = -wide / 2 + 3.5 + i * 5, 4 + j * 6
                    at = (u, -d / 2 - 0.15, v) if face == 0 else (-w / 2 - 0.15, u, v)
                    size = (3, 0.3, 3.4) if face == 0 else (0.3, 3, 3.4)
                    m = base @ Matrix.Translation(at) @ Matrix.Diagonal((*size, 1))
                    bmesh.ops.create_cube(bm, size=1, matrix=m)
    mesh = bpy.data.meshes.new("win")
    bm.to_mesh(mesh)
    bm.free()
    ob = bpy.data.objects.new("win", mesh)
    mesh.materials.append(mat((63, 56, 30), glow=True) if lit else mat((14, 8, 20), glow=True))
    bpy.context.scene.collection.objects.link(ob)


def skyline(spots: list[tuple[float, float, float, float]], y: float, m: Mat, seed: int) -> None:
    """A row of towers at depth y, turned to show two faces, each also a screen to its left and right so the
    picture meets itself; a block and a mast on some roofs."""
    turn = 0.55
    towers = [(x + k, y, w, d, h) for x, w, d, h in spots for k in (-320, 0, 320)]
    for i, (x, _, w, d, h) in enumerate(towers):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, h / 2), rotation=(0, 0, turn))
        done(m).scale = (w, d, h)
        if i // 3 % 2 == 0:
            bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, h + 3), rotation=(0, 0, turn))
            done(m).scale = (w * 0.5, d * 0.5, 6)
        if i // 3 % 3 == 0:
            rod(x, h, x, h + 16, 0.5, m, y=y)
    windows(towers, turn, True, seed)
    windows(towers, turn, False, seed)


def city() -> tuple[np.ndarray, np.ndarray]:
    """The Chrome Works' sky and city, 320 by 200, meeting itself across: a dusk sky, a low sun's glow, streaks
    of cloud, far towers in its haze and near ones dark against it, their windows lit."""
    reset(320, 200, 4)
    skyline([(20, 34, 26, 150), (78, 26, 22, 122), (128, 40, 30, 104), (196, 30, 24, 158), (256, 28, 22, 128)], 80,
            mat((58, 40, 46)), 1)
    skyline([(48, 40, 30, 112), (112, 32, 26, 90), (164, 46, 34, 74), (232, 36, 28, 118), (296, 34, 26, 94)], 0,
            mat((40, 20, 36)), 2)
    rgb, alpha = shoot(320, 200)
    yy, xx = np.mgrid[0:200, 0:320].astype(np.float32)
    t = np.clip(yy / 150, 0, 1)[..., None]
    top, mid, low = np.array((34, 20, 44.0)), np.array((58, 34, 44.0)), np.array((63, 56, 36.0))
    sky = np.where(t < 0.55, top + (mid - top) * t / 0.55, mid + (low - mid) * (t - 0.55) / 0.45)
    dx = np.minimum(np.abs(xx - 236), 320 - np.abs(xx - 236))
    sky += np.exp(-(dx**2 / 9000 + (yy - 150) ** 2 / 3200))[..., None] * (np.array((63, 62, 50.0)) - sky) * 0.75
    rnd = random.Random(5)
    for _ in range(11):
        cx, cy, wide, thick = rnd.uniform(0, 320), rnd.uniform(14, 120), rnd.uniform(30, 70), rnd.uniform(1.2, 2.6)
        ex = np.minimum(np.abs(xx - cx), 320 - np.abs(xx - cx))
        streak = np.exp(-(ex**2 / wide**2 + (yy - cy) ** 2 / thick**2))[..., None]
        sky += streak * (np.array((63, 58, 52.0)) - sky) * 0.5
    rgb = np.where(alpha[..., None] < 0.5, sky, rgb * alpha[..., None] + sky * (1 - alpha[..., None]))
    rgb[152:] = (10, 14, 26)
    return rgb, np.ones_like(alpha)


def plant() -> tuple[np.ndarray, np.ndarray]:
    """The works behind the ground, 320 by 80 and meeting itself across: ball tanks, drums, banded chimneys and
    sheds over a rack of pipes."""
    reset(320, 80)
    steel, dark = mat((44, 54, 62), 0.2), mat((10, 14, 26))
    copper, red, white = mat((60, 34, 12), 0.3), mat((60, 12, 10)), mat((62, 60, 58))
    box(-8, 0, 328, 6, dark, y=-20, deep=40)
    for k in (-320, 0, 320):
        for x in (106, 290):
            ball(x + k, 44, 19, steel)
            bpy.ops.mesh.primitive_torus_add(major_radius=19.6, minor_radius=1.0, location=(x + k, 0, 42))
            done(mat((63, 50, 10)), True)
            for lx in (-12, -5, 5, 12):
                rod(x + k + lx, 6, x + k + lx, 30, 1.0, dark, y=-8 if abs(lx) == 5 else 6)
        for x, h in ((143, 76), (165, 70), (67, 73)):
            rod(x + k, 6, x + k, h, 4.6, steel, y=20)
            for z in range(3):
                rod(x + k, h - 5 - z * 12, x + k, h - z * 12, 4.9, red if z % 2 == 0 else white, y=20)
        for x in (32, 220):
            box(x + k - 24, 6, x + k + 24, 34, steel, y=10, deep=30, bevel=0.6)
            for i in range(6):
                box(x + k - 21 + i * 7.2, 12, x + k - 17 + i * 7.2, 28, dark, y=9.6, deep=1)
            rod(x + k - 26, 36, x + k + 26, 36, 2.6, steel, y=25)
        for x in (190, 250):
            rod(x + k, 6, x + k, 46, 9, steel, y=14)
            ball(x + k, 46, 9, steel, y=14, squash=(1, 1, 0.4))
    for z, r in ((30, 3.0), (21, 2.2)):
        rod(-8, z, 328, z, r, copper, y=-14)
    for x in range(-16, 336, 32):
        box(x - 1.2, 6, x + 1.2, 34, dark, y=-12, deep=3)
    return shoot(320, 80)


def zone(n: int, jobs: dict[str, tuple], weigh: dict[str, int]) -> None:
    """Zone n's pictures, rendered, in a palette of their own colours: as many as its pool has, but 16 kept back."""
    shot = {name: job() for name, job in jobs.items()}
    parts = [np.repeat(rgb[alpha >= 0.5], weigh.get(name, 1), axis=0) for name, (rgb, alpha) in shot.items()]
    px = np.concatenate(parts)
    pool = art.POOL[n]
    table = palette(px.astype(np.float32), len(pool) - 16)
    table = table[np.argsort(table @ (2, 4, 3))]
    rows = (f"{k} {int(r)} {int(g)} {int(b)}\n" for k, (r, g, b) in zip(pool, table, strict=False))
    (OUT / f"zone{n}.pal").write_text("".join(rows))
    print("charlierender: zone", n, len(table), "colours")
    for name, (rgb, alpha) in shot.items():
        idx = np.array(pool)[nearest(rgb.reshape(-1, 3), table.astype(np.float32))].reshape(alpha.shape)
        save(name, np.where(alpha < 0.5, 0, idx).tolist())


if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    zone(1, {"city": city, "plant": plant, "mural": mural}, {"mural": 4, "plant": 2})
