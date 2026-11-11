#!/usr/bin/env python3
"""x86f_test.py [EXE]: regressions of the flat build (build/x86/basic32.exe).

Each case: its program, the output it must print and one it must not."""

import os
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
os.environ.setdefault("X86_COM", sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "build/x86/basic32.exe"))
import x86run  # noqa: E402

CASES = {
    # ctail's inc esi after the and: ZF was the inc's, so a false IF ran the
    # rest of the line.
    "cmpif_false": ('A=5:IF A=4 THEN PRINT "NO":PRINT "NO"\r\nPRINT "END"\r\n', "\r\nEND\r\n", "NO"),
    # cm_p's stack check kept the 16-bit room while each level took 12 bytes,
    # not 6: 14 parentheses were OUT OF MEMORY.
    "paren_depth": ("PRINT " + "(" * 29 + "7" + ")" * 29 + "\r\n", "\r\n 7 \r\n", "MEMORY"),
    # The lexer took a name after an A5h byte (spaces skipped) for FN's: 42405
    # is M_INT A5h A5h, so A was FN A.
    "fn_lookback": ("PRINT 42405 A\r\n", " 42405  0 \r\n", "ERROR"),
}


def main() -> None:
    res = x86run.run({k: v[0] for k, v in CASES.items()}, Path(tempfile.mkdtemp()) / "w", 10000)
    bad = [k for k, (_, want, forbid) in CASES.items() if want not in res[k].out or (forbid and forbid in res[k].out)]
    for k in bad:
        print(f"FAIL {k}: {res[k].reason} {res[k].out!r}")
    sys.exit(1 if bad else print("ok") or 0)


if __name__ == "__main__":
    main()
