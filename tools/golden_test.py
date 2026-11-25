"""golden.py measures with its own build: it once ran a golden_dump a day older than the compiler and
printed 30098/30098 lines as golden while CI found 104 changed. golden_dump -d steps by each op's
linked size: by its unlinked one, it read a 6-byte INBEG's operands as ops, past x_names[] and into
loops."""

import contextlib
import os
import sys
from pathlib import Path

sys.path.append(str(Path(__file__).parent))
import golden


def named_in_step() -> None:
    for p in sorted((golden.ROOT / "tests/golden/stmts").glob("*.bas")):
        for ln in (ln for ln in golden.dump(p, "-d") if not ln.startswith("error")):
            ops = [op.split()[0] for op in ln.split("|")[1:]]
            ends = [i for i, op in enumerate(ops) if op in ("EOL", "EOL_TO")]
            assert ends == [len(ops) - 1], f"{p.name}: ops out of step: {ln}"


def main() -> None:
    golden.DUMP.parent.mkdir(parents=True, exist_ok=True)
    golden.DUMP.touch()
    os.utime(golden.DUMP, (0, 0))  # older than any source
    golden.CORPORA = {}
    with contextlib.suppress(SystemExit):
        golden.main()
    assert golden.DUMP.stat().st_mtime > 0, "a stale golden_dump measured"
    named_in_step()
    print("ok")


if __name__ == "__main__":
    main()
