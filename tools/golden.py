"""golden.py [--update]: each corpus program's lines compiled (build/host/golden_dump), against
tests/golden/code.txt: one row per line, its program, index and a hash of its code; a second row,
key "L", the line as linked (comp/select.c's modes, cells, jumps). A changed line prints its new
bytes. A dump that hangs fails. The corpus: tests/corpus/bcg, tests/golden/stmts (each statement's
code and its errors', one file each), and tests/qbrun (llrm's tests/run/qb)."""

import os
import sys
import hashlib
import subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parents[1]
DUMP = ROOT / "build/host/golden_dump"
GOLDEN = ROOT / "tests/golden/code.txt"
CORPORA = {"bcg": ROOT / "tests/corpus/bcg", "qb": ROOT / "tests/qbrun", "stmts": ROOT / "tests/golden/stmts"}


def programs() -> list[tuple[str, Path]]:
    return [(f"{name}/{p.name}", p) for name, d in CORPORA.items() if d.is_dir() for p in sorted(d.glob("*.bas"))]


def dump(path: Path, *mode: str) -> list[str]:
    args = [str(DUMP), *mode, str(path)]
    return subprocess.run(args, capture_output=True, text=True, check=True, timeout=30).stdout.splitlines()


def rows(key: str, lines: list[str]) -> dict[str, tuple[str, str]]:
    out = {}
    for ln in lines:
        n, _, code = ln.partition(" ")
        out[f"{key} {n}"] = (hashlib.sha1(code.encode()).hexdigest()[:12], code)
    return out


def main() -> None:
    subprocess.run(["make", "-s", "-C", str(ROOT), "build/host/golden_dump"], check=True)  # the compiler's own
    progs = programs()
    with ThreadPoolExecutor(os.cpu_count()) as ex:
        dumps = list(ex.map(lambda kp: rows(kp[0], dump(kp[1])) | rows(f"{kp[0]} L", dump(kp[1], "-l")), progs))
    new = {k: v for d in dumps for k, v in d.items()}
    if "--update" in sys.argv:
        GOLDEN.write_text("".join(f"{k} {h}\n" for k, (h, _) in new.items()))
        print(f"{len(new)} lines from {len(progs)} programs")
        return
    old = dict(ln.rsplit(" ", 1) for ln in GOLDEN.read_text().splitlines())
    keys = {k.split(" ")[0] for k in new}
    old = {k: h for k, h in old.items() if k.split(" ")[0] in keys}  # a corpus not here: not checked
    bad = [k for k in old.keys() | new.keys() if old.get(k) != new.get(k, (None,))[0]]
    for k in sorted(bad)[:20]:
        print(f"CHANGED {k}: {new[k][1] if k in new else '(gone)'}")
    print(f"{len(new) - len(bad)}/{len(new)} lines as golden" + ("" if not bad else f", {len(bad)} changed"))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
