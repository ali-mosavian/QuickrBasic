#!/usr/bin/env python3
"""x86demo.py [tour|themes]...: the IDE's animated pictures for the README, played in DOSBox.

    tour    docs/img/tour.gif: a program opened, typed into, run, debugged, its theme changed
    themes  docs/img/themes.gif: the same code in each theme

X86_IDE picks the build (build/c/ide32.exe).
"""

import os
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

LLRM = Path("/Users/alim/work/personal/llrm-qbmac/tools")
sys.path[:0] = [str(LLRM), str(LLRM / "dosbatch")]
import llrmbin  # noqa: E402

llrmbin.stale_binaries = lambda *a, **k: []  # the emulator is the stamped one; the llrm sources are not ours
import qbplay  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
IMG = ROOT / "docs/img"
IDE = Path(os.environ.get("X86_IDE", ROOT / "build/c/ide32.exe"))

ALT_F, ALT_O, ALT_V = 0x21, 0x18, 0x2F
BAR, BAR_BG, BAR_FG = 40, (24, 24, 27), (235, 235, 240)  # the caption's strip under the screen
FONT = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 20)
F2, F5, F6, F8, F9, SHIFT_F2, CTRL_F2, INS = 0x3C, 0x3F, 0x40, 0x42, 0x43, 0x55, 0x5F, 0x52
TAB, ENTER, ESC, BKSP, UP, DOWN, CTRL_HOME, CTRL_END, END = 0x0F, 0x1C, 0x01, 0x0E, 0x48, 0x50, 0x77, 0x75, 0x4F

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


class Film:
    """Frames, each with the caption under it, and how long each shows."""

    def __init__(self) -> None:
        self.frames: list[tuple[Image.Image, int]] = []
        self.caption = ""

    def add(self, screen: Image.Image, ms: int) -> None:
        # one size for all: a graphics mode's smaller picture left the GIF's later frames stale
        if self.frames:
            screen = screen.resize((self.frames[0][0].width, self.frames[0][0].height - BAR), Image.Resampling.NEAREST)
        img = Image.new("RGB", (screen.width, screen.height + BAR), BAR_BG)
        img.paste(screen)
        draw = ImageDraw.Draw(img)
        box = draw.textbbox((0, 0), self.caption, font=FONT)
        x, y = (img.width - box[2]) // 2, screen.height + (BAR - box[3]) // 2
        draw.text((x, y), self.caption, font=FONT, fill=BAR_FG)
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
        self.save_to(path)
        print(f"{path.relative_to(ROOT)}: {len(self.frames)} frames, {path.stat().st_size // 1024} KB")


class Ide:
    """The IDE in DOSBox, a frame taken after each step."""

    def __init__(self, film: Film) -> None:
        self.film = film
        self.work = Path(tempfile.mkdtemp())
        exe = "BASIC" + IDE.suffix.upper()
        (self.work / exe).write_bytes(IDE.read_bytes())
        (self.work / "DOS32A.EXE").write_bytes((IDE.parent / "DOS32A.EXE").read_bytes())
        (self.work / "BASIC.HLP").write_bytes((ROOT / "src/ui/basic.hlp").read_bytes())
        (self.work / "DEMO.BAS").write_bytes(DEMO.replace("\n", "\r\n").encode())
        self.s = qbplay.Session(self.work)
        # INT 33h replaced: AX=3 answers from 0:0540h, the rest return; no cursor in the pictures
        stub = "3d030075102e8b1e40052e8b0e42052e8b164405cfcf"
        self.s.send({"cmd": "mem_write_linear", "addr": 0x520, "data": stub})
        self.s.send({"cmd": "mem_write_linear", "addr": 0x33 * 4, "data": "20050000"})
        self.mouse(0, 0, False)
        self.s.launch(3000, exe)
        self.s.keys_in_lockstep(["esc"], 3000)  # the first key after the launch is lost
        self.s.resume(100)
        self.s.run_until(lambda: "PRESS ESC" in self.s.screen().upper(), 5000, "the welcome box")
        self.shot = self.work / "shot.png"

    def caption(self, text: str) -> None:
        self.film.caption = text

    def frame(self, ms: int) -> None:
        self.s.snap(self.shot)
        self.film.add(Image.open(self.shot).convert("RGB"), ms)

    def mouse(self, row: int, col: int, down: bool) -> None:
        state = bytes([int(down), 0, col * 8 % 256, col * 8 // 256, row * 8 % 256, row * 8 // 256])
        self.s.send({"cmd": "mem_write_linear", "addr": 0x540, "data": state.hex()})
        self.s.resume(60)

    def drag(self, row: int, cols: range, ms: int) -> None:
        """The mouse pressed at the first column, moved through the rest, let go: a frame a step."""
        for col in cols:
            self.mouse(row, col, True)
            self.frame(ms)
        self.mouse(row, cols[-1], False)

    def scan(self, code: int, ascii_: int = 0, ms: int = 0) -> None:
        ascii_ = ascii_ or {ESC: 27, ENTER: 13, TAB: 9, BKSP: 8}.get(code, 0)
        self.s.send({"cmd": "key", "scancode": code, "ascii": ascii_, "stop": True})
        self.s.run_to_key(3000)
        self.s.resume(60)
        if ms:
            self.frame(ms)

    def keys(self, *codes: int, ms: int = 0) -> None:
        for code in codes:
            self.scan(code, ms=ms)

    def type(self, text: str, ms: int = 70) -> None:
        for c in text:
            if c == "\n":
                self.scan(ENTER, 13, ms)
            elif c == "\t":
                self.scan(TAB, 9, ms)
            else:
                self.scan(SCAN[c], ord(c), ms)

    def menu(self, alt: int, *letters: str, ms: int = 0) -> None:
        """A menu opened, its items picked by letter; a frame of each when ms."""
        self.scan(alt, ms=ms)
        for c in letters:
            self.scan(SCAN[c], ord(c), ms)

    def theme(self, letter: str, ms: int = 0) -> None:
        self.menu(ALT_O, "t", letter)
        if ms:
            self.frame(ms)

    def run_until_key(self) -> None:
        """F5 to the run's last key wait: the program's output drawn."""
        self.scan(F5)
        for _ in range(100):
            if "PRESS ANY KEY" in self.s.screen().upper():
                return
            self.s.resume(100)

    def close(self) -> None:
        self.s.close()


def tour() -> None:
    film = Film()
    ide = Ide(film)
    ide.caption("QuickrBasic: QBasic's IDE, rebuilt for 32-bit DOS")
    ide.frame(2200)
    ide.scan(ESC)
    ide.theme("d")
    ide.caption("Syntax highlighting from the compiler's own lexer")
    ide.menu(ALT_F, "o", ms=400)
    ide.type("demo.bas", ms=60)
    ide.scan(ENTER, 13, 2500)
    ide.caption("Completion: only what fits, as the compiler decides")
    ide.keys(CTRL_END)
    ide.type("\nmsg$ = l", ms=90)
    ide.frame(2200)
    ide.type("\t", ms=600)
    ide.type('"QuickrBasic", 6)\n', ms=60)
    ide.caption("Tab completes; the indent follows the block")
    ide.type("for r% = 10 to 90 step 8\n", ms=60)
    ide.type("cir\t", ms=90)
    ide.type(" (160, 100), r%, 15\nnext\n", ms=60)
    ide.frame(1200)
    ide.caption("Each line checked as it is entered")
    ide.type("print (1 +\n", ms=70)
    ide.frame(2200)
    ide.scan(ESC)
    ide.scan(UP)
    ide.scan(0x15, 0x19, 900)  # Ctrl+Y: the line deleted
    ide.caption("F2: the program's SUBs and FUNCTIONs")
    ide.scan(F2, ms=1800)
    ide.scan(ESC)
    ide.caption("F5 runs it: QBasic's graphics, sound and runtime")
    ide.run_until_key()
    ide.frame(3000)
    ide.scan(ENTER, 13)
    ide.caption("Windows side by side; drag any line between them")
    ide.menu(ALT_V, "v")
    ide.frame(900)
    ide.drag(5, range(39, 56, 2), 90)
    ide.frame(500)
    ide.drag(5, range(55, 26, -2), 90)
    ide.frame(1200)
    ide.menu(ALT_V, "v")
    ide.caption("Debugger: breakpoints, stepping, the call stack")
    ide.scan(SHIFT_F2, ms=900)
    ide.keys(DOWN, DOWN)
    ide.scan(F9, ms=900)
    ide.scan(CTRL_F2)
    ide.scan(F5, ms=1800)
    ide.caption("Watches, in the stopped call's scope")
    ide.scan(F6)
    ide.scan(INS, ms=600)
    ide.type("n% * n%", ms=70)
    ide.scan(ENTER, ms=1200)
    ide.scan(ESC)
    ide.caption("Locals, the last changed first")
    for _ in range(3):
        ide.scan(F5, ms=1000)
    ide.caption("Any caller's frame: its locals, the line it called from")
    ide.scan(F6)
    ide.keys(TAB, TAB, ms=500)
    ide.keys(DOWN, DOWN, ms=500)
    ide.scan(ENTER, ms=2000)
    ide.scan(ESC)
    for _ in range(2):
        ide.scan(F8, ms=800)
    ide.caption("43 and 50 line text modes")
    ide.menu(ALT_O, "z", ms=900)
    ide.scan(SCAN["5"], ord("5"), 2500)
    ide.menu(ALT_O, "z", "2")
    ide.caption("Nine themes")
    ide.menu(ALT_O, "t", ms=1500)
    ide.scan(ESC)
    ide.scan(ESC)
    for name, letter in THEMES[2:]:
        ide.caption(f"Theme: {name}")
        ide.theme(letter, ms=1200)
    ide.close()
    film.save(IMG / "tour.gif")


def themes() -> None:
    film = Film()
    ide = Ide(film)
    ide.scan(ESC)
    ide.menu(ALT_F, "o")
    ide.type("demo.bas\n", ms=0)
    for name, letter in THEMES:
        ide.caption(name)
        ide.theme(letter, ms=1600)
    ide.close()
    film.save(IMG / "themes.gif")


if __name__ == "__main__":
    for name in sys.argv[1:] or ["tour", "themes"]:
        {"tour": tour, "themes": themes}[name]()
