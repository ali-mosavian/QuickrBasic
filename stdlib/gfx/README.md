# gfx

2D graphics for QuickrBasic programs, with 3D to come. It is a native extension, `gfx.qbx`. It draws
in software on every platform, and on the GPU where there is one.

```basic
import gfx.display, gfx.surface, gfx.draw

canvas& = gfx.display.open(320, 200, 8, gfx.display.VSYNC)
hero& = gfx.surface.load("HERO.PNG")
do
    gfx.draw.clear canvas&, 1
    gfx.draw.sprite canvas&, 100, 80, hero&, 0
    gfx.draw.text canvas&, 4, 4, "HELLO", 15
    gfx.display.flip
loop while inkey$ = ""
gfx.display.close
```

## Modules

| Module | For |
|---|---|
| `gfx.display` | the screen, the palette and the host: monitors, events, the clock |
| `gfx.surface` | images, views and the screen's surface; each one's clip, camera, anchor and depth; pixels |
| `gfx.draw` | 2D drawing into any surface, and the settings it draws with |
| `gfx.scene` | 3D: declared, not built |

The interfaces are in [src/api](src/api). Every procedure there has a docstring.

## Layout

| Path | Holds |
|---|---|
| [docs](docs) | [architecture](docs/architecture.md), [design rules](docs/design.md), [history](docs/history.md) |
| [src](src) | the extension's C and its BASIC interfaces |
| [tests](tests) | the unit tests |
| [demos](demos) | `GFXDEMO.BAS`, `CUBE.BAS`, `ORBITS.BAS`, `PLASMA.BAS`, `TUNNEL.BAS`, `SPARKS.BAS`, `ISLES.BAS`, `MEADOW.BAS`, `TEMPLE.BAS` |

## Build and test

```bash
make build/host/lib/gfx.qbx
```

```bash
make build/host/unit_tests && ./build/host/unit_tests gfx_
```

```bash
sh tests/host/gfx.sh build/host/basic
```

```bash
python3 tools/gfx_test.py --sums
```

The last one prints a checksum of a test scene at each depth. A change that must not move a pixel
keeps them all.
