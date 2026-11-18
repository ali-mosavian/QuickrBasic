#!/usr/bin/env python3
"""rustline_test.py: RUSTLINE's PLAY strings stay in the octaves PLAY takes; its effects stay on the synth.

"O7" in the spire's shot stopped the game with "Illegal function call" the first time a spire in
sight fired; the same in the battle tune's hats stopped it on the first bar.
"""

import re
import sys
from itertools import pairwise
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # run with -I too
import bench

BAS = Path(__file__).resolve().parent.parent / "src/plat/wasm/web/demos/RUSTLINE.BAS"


def play_strings(src: str) -> list[str]:
    tunes = src.split("\nTunes:\n")[1].split("\nREM $STATIC")[0]
    lines = [x for x in src.splitlines() if re.search(r"\bPLAY\b", x)] + tunes.splitlines()
    return [s for x in lines for s in re.findall(r'"([^"]*)"', x)]


def battle_rows(src: str) -> list[list[str]]:
    """The battle tune's beats: the Tunes rows of four strings (bass, lead, stabs, drums)."""
    tunes = src.split("\nTunes:\n")[1].split("\nREM $STATIC")[0]
    rows = [re.findall(r'"([^"]*)"', x) for x in tunes.splitlines()]
    return [r for r in rows if len(r) == 4]


def test_octaves() -> None:
    bad = [s for s in play_strings(BAS.read_text()) if re.search(r"O[7-9]", s)]
    assert not bad, bad


def test_effects_on_synth() -> None:
    """The harvester's unload beep, a speaker SOUND, silenced the battle tune for 2.5 s an unload."""
    sfx = re.search(r"^SUB Sfx .*?^END SUB", BAS.read_text(), re.M | re.S).group(0)
    assert "SOUND" not in sfx, "a speaker SOUND in Sfx"
    bad = [s for x in sfx.splitlines() if "PLAY " in x for s in re.findall(r'"([^"]*)"', x) if s and "W" not in s]
    assert not bad, bad


def test_battle_bass_is_low() -> None:
    """The battle tune's bass ran at 220 to 440 Hz, under 1% of the mix below 120 Hz: it sounded thin."""
    rows = battle_rows(BAS.read_text())
    bad = [r[0] for r in rows if re.search(r"O[3-6]", r[0])]
    assert not bad, bad[:2]


def whole_notes(tune: str) -> float:
    """A PLAY string's length in whole notes (every note and rest carries its own length)."""
    return sum(1.5 ** bool(d) / int(n) for n, d in re.findall(r"[A-GP][#+-]?(\d+)(\.?)", re.sub(r"O\d", "", tune)))


def test_battle_bars_are_whole() -> None:
    """A voice whose bar (4 rows, a beat each) is not a whole note drifts off the others."""
    rows = battle_rows(BAS.read_text())
    beats = [r for r in rows if r[0] and not r[0].startswith("T")]
    bars = [[sum(whole_notes(r[v]) for r in beats[i : i + 4]) for v in range(3)] for i in range(0, len(beats), 4)]
    bad = [b for b in bars if any(abs(w - 1) > 1e-9 for w in b)]
    assert not bad, bad[:2]


def notes(tune: str) -> int:
    """The notes and rests a PLAY string queues."""
    return len(re.findall(r"[A-GP][#+-]?\d*\.?", re.sub(r"[WHYQVTOL][\d,]*|M[NLSBF]", "", tune)))


def test_battle_never_waits() -> None:
    """A voice queues 18 notes before PLAY waits: 24-note drum bars stalled the game 0.2-0.8 s a bar.

    A beat is queued while the one before it still plays, so the two together must fit; and the beat is
    queued by voice 1's queue, PLAY(0), not the program's clock, which ran 7% ahead of the synth's and
    stalled every PLAY 0.1-0.36 s once the queues filled."""
    rows = [r for r in battle_rows(BAS.read_text()) if r[0] and not r[0].startswith("T")]
    bad = [(a[v], b[v]) for a, b in pairwise(rows) for v in range(4) if notes(a[v]) + notes(b[v]) > 18]
    assert not bad, bad[:1]
    music = re.search(r"^SUB Music .*?^END SUB", BAS.read_text(), re.M | re.S).group(0)
    assert "g.clock" not in music and "IF PLAY(0) = 0 THEN" in music, "battle music paced by the clock"
    assert all(r[0] for r in rows), "a beat without bass: PLAY(0) cannot pace it"


def test_map_fits_its_size() -> None:
    """MapData and HeightData are MAPW by MAPH: the 64 by 48 map was written while the game read 40 by 30."""
    src = BAS.read_text()
    w, h = map(int, re.search(r"CONST MAPW = (\d+), MAPH = (\d+)", src).groups())
    for label in ("MapData", "HeightData"):
        rows = re.findall(r'^DATA "([^"]*)"$', src.split(f"\n{label}:\n")[1].split("\n\n")[0], re.M)
        assert len(rows) == h and {len(r) for r in rows} == {w}, (label, len(rows), {len(r) for r in rows})


def test_explored_ground_not_dark() -> None:
    """Ground the Coalition has seen but no longer sees was drawn under fog; it stays lit, only the unseen is dark."""
    assert not re.search(r"\.vis = 0 THEN (PUT|c = FOG)", BAS.read_text())


def field_program(w: int, h: int, builds: int, bas: str = "", body: str = "") -> str:
    """RUSTLINE on a w by h map (its own map tiled) building the ore route map `builds` times; it
    prints the map's sum, to compare. A `body` runs instead, on the map loaded."""
    src = re.sub(r"CONST MAPW = \d+, MAPH = \d+", f"CONST MAPW = {w}, MAPH = {h}", bas or BAS.read_text())
    src = re.sub(r"CONST (.*)\bNF = \d+", r"CONST \1NF = 1", src)
    main = f"""DIM mp(47) AS STRING, hd(47) AS STRING, r AS STRING, c AS STRING, x AS INTEGER, y AS INTEGER
RESTORE MapData: FOR y = 0 TO 47: READ mp(y): NEXT
RESTORE HeightData: FOR y = 0 TO 47: READ hd(y): NEXT
FOR y = 0 TO MAPH - 1
    FOR x = 0 TO MAPW - 1
        c = MID$(mp(y MOD 48), x MOD 64 + 1, 1)
        m(x, y).terr = TGROUND
        IF c = "#" THEN m(x, y).terr = TROCK
        IF c = "~" THEN m(x, y).terr = TWATER
        IF c = "m" THEN m(x, y).terr = TORE
        IF c = "T" THEN m(x, y).terr = TTREE
        c = MID$(hd(y MOD 48), x MOD 64 + 1, 1)
        IF c >= "a" THEN
            m(x, y).hgt = ASC(c) - 97: m(x, y).rmp = 1
        ELSEIF c >= "A" THEN
            m(x, y).hgt = ASC(c) - 65: m(x, y).rmp = 2
        ELSE
            m(x, y).hgt = VAL(c)
        END IF
    NEXT
NEXT
{"WalkBits m()" if "SUB WalkBits" in src else ""}
{"StepMask m(), 0, 0, MAPW - 1, MAPH - 1" if "SUB StepMask" in src else ""}
{
        body
        or f'''FOR x = 1 TO {builds}: BuildField m(), fd(), 0, OREKEY: NEXT
DIM sum AS LONG
FOR y = 0 TO MAPH - 1: FOR x = 0 TO MAPW - 1: sum = sum + fd(0, x, y): NEXT: NEXT
PRINT "built"; sum'''
    }
"""
    i, j = src.index("\nSetup g, tune()\n") + 1, src.index("\nWIDTH 80\nEND\n") + len("\nWIDTH 80\nEND\n")
    return src[:i] + main + "END\n" + src[j:]


def field_cost(w: int, h: int, bas: str = "") -> bench.Cost:
    """One ore route map's cost on a w by h map: 3 builds less 1, halved."""
    progs = {"one": field_program(w, h, 1, bas), "three": field_program(w, h, 3, bas)}
    r = bench.run_native(bench.build(".", False), progs, runs=1)
    return bench.Cost("", (r["three"].insns - r["one"].insns) // 2, (r["three"].ops - r["one"].ops) // 2)


def test_route_map_cost() -> None:
    """A route map took 4,700 VM ops a tile, Walk% working out each step's corners: 27 ms on 64 by 48,
    a hitch on every new order. The headings each tile's ground runs on to are worked out once a game;
    the steps open now are cached a tile (StepMask), and the eight headings written out: 130 ops."""
    c = field_cost(64, 48)
    assert c.ops / (64 * 48) < 200, c.ops / (64 * 48)


def test_step_mask_follows_buildings() -> None:
    """Each tile's step mask is cached; a building placed or destroyed must redo the masks round it, or
    units walk through new buildings and route maps go round dead ones."""
    body = """DIM b AS INTEGER, s AS INTEGER, d AS INTEGER, bad AS INTEGER
x = 20: y = 30
PlaceBuilding g, tm(), m(), u(), bl(), fk(), 0, -1, x, y
GOSUB Compare
b = m(x, y).bld
KillBuilding g, m(), bl(), fx(), fk(), b
GOSUB Compare
PRINT "stale"; bad
END
Compare:
FOR y = 0 TO MAPH - 1
    FOR x = 0 TO MAPW - 1
        s = 0
        FOR d = 0 TO 7: IF Walk%(m(), x, y, d) THEN s = s OR 2 ^ d
        NEXT
        IF s <> m(x, y).sk THEN bad = bad + 1
    NEXT
NEXT
x = 20: y = 30
RETURN"""
    r = bench.run_native(bench.build(".", False), {"mask": field_program(64, 48, 0, body=body)}, runs=1)
    assert r["mask"].out.split() == ["stale", "0"], r["mask"].out


# the frame's parts profile_program times: a label, then the statement it wraps (once in the game)
PARTS = (
    ("keys", "    Keys g, tm(), m(), u(), bl(), fx(), fk(), art(), gl()\n"),
    ("controls", "        Controls g, tm(), m(), u(), bl(), fx(), fk(), dt\n"),
    ("produce", "    Produce g, tm(), m(), u(), bl(), 0, dt\n    Produce g, tm(), m(), u(), bl(), 1, dt\n"),
    ("routes", "    BuildField m(), fd(), old, goal\n"),
    ("units", "        IF u(i).alive THEN UnitTick g, tm(), m(), u(), bl(), fx(), fd(), fk(), i, dt\n"),
    ("fxtick", "    FxTick fx(), dt\n"),
    ("ai", "    IF g.ai.think <= 0 THEN g.ai.think = 1: AIThink g, tm(), m(), u(), bl(), fk()\n"),
    ("vis", "    IF g.visT <= 0 THEN g.visT = .25: UpdateVis g, m(), u(), bl()\n"),
    ("objects", "    DrawObjects g, m(), u(), bl(), art(), tx(), ty(), nt\n"),
    ("fx", "    DrawFx g, m(), fx()\n"),
    ("minimap", "    DrawMinimap g, m(), u(), bl(), art()\n"),
    ("sidebar", "    DrawSidebar g, tm(), m(), u(), bl(), art(), gl()\n"),
    ("render", "        IF g.state = GPLAY THEN RenderGame g, tm(), m(), u(), bl(), fx(), art(), gl()\n"),
    ("music", "    Music g, tune()\n"),
)


def profile_program(w: int, h: int, until: float, nf: int = 24) -> str:
    """RUSTLINE on a w by h map (its own tiled), uncapped, timing PARTS until game second `until`
    and writing them to PROF.TXT. TIMER steps 10 ms; summed over thousands of calls out of step with
    it, a part's total is its share."""
    src = re.sub(r"CONST MAPW = \d+, MAPH = \d+", f"CONST MAPW = {w}, MAPH = {h}", BAS.read_text())
    msc = max(-(-w // 72), -(-h // 57))  # the minimap fits the sidebar above its buttons
    src = src.replace("CONST MSC = 1,", f"CONST MSC = {msc},")
    src = src.replace("ABACK = AMINI + 19,", f"ABACK = AMINI + {w * h // msc // msc // 260 + 2},")
    src = src.replace("    DO: t = TIMER: LOOP WHILE t - lastT < .015 AND t >= lastT\n", "    t = TIMER\n")
    # a tick each 4th frame, not each TIMER step: started just after a step, a part shorter than
    # the step reads 0
    src = src.replace("    dt = t - lastT\n", "    dt = t - lastT: IF g.state = GPLAY THEN dt = STEPT / 4\n", 1)
    src = re.sub(r"\bNF = \d+", f"NF = {nf}", src, count=1)
    i = src.index("\nSUB NewGame ")
    j = src.index("\nEND SUB\n", i)
    ng = src[i:j]
    rd = "    FOR y = 0 TO 47: READ {}(y): NEXT\n"
    ng = ng.replace(
        "    RESTORE MapData\n",
        "    DIM mp(47) AS STRING, hd(47) AS STRING\n    RESTORE MapData\n"
        + rd.format("mp")
        + "    RESTORE HeightData\n"
        + rd.format("hd"),
        1,
    )
    ng = ng.replace("        READ r\n", "        r = mp(y MOD 48)\n", 1).replace(
        "        READ r\n", "        r = hd(y MOD 48)\n", 1
    )
    ng = ng.replace("MID$(r, x + 1, 1)", "MID$(r, x MOD 64 + 1, 1)")
    src = src[:i] + ng + src[j:]
    for k, (_, stmt) in enumerate(PARTS):
        assert src.count(stmt) == 1, stmt
        pad = stmt[: len(stmt) - len(stmt.lstrip())]
        src = src.replace(
            stmt, f"{pad}zq# = TIMER\n{stmt}{pad}zt({k}) = zt({k}) + TIMER - zq#: zc&({k}) = zc&({k}) + 1\n"
        )
    names = ", ".join(f'"{n}"' for n, _ in PARTS)
    # written at game second `until`, or at the game's end if sooner
    report = f"""    IF g.state = GPLAY THEN
        zFrames& = zFrames& + 1
        IF zFrames& = 1 THEN zT0# = TIMER
    END IF
    IF zFrames& > 0 AND (g.now > {until} OR g.state <> GPLAY) AND zDone = 0 THEN
        zDone = 1: zWall# = TIMER - zT0#
        RESTORE ZNames
        OPEN "PROF.TXT" FOR OUTPUT AS #9
        PRINT #9, "frames"; zFrames&; "wall"; zWall#; "game"; g.now
        FOR zk = 0 TO {len(PARTS) - 1}: READ zn$: PRINT #9, zn$; zt(zk); zc&(zk): NEXT
        CLOSE #9
    END IF
LOOP UNTIL g.quitting"""
    src = src.replace("LOOP UNTIL g.quitting", report, 1)
    src = src.replace("'$DYNAMIC\n", f"'$DYNAMIC\nDIM SHARED zt({len(PARTS)}) AS DOUBLE, zc&({len(PARTS)})\n", 1)
    return src + f"\nZNames:\nDATA {names}\n"


if __name__ == "__main__" and sys.argv[1:2] == ["profile"]:
    w, h, until, nf = int(sys.argv[2]), int(sys.argv[3]), float(sys.argv[4]), int(sys.argv[5])
    Path(sys.argv[6]).write_text(profile_program(w, h, until, nf))
    sys.exit()

if __name__ == "__main__" and sys.argv[1:] == ["fields"]:
    for w, h in ((32, 24), (64, 48), (96, 72), (128, 96)):
        c = field_cost(w, h)
        per = c.ops / (w * h)
        print(f"{w:>3} x {h:<3} {c.insns / 1e6:8.1f} M insns  {c.ops / 1e6:6.2f} M ops  {per:6.0f} ops a tile")
    sys.exit()

if __name__ == "__main__":
    test_octaves()
    test_effects_on_synth()
    test_battle_bass_is_low()
    test_battle_bars_are_whole()
    test_battle_never_waits()
    test_map_fits_its_size()
    test_explored_ground_not_dark()
    test_route_map_cost()
    test_step_mask_follows_buildings()
    print("ok")
