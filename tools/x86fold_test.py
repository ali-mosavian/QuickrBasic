#!/usr/bin/env python3
"""x86fold_test.py [EXE]: constant expressions and CONSTs cost what their value does.

Each case's loop runs as many instructions as the same loop with the value written out: an
unfolded 2 * 3 + 4 * 5 ran four more ops per pass. Its output must match too.
"""

import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import x86run

LOOP = "FOR i% = 1 TO 20000: x{} = {}: NEXT: PRINT x{}\r\n"

# name: (declarations, suffix, expression, its value written out)
CASES = {
    "int": ("", "%", "2 * 3 + 4 * 5", "26"),
    "single": ("", "!", "1.5 * 4 - 0.5", "5.5"),
    "double": ("", "#", "1# / 4 + 2#", "2.25#"),
    "unary": ("", "%", "-(3 + 4) * NOT 0", "7"),
    "const": ("CONST K = 6, J = K * 2\r\n", "%", "K + J + 8", "26"),
}

SLACK = 1.01  # the compile's own cost, once


def use(exe: str | None) -> Path:
    """The program measured. x86run reads X86_COM on import: setting only the variable after it
    measured build/x86/basic32.exe whatever EXE named, so a build without folding passed or failed
    for another build's reason."""
    if exe:
        x86run.COM = Path(exe)
    return x86run.COM


def main() -> None:
    exe = use(sys.argv[1] if len(sys.argv) > 1 else os.environ.get("X86_COM"))
    jobs = {}
    for name, (decl, sfx, expr, value) in CASES.items():
        jobs[name] = decl + LOOP.format(sfx, expr, sfx)
        jobs[name + "_k"] = decl + LOOP.format(sfx, value, sfx)
    res = x86run.run(jobs, Path(tempfile.mkdtemp()) / "w", 20000, file=True)
    bad = []
    for name in CASES:
        got, want = res[name], res[name + "_k"]
        if got.out != want.out or got.instructions > want.instructions * SLACK:
            bad.append(f"FAIL {name}: {got.out!r} {got.instructions} vs {want.out!r} {want.instructions}")
    print("\n".join(bad) or f"ok {exe}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
