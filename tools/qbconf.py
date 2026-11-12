#!/usr/bin/env python3
"""qbconf.py [-v] [NAME...]: llrm's QBasic cases on the flat build (build/x86/basic32.exe).

The qb45 suite's required cases must print their `expected` line; tests/run/qb's NAME.bas must
print NAME.out. Cases are read from llrm's tree (LLRM, default ~/work/personal/llrm-qbmac). NAME
picks cases; -v prints a failure's output. SKIP's cases are not run, each for its reason."""

import os
import platform
import re
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
INT16 = "% is 32-bit here, QBasic's 16"
NOD = "BC's code without /D: integer overflow wraps; QBasic raises Overflow, as this build"
SKIP = {
    "Q45U08": INT16 + ": AS INTEGER is 4 bytes in a record",
    "hex_oct_negative": INT16 + ": HEX$, OCT$ of a negative %",
    "ovfpol": INT16 + ": 32767 + 1 fits",
    "farchk": INT16 + ": a LONG subscript is not Overflow",
    "hugechk": INT16 + ": a LONG subscript is not Overflow",
    "arrays": NOD,
    "divmod": NOD,
    "palette_using_pds71": "PDS 7.1's PALETTE USING, and the screen",
    "redim_preserve_pds71": "PDS 7.1's REDIM PRESERVE",
    "resumelocals_local": "PDS 7.1's ON LOCAL ERROR",
}
# DOSBox-X off x86 runs the x87 in 64-bit doubles: the last digit of LOG, EXP, ^ and PRINT can differ.
if platform.machine() not in ("x86_64", "AMD64", "i386", "i686"):
    SKIP |= dict.fromkeys(["log_exp_digits", "range_power_digits", "data_after_checked", "pow_stack_depth",
                           "pow_unrolled"], "the x87 in doubles")
RUNS = LLRM / "tests/run/qb"


def text(p: Path) -> str:
    return p.read_bytes().decode("latin-1").replace("\r\n", "\n").replace("\n", "\r\n")


def trim(t: str, mask: str = "") -> str:
    """As llrm's runner compares: lines without trailing blanks, blank lines dropped, each match of the
    case's mask (text that varies run to run) read as N."""
    return "\r\n".join(re.sub(mask, "N", x) if mask else x for x in (y.rstrip() for y in t.split("\r\n")) if x)


COMPANIONS: dict[str, bytes] = {}  # files the cases read, by name


def cases() -> dict[str, tuple[str | tuple, str, bool | str]]:
    """name: (program, or (program, input, arguments); wanted output; False: it is in the output,
    else the whole output must match, under its mask if a str)."""
    out = {}
    files = {p.name.upper(): p for p in SUITE.iterdir()}
    for c in tomllib.loads((SUITE / "suite.toml").read_text())["case"]:
        if c.get("runtime") == "required" and not c.get("switches") and not c.get("artifacts"):
            prog = text(files[c["source"].upper()])
            for n in c.get("companions", []):
                COMPANIONS[n.upper()] = text(files[n.upper()]).encode("latin-1")
            if c.get("arguments"):
                prog = (prog, "", " ".join(c["arguments"]))
            out[c["source"].split(".")[0]] = (prog, c["expected"], False)
    for b in sorted(RUNS.glob("*.bas")):
        if b.with_suffix(".out").exists():
            m = re.search(r"^' mask: (.*?)\s*$", text(b), re.M)
            out[b.stem] = (text(b), text(b.with_suffix(".out")), m.group(1) if m else True)
    return out


def main() -> None:
    args = [a for a in sys.argv[1:] if a != "-v"]
    every = cases()
    pick = {k: v for k, v in every.items() if (not args and k not in SKIP) or k in args or k.upper() in args}
    names = list(pick)
    chunks = [names[i : i + 24] for i in range(0, len(names), 24)]
    tmp = Path(tempfile.mkdtemp())
    res = {}
    with ThreadPoolExecutor(4) as ex:  # one DOSBox per chunk, under x86run's 600 s
        for r in ex.map(lambda ic: x86run.run({k: pick[k][0] for k in ic[1]}, tmp / f"w{ic[0]}", 3000, file=True,
                                              files=COMPANIONS),
                        enumerate(chunks)):
            res.update(r)
    ok = 0
    for k, (_, want, whole) in pick.items():
        got = res[k].out
        mask = whole if isinstance(whole, str) else ""
        good = trim(got, mask) == trim(want) if whole else want in got
        ok += good
        print(f"{'pass' if good else 'FAIL'} {k}" + ("" if good else f": {res[k].reason} {got[:60]!r}"))
        if not good and "-v" in sys.argv:
            print(got)
    print(f"{ok}/{len(pick)}" + (f", {len(SKIP)} skipped" if not args else ""))


if __name__ == "__main__":
    main()
