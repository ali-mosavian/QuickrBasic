#!/usr/bin/env python3
"""x86cost_test.py [EXE]: what a statement's part costs, in instructions, against a budget.

Each case is a pair of loops differing in one part; the extra instructions a pass, over the part's
count, must stay within its budget. QBasic 1.1 is the reference: its PRINT puts a string's
characters with a two-instruction loop.
"""

import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import x86run

N = 2000
LOOP = "CLS\r\nFOR i% = 1 TO {}: LOCATE 1, 1: {}: NEXT\r\n"

# name: (the loop's statement, the base's, the parts a pass, the budget for each)
CASES = {
    # PRINT asked DOS whether standard output was the screen for each character: 466 a character.
    "print_char": ('PRINT "abcdefghijklmnopqrstuvwx";', 'PRINT "a";', 23, 250),
}


def use(exe: str | None) -> Path:
    if exe:
        x86run.COM = Path(exe)
    return x86run.COM


def main() -> None:
    exe = use(sys.argv[1] if len(sys.argv) > 1 else os.environ.get("X86_COM"))
    jobs = {}
    for name, (stmt, base, _, _) in CASES.items():
        jobs[name] = LOOP.format(N, stmt)
        jobs[name + "_base"] = LOOP.format(N, base)
    res = x86run.run(jobs, Path(tempfile.mkdtemp()) / "w", 60000, file=True, screen=True)
    bad, report = [], []
    for name, (_, _, parts, budget) in CASES.items():
        got, base = res[name].instructions, res[name + "_base"].instructions
        if not got or not base:
            bad.append(f"FAIL {name}: no count from {exe}")
            continue
        each = (got - base) / (N * parts)
        report.append(f"{name} {each:.1f}/{budget}")
        if each > budget:
            bad.append(f"FAIL {name}: {each:.1f} instructions each, budget {budget}")
    print("\n".join(bad) or f"ok {exe}: {' '.join(report)}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
