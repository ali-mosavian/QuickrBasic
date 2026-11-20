#!/usr/bin/env python3
"""conformance.py [-v] [NAME...]: llrm's QBasic cases on the DOS build (BASIC_EXE, build/c/basic32.exe).

The qb45 suite's required cases (tests/qb45) must print their `expected` line; tests/qbrun's
NAME.bas must print NAME.out. Both are copies of llrm's. NAME picks cases; -v prints a failure's
output. SKIP's cases are not run, each for its reason."""

import os
import platform
import re
import sys
import tempfile
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
os.environ.setdefault("BASIC_EXE", str(ROOT / "build/c/basic32.exe"))
import dosrun  # noqa: E402

SUITE = ROOT / "tests/qb45"  # llrm's tests/differential/conformance/qb45
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
}
SKIP |= dict.fromkeys(["stackover", "stackoverp", "stackoverv", "stackrt", "stackrtp", "stackrtv"],
                      "BC's stack is fixed, this one grows to memory: 30,000 calls deep fit")
# DOSBox-X off x86 runs the x87 in 64-bit doubles: the last digit of LOG, EXP, ^ and PRINT can differ.
if platform.machine() not in ("x86_64", "AMD64", "i386", "i686"):
    SKIP |= dict.fromkeys(["log_exp_digits", "range_power_digits", "data_after_checked", "pow_stack_depth",
                           "pow_unrolled"], "the x87 in doubles")
elif dosrun.is_host(dosrun.COM):  # an open bug: x86-64's host misses BC's last digit where the DOS build hits it
    SKIP |= dict.fromkeys(["log_exp_digits", "pow_stack_depth", "pow_unrolled"], "the x86-64 host's x87 path")
RUNS = ROOT / "tests/qbrun"  # llrm's tests/run/qb


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
    tmp = Path(tempfile.mkdtemp())
    batches = [dict(jobs=b, limit_ms=3000, file=True, files=COMPANIONS) for b in dosrun.split({k: v[0] for k, v in pick.items()})]
    res = dosrun.run_all(batches, tmp)
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
    sys.exit(ok != len(pick))


if __name__ == "__main__":
    main()
