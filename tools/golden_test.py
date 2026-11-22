"""golden.py measures with its own build: it once ran a golden_dump a day older than the compiler and
printed 30098/30098 lines as golden while CI found 104 changed. A dump is the same wherever the
checkout is: the program's path once took heap ahead of the variables, and CI's shorter one moved
every linked address."""

import contextlib
import os
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.append(str(Path(__file__).parent))
import golden


def main() -> None:
    golden.DUMP.parent.mkdir(parents=True, exist_ok=True)
    golden.DUMP.touch()
    os.utime(golden.DUMP, (0, 0))  # older than any source
    golden.CORPORA = {}
    with contextlib.suppress(SystemExit):
        golden.main()
    assert golden.DUMP.stat().st_mtime > 0, "a stale golden_dump measured"
    checkout_moved()
    print("ok")


def checkout_moved() -> None:
    prog = Path("tests/corpus/bcg/01_aceyducey.bas")
    here = golden.dump(golden.ROOT / prog, "-l")
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / ("a" * 120)
        (root / prog).parent.mkdir(parents=True)
        shutil.copy(golden.ROOT / prog, root / prog)
        golden.ROOT, real = root, golden.ROOT
        try:
            there = golden.dump(root / prog, "-l")
        finally:
            golden.ROOT = real
    assert here == there, "the dump depends on the checkout's path"


if __name__ == "__main__":
    main()
