"""golden.py measures with its own build: it once ran a golden_dump a day older than the compiler and
printed 30098/30098 lines as golden while CI found 104 changed."""

import contextlib
import os
import sys
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
    print("ok")


if __name__ == "__main__":
    main()
