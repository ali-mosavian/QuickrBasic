"""The Mac's window (plat/sdl), drawn off-screen: python3 tools/mac_test.py QUICKRBASIC.

The program is opened, the frame QB_SHOT writes is read back: the editor's screen at the VGA's
720x400, its menu bar grey, the program's window blue, its text on it.
"""

import os
import sys
import tempfile
import subprocess
from pathlib import Path

GREY, BLUE = (0xAA, 0xAA, 0xAA), (0x00, 0x00, 0xAA)


def frame(binary: Path, program: Path) -> tuple[int, int, bytes]:
    with tempfile.TemporaryDirectory() as tmp:
        shot = Path(tmp) / "shot.ppm"
        # by its name: the editor's file field takes a DOS-length path, not a temporary one
        env = os.environ | {"QB_SHOT": str(shot), "SDL_VIDEO_DRIVER": "offscreen", "SDL_AUDIO_DRIVER": "dummy"}
        subprocess.run([binary, program.name], env=env, timeout=30, check=True, cwd=program.parent)
        data = shot.read_bytes()
    head, pixels = data.split(b"\n", 1)
    _, w, h, _ = head.split()
    return int(w), int(h), pixels


def pixel(w: int, pixels: bytes, x: int, y: int) -> tuple[int, ...]:
    at = (y * w + x) * 3
    return tuple(pixels[at : at + 3])


def main() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        program = Path(tmp) / "HELLO.BAS"
        program.write_text('PRINT "HELLO"\n')
        w, h, pixels = frame(Path(sys.argv[1]).resolve(), program)
    checks = {
        "720x400": (w, h) == (720, 400),
        "menu bar grey": pixel(w, pixels, 700, 8) == GREY,
        "editor blue": pixel(w, pixels, 400, 200) == BLUE,
        "text on it": len({pixel(w, pixels, x, y) for x in range(9, 9 * 14) for y in range(32, 48)}) > 1,
    }
    failed = [name for name, ok in checks.items() if not ok]
    print("mac " + ("ok" if not failed else "FAIL " + ", ".join(failed)))
    sys.exit(1 if failed else 0)


main()
