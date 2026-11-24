"""bench_test.py: bench.py's native cost grows with the work and repeats. Counted from process start,
the loader's ~45M instructions swamped it; under Rosetta the address layout moved a count by a third.
Its DOS profile names static functions: the link map has globals alone, and the lexer's statics
read as lexr_line's, 19% of a load."""

import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bench


def loop(n: int) -> str:
    return f"FOR i& = 1 TO {n}: a& = a& + 1: NEXT: PRINT a&"


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
# its registers on every allocation, 238 (217 out of line).
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
    cases = sorted(bench.QR.glob("*.qr"))
    qr = bench.run_natives([b], {p.stem: p.read_text() for p in cases}, runs=1, ext="QR")[0]
    qb = bench.run_native(b, {p.stem: p.with_suffix(".bas").read_text() for p in cases}, runs=1)
    for k, q in qr.items():
        assert q.out.split() == qb[k].out.split(), (k, q.out, qb[k].out)
        assert q.ops <= qb[k].ops * 1.001, f"{k}: Quickr's {q.ops:,} ops, QBasic's {qb[k].ops:,}"


def quickr_push(b: bench.Build) -> None:
    """A list's push is amortized O(1): each doubling of the pushes costs about twice the last. Grown by
    one element a push (REDIM PRESERVE's way), it cost 4.0x. Differences, as the bench subtracts a
    baseline of its own: raw counts at 8000 pushes read 2.45x, the offset not the growth. A run's count
    swings by 1M, so n large enough to drown that."""
    prog = "var a as int[]\nfor i in 0..{n}\n    a.push(i)\nend for\nprint(len(a))\n"
    progs = {str(n): prog.format(n=n) for n in (32000, 64000, 128000)}
    r = bench.run_natives([b], progs, runs=3, ext="QR")[0]
    ratio = (r["128000"].insns - r["64000"].insns) / (r["64000"].insns - r["32000"].insns)
    assert ratio < 2.3, f"doubling the pushes cost {ratio:.2f}x the last doubling"


def main() -> int:
    each_reps()
    charlie_each()
    charlie_think()
    kind_compares()
    profile_cycles()
    profile_stop()
    broken_counter()
    fps_start_up()
    b = bench.build(".", False)  # the tree's own instruments: copying them onto themselves raised
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
    print("ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
