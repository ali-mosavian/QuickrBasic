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
full at every step, each entry set by PALETTE.
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
BuildMoth
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

# The frames' MD5, from the drawing at 66c90a5 (seeded alike).
FRAMES_MD5 = "ca7941b583e76e4b7fb124d023276d12"


def frames_program() -> str:
    head, rest = DEMO.read_text().split("\nMain\nEND\n", 1)
    rest = rest.replace("RANDOMIZE TIMER", "RANDOMIZE 5").replace(
        "    Flip w.page\n", "    Dump\n    Flip w.page\n"
    )
    dump = "\nSUB Dump\n    DIM a(8001) AS LONG, y AS INTEGER, i AS INTEGER\n    FOR y = 0 TO 100 STEP 100\n"
    dump += "        GET (0, y)-(319, y + 99), a\n        FOR i = 1 TO 8000: PUT #1, , a(i): NEXT\n    NEXT\nEND SUB\n"
    return "DECLARE SUB Dump ()\n" + head + "\n" + FRAMES + rest + dump


def frames(exe: str, tmp: str) -> str:
    path = Path(tmp) / "FRAMES.BAS"
    path.write_text(frames_program())
    run = subprocess.run([exe, path.name], cwd=tmp, capture_output=True, timeout=120)
    if "frames ok" not in run.stdout.decode(errors="replace"):
        return "FAIL " + (run.stdout + run.stderr).decode(errors="replace")[-600:]
    return hashlib.md5((Path(tmp) / "FRAMES.BIN").read_bytes()).hexdigest()


PALETTES = """OpenScreen
LoadTables
DIM w AS World, cur(NKEY - 1, 2) AS SINGLE, p AS INTEGER, n AS INTEGER, q AS INTEGER
OPEN "PAL.BIN" FOR BINARY AS #1
NewWorld w, cur()
FOR p = 0 TO 2 * NSKIES
    w.sky.phase = p MOD NSKIES
    IF p = NSKIES THEN w.sky.fadeTo = 0
    IF p = NSKIES + 1 THEN w.sky.fadeTo = 1
    FOR n = 1 TO 240
        EaseSky w.sky, cur()
        IF n MOD 4 = 0 THEN
            FOR q = 0 TO 255: PUT #1, , Dacs(q): NEXT
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
    assert "PALETTE i, " in rest
    rest = rest.replace("PALETTE i, ", "SetDac i, ")
    sub = "\nSUB SetDac (i AS INTEGER, d AS LONG)\n    Dacs(i) = d\n    PALETTE i, d\nEND SUB\n"
    path = Path(tmp) / "PAL.BAS"
    decl = "DECLARE SUB SetDac (i AS INTEGER, d AS LONG)\nDIM SHARED Dacs(255) AS LONG\n"
    path.write_text(head.replace("\nDECLARE ", "\n" + decl + "DECLARE ", 1) + "\n" + PALETTES + rest + sub)
    run = subprocess.run([exe, path.name], cwd=tmp, capture_output=True, timeout=120)
    if "palettes ok" not in run.stdout.decode(errors="replace"):
        return "FAIL " + (run.stdout + run.stderr).decode(errors="replace")[-600:]
    return hashlib.md5((Path(tmp) / "PAL.BIN").read_bytes()).hexdigest()


SPARKS = """LoadTables
SparkPaths
DIM sp(NSPARK - 1) AS Spark, rf(NSPARK - 1, 4) AS SINGLE, m AS Moth
DIM t AS INTEGER, i AS INTEGER, j AS INTEGER, l AS SINGLE, n AS INTEGER
DIM seen AS LONG, bad AS LONG
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
        END IF
    NEXT
NEXT
PRINT "sparks"; seen; bad
END
"""


def sparks(exe: str, tmp: str) -> tuple[int, int]:
    """The sparks drawn alive, and those not where moving them put them, or not alive or coloured as it."""
    head, rest = DEMO.read_text().split("\nMain\nEND\n", 1)
    path = Path(tmp) / "SPARKS.BAS"
    path.write_text(head + "\n" + SPARKS + rest)
    out = subprocess.run([exe, path.name], cwd=tmp, capture_output=True, timeout=120).stdout.decode(errors="replace")
    got = out.split("sparks")[-1].split()
    return (int(got[0]), int(got[1])) if len(got) == 2 else (0, 0)


VINES = """OpenScreen
DIM t AS SINGLE, k AS INTEGER, x AS INTEGER, y AS INTEGER, h AS INTEGER, bad AS LONG
RANDOMIZE 7
FOR k = 1 TO 400
    t = RND * 9000
    h = 20 + INT(RND * 90)
    LINE (0, 0)-(99, h + 1), 0, BF
    LINE (100, 0)-(199, h + 1), 0, BF
    Vines 20, h, t
    VinesRow 120, h, t
    FOR y = 0 TO h + 1
        FOR x = 20 TO 50
            IF POINT(x, y) <> POINT(x + 100, y) THEN bad = bad + 1
        NEXT
    NEXT
NEXT
PRINT "vines"; bad
END
"""
VINES_ROW = """
SUB VinesRow (x AS INTEGER, y2 AS INTEGER, t AS SINGLE)
    DIM j AS INTEGER, y AS INTEGER, px AS INTEGER
    FOR j = 0 TO 2
        FOR y = 0 TO y2
            px = x + 5 + j * 9
            px = px + INT(1.5 * SIN(t * 1.7 + y * .16 + j))
            PSET (px, y), CVINE
            IF (y + j * 2) MOD 6 = 0 THEN
                PSET (px + 1, y), CMOSS
                PSET (px - 1, y + 1), CMOSS
            END IF
        NEXT
    NEXT
END SUB
"""


def vines(exe: str, tmp: str) -> int:
    """The pixels where Vines' runs and the row by row drawing it replaced differ, at 400 times and heights."""
    head, rest = DEMO.read_text().split("\nMain\nEND\n", 1)
    path = Path(tmp) / "VINES.BAS"
    decl = "DECLARE SUB VinesRow (x AS INTEGER, y2 AS INTEGER, t AS SINGLE)\n"
    path.write_text(decl + head + "\n" + VINES + rest + VINES_ROW)
    out = subprocess.run([exe, path.name], cwd=tmp, capture_output=True, timeout=300).stdout.decode(errors="replace")
    got = out.split("vines")[-1].split()
    return int(got[0]) if len(got) == 1 else -1


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
        seen, off = sparks(exe, tmp)
        vine = vines(exe, tmp)
    out += f"\nframes {digest}\npalettes {dacs}\nsparks {off} of {seen} off\nvines {vine} off"
    ok = "stones ok" in out and "sounds ok" in out and digest == FRAMES_MD5 and dacs == PALETTES_MD5 and seen > 10000 and off * 1000 < seen and vine == 0
    print("mothlite " + ("ok" if ok else "FAIL\n" + out[-600:]))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
