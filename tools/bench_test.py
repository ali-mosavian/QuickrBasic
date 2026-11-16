"""bench_test.py: bench.py's native cost grows with the work. Counted from process start, the loader's
and Rosetta's ~45M instructions swamped it; past boot, ±3M still did for a 20M benchmark."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bench


def loop(n: int) -> str:
    return f"FOR i& = 1 TO {n}: a& = a& + 1: NEXT: PRINT a&"


def main() -> int:
    r = bench.run_native(bench.Build(bench.ROOT), {"one": loop(1000000), "two": loop(2000000)}, runs=5)
    ratio = r["two"].insns / r["one"].insns
    assert (r["one"].out, r["two"].out) == ("1000000", "2000000"), r
    assert 1.95 < ratio < 2.05, f"twice the loop cost {ratio:.3f}x"
    print("ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
