"""bench_test.py: bench.py's native cost grows with the work and repeats. Counted from process start,
the loader's ~45M instructions swamped it; under Rosetta the address layout moved a count by a third.
Its DOS profile names static functions: the link map has globals alone, and the lexer's statics
read as lexr_line's, 19% of a load."""

import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bench


def loop(n: int) -> str:
    return f"FOR i& = 1 TO {n}: a& = a& + 1: NEXT: PRINT a&"


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


def main() -> int:
    each_reps()
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
    exe = bench.ROOT / "build/c/basic32.exe"
    if exe.exists():  # the loaded bytes, not the file's: its fixups moved with layout
        assert bench.image(exe) > 100_000 and bench.image(exe) != exe.stat().st_size, bench.image(exe)
        profile_map()
    print("ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
