"""bench_test.py: bench.py's native cost grows with the work and repeats. Counted from process start,
the loader's ~45M instructions swamped it; under Rosetta the address layout moved a count by a third.
Its DOS profile names static functions: the link map has globals alone, and the lexer's statics
read as lexr_line's, 19% of a load."""

import io
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile
import unittest.mock
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bench


def loop(n: int) -> str:
    return f"FOR i& = 1 TO {n}: a& = a& + 1: NEXT: PRINT a&"


def dos_by_its_own_rules() -> None:
    """A rev's DOS executables made by its own Makefile: ours, its source list a newer tree's, left main's
    gfxdev.c without the pixmem.c it called, and its link failed."""
    calls: list[list[str]] = []

    def run(cmd: list[str], **_: object) -> subprocess.CompletedProcess[str]:
        calls.append(cmd)
        return subprocess.CompletedProcess(cmd, 0, "", "")

    with tempfile.TemporaryDirectory() as d, unittest.mock.patch.object(bench.subprocess, "run", run):
        root = Path(d)
        for src in bench.INSTRUMENT_SRC:
            (root / src).parent.mkdir(parents=True, exist_ok=True)
        bench.Build(root).make(True)
    ours = str(bench.ROOT / "Makefile")
    dos = [c for c in calls if "all" in c]
    assert dos and all(ours not in c for c in dos), calls
    assert all(ours in c for c in calls if "build/host/basic" in c), calls


def profile_cycles() -> None:
    """The profile's cycles: virtual time at dosrun's rate. Instructions alone count a rep as one,
    however many bytes it moves."""
    assert bench.cycles_line(1000) == "20,000,000 cycles: 2.00 s at 10k cycles/ms", bench.cycles_line(1000)


def profile_map() -> None:
    """A static's map entry: its module's base plus its object offset, between the globals around it."""
    lexer = bench.ROOT / "build/c/obj/core/comp/lexer.obj"
    statics = [(off, n) for off, k, n in bench.nm(lexer) if k == "t" and n.startswith("_lexr_")]
    assert statics, "lexer.obj has no static lexr_ function"
    with tempfile.TemporaryDirectory() as d:
        listing = bench.profiled(bench.build(".", True), Path(d)).with_suffix(".map").read_text()
    at = {n: int(a, 16) for a, n in re.findall(r"^0001:([0-9a-f]{8})\*?\s+(\S+)$", listing, re.M)}
    line = next(off for off, k, n in bench.nm(lexer) if n == "_lexr_line")
    for off, n in statics:
        assert at.get(n) == at["_lexr_line"] - line + off, (n, at.get(n))


def profile_map_slice() -> None:
    """gfx's slice's statics are named too, from a directory behind a symlink as tempfile's is on the Mac: the map
    gives each object's path resolved, and every filler read as fill_blend, the global before them."""
    fill = bench.ROOT / "build/c/gfx/stdlib/gfx/src/backend/soft/fill.obj"
    off = {n: o for o, k, n in bench.nm(fill)}
    with tempfile.TemporaryDirectory() as real, tempfile.TemporaryDirectory() as d:
        (Path(d) / "via").symlink_to(real)
        listing = bench.profiled(bench.build(".", True), Path(d) / "via").with_suffix(".map").read_text()
    at = {n: int(a, 16) for a, n in re.findall(r"^0001:([0-9a-f]{8})\*?\s+(\S+)$", listing, re.M)}
    base = at["_slice_fill_argb"] - off["_fill_argb"]
    assert at.get("_slice_fill8_tinted") == base + off["_fill8_tinted"], at.get("_slice_fill8_tinted")


# Counters that fail, as on the CI VMs: the first symbol found wins over the system's.
BROKEN = """
#include <stddef.h>
int thread_selfcounts(int k, void *p, size_t n) { return 1; }
long syscall(long n, ...) { return -1; }
int main(void) { return 0; }
"""


def broken_counter() -> None:
    """A counter that fails reports so: it read 0, and twice that loop's 0 cost divided by zero."""
    with tempfile.TemporaryDirectory() as d:
        exe, stub = Path(d) / "insns", Path(d) / "stub.c"
        stub.write_text(BROKEN)
        cc = os.environ.get("CLANG", "clang")
        subprocess.run(
            [cc, "-std=c23", str(bench.ROOT / "src/plat/host/app/insns.c"), str(stub), "-o", str(exe)], check=True
        )
        err = subprocess.run([str(exe)], env=os.environ | {"QB_INSNS": "1"}, capture_output=True, text=True).stderr
        assert err == "\ninsns unavailable\n", f"a failed counter reported {err!r}"
        try:
            bench.count(exe, stub, "QB_INSNS", b"insns")
        except bench.NoCounter:
            return
        raise AssertionError("bench counted a failed counter")


def demo_colours(b: bench.Build) -> None:
    """Each demo runs at 8 bits: ISLES' &H1E4E8C on an 8-bit surface raised Illegal function call, and every column of
    its table read exit."""
    with tempfile.TemporaryDirectory() as d:
        for n in ("stage.bas", "meter.bas"):
            shutil.copy(bench.DEMOS / n, d)
        for name in bench.DEMO_EDITS:
            (Path(d) / "DEMO.BAS").write_text(bench.demo_program(name, 0.5, False, 0))
            r = subprocess.run([str(b.counted), "DEMO.BAS"], cwd=d, capture_output=True, text=True, timeout=60)
            assert re.fullmatch(r"\s*[\d.]+\s+\d+\s*", r.stdout), f"{name}: {r.stdout}{r.stderr}"


# a label drawn and its surface marked, then drawn again with the same text: the zoom, then the marked pixel's bytes
# as marked and as found after
LABEL_KEPT = """import gfx.display, gfx.draw, gfx.surface
from stage import Stage, measure, label
canvas& = gfx.display.open(640, 480, 32, 0)
dim st as Stage
measure canvas&, st
grey& = gfx.display.rgb(200, 200, 200)
dim px(3) as byte
label canvas&, st.top, 0, 0, "60 fps", grey&, st.zoom
gfx.draw.clear st.top.surface, gfx.display.rgb(255, 0, 0)
gfx.surface.read st.top.surface, 0, 0, 1, 1, px()
print st.zoom; px(0); px(1); px(2); px(3);
label canvas&, st.top, 0, 0, "60 fps", grey&, st.zoom
gfx.surface.read st.top.surface, 0, 0, 1, 1, px()
print px(0); px(1); px(2); px(3)
gfx.display.close
"""


def demo_label_kept(b: bench.Build) -> None:
    """A demo's scaled text keeps its surface while the text stays: made, drawn into and freed each frame, a 3D card
    drew it, read it back and sent it again, and CUBE's text took 78% of a P100's frame on a Voodoo 1."""
    with tempfile.TemporaryDirectory() as d:
        shutil.copy(bench.DEMOS / "stage.bas", d)
        (Path(d) / "KEPT.BAS").write_text(LABEL_KEPT)
        r = subprocess.run([str(b.counted), "KEPT.BAS"], cwd=d, capture_output=True, text=True, timeout=60)
        zoom, *px = r.stdout.split() or ["", ""]
        assert zoom == "2" and len(px) == 8 and px[:4] == px[4:] and px[:4] != ["0"] * 4, f"{r.stdout}{r.stderr}"


def demo_one_world() -> None:
    """demos' runs draw one world and, by frames, one frame n. ISLES seeded its islands and boats from the timer, so
    two builds profiled drew two worlds: a third of one's frame was circles the other never drew."""
    for name in bench.DEMO_EDITS:
        assert "randomize timer" not in bench.demo_program(name, 1, False, 0), name
        framed = bench.demo_program(name, 0, False, 0, frames=5)
        assert "gfx.display.clock" not in framed and "frames& >= 5" in framed, name


def games_meadow16_at_16_bits() -> None:
    """games' MEADOW16 is MEADOW at 16 bits and its own size. Its job gave 16 and True as frames and depth: its screen
    opened at depth True, an Illegal function call on DOS before its first frame."""
    program = bench.game_jobs()["MEADOW16"][0](4)[0]
    assert "const DEPTH = 16\n" in program and "gfx.display.open(W, H, 16, 0)" in program, "not at 16 bits"
    assert "const W = 640\n" in program and "(frames& / 30#)" not in program, "not at its size, or by frames"


def frame_profile_unseen() -> None:
    """A frame's profile shows the instructions no function was seen running. On DOSBox-X's Voodoo 2, 2.7M of a
    frame's 3.3M ran where its profiler named none, and the table read as if the frame were BASIC's and gfx's."""
    runs = [
        (bench.dosrun.Result("demo", "", "exit", 100, 10, selfs={"f": 50}), 1),
        (bench.dosrun.Result("demo", "", "exit", 1100, 20, selfs={"f": 250}), 11),
    ]
    with (
        unittest.mock.patch.object(bench, "dos_profile", side_effect=runs),
        unittest.mock.patch("sys.stdout", new_callable=io.StringIO) as out,
    ):
        bench.print_frame_profile("t", Path("t.exe"), [(), ()], 1000, {}, 5)
    unseen = [line for line in out.getvalue().splitlines() if "no function" in line]
    assert len(unseen) == 1 and unseen[0].split()[:2] == ["80", "80.0%"], out.getvalue()


def fps_start_up() -> None:
    """fps --profile's baseline is the start-up alone: one frame logged. Without it the frames' profile held the
    start-up's sprites and tables, a third of a run (STR$'s digits 1.3M calls that no frame makes)."""
    with tempfile.TemporaryDirectory() as d:
        (Path(d) / "M.BAS").write_text(bench.fps_program(one=True), encoding="latin-1")
        subprocess.run([str(bench.ROOT / "build/host/basic"), "M.BAS"], cwd=d, stdin=subprocess.DEVNULL,
                       capture_output=True, timeout=120, env=os.environ | {"SDL_VIDEODRIVER": "dummy"})
        rows = (Path(d) / "FPS.TXT").read_text().split("\n")
        assert len([r for r in rows if r.strip()]) == 1, rows[:3]


def profile_stop() -> None:
    """A profiled program that stops on an error says so. Out of memory in DOSBox's 4 MB, a program's
    profile read as its compile alone: 41.8M instructions, no line of it run."""
    oom = bench.dosrun.Result("prog", "Out of memory in line 12", "exit", 0, 0)
    assert "Out of memory" in bench.stopped(oom), bench.stopped(oom)
    assert bench.stopped(bench.dosrun.Result("prog", "", "exit", 0, 0)) == ""


def each_reps() -> None:
    """fps --each reads TIMER once per EACH_REPS calls. Read once a call, its millisecond on DOS rounded each
    call's cost: DrawMoth read 0 ms, the rest whole ms."""
    loops = bench.each_program().split("\nDO\n")[1:]
    assert len(loops) == 1 + len(bench.FPS_EACH), len(loops)
    for body in loops:
        body = body.split("\nLOOP UNTIL")[0]
        assert body.count("TIMER") == 0 and f"FOR q = 1 TO {bench.EACH_REPS}" in body, body


def charlie_each() -> None:
    """bench.py charlie's program is the game as it is written now, every call timed. Made for an upper-case END
    and a LoadLevel without its level, it found no Main in the game and stopped before a single measurement."""
    with tempfile.TemporaryDirectory() as d:
        (Path(d) / "CHARLIE.DAT").write_bytes(bench.CHARLIE.with_suffix(".DAT").read_bytes())
        for package in ("barkley", "engine"):
            shutil.copytree(bench.CHARLIE.with_name(package), Path(d) / package)
        (Path(d) / "C.BAS").write_text(bench.charlie_program().replace("TIMER - t0 >= 2", "TIMER - t0 >= 0"))
        subprocess.run([str(bench.ROOT / "build/host/basic"), "C.BAS"], cwd=d, stdin=subprocess.DEVNULL,
                       capture_output=True, timeout=120, env=os.environ | {"SDL_VIDEODRIVER": "dummy"})
        log = Path(d) / "EACH.TXT"
        rows = [r for r in log.read_text().split("\n") if r.strip()] if log.exists() else []
        assert len(rows) == 1 + len(bench.CHARLIE_EACH), rows[:3]


def charlie_think() -> None:
    """The think count is the cases of the things' own rules' switch on a kind. Once the things' pass switched on
    their kind's mind it still read the pass, and counted 0 of 14."""
    think = bench.CHARLIE.with_name("barkley") / "stage/things/think.bas"
    body = think.read_text().split("private sub Think (")[1].split("\nend sub")[0]
    want = len(re.findall(r"(?m)^    case K", body))
    got = bench.engine_measures()["`case` branches that name a kind: draw, touch, think"].split(", ")[2]
    assert want and got == str(want), f"think counted {got} of {want}"


KIND_READS = """type Thing
    kind as integer
end type
sub S (t as Thing)
    dim k as integer, b as integer
    k = t.kind
    if t.kind = 3 then k = 1
    b = t.kind = 4
    t.kind = 2
end sub
"""


def kind_compares() -> None:
    """A kind read into a variable is not compared: counted as compared, the reads in DrawThings, Touch and
    ResetWorld raised the Today table's comparisons by three."""
    prog = bench.basic_src.Program({Path("k.bas"): KIND_READS})
    mod = prog.modules[0]
    got = [bench.kind_compares(prog, mod, st) for st in mod.stmts]
    assert sum(got) == 2, f"{sum(got)} comparisons of a kind, of 2: {got}"


# Instructions a pass on arm64 macOS, with room over what they measured; past one, a regression.
# A string's allocation over a literal's store: strg_gc inlined into strg_alloc saved and set up
# its registers on every allocation, 238 (217 out of line). The collector asking the stack whether
# each dead block's owner was popped before checking its pointer, 251.
ALLOC_BUDGET = 228
# A SUB with a fixed local array over an empty SUB: its return looped over its locals and swept
# the arrays, 457 (402 by the inline PRET).
FRAME_ARRAY_BUDGET = 430
# A SUB DIMming a dynamic local array over one that does not: on the heap, made and swept at its
# return, 900 (850 on main); 536 on its call's arena.
DYN_ARRAY_BUDGET = 590
# A DOUBLE function's pass, its op a cold one, a million passes for a tenth of an instruction: back
# through next's join every op copied ip through another register, 101 (100 on main); 92 without.
COLD_OP_BUDGET = 94
# A read of a record's dynamic array field's element over a literal's store: through CALL FELEM and
# arry_ref, 173; 66 by FE.ADDR from its header.
FIELD_ELEM_BUDGET = 72
# A read of a record's fixed array field's element by a variable subscript over the subscript's
# store: its bounds pushed and CALL FIDX, 104, more than a dynamic one's; 58 by FX.IDX inline.
FIXED_ELEM_BUDGET = 64


def per_pass(b: bench.Build, body: str, setup: str = "", tail: str = "", n: int = 100000) -> float:
    """body's instructions a pass of a FOR loop: the loop run n and 2n times, the difference."""
    progs = {k: f"{setup}\nFOR i& = 1 TO {m}: {body}: NEXT\n{tail}" for k, m in (("one", n), ("two", 2 * n))}
    r = bench.run_native(b, progs, runs=3)
    return (r["two"].insns - r["one"].insns) / n


def costs(b: bench.Build) -> None:
    """The budgets: each part's instructions over its base's."""
    alloc = per_pass(b, 's$ = a$ + "x"', 'a$ = "abcdefgh"') - per_pass(b, 's$ = "abc"', 'a$ = "abcdefgh"')
    assert alloc <= ALLOC_BUDGET, f"a string's allocation {alloc:.0f} instructions, budget {ALLOC_BUDGET}"
    frame = per_pass(b, "s", tail="SUB s\nDIM b(10)\nEND SUB") - per_pass(b, "z", tail="SUB z\nEND SUB")
    assert frame <= FRAME_ARRAY_BUDGET, f"a frame array's call {frame:.0f} instructions, budget {FRAME_ARRAY_BUDGET}"
    dyn = per_pass(b, "s 10", tail="SUB s (n)\nDIM b(n)\nEND SUB") - per_pass(b, "z 10", tail="SUB z (n)\nEND SUB")
    assert dyn <= DYN_ARRAY_BUDGET, f"a dynamic local array's call {dyn:.0f} instructions, budget {DYN_ARRAY_BUDGET}"
    cold = per_pass(b, "y# = ABS(i&)", n=1000000)
    assert cold <= COLD_OP_BUDGET, f"a cold op's pass {cold:.0f} instructions, budget {COLD_OP_BUDGET}"
    rec = "TYPE R\n w AS INTEGER\n a() AS INTEGER\nEND TYPE\nDIM r AS R\nREDIM r.a(10)"
    elem = per_pass(b, "x% = r.a(5)", rec, n=1000000) - per_pass(b, "x% = 0", rec, n=1000000)
    assert elem <= FIELD_ELEM_BUDGET, f"a record's array element {elem:.0f} instructions, budget {FIELD_ELEM_BUDGET}"
    fix = "TYPE R\n w AS INTEGER\n f(10) AS INTEGER\nEND TYPE\nDIM r AS R\nk% = 5"
    fixed = per_pass(b, "x% = r.f(k%)", fix, n=1000000) - per_pass(b, "x% = k%", fix, n=1000000)
    assert fixed <= FIXED_ELEM_BUDGET, f"a fixed field's element {fixed:.0f} instructions, budget {FIXED_ELEM_BUDGET}"


def quickr_ops(b: bench.Build) -> None:
    """Each Quickr bench case (tools/bench/qr) runs no more ops than its QBasic twin, a set-up op or
    two aside, and prints the same values. Unfused, its for loop not I.NEXT, and a fn's result a
    frame cell, it ran 2.1x QBasic's ops on loop and 1.9x on fib."""
    cases = sorted((bench.QR / "quickr").glob("*.bas"))
    qr = bench.run_natives([b], {p.stem: p.read_text() for p in cases}, runs=1, quickr=True)[0]
    qb = bench.run_native(b, {p.stem: (bench.QR / p.name).read_text() for p in cases}, runs=1)
    for k, q in qr.items():
        assert q.out.split() == qb[k].out.split(), (k, q.out, qb[k].out)
        assert q.ops <= qb[k].ops * 1.001, f"{k}: Quickr's {q.ops:,} ops, QBasic's {qb[k].ops:,}"


def quickr_push(b: bench.Build) -> None:
    """A list's push is amortized O(1): each doubling of the pushes costs about twice the last. Grown by
    one element a push (REDIM PRESERVE's way), it cost 4.0x. Differences, as the bench subtracts a
    baseline of its own: raw counts at 8000 pushes read 2.45x, the offset not the growth. A run's count
    swings by 1M, so n large enough to drown that."""
    prog = bench.QUICKR + "var a as int[]\nfor i in 0..{n}\n    a.push(i)\nnext\nprint len(a)\n"
    progs = {str(n): prog.format(n=n) for n in (32000, 64000, 128000)}
    r = bench.run_natives([b], progs, runs=3, quickr=True)[0]
    ratio = (r["128000"].insns - r["64000"].insns) / (r["64000"].insns - r["32000"].insns)
    assert ratio < 2.3, f"doubling the pushes cost {ratio:.2f}x the last doubling"


def stdlib_absent() -> None:
    """A rev before stdlib/ builds: cp with no files failed, so cmp and size couldn't build main."""
    with tempfile.TemporaryDirectory() as d:
        p = subprocess.run(
            ["make", "-C", d, "-f", str(bench.ROOT / "Makefile"), "build/t/lib/.stdlib"], capture_output=True, text=True
        )
        assert p.returncode == 0 and (Path(d) / "build/t/lib/.stdlib").exists(), p.stderr


def vm_rate() -> None:
    """A demo's rate on 86Box: DOS/32A's warning, no mouse driver, came first in its output and read as
    no rate at all."""
    out = "DOS/32A warning (9004): mouse initialization failed\r\n 42.93390490470216  131 \r\n"
    assert bench.rate_of("", out) == "42.9", bench.rate_of("", out)
    assert bench.rate_of("51.23 256 1 9000\r\n", out) == "51.2"
    assert bench.rate_of("", "Out of memory\r\n") == ""


def vm_object_base() -> None:
    """The program's place in a VM's memory, found by its code: a fixup changes the bytes it patches,
    so the code's first window, patched, found nothing, and the profile mapped no sample."""
    code = random.Random(1).randbytes(9000)
    memory = bytearray(b"\0" * 70000) + bytearray(code) + bytearray(b"\0" * 5000)
    memory[70000 + 4 : 70000 + 8] = b"\xff\xff\xff\xff"  # a fixup in the first window
    assert bench.object_base(bytes(memory), code) == 70000, bench.object_base(bytes(memory), code)


def vm_window() -> None:
    """A VM's profile samples the job alone. It sampled from boot until the host stopped it, so the host's delay in
    seeing the end read as frame time: one build's ISLES on the 486, 14.8M cycles a frame, then 11.9M."""
    b = bench.build(".", True)
    own = {n: (bench.DEMOS / n).read_bytes() for n in ("stage.bas", "meter.bas")}
    job = (bench.demo_program("ISLES", 0, False, 0, frames=2), "", "", own)
    lib = {"lib/gfx.qbx": (b.dos.parent / "lib/gfx.qbx").read_bytes()}
    with tempfile.TemporaryDirectory() as d:
        exe = bench.profiled(b, Path(d))
        vm = bench.MACHINES["486"]
        soon, late = (sum(bench.vm_samples(vm, exe, job, lib, s).by.values()) for s in (0.0, 2.0))
    assert abs(late - soon) < soon * 0.001, (soon, late)


def main() -> int:
    dos_by_its_own_rules()
    stdlib_absent()
    each_reps()
    demo_one_world()
    games_meadow16_at_16_bits()
    frame_profile_unseen()
    charlie_each()
    charlie_think()
    kind_compares()
    profile_cycles()
    profile_stop()
    broken_counter()
    fps_start_up()
    vm_rate()
    vm_object_base()
    if (bench.HL86 / "build/src/86Box-headless").exists():
        vm_window()
    b = bench.build(".", False)  # the tree's own instruments: copying them onto themselves raised
    demo_colours(b)
    demo_label_kept(b)
    try:
        r = bench.run_native(b, {"one": loop(1000000), "two": loop(2000000)}, runs=5)
    except bench.NoCounter:
        print("skipped: no instruction counter")
        return 0
    ratio = r["two"].insns / r["one"].insns
    assert (r["one"].out, r["two"].out) == ("1000000", "2000000"), r
    assert 1.95 < ratio < 2.05, f"twice the loop cost {ratio:.3f}x"
    again = [bench.run_native(b, {"two": loop(2000000)}, runs=1)["two"].insns for _ in range(5)]
    assert max(again) / min(again) < 1 + bench.INSNS_SLACK, f"one program's counts {again}"
    costs(b)
    quickr_ops(b)
    quickr_push(b)
    exe = bench.ROOT / "build/c/basic32.exe"
    if exe.exists():  # the loaded bytes, not the file's: its fixups moved with layout
        assert bench.image(exe) > 100_000 and bench.image(exe) != exe.stat().st_size, bench.image(exe)
        profile_map()
        profile_map_slice()
    print("ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
