"""The Mac's window (plat/sdl), drawn off-screen: python3 tools/mac_test.py QUICKRBASIC [ARCH].

The program is opened, the frame QB_SHOT writes is read back: the editor's screen at the VGA's
720x400, its menu bar grey, the program's window blue, its text on it. With Alt+F typed (QB_KEYS),
the File menu's help line is on the status bar: BASIC.HLP was found, in the app's Resources. ARCH:
the universal binary's half run (arch -ARCH), the machine's own if none. A graphics loop run (F5),
broken and restarted (Shift+F5) draws again: the kept screen once left the display on the text.
QB_SHOT's timed shots, one run: the status line before Alt+F, then its help line. QB_MOUSE's press, read by
PEN, draws a box where it was.
A box drawn on page 1 is shown only once page 1 is, in SCREEN 13 and in text mode.
"""

import os
import sys
import tempfile
import subprocess
from pathlib import Path

GREY, BLUE, RED = (0xAA, 0xAA, 0xAA), (0x00, 0x00, 0xAA), (0xAA, 0x00, 0x00)
BOX = "SCREEN 9\nDO\nLINE (0, 0)-(100, 100), 4, BF\nLOOP\n"
RESTART = "3F00 120:FFFF 180:5800"  # F5, Ctrl+Break, Shift+F5
PEN = "SCREEN 9\nPEN ON\nDO\nIF PEN(0) THEN x = PEN(1): y = PEN(2): LINE (x - 5, y - 5)-(x + 5, y + 5), 4, BF\nLOOP\n"
CLICK = "2s:300,200 2.2s:300,200:1 2.4s:300,200"  # after F5 at 1s
HIDDEN = {
    "graphics": "SCREEN 13, , 1, 0\nLINE (0, 0)-(100, 100), 4, BF\n",
    "text": "SCREEN 0, , 1, 0\nCOLOR 4, 4\nCLS\n",
}
LOOP = "DO\nLOOP\n"


def frames(
    run: list[str], program: Path, keys: str = "", at: tuple[str, ...] = ("",), mouse: str = ""
) -> list[tuple[int, int, bytes]]:
    """A shot at each of at's times ("" the default), one run."""
    with tempfile.TemporaryDirectory() as tmp:
        shots = [Path(tmp) / f"shot{i}.ppm" for i in range(len(at))]
        # by its name: the editor's file field takes a DOS-length path, not a temporary one
        env = os.environ | {
            "QB_SHOT": ",".join(f"{shot}@{t}" if t else str(shot) for shot, t in zip(shots, at)),
            "QB_KEYS": keys,
            "QB_MOUSE": mouse,
            "SDL_VIDEO_DRIVER": "offscreen",
            "SDL_AUDIO_DRIVER": "dummy",
        }
        subprocess.run([*run, program.name], env=env, timeout=30, check=True, cwd=program.parent)
        data = [shot.read_bytes() for shot in shots]
    out = []
    for d in data:
        head, pixels = d.split(b"\n", 1)
        _, w, h, _ = head.split()
        out.append((int(w), int(h), pixels))
    return out


def frame(run: list[str], program: Path, keys: str = "") -> tuple[int, int, bytes]:
    return frames(run, program, keys)[0]


def pixel(w: int, pixels: bytes, x: int, y: int) -> tuple[int, ...]:
    at = (y * w + x) * 3
    return tuple(pixels[at : at + 3])


def main() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        program = Path(tmp) / "HELLO.BAS"
        program.write_text('PRINT "HELLO"\n')
        binary = str(Path(sys.argv[1]).resolve())
        run = ["arch", f"-{sys.argv[2]}", binary] if len(sys.argv) > 2 else [binary]
        w, h, pixels = frame(run, program)
        _, _, menu = frame(run, program, "2100")
        box = Path(tmp) / "BOX.BAS"
        box.write_text(BOX)
        bw, _, again = frame(run, box, RESTART)
        (_, _, before), (_, _, after) = frames(run, program, "90:2100", ("1s", "2.5s"))
        pen = Path(tmp) / "PEN.BAS"
        pen.write_text(PEN)
        pw, _, clicked = frames(run, pen, "60:3F00", ("3s",), CLICK)[0]
        pages = {}
        for name, draw in HIDDEN.items():
            for shown, tail in (("hidden", ""), ("shown", "SCREEN , , , 1\n")):
                page = Path(tmp) / "PAGE.BAS"
                page.write_text(draw + tail + LOOP)
                gw, _, px = frame(run, page, "3F00")
                pages[name, shown] = pixel(gw, px, 50, 50)
    status = {pixel(w, menu, x, 392) for x in range(9 * 30, 9 * 60)}

    def status_row(shot: bytes) -> list[tuple[int, ...]]:
        return [pixel(w, shot, x, 392) for x in range(9 * 30, 9 * 60)]

    checks = {
        "720x400": (w, h) == (720, 400),
        "menu bar grey": pixel(w, pixels, 700, 8) == GREY,
        "editor blue": pixel(w, pixels, 400, 200) == BLUE,
        "text on it": len({pixel(w, pixels, x, y) for x in range(9, 9 * 14) for y in range(32, 48)}) > 1,
        "File's help line": len(status) > 1,
        "restarted graphics drawn": pixel(bw, again, 50, 50) == RED,
        "timed shots, Alt+F between": status_row(before) != status_row(after) == status_row(menu),
        "click drawn": pixel(pw, clicked, 300, 200) == RED and pixel(pw, clicked, 320, 200) != RED,
        "graphics page 1 hidden": pages["graphics", "hidden"] != RED,
        "graphics page 1 shown": pages["graphics", "shown"] == RED,
        "text page 1 hidden": pages["text", "hidden"] != RED,
        "text page 1 shown": pages["text", "shown"] == RED,
    }
    failed = [name for name, ok in checks.items() if not ok]
    print(f"mac {sys.argv[2] if len(sys.argv) > 2 else ''} " + ("ok" if not failed else "FAIL " + ", ".join(failed)))
    sys.exit(1 if failed else 0)


main()
