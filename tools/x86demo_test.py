"""x86demo_test.py: the GIF plays back the frames it was given.

A graphics mode's picture, smaller than the text modes', made a frame of another size, and the
frames after it kept a closed dialog's shadow on screen for the rest of the tour.
"""

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


if __name__ == "__main__":
    test_frames_round_trip()
    print("ok")
