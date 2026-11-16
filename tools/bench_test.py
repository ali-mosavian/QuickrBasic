"""bench_test.py: bench.py's native cost grows with the work and repeats. Counted from process start,
the loader's ~45M instructions swamped it; under Rosetta the address layout moved a count by a third.
Its DOS profile names static functions: the link map has globals alone, and the lexer's statics
read as lexr_line's, 19% of a load."""

import re
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


def main() -> int:
    b = bench.build(".", False)  # the tree's own instruments: copying them onto themselves raised
    r = bench.run_native(b, {"one": loop(1000000), "two": loop(2000000)}, runs=5)
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
