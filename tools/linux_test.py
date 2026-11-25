"""Linux's window (plat/sdl), drawn off-screen: python3 tools/linux_test.py QUICKRBASIC.

An 8-bit scene played on the GPU is software's pixel for pixel. The offscreen driver's GPU draws into
a texture: it once asked for one presenting to the window, which NVIDIA's refuses, and QB_GFX=gpu
exited.
"""

import sys
import tempfile
from pathlib import Path

from mac_test import GFX_SCENE, frame


def main() -> None:
    run = [str(Path(sys.argv[1]).resolve())]
    with tempfile.TemporaryDirectory() as tmp:
        scene = Path(tmp) / "SCENE.BAS"
        scene.write_text(GFX_SCENE.format(gpu=0))
        soft = frame(run, scene, "3F00", {"QB_GFX": "soft"})
        scene.write_text(GFX_SCENE.format(gpu=1))
        gpu = frame(run, scene, "3F00", {"QB_GFX": "gpu"})
    ok = gpu == soft and soft[:2] == (160, 120)
    print("linux " + ("ok" if ok else "FAIL gfx's scene played on the GPU is software's"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
