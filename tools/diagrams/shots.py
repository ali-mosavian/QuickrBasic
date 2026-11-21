"""The editor's features as the Mac's window draws them: python3 tools/diagrams/shots.py APP OUT_DIR

Each picture is one run of APP (the windowed build) off-screen on FEEL.BAS, its keys typed by
QB_KEYS and the screen saved by QB_SHOT (help/ext/test-scripts.md), written as OUT_DIR/NAME.png.
docs/ide-design.md shows them."""

import os
import sys
import tempfile
import subprocess
from pathlib import Path

from PIL import Image

PROGRAM = """declare sub drawworld (w%)
declare sub drawwave (n%)
declare function total! (n%)
dim shared price(10)
for i = 1 to 10
    price(i) = i * 1.5
next
count% = 10
print total(count%)
drawworld 3
end

sub drawworld (w%)
    for x = 1 to w%
        if x mod 2 = 0 then
            print "even"; x
        else
            print "odd"; x
        end if
    next
end sub

sub drawwave (n%)
    print n%
end sub

function total! (n%)
    sum = 0
    for i = 1 to n%
        sum = sum + price(i)
    next
    stop
    total = sum
end function
"""

DOWN, UP, RIGHT, HOME, END, ENTER = "5000", "4800", "4D00", "4700", "4F00", "1C0D"
CTRL_P, ALT_S, SHIFT_F5 = "1910", "1F00", "5800"
SCANS = dict(
    zip(
        "abcdefghijklmnopqrstuvwxyz", bytes.fromhex("1E302E2012212223172425263231181910131F14162F112D152C"), strict=True
    )
)
SCANS |= {" ": 0x39, "+": 0x0D}
START, STEP, SETTLE = 60, 6, 30  # frames: the first key, between keys, the last key to the shot


def typed(text: str) -> list[str]:
    return [f"{SCANS[c]:02X}{ord(c):02X}" for c in text]


def go_to(letters: str) -> list[str]:
    """Ctrl+P, the letters, Enter: the view they name shown."""
    return [CTRL_P, *typed(letters), ENTER]


# a picture's name: its keys, and frames to wait after them
SHOTS: dict[str, tuple[list[str], int]] = {
    "error": ([*[DOWN] * 7, END, *typed(" +"), DOWN, UP], 0),
    "find": ([ALT_S, "2166", *typed("price")], 0),
    "matches": ([*go_to("dw"), DOWN, DOWN, HOME], 0),
    "uses": ([*go_to("dw"), DOWN, HOME, *[RIGHT] * 4], 0),
    "goto": ([CTRL_P, *typed("drw")], 0),
    "values": ([SHIFT_F5], 90),
}


def shot(app: Path, keys: list[str], wait: int, out: Path) -> None:
    """One run of app, keys typed, its screen wait frames after the last to out as a PNG."""
    with tempfile.TemporaryDirectory() as tmp:
        program, ppm = Path(tmp) / "FEEL.BAS", Path(tmp) / "shot.ppm"
        program.write_text(PROGRAM.replace("\n", "\r\n"))
        at = [START + i * STEP for i in range(len(keys))]
        env = os.environ | {
            "QB_KEYS": " ".join(f"{t}:{k}" for t, k in zip(at, keys, strict=True)),
            "QB_SHOT": f"{ppm}@{at[-1] + SETTLE + wait}",
            "SDL_VIDEO_DRIVER": "offscreen",
            "SDL_AUDIO_DRIVER": "dummy",
        }
        subprocess.run([str(app), program.name], env=env, timeout=60, check=True, cwd=tmp)
        head, pixels = ppm.read_bytes().split(b"\n", 1)
        _, w, h, _ = head.split()
        Image.frombytes("RGB", (int(w), int(h)), pixels).save(out)


def main() -> None:
    app, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)

    for name, (keys, wait) in SHOTS.items():
        shot(app, keys, wait, out / f"{name}.png")


if __name__ == "__main__":
    main()
