#!/usr/bin/env python3
"""qbconf.py [-v] [NAME...]: llrm's QBasic cases on the flat build (build/x86/basic32.exe).

The qb45 suite's required cases must print their `expected` line; tests/run/qb's NAME.bas must
print NAME.out. Cases are read from llrm's tree (LLRM, default ~/work/personal/llrm-qbmac). NAME
picks cases; -v prints a failure's output."""

import os
import sys
import tempfile
import tomllib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LLRM = Path(os.environ.get("LLRM", Path.home() / "work/personal/llrm-qbmac"))
os.environ.setdefault("X86_COM", str(ROOT / "build/x86/basic32.exe"))
import x86run  # noqa: E402

SUITE = LLRM / "tests/differential/conformance/qb45"
RUNS = LLRM / "tests/run/qb"


def text(p: Path) -> str:
    return p.read_bytes().decode("latin-1").replace("\r\n", "\n").replace("\n", "\r\n")


def trim(t: str) -> str:
    """Lines without trailing blanks: llrm's .out files drop the blank QBasic prints after a number."""
    return "\r\n".join(x.rstrip() for x in t.rstrip("\r\n").split("\r\n"))


def cases() -> dict[str, tuple[str, str, bool]]:
    """name: (program, wanted output, whole output must match)."""
    out = {}
    files = {p.name.upper(): p for p in SUITE.iterdir()}
    for c in tomllib.loads((SUITE / "suite.toml").read_text())["case"]:
        if c.get("runtime") == "required" and not c.get("switches") and not c.get("artifacts"):
            out[c["source"].split(".")[0]] = (text(files[c["source"].upper()]), c["expected"], False)
    for b in sorted(RUNS.glob("*.bas")):
        if b.with_suffix(".out").exists():
            out[b.stem] = (text(b), text(b.with_suffix(".out")), True)
    return out


def main() -> None:
    args = [a for a in sys.argv[1:] if a != "-v"]
    every = cases()
    pick = {k: v for k, v in every.items() if not args or k in args or k.upper() in args}
    names = list(pick)
    chunks = [names[i : i + 24] for i in range(0, len(names), 24)]
    tmp = Path(tempfile.mkdtemp())
    res = {}
    with ThreadPoolExecutor(4) as ex:  # one DOSBox per chunk, under x86run's 600 s
        for r in ex.map(lambda ic: x86run.run({k: pick[k][0] for k in ic[1]}, tmp / f"w{ic[0]}", 3000, file=True),
                        enumerate(chunks)):
            res.update(r)
    ok = 0
    for k, (_, want, whole) in pick.items():
        got = res[k].out
        good = trim(got) == trim(want) if whole else want in got
        ok += good
        print(f"{'pass' if good else 'FAIL'} {k}" + ("" if good else f": {res[k].reason} {got[:60]!r}"))
        if not good and "-v" in sys.argv:
            print(got)
    print(f"{ok}/{len(pick)}")


if __name__ == "__main__":
    main()
