#!/usr/bin/env python3
"""demo.py [versus] [ide] [games] [themes]: the README's animated pictures, recorded in DOSBox.

DOSBox records the screen as it is drawn (the debug socket's video_start, 70 frames an emulated
second); each clip's caption, the key pressed and the parts to look at are put on its frames
after. docs/img/clips/*.gif, one a feature; docs/img/tour.gif, the clips in a grid, zoomed into
one by one; docs/img/versus.gif, QBasic 1.1 beside QuickrBasic; docs/img/themes.gif.

IDE_EXE picks the build (build/c/ide32.exe); DEMO_DOSBOX a dosbox-x whose socket has video_start
(tools/deps.sh capture). QB_SAMPLES the directory with GORILLA.BAS and NIBBLES.BAS, QBASIC QBasic
1.1's QBASIC.EXE (Microsoft's, so not in this repository; both default to build/qbasic): the clips
that need them are skipped without them.
"""

import os
import sys
import json
import zlib
import tempfile
import subprocess
from pathlib import Path
from dataclasses import field
from dataclasses import replace
from dataclasses import dataclass
from collections.abc import Iterator

from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont

sys.path.insert(0, str(Path(__file__).parent))
import dosplay  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
IMG = ROOT / "docs/img"
IDE = Path(os.environ.get("IDE_EXE", ROOT / "build/c/ide32.exe"))
SAMPLES = Path(os.environ.get("QB_SAMPLES", ROOT / "build/qbasic"))
QBASIC = Path(os.environ.get("QBASIC", ROOT / "build/qbasic/QBASIC.EXE"))
# the emulator with the debug socket's video_start (tools/deps.sh capture)
dosplay.DOSBOX = Path(os.environ.get("DEMO_DOSBOX", ROOT / "build/deps/bin/dosbox-x-capture"))

ALT_F, ALT_O, ALT_V = 0x21, 0x18, 0x2F
F2, F5, F6, F8, F9, SHIFT_F2, CTRL_F2, INS = 0x3C, 0x3F, 0x40, 0x42, 0x43, 0x55, 0x5F, 0x52
TAB, ENTER, ESC, BKSP, UP, DOWN, LEFT, RIGHT = 0x0F, 0x1C, 0x01, 0x0E, 0x48, 0x50, 0x4B, 0x4D
CTRL_END = 0x75
ASCII = {ESC: 27, ENTER: 13, TAB: 9, BKSP: 8}

SIZE = (720, 400)  # every frame scaled to the text modes' size
FPS = 20  # the GIFs' frames a second
BAR, BAR_BG, BAR_FG = 40, (24, 24, 27), (235, 235, 240)  # the caption's strip under the screen
KEY_BG, KEY_FG, MARK = (70, 70, 80), (255, 255, 255), (255, 196, 0)
FONT = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 20)
KEY_FONT = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 17)
COLS = 80

# The themes as Options > Theme lists them: their names and hot letters.
THEMES = [
    ("QBasic Colors", "q"),
    ("One Dark", "d"),
    ("One Light", "l"),
    ("Ayu Mirage", "a"),
    ("Ayu Light", "u"),
    ("Gruvbox Dark Soft", "g"),
    ("Gruvbox Light", "b"),
    ("JetBrains Dark", "j"),
    ("JetBrains Light", "t"),
]

# A US keyboard's scan code of each character typed.
ROWS = [
    ("1234567890-=", "!@#$%^&*()_+", 0x02),
    ("qwertyuiop[]", "QWERTYUIOP{}", 0x10),
    ("asdfghjkl;'`", 'ASDFGHJKL:"~', 0x1E),
    ("\\zxcvbnm,./", "|ZXCVBNM<>?", 0x2B),
]
SCAN = {" ": 0x39} | {
    c: base + i for low, high, base in ROWS for i, (a, b) in enumerate(zip(low, high, strict=True)) for c in (a, b)
}

DEMO = """DECLARE FUNCTION Fib& (n%)
DECLARE SUB Rays (cx%, cy%, n%)
' QuickrBasic: QBasic's IDE, rebuilt
SCREEN 13
FOR i% = 0 TO 99
    LINE (i%, i%)-(319 - i%, 199 - i%), 32 + i% MOD 72, B
NEXT
Rays 160, 100, 48
LOCATE 2, 2: PRINT "Fib(18) ="; Fib&(18)

FUNCTION Fib& (n%)
    IF n% < 2 THEN Fib& = n%: EXIT FUNCTION
    Fib& = Fib&(n% - 1) + Fib&(n% - 2)
END FUNCTION

SUB Rays (cx%, cy%, n%)
    FOR k% = 1 TO n%
        a! = k% * 6.2832 / n%
        LINE (cx%, cy%)-(cx% + 90 * COS(a!), cy% + 90 * SIN(a!)), 40 + k% MOD 16
    NEXT
END SUB
"""

Rect = tuple[int, int, int, int]  # top row, left column, rows, columns


@dataclass(frozen=True)
class Look:
    """What is drawn over the screen: the caption, the key pressed, the parts to look at, the mouse."""

    caption: str = ""
    key: str = ""
    marks: tuple[Rect, ...] = ()
    pointer: tuple[int, int] | None = None  # the mouse's row and column
    rows: int = 25


class Film:
    """Frames, each with its Look drawn on, and how long each shows."""

    def __init__(self) -> None:
        self.frames: list[tuple[Image.Image, int]] = []

    def add(self, screen: Image.Image, ms: int, look: Look = Look()) -> None:
        # one size for all: a graphics mode's smaller picture left the GIF's later frames stale
        screen = screen.resize(SIZE, Image.Resampling.NEAREST) if screen.size != SIZE else screen
        img = Image.new("RGB", (SIZE[0], SIZE[1] + BAR), BAR_BG)
        img.paste(screen)
        draw = ImageDraw.Draw(img)
        cw, ch = SIZE[0] / COLS, SIZE[1] / look.rows
        for top, left, rows, cols in look.marks:
            box = (left * cw - 2, top * ch - 2, (left + cols) * cw + 1, (top + rows) * ch + 1)
            draw.rounded_rectangle(box, radius=4, outline=MARK, width=3)
        if look.pointer:
            x, y = look.pointer[1] * cw + cw / 2, look.pointer[0] * ch + ch / 2
            arrow = [(0, 0), (0, 17), (4, 13), (7, 20), (10, 19), (7, 12), (12, 12)]
            draw.polygon([(x + a, y + b) for a, b in arrow], fill=(255, 255, 255), outline=(0, 0, 0))
        box = draw.textbbox((0, 0), look.caption, font=FONT)
        draw.text((16, SIZE[1] + (BAR - box[3]) // 2), look.caption, font=FONT, fill=BAR_FG)
        if look.key:
            box = draw.textbbox((0, 0), look.key, font=KEY_FONT)
            w, h = box[2] + 20, 26
            x, y = img.width - w - 14, SIZE[1] + (BAR - h) // 2
            draw.rounded_rectangle((x, y, x + w, y + h), radius=6, fill=KEY_BG)
            draw.text((x + 10, y + (h - box[3]) // 2 - 1), look.key, font=KEY_FONT, fill=KEY_FG)
        if self.frames and self.frames[-1][0].tobytes() == img.tobytes():
            prev, was = self.frames[-1]
            self.frames[-1] = (prev, was + ms)
        else:
            self.frames.append((img, ms))

    def save_to(self, path: Path | str) -> None:
        frames = [
            f.quantize(colors=64, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE) for f, _ in self.frames
        ]
        frames[0].save(
            path, save_all=True, append_images=frames[1:], duration=[ms for _, ms in self.frames], loop=0, optimize=True
        )

    def save(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        self.save_to(path)
        print(f"{path.relative_to(ROOT)}: {len(self.frames)} frames, {path.stat().st_size // 1024} KB", flush=True)


@dataclass
class Recording:
    """A session's video: its Looks over emulated time, the clips cut from it."""

    video_us: int = 0  # the capture's start
    start_us: int = 0  # the first clip's
    end_us: int = 0
    looks: list[tuple[int, Look]] = field(default_factory=list)  # from each emulated time on
    clips: list[tuple[str, int, int]] = field(default_factory=list)  # name, from, to (emulated us)

    def look_at(self, t: int) -> Look:
        current = Look()
        for at, look in self.looks:
            if at > t:
                break
            current = look
        return current


def videos(directory: Path, start_us: int, end_us: int) -> list[tuple[Path, float, int, float]]:
    """The AVIs DOSBox wrote, in order (a new one at each change of the picture's size): each with
    its first frame's emulated time, its frames and their rate."""
    out, t = [], float(start_us)
    for avi in sorted(directory.glob("*.avi")):
        probe = json.loads(
            subprocess.run(
                [
                    "ffprobe",
                    "-v",
                    "error",
                    "-select_streams",
                    "v",
                    "-count_frames",
                    "-show_entries",
                    "stream=r_frame_rate,nb_read_frames",
                    "-of",
                    "json",
                    avi,
                ],
                capture_output=True,
                text=True,
                check=True,
            ).stdout
        )["streams"][0]
        num, den = probe["r_frame_rate"].split("/")
        fps, n = int(num) / int(den), int(probe["nb_read_frames"])
        out.append((avi, t, n, fps))
        t += n / fps * 1e6
    # a frame is an emulated refresh: the frames' time is the time recorded, or the Looks land wrong
    assert abs(t - end_us) < 0.02 * (end_us - start_us) + 100_000, (t - start_us, end_us - start_us)
    return out


def frames(directory: Path, start_us: int, end_us: int, work: Path) -> Iterator[tuple[int, Path]]:
    """(emulated us, picture) of the recording, about FPS a second."""
    for avi, t0, _, fps in videos(directory, start_us, end_us):
        step = max(1, round(fps / FPS))
        out = work / avi.stem
        out.mkdir()
        subprocess.run(
            [
                "ffmpeg",
                "-v",
                "error",
                "-i",
                avi,
                "-vf",
                f"select=not(mod(n\\,{step}))",
                "-fps_mode",
                "passthrough",
                out / "%06d.png",
            ],
            check=True,
        )
        for k, png in enumerate(sorted(out.glob("*.png"))):
            yield int(t0 + k * step / fps * 1e6), png


class Ide:
    """The IDE in DOSBox, recorded from the DOS prompt on: BASIC args typed, the IDE loading."""

    def __init__(
        self, rec: Recording, args: str = "DEMO.BAS", files: dict[str, Path] | None = None, exe: str = "basic"
    ) -> None:
        self.rec = rec
        self.work = Path(tempfile.mkdtemp())
        self.video = self.work / "video"
        self.video.mkdir()
        (self.work / "BASIC.EXE").write_bytes(IDE.read_bytes())
        if QBASIC.exists():
            (self.work / "QBASIC.EXE").write_bytes(QBASIC.read_bytes())
        (self.work / "DOS32A.EXE").write_bytes((IDE.parent / "DOS32A.EXE").read_bytes())
        (self.work / "BASIC.HLP").write_bytes((ROOT / "src/basic.hlp").read_bytes())
        (self.work / "DEMO.BAS").write_bytes(DEMO.replace("\n", "\r\n").encode())
        for name, src in (files or {}).items():
            (self.work / name).write_bytes(src.read_bytes())
        self.s = dosplay.Session(self.work)
        # INT 33h replaced: AX=3 answers from 0:0540h, the rest return; no cursor in the pictures
        stub = "3d030075102e8b1e40052e8b0e42052e8b164405cfcf"
        self.s.send({"cmd": "mem_write_linear", "addr": 0x520, "data": stub})
        self.s.send({"cmd": "mem_write_linear", "addr": 0x33 * 4, "data": "20050000"})
        self.mouse(0, 0, False)
        # recording from before the cls: a line unchanged since the capture began is never sent to it
        if self.s.send({"cmd": "video_start", "dir": str(self.video)}).get("status") != "ok":
            sys.exit("demo: this dosbox-x's socket has no video_start (DOSBOX_BIN)")
        rec.video_us = self.now()
        self.s.keys_in_lockstep([*"cls", "enter"], 3000)  # the mount's line names the host's directory
        # the BIOS's tick count from 0: RANDOMIZE TIMER, the games' cities and food, the same every run
        self.s.send({"cmd": "mem_write_linear", "addr": 0x46C, "data": "00000000"})
        self.hold(100)
        rec.start_us = self.now()
        self.look(caption=f"{exe.upper()} {args}, from the DOS prompt".replace("  ", " "))
        self.hold(600)
        for c in f"{exe} {args}".strip().lower():
            self.s.keys_in_lockstep(["space" if c == " " else c], 3000)
            self.hold(60 + zlib.crc32(c.encode()) % 90)
        self.s.keys_in_lockstep(["enter"], 10000)
        # a key read while it loads is not the IDE's: on to its menu bar
        self.s.run_until(lambda: b"File" in self.chars()[0], 20000, "the menu bar")
        self.s.send({"cmd": "key_wait_arm", "stop": True})
        self.s.run_to_key(5000)

    # ---- the recording ----

    def now(self) -> int:
        return self.s.now_us

    def hold(self, ms: int) -> None:
        """ms of emulated time recorded; a key read on the way does not stop it."""
        until = self.now() + ms * 1000
        while self.now() < until:
            self.s.resume(max(1, (until - self.now()) // 1000))

    def look(self, **changes: object) -> None:
        last = self.rec.looks[-1][1] if self.rec.looks else Look()
        if "marks" in changes:
            changes["marks"] = tuple(changes["marks"])
        self.rec.looks.append((self.now(), replace(last, rows=self.rows(), **changes)))

    def caption(self, text: str) -> None:
        self.look(caption=text, key="", marks=())

    def clip(self, name: str, start: int) -> None:
        self.rec.clips.append((name, start, self.now()))

    def stop(self) -> None:
        self.rec.end_us = self.now()
        self.s.send({"cmd": "video_stop"})
        self.s.close()

    # ---- the screen ----

    def rows(self) -> int:
        return self.peek(0x484, 1)[0] + 1 or 25

    def peek(self, addr: int, n: int) -> bytes:
        return bytes.fromhex(self.s.send({"cmd": "mem_read_linear", "addr": addr, "len": n})["data"])

    def chars(self) -> list[bytes]:
        rows = self.rows()
        data = self.peek(0xB8000, rows * COLS * 2)[0::2]
        return [data[r * COLS : (r + 1) * COLS] for r in range(rows)]

    def cursor(self) -> tuple[int, int]:
        col, row = self.peek(0x450, 2)
        return row, col

    def find(self, text: str) -> tuple[int, int]:
        for r, line in enumerate(self.chars()):
            if (c := line.find(text.encode())) >= 0:
                return r, c
        raise LookupError(text)

    def box(self, title: str, bottom: int | None = None) -> Rect:
        """The box whose top line names title, its frame included, to its bottom corner or row bottom."""
        lines = self.chars()
        r, c = self.find(title)
        left = max(i for i in range(c) if lines[r][i] in b"\xda\xc9\xc2\xc3")
        right = min(i for i in range(c + len(title), COLS) if lines[r][i] in b"\xbf\xbb\xb4\xc2")
        if bottom is None:
            bottom = next(i for i in range(r + 1, len(lines)) if lines[i][left] in b"\xc0\xc8\xc3")
        return r, left, bottom - r + 1, right - left + 1

    # ---- the keyboard and the mouse ----

    def scan(self, code: int, ascii_: int = 0, ms: int = 0, key: str | None = None) -> None:
        if key is not None:
            self.look(key=key)
        ascii_ = ascii_ or ASCII.get(code, 0)
        self.s.send({"cmd": "key", "scancode": code, "ascii": ascii_, "stop": True})
        self.s.run_to_key(5000)
        self.hold(ms or 60)

    def keys(self, *codes: int, ms: int = 0, key: str | None = None) -> None:
        for code in codes:
            self.scan(code, ms=ms, key=key)

    def type(self, text: str, ms: int = 70) -> None:
        """Typed at a person's uneven pace, about ms a key; Tab and Enter shown as keys."""
        for i, c in enumerate(text):
            pace = ms // 2 + zlib.crc32(f"{text}{i}".encode()) % ms
            if c == "\n":
                self.scan(ENTER, 13, pace, key="Enter")
            elif c == "\t":
                self.scan(TAB, 9, pace, key="Tab")
            else:
                self.scan(SCAN[c], ord(c), pace, key="")

    def menu(self, alt: int, *letters: str, ms: int = 400, key: str = "") -> None:
        """A menu opened, its items picked by letter, ms between."""
        self.scan(alt, ms=ms, key=key)
        for c in letters:
            self.scan(SCAN[c], ord(c), ms)

    def theme(self, letter: str, ms: int) -> None:
        self.menu(ALT_O, "t", letter, ms=1)
        self.hold(ms)

    def mouse(self, row: int, col: int, down: bool) -> None:
        state = bytes([int(down), 0, col * 8 % 256, col * 8 // 256, row * 8 % 256, row * 8 // 256])
        self.s.send({"cmd": "mem_write_linear", "addr": 0x540, "data": state.hex()})

    def drag(self, row: int, cols: range, ms: int) -> None:
        """The mouse pressed at the first column, moved through the rest, let go: ms a step."""
        for col in cols:
            self.mouse(row, col, True)
            self.look(pointer=(row, col))
            self.hold(ms)
        self.mouse(row, cols[-1], False)
        self.hold(ms * 4)
        self.look(pointer=None)

    def open(self, name: str) -> None:
        """The welcome cleared, name opened with File > Open."""
        self.scan(ESC, ms=500, key="Esc")
        self.menu(ALT_F, "o", ms=500, key="Alt+F  O")
        self.type(name.lower(), ms=90)
        self.scan(ENTER, 13, 1800, key="Enter")

    # ---- a program's run ----

    def run(self) -> None:
        """F5 to the run's first key wait."""
        self.look(key="F5")
        self.s.send({"cmd": "key", "scancode": F5, "ascii": 0, "stop": True})
        self.s.run_to_key(60000)

    def play(self, steps: int, ms: int, turns: dict[int, int] | None = None) -> None:
        """The program left running ms at a time; before step i the key turns[i] goes in."""
        for i in range(steps):
            if turns and i in turns:
                self.s.send({"cmd": "key", "scancode": turns[i], "ascii": 0, "stop": True})
            self.hold(ms)

    def throw(self, angle: int, velocity: int, ms: int) -> None:
        """GORILLA's angle and velocity typed, the banana's flight recorded for ms."""
        self.type(f"{angle}\n{velocity}", ms=140)
        self.look(key="Enter")
        self.s.send({"cmd": "key", "scancode": ENTER, "ascii": 13, "stop": True})
        self.hold(ms)


def films(rec: Recording, ide: Ide) -> dict[str, Film]:
    """Each clip's frames from the recording, its Looks drawn on."""
    work = ide.work / "frames"
    work.mkdir()
    shots = list(frames(ide.video, rec.video_us, rec.end_us, work))
    out = {}
    for name, t0, t1 in rec.clips:
        film = Film()
        inside = [(t, png) for t, png in shots if t0 <= t < t1]
        for k, (t, png) in enumerate(inside):
            nxt = inside[k + 1][0] if k + 1 < len(inside) else t1
            film.add(Image.open(png).convert("RGB"), max(20, round((nxt - t) / 10_000) * 10), rec.look_at(t))
        out[name] = film
    return out


def ide_clips() -> tuple[Recording, dict[str, Film]]:
    rec = Recording()
    ide = Ide(rec)

    t = rec.start_us
    ide.caption("The IDE, the program loaded")
    ide.hold(1800)
    ide.caption("QBasic's colours, or a theme: here One Dark")
    ide.menu(ALT_O, "t", ms=700, key="Alt+O  T")
    ide.scan(SCAN["d"], ord("d"), 1600, key="D")
    ide.clip("start", t)

    t = ide.now()
    ide.caption("Completion offers only what fits: here, a string function")
    ide.scan(CTRL_END, ms=500, key="Ctrl+End")
    ide.type("\nmsg$ = l", ms=110)
    row, col = ide.cursor()
    ide.look(marks=[(row, col - 1, 1, 7)])
    ide.hold(2200)
    ide.look(marks=[])
    ide.scan(TAB, 9, 900, key="Tab")
    ide.type('"QuickrBasic", 6)\n', ms=70)
    ide.hold(800)
    ide.clip("complete", t)

    t = ide.now()
    ide.caption("Tab completes; the indent follows the block")
    ide.type("for r% = 10 to 90 step 8\n", ms=70)
    ide.type("cir\t", ms=110)
    ide.type(" (160, 100), r%, 15\nnext\n", ms=70)
    ide.hold(1500)
    ide.clip("indent", t)

    t = ide.now()
    ide.caption("Each line is checked as it is entered")
    ide.type("print (1 +\n", ms=90)
    ide.look(marks=[ide.box("Syntax error")])
    ide.hold(2800)
    ide.look(marks=[])
    ide.scan(ESC, key="Esc")
    ide.scan(UP, key="Up")
    ide.scan(0x15, 0x19, 1000, key="Ctrl+Y")  # the line deleted
    ide.clip("check", t)

    t = ide.now()
    ide.caption("Every SUB and FUNCTION in its own view")
    ide.scan(F2, key="F2")
    ide.look(marks=[ide.box("SUBs")])
    ide.hold(2000)
    ide.look(marks=[])
    ide.scan(DOWN, ms=500, key="Down")
    ide.scan(ENTER, 13, 2000, key="Enter")
    ide.scan(CTRL_F2, ms=900, key="Ctrl+F2")
    ide.clip("procedures", t)

    t = ide.now()
    ide.caption("F5 runs it: QBasic's graphics and runtime")
    ide.run()
    ide.hold(2500)
    ide.scan(ENTER, 13, 300, key="Enter")
    ide.clip("run", t)

    t = ide.now()
    ide.caption("Side by side; drag any line between windows")
    ide.menu(ALT_V, "v", ms=600, key="Alt+V  V")
    ide.hold(600)
    ide.look(key="Drag")
    ide.drag(5, range(39, 58), 50)
    ide.drag(5, range(57, 25, -1), 50)
    ide.hold(600)
    ide.menu(ALT_V, "v", ms=600, key="Alt+V  V")
    ide.clip("split", t)

    t = ide.now()
    ide.caption("Breakpoints: F9 marks the line, F5 runs to it")
    ide.scan(SHIFT_F2, ms=900, key="Shift+F2")
    ide.keys(DOWN, DOWN, ms=300, key="Down")
    ide.scan(F9, ms=1200, key="F9")
    ide.scan(CTRL_F2, key="Ctrl+F2")
    ide.scan(F5, key="F5")
    ide.look(marks=[ide.box("Locals", ide.find("Call stack")[0])])
    ide.hold(2200)
    ide.look(marks=[])
    for _ in range(2):
        ide.scan(F5, ms=900, key="F5")
    ide.clip("breakpoint", t)

    t = ide.now()
    ide.caption("Watches, evaluated in the stopped call's scope")
    ide.scan(F6, key="F6")
    ide.scan(INS, ms=700, key="Ins")
    ide.type("n% * n%", ms=90)
    ide.scan(ENTER, key="Enter")
    r, _ = ide.find("n% * n%")
    ide.look(marks=[(r, 49, 1, 31)])
    ide.hold(1600)
    ide.look(marks=[])
    ide.scan(ESC, key="Esc")
    for _ in range(2):
        ide.scan(F5, ms=900, key="F5")
    ide.clip("watch", t)

    t = ide.now()
    ide.caption("Any caller's frame: its locals, the line it called from")
    ide.scan(F6, key="F6")
    ide.keys(TAB, TAB, ms=500, key="Tab")
    ide.look(marks=[ide.box("Call stack", ide.rows() - 2)])
    ide.keys(DOWN, DOWN, ms=600, key="Down")
    ide.scan(ENTER, ms=1600, key="Enter")
    ide.look(marks=[])
    ide.scan(ESC, key="Esc")
    ide.scan(F8, ms=800, key="F8")
    ide.scan(F8, ms=800, key="F8")
    ide.clip("stack", t)

    t = ide.now()
    ide.caption("25, 43 and 50 line text modes")
    ide.menu(ALT_O, "z", ms=900, key="Alt+O  Z")
    ide.scan(SCAN["5"], ord("5"), 2800, key="5")
    ide.menu(ALT_O, "z", "2", ms=300, key="Alt+O  Z  2")
    ide.hold(500)
    ide.clip("rows", t)

    t = ide.now()
    ide.caption("Nine themes, from Zed's")
    ide.menu(ALT_O, "t", ms=1500, key="Alt+O  T")
    ide.scan(ESC)
    ide.scan(ESC)
    for name, letter in THEMES[2:]:
        ide.look(caption=f"Theme: {name}", key="")
        ide.theme(letter, 1200)
    ide.clip("themes", t)

    ide.stop()
    return rec, films(rec, ide)


def sample(name: str) -> Path | None:
    return next((p for p in (SAMPLES / name.upper(), SAMPLES / name.lower()) if p.exists()), None)


# Tuned in these flows: what came before the run decides RANDOMIZE TIMER's seed, so the city and the food.
GORILLA_THROW = (60, 74)
NIBBLES_TURNS = {
    1: DOWN,
    8: LEFT,
    59: DOWN,
    71: RIGHT,
    118: UP,
    157: LEFT,
    206: DOWN,
    232: RIGHT,
}


def gorillas(angle: int, velocity: int) -> dict[str, Film]:
    rec = Recording()
    ide = Ide(rec, "", {"GORILLA.BAS": sample("gorilla.bas")})
    t = ide.now()
    ide.caption("QBasic's GORILLA.BAS, unchanged: opened, F5")
    ide.open("GORILLA.BAS")
    ide.run()
    ide.hold(2500)
    ide.scan(SCAN[" "], 32, 500, key="Space")
    for _ in range(4):
        ide.scan(ENTER, 13, 400, key="Enter")
    ide.scan(SCAN["p"], ord("p"), key="P")
    ide.hold(3000)
    ide.throw(angle, velocity, 6500)
    ide.clip("gorillas", t)
    ide.stop()
    return films(rec, ide)


def nibbles(turns: dict[int, int]) -> dict[str, Film]:
    rec = Recording()
    ide = Ide(rec, "", {"NIBBLES.BAS": sample("nibbles.bas")})
    t = ide.now()
    ide.caption("QBasic's NIBBLES.BAS, unchanged: opened, F5")
    ide.open("NIBBLES.BAS")
    ide.run()
    ide.hold(2000)
    ide.scan(SCAN[" "], 32, 500, key="Space")
    for answer in ("1", "40", "n", "c"):
        ide.type(answer + "\n", ms=250)
    ide.look(key="Space")
    ide.s.send({"cmd": "key", "scancode": SCAN[" "], "ascii": 32, "stop": True})
    ide.s.run_to_key(3000)  # the turns are timed from this key
    ide.look(key="Arrows")
    ide.play(260, 50, turns)
    ide.clip("nibbles", t)
    ide.stop()
    return films(rec, ide)


def themes() -> None:
    rec = Recording()
    ide = Ide(rec)
    t = ide.now()
    for name, letter in THEMES:
        ide.look(caption=name)
        ide.theme(letter, 1600)
    ide.clip("themes", t)
    ide.stop()
    films(rec, ide)["themes"].save(IMG / "themes.gif")


def timeline(film: Film) -> list[tuple[int, Image.Image]]:
    """The film's frames with the ms each starts at."""
    out, t = [], 0
    for img, ms in film.frames:
        out.append((t, img))
        t += ms
    return out


def versus() -> Film:
    """QBasic 1.1 and QuickrBasic side by side: DEMO.BAS loaded and run, the same keys in each."""
    takes = []
    for exe in ("qbasic", "basic"):
        rec = Recording()
        ide = Ide(rec, "DEMO.BAS", exe=exe)
        ide.caption("QBasic 1.1" if exe == "qbasic" else "QuickrBasic")
        ide.hold(1500)
        ide.run()
        ide.hold(2500)
        ide.clip("versus", rec.start_us)
        ide.stop()
        takes.append(timeline(films(rec, ide)["versus"]))
    film, step = Film(), 50
    end = max(take[-1][0] for take in takes) + 1000
    for t in range(0, end, step):
        pair = [next(img for at, img in reversed(take) if at <= t) for take in takes]
        img = Image.new("RGB", (2 * SIZE[0] + 8, pair[0].height), BAR_BG)
        img.paste(pair[0])
        img.paste(pair[1], (SIZE[0] + 8, 0))
        if film.frames and film.frames[-1][0].tobytes() == img.tobytes():
            film.frames[-1] = (film.frames[-1][0], film.frames[-1][1] + step)
        else:
            film.frames.append((img, step))
    return film


def ease(x: float) -> float:
    return x * x * (3 - 2 * x)


def showcase(clips: dict[str, Film], cols: int = 4, steps: int = 4, ms: int = 70) -> Film:
    """Every clip's last frame in a grid; the view zooms into each, plays it, zooms out to the grid."""
    w, h = clips[next(iter(clips))].frames[0][0].size
    gap, rows = 24, -(-len(clips) // cols)
    gw, gh = cols * w + (cols + 1) * gap, rows * h + (rows + 1) * gap
    # the canvas: the grid centred in a box of the frames' aspect
    cw, ch = max(gw, gh * w // h), max(gh, gw * h // w)
    dx, dy = (cw - gw) // 2, (ch - gh) // 2
    canvas = Image.new("RGB", (cw, ch), BAR_BG)
    tiles = []
    for k, film in enumerate(clips.values()):
        x, y = dx + gap + k % cols * (w + gap), dy + gap + k // cols * (h + gap)
        canvas.paste(film.frames[-1][0], (x, y))
        tiles.append((x, y, x + w, y + h))
    whole = (0, 0, cw, ch)

    def view(box: tuple[float, ...]) -> Image.Image:
        return canvas.resize((w, h), Image.Resampling.BOX, box=box)  # LANCZOS: twice the GIF

    out = Film()
    out.frames.append((view(whole), 1500))
    for tile, film in zip(tiles, clips.values(), strict=True):
        boxes = [
            tuple(a + (b - a) * ease(i / steps) for a, b in zip(whole, tile, strict=True)) for i in range(1, steps)
        ]
        out.frames += [(view(b), ms) for b in boxes]
        out.frames += film.frames
        out.frames += [(view(b), ms) for b in reversed(boxes)]
        out.frames.append((view(whole), 700))
    return out


def main() -> None:
    names = sys.argv[1:] or ["versus", "ide", "games", "themes"]
    clips: dict[str, Film] = {}
    if "ide" in names:
        clips |= ide_clips()[1]
    if "games" in names and sample("gorilla.bas") and sample("nibbles.bas"):
        clips |= gorillas(*GORILLA_THROW) | nibbles(NIBBLES_TURNS)
    for name, film in clips.items():
        film.save(IMG / "clips" / f"{name}.gif")
    if "versus" in names and QBASIC.exists():
        versus().save(IMG / "versus.gif")
    if "ide" in names and "games" in names:
        showcase(clips).save(IMG / "tour.gif")
    if "themes" in names:
        themes()


if __name__ == "__main__":
    main()
