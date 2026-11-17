"""x86demo_test.py: the GIF plays back the frames it was given; the recording starts clean.

A graphics mode's picture, smaller than the text modes', made a frame of another size, and the
frames after it kept a closed dialog's shadow on screen for the rest of the tour.

The capture's first frame is the screen before the cls. Timed from the first clip's start, not
the capture's, it landed in the first clip: the mount's line, the host's temporary directory.
"""

import os

import x86demo
from PIL import Image


def test_frames_round_trip(tmp: str = "/tmp") -> None:
    ground, shadow, text, other = (40, 44, 52), (10, 10, 12), (200, 120, 220), (230, 190, 90)
    a = Image.new("RGB", (64, 32), ground)
    a.paste(text, (4, 4, 20, 12))
    b = a.copy()
    b.paste(shadow, (40, 8, 48, 24))  # a dialog's shadow
    c = Image.new("RGB", (48, 24), other)  # a graphics mode's smaller picture
    film = x86demo.Film()
    for img in (a, b, c, a):
        film.add(img, 100)
    path = f"{tmp}/x86demo_test.gif"
    film.save_to(path)
    gif = Image.open(path)
    for i, want in enumerate(f for f, _ in film.frames):
        gif.seek(i)
        got = gif.convert("RGB")
        assert got.size == film.frames[0][0].size, (i, got.size)
        assert got.tobytes() == want.tobytes(), i


def test_recording_starts_clean() -> None:
    if not os.environ.get("DOSBOX_BIN"):
        print("skipped: DOSBOX_BIN, a dosbox-x with video_start")
        return
    rec = x86demo.Recording()
    ide = x86demo.Ide(rec)
    ide.hold(200)
    ide.clip("x", rec.start_us)
    ide.stop()
    first = x86demo.films(rec, ide)["x"].frames[0][0]
    lit = [x for x in range(first.width) for y in range(32, 80) if max(first.getpixel((x, y))) > 64]
    assert not lit, f"rows 2-4 should be blank after the cls: {len(lit)} lit pixels"


if __name__ == "__main__":
    test_frames_round_trip()
    test_recording_starts_clean()
    print("ok")
