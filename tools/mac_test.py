"""The Mac's window (plat/sdl), drawn off-screen: python3 tools/mac_test.py QUICKRBASIC [ARCH].

The program is opened, the frame QB_SHOT writes is read back: the editor's screen at the VGA's
720x400, its menu bar grey, the program's window blue, its text on it. With Alt+F typed (QB_KEYS),
the File menu's help line is on the status bar: BASIC.HLP was found, in the app's Resources. ARCH:
the universal binary's half run (arch -ARCH), the machine's own if none. A graphics loop run (F5),
broken and restarted (Shift+F5) draws again: the kept screen once left the display on the text.
QB_SHOT's timed shots, one run: the status line before Alt+F, then its help line. QB_MOUSE's press, read by
PEN, draws a box where it was.
A box drawn on page 1 is shown only once page 1 is, in SCREEN 13 and in text mode.
F2 lists every procedure: 64 views at most once left SUB 70 out, End, Enter showing SUB 63 with the
lines of the seven after it. A procedure's name is any length: 40 characters at most once cut
the window's title. F2's list is by name, in columns (Right a column on): once one column in the
file's order, Down chose Bbbbbb, Right did nothing. Its box's right side was the dialog's, and a list
longer than its box had no scroll bar: now the SUBs list's arrow moves a column. The Open box's files
are by name in three columns, their bar sideways: once one column in the directory's order.
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
SUBS = "PRINT 1\n" + "".join(f"SUB S{i}\nEND SUB\n" for i in range(1, 71))
LAST_SUB = "60:3C00 70:4F00 80:1C0D"  # F2, End, Enter
LIST = "60:3C00"  # F2
FIRST_SUB = "60:3C00 70:5000 80:1C0D"  # F2, Down, Enter
ACROSS = "60:3C00 70:4D00 80:1C0D"  # F2, Right, Enter: a column on, S2 by name
MANY = "PRINT 1\n" + "".join(f"SUB S{i}\nEND SUB\n" for i in range(1, 121))
BAR_RIGHT = "2s:670,280 2.2s:670,280:1 2.4s:670,280"  # the SUBs list's bar's right arrow, cell 74 row 17
OPEN = "60:2100 70:1800"  # Alt+F, O
OPEN_FIRST = "60:2100 70:1800 80:0F09 90:5000 100:4800 110:1C0D"  # Open, Tab, Down, Up, Enter: the first file
SORTED = "PRINT 1\nSUB Bbbbbb\nEND SUB\nSUB a\nEND SUB\n"
LONG_NAME = "N" * 60
LONG = f"PRINT 1\nSUB {LONG_NAME}\nEND SUB\n"


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


def text_rows(w: int, pixels: bytes) -> int:
    """The edit window's rows with text on them, its 17 above the scroll bar: 16 pixels a row from y 32."""
    return sum(
        any(pixel(w, pixels, x, y) != BLUE for x in range(9, 9 * 40) for y in range(32 + r * 16, 48 + r * 16))
        for r in range(17)
    )


def title_cells(w: int, pixels: bytes) -> int:
    """The edit window's title bar's lit cells: the title, a space either side, the arrow."""
    return sum(pixel(w, pixels, c * 9 + 1, 17) != BLUE for c in range(80))


def list_column(w: int, pixels: bytes, column: int) -> bool:
    """F2's second row (row 7, past the highlight), the list's 10-cell column column from cell 6, has text."""
    return any(
        pixel(w, pixels, x, y) != GREY
        for x in range(9 * (6 + 10 * column), 9 * (14 + 10 * column))
        for y in range(112, 128)
    )


def clear_column(w: int, pixels: bytes, col: int) -> bool:
    """Cell column col, rows 5 to 16, is the dialog's grey alone."""
    return all(pixel(w, pixels, x, y) == GREY for x in range(9 * col, 9 * col + 9) for y in range(80, 272))


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
        subs = Path(tmp) / "SUBS.BAS"
        subs.write_text(SUBS)
        sw, _, last_sub = frames(run, subs, LAST_SUB, ("2s",))[0]
        _, _, listed = frames(run, subs, LIST, ("2s",))[0]
        _, _, across = frames(run, subs, ACROSS, ("2s",))[0]
        many = Path(tmp) / "MANY.BAS"
        many.write_text(MANY)
        _, _, bar_right = frames(run, many, "60:3C00 3s:1C0D", ("4s",), BAR_RIGHT)[0]
        files = Path(tmp) / "files"
        files.mkdir()
        for i in range(30):
            (files / f"F{i}.BAS").write_text("PRINT 1\n")
        (files / "A.BAS").write_text("PRINT 1\n")
        _, _, opened = frames(run, files / "F0.BAS", OPEN, ("2s",))[0]
        _, _, first_file = frames(run, files / "F0.BAS", OPEN_FIRST, ("3s",))[0]
        sorted_bas = Path(tmp) / "SORTED.BAS"
        sorted_bas.write_text(SORTED)
        _, _, first_sub = frames(run, sorted_bas, FIRST_SUB, ("2s",))[0]
        long = Path(tmp) / "LONG.BAS"
        long.write_text(LONG)
        lw, _, long_title = frames(run, long, "60:5500", ("2s",))[0]  # Shift+F2
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
        "F2 lists SUB 70": text_rows(sw, last_sub) == 2,
        "F2's list in columns": list_column(sw, listed, 1) and list_column(sw, listed, 5),
        "F2's Right a column on": title_cells(sw, across) == len("SUBS.BAS:S2") + 3,
        "F2's list by name": title_cells(sw, first_sub) == len("SORTED.BAS:a") + 3,
        "F2's list clear of the box's side": clear_column(sw, listed, 76),
        "F2's bar's arrow a column on": title_cells(sw, bar_right) == len("MANY.BAS:S109") + 3,
        "Open's files' thumb sideways": pixel(sw, opened, 10 * 9 + 1, 274)
        == pixel(sw, opened, 10 * 9 + 1, 286)
        == (0, 0, 0),
        "Open's files in columns": any(
            pixel(sw, opened, x, y) != GREY for x in range(23 * 9, 31 * 9) for y in range(160, 176)
        ),
        "Open's files by name": title_cells(sw, first_file) == len("A.BAS") + 3,
        "a long name's title whole": title_cells(lw, long_title) == len(f"LONG.BAS:{LONG_NAME}") + 3,
    }
    failed = [name for name, ok in checks.items() if not ok]
    print(f"mac {sys.argv[2] if len(sys.argv) > 2 else ''} " + ("ok" if not failed else "FAIL " + ", ".join(failed)))
    sys.exit(1 if failed else 0)


main()
