#!/usr/bin/env python3
"""mothlite_test.py [EXE]: MOTHLITE.BAS's stones drawn at every place they scroll through, and
every PLAY string it builds played, whole frames drawn at fixed times and scrolls, and the
palette eased through each sky and faded, and sparks drawn from their paths where moving them a
tick at a time put them, and vines drawn by runs where the row by row ones were.

The demo's main loop is swapped for one that draws each kind of stone from off the left edge to
off the right. A stone with runes partly off the left (x < 0) took a negative rune index:
Subscript out of range mid-game. Then each song's beats, layers and sound effects are played: a
bird call written in octave 7 was an Illegal function call once the sky's mood reached it.
The frames' digest is the procedural drawing's, from before the sky and ridges were kept as
pictures: a picture cut at a wrong row or column changes it. The palette's is the one shaded in
full at every step, as gfx's palette holds it, in the DAC's 6 bits a channel.
"""

import sys
import hashlib
import tempfile
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEMO = ROOT / "src/plat/wasm/web/demos/MOTHLITE.BAS"

DRIVER = """OpenScreen
LoadTables
LoadTerrain
BuildSprites
DIM st(0) AS Stone, kind AS INTEGER, x AS INTEGER
FOR kind = 0 TO NKIND - 1
    FOR x = -STONEW - 6 TO 645
        st(0).x = x
        st(0).kind = kind
        st(0).gap = 100
        DrawStone st(), 0, 0
    NEXT
NEXT
PRINT "stones ok"
DIM id AS INTEGER, b AS INTEGER, v AS INTEGER
PLAY "MB"
FOR id = TITLESONG TO GAMESONG
    FOR b = 0 TO SongLen(id) - 1
        FOR v = 0 TO 2
            PLAY Tune(id, b, v)
            SOUND 37, 0
        NEXT
        FOR v = 0 TO NSKIES - 1
            PLAY Mood(id, v, b)
            SOUND 37, 0
        NEXT
        PLAY Low(b), Fast(b), Echo(id, b)
        SOUND 37, 0
    NEXT
NEXT
FOR id = 0 TO NSFX - 1
    PlaySfx id
    SOUND 37, 0
NEXT
PRINT "sounds ok"
END
"""


FRAMES = """OpenScreen
LoadTables
LoadTerrain
BuildGlyphs
LoadMoth
BuildSprites
BuildLand
DIM w AS World, joy AS Pad, cur(NKEY - 1, 2) AS SINGLE
DIM st(NSTONE - 1) AS Stone, sp(NSPARK - 1) AS Spark
DIM k AS INTEGER, i AS INTEGER, kk AS INTEGER
NewWorld w, cur()
OPEN "FRAMES.BIN" FOR BINARY AS #1
FOR k = 0 TO 5
    w.g.clock = k * 1.37
    Orbit w, joy, st(), sp()
    Render w, st(), sp()
NEXT
Launch w, st(), sp()
RESTORE Scrolls
DO
    READ w.g.scroll
    IF w.g.scroll < 0 THEN EXIT DO
    w.g.clock = w.g.scroll / 7
    FOR i = 0 TO NSTONE - 1
        st(i).x = -37 + i * 97 + kk * 3
        st(i).kind = (i + kk) MOD NKIND
        st(i).gap = 60 + (i * 23 + kk * 7) MOD 71
    NEXT
    kk = kk + 1
    Render w, st(), sp()
LOOP
w.g.mode = OVER: w.g.score = 12: w.g.best = 30: w.g.newBest = -1: w.g.scroll = 3001
FOR k = 0 TO 5
    w.g.modeT = k * .2: w.g.clock = k * .3
    Render w, st(), sp()
NEXT
w.g.mode = TITLE
FOR k = 0 TO 3
    w.g.clock = 2.1 + k * .9
    Orbit w, joy, st(), sp()
    Render w, st(), sp()
NEXT
CLOSE #1
PRINT "frames ok"
END
Scrolls:
DATA 0,1,2.5,5,13,100,799,1597.5,1600,2400,4266.7,6400,8533,12790,12800,12817,25000,-1
"""

# The frames' MD5, from the drawing at 66c90a5 (seeded alike) with its text, then drawn by gfx. Its
# own was ca7941b5, taken with no text: PRINT drew nothing on a screen with no window, so BuildGlyphs
# read blank glyphs. gfx's lines step other pixels than QB's: of the sun's rays, 589 pixels in the 33
# frames are a pixel aside, their count the same.
FRAMES_MD5 = "cdf7d424a7993d4f134b6b71a3a31588"
# Title text's colours, in the first frame.
GOLD = range(80, 88)


def frames_program() -> str:
    head, rest = DEMO.read_text().split("\nMain\nEND\n", 1)
    rest = rest.replace("RANDOMIZE TIMER", "RANDOMIZE 5").replace(
        "    display.flip\n", "    Dump\n    display.flip\n"
    )
    dump = "\nSUB Dump\n    DIM a(SW * SH - 1) AS BYTE, i AS LONG\n    surface.read Canvas, 0, 0, SW, SH, a()\n"
    dump += "    FOR i = 0 TO SW * SH - 1: PUT #1, , a(i): NEXT\nEND SUB\n"
    return "DECLARE SUB Dump ()\n" + head + "\n" + FRAMES + rest + dump


def frames(exe: str, tmp: str) -> str:
    path = Path(tmp) / "FRAMES.BAS"
    path.write_text(frames_program())
    run = subprocess.run([exe, path.name], cwd=tmp, capture_output=True, timeout=120)
    if "frames ok" not in run.stdout.decode(errors="replace"):
        return "FAIL " + (run.stdout + run.stderr).decode(errors="replace")[-600:]
    data = (Path(tmp) / "FRAMES.BIN").read_bytes()
    if not any(b in GOLD for b in data[: 320 * 200]):
        return "FAIL no title text drawn"
    return hashlib.md5(data).hexdigest()


PALETTES = """OpenScreen
LoadTables
DIM w AS World, cur(NKEY - 1, 2) AS SINGLE, p AS INTEGER, n AS INTEGER, q AS INTEGER
DIM pal(255) AS LONG, c AS LONG, d AS LONG
OPEN "PAL.BIN" FOR BINARY AS #1
NewWorld w, cur()
FOR p = 0 TO 2 * NSKIES
    w.sky.phase = p MOD NSKIES
    IF p = NSKIES THEN w.sky.fadeTo = 0
    IF p = NSKIES + 1 THEN w.sky.fadeTo = 1
    FOR n = 1 TO 240
        EaseSky w.sky, cur()
        IF PalDirty THEN ApplyPalette w.sky, cur(): PalDirty = 0
        IF n MOD 4 = 0 THEN
            display.get_palette 0, pal()
            FOR q = 0 TO 255
                c = pal(q)
                d = ((c \\ 65536) AND 255) \\ 4
                d = d + (((c \\ 256) AND 255) \\ 4) * 256
                d = d + ((c AND 255) \\ 4) * 65536
                PUT #1, , d
            NEXT
        END IF
    NEXT
NEXT
CLOSE #1
PRINT "palettes ok"
END
"""

# The DAC's MD5 at each step, from the palette at 66c90a5.
PALETTES_MD5 = "5466934f815a3ff955d663c4ac091b3e"


def palettes(exe: str, tmp: str) -> str:
    head, rest = DEMO.read_text().split("\nMain\nEND\n", 1)
    path = Path(tmp) / "PAL.BAS"
    path.write_text(head + "\n" + PALETTES + rest)
    run = subprocess.run([exe, path.name], cwd=tmp, capture_output=True, timeout=120)
    if "palettes ok" not in run.stdout.decode(errors="replace"):
        return "FAIL " + (run.stdout + run.stderr).decode(errors="replace")[-600:]
    return hashlib.md5((Path(tmp) / "PAL.BIN").read_bytes()).hexdigest()


SPARKS = """LoadTables
SparkPaths
DIM sp(NSPARK - 1) AS Spark, rf(NSPARK - 1, 4) AS SINGLE, m AS Moth
DIM t AS INTEGER, i AS INTEGER, j AS INTEGER, l AS SINGLE, n AS INTEGER
DIM seen AS LONG, bad AS LONG, ages AS LONG
RANDOMIZE 3
m.x = 100: m.y = 90
FOR t = 1 TO 3000
    IF t MOD 6 = 0 THEN Emit sp(), m, BTRAIL
    IF t MOD 50 = 0 THEN Emit sp(), m, BFLAP
    IF t MOD 333 = 0 THEN Emit sp(), m, BSCORE
    FOR i = 0 TO NSPARK - 1
        IF sp(i).born = Ticks THEN
            rf(i, 0) = sp(i).x: rf(i, 1) = sp(i).y: rf(i, 2) = sp(i).vx
            rf(i, 3) = sp(i).vy: rf(i, 4) = sp(i).life
        END IF
        IF rf(i, 4) > 0 THEN
            rf(i, 0) = rf(i, 0) + rf(i, 2) * STEPT
            rf(i, 1) = rf(i, 1) + rf(i, 3) * STEPT
            rf(i, 2) = rf(i, 2) * (1 - 1.5 * STEPT)
            rf(i, 3) = rf(i, 3) * (1 - 2 * STEPT)
            rf(i, 3) = rf(i, 3) - 10 * STEPT
            rf(i, 4) = rf(i, 4) - STEPT
        END IF
    NEXT
    Ticks = Ticks + 1
    FOR i = 0 TO NSPARK - 1
        l = SparkLife!(sp(i))
        IF l > 0 OR rf(i, 4) > 0 THEN
            seen = seen + 1
            n = Ticks - sp(i).born
            j = (l > 0) <> (rf(i, 4) > 0)
            j = j OR (l > .35) <> (rf(i, 4) > .35) OR (l > .7) <> (rf(i, 4) > .7) OR (l > .6) <> (rf(i, 4) > .6)
            j = j OR INT(sp(i).x + sp(i).vx * PathX(n)) <> INT(rf(i, 0))
            j = j OR INT(sp(i).y + sp(i).vy * PathY(n) + PathG(n)) <> INT(rf(i, 1))
            IF j THEN bad = bad + 1
            j = (n < sp(i).alive) <> (l > 0) OR (n < sp(i).warm) <> (l > .35)
            IF j OR (n < sp(i).hot) <> (l > .7) OR (n < sp(i).wide) <> (l > .6) THEN ages = ages + 1
        END IF
    NEXT
NEXT
PRINT "sparks"; seen; bad; ages
END
"""


def sparks(exe: str, tmp: str) -> tuple[int, int]:
    """The sparks drawn alive, those not where moving them put them, or not alive or coloured as it, and those
    whose ages kept at Emit say otherwise than their life left."""
    head, rest = DEMO.read_text().split("\nMain\nEND\n", 1)
    path = Path(tmp) / "SPARKS.BAS"
    path.write_text(head + "\n" + SPARKS + rest)
    out = subprocess.run([exe, path.name], cwd=tmp, capture_output=True, timeout=120).stdout.decode(errors="replace")
    got = out.split("sparks")[-1].split()
    return (int(got[0]), int(got[1]), int(got[2])) if len(got) == 3 else (0, 0, -1)


VINES = """OpenScreen
BuildVines
DIM t AS SINGLE, k AS INTEGER, x AS INTEGER, y AS INTEGER, h AS INTEGER, bad AS LONG, seen AS LONG
DIM px(200 * 112 - 1) AS BYTE
RANDOMIZE 7
FOR k = 1 TO 400
    t = RND * 30000
    h = 20 + INT(RND * 90)
    draw.rect Canvas, 0, 0, 199, h + 1, 0, draw.FILL
    Vines 20, h, t
    VinesRow 120, h, t
    surface.read Canvas, 0, 0, 200, h + 2, px()
    FOR y = 0 TO h + 1
        FOR x = 20 TO 50
            IF px(y * 200 + x) <> px(y * 200 + x + 100) THEN bad = bad + 1
            IF px(y * 200 + x + 100) THEN seen = seen + 1
        NEXT
    NEXT
NEXT
PRINT "vines"; bad; seen
END
"""
VINES_ROW = """
SUB VinesRow (x AS INTEGER, y2 AS INTEGER, t AS SINGLE)
    DIM j AS INTEGER, y AS INTEGER, px AS INTEGER, tb AS SINGLE
    tb = t * 1.7
    FOR j = 0 TO 2
        FOR y = 0 TO y2
            px = x + 5 + j * 9
            px = px + INT(1.5 * SIN(CDBL(tb) + y * CDBL(.16!) + j))
            draw.pset Canvas, px, y, CVINE
            IF (y + j * 2) MOD 6 = 0 THEN
                draw.pset Canvas, px + 1, y, CMOSS
                draw.pset Canvas, px - 1, y + 1, CMOSS
            END IF
        NEXT
    NEXT
END SUB
"""


def vines(exe: str, tmp: str) -> tuple[int, int]:
    """The pixels where Vines' strips and the row by row drawing differ, and the row by row's vine pixels, at 400
    times and heights. The rows' angles are DOUBLEs: in SINGLEs, each row's own rounding at t = 30000 moved 95
    pixels of 104395."""
    head, rest = DEMO.read_text().split("\nMain\nEND\n", 1)
    path = Path(tmp) / "VINES.BAS"
    decl = "DECLARE SUB VinesRow (x AS INTEGER, y2 AS INTEGER, t AS SINGLE)\n"
    path.write_text(decl + head + "\n" + VINES + rest + VINES_ROW)
    out = subprocess.run([exe, path.name], cwd=tmp, capture_output=True, timeout=300).stdout.decode(errors="replace")
    got = out.split("vines")[-1].split()
    return (int(got[0]), int(got[1])) if len(got) == 2 else (-1, 0)


def program() -> str:
    head, rest = DEMO.read_text().split("\nMain\nEND\n", 1)
    return head + "\n" + DRIVER + rest


def main() -> None:
    exe = str(Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build/host/basic").resolve())
    check = subprocess.run([exe, "--check", str(DEMO)], capture_output=True, text=True, timeout=60)
    if check.returncode:
        print("mothlite FAIL\n" + (check.stdout + check.stderr)[-600:])
        sys.exit(1)
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "STONES.BAS"
        path.write_text(program())
        run = subprocess.run([exe, path.name], cwd=tmp, capture_output=True, timeout=60)
        out = run.stdout.decode(errors="replace")
        digest = frames(exe, tmp)
        dacs = palettes(exe, tmp)
        seen, off, ages = sparks(exe, tmp)
        vine, vseen = vines(exe, tmp)
    out += f"\nframes {digest}\npalettes {dacs}\nsparks {off} of {seen} off, {ages} aged otherwise\nvines {vine} of {vseen} off"
    ok = "stones ok" in out and "sounds ok" in out and digest == FRAMES_MD5 and dacs == PALETTES_MD5 and seen > 10000 and off * 1000 < seen and ages == 0 and vseen > 50000 and vine == 0
    print("mothlite " + ("ok" if ok else "FAIL\n" + out[-600:]))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
