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
LOOP = "{}CLS\r\nFOR i% = 1 TO {}: LOCATE 1, 1: {}: NEXT\r\nEND\r\n{}"
SUBS = ("DECLARE SUB A (x%, y%)\r\nDECLARE SUB Z ()\r\n",
        "SUB A (x%, y%)\r\nEND SUB\r\nSUB Z\r\nEND SUB\r\n")

# name: (the loop's statement, the base's, the parts a pass, the budget for each, (head, tail))
CASES = {
    # PRINT asked DOS whether standard output was the screen for each character: 466 a character.
    # Then the runtime's console took one at a time: 188.
    "print_char": ('PRINT "abcdefghijklmnopqrstuvwx";', 'PRINT "a";', 23, 40, ("", "")),
    # A parameter was set aside and back as a local too, its own cell unused by reference: 182.
    "sub_arg": ("A i%, i%", "Z", 2, 150, SUBS),
}


# The IDE's debugger op at each statement, over the same loop's in basic32 (IDE32.EXE beside EXE).
# With a breakpoint possible it looked up the statement's place in its line: 47.
IDE_BUDGET = 25


def per_pass(exe: Path, ide: bool) -> float:
    """An empty FOR's instructions a pass, run by exe: the IDE's F5, else the file given."""
    x86run.COM = exe
    jobs = {"a": f"FOR i% = 1 TO {N}: NEXT\r\nSYSTEM\r\n", "b": f"FOR i% = 1 TO {2 * N}: NEXT\r\nSYSTEM\r\n"}
    more = dict(files={"BASIC.HLP": (x86run.ROOT / "src/ui/basic.hlp").read_bytes()},
                before=("AUTOTYPE -w 2 f5",), keyboard=True) if ide else {}
    r = x86run.run(jobs, Path(tempfile.mkdtemp()) / "w", 60000, file=True, screen=True, **more)
    return (r["b"].instructions - r["a"].instructions) / N


# The IDE's open of a file and its F5, a line: each line was tokenized and spelled by LIST twice,
# each keyword spelled found by a scan of the list: 76,400.
OPEN_LINE = "IF a THEN PRINT LEN(b$); TAB(5); ELSE LOCATE 1, 1: COLOR 7"
OPEN_LINES = 400
OPEN_BUDGET = 55000


def open_per_line(ide: Path) -> float:
    """The IDE's instructions to open a file and F5 it, a line of OPEN_LINE, over SYSTEM alone."""
    x86run.COM = ide
    jobs = {"a": "SYSTEM\r\n", "b": "SYSTEM\r\n" + (OPEN_LINE + "\r\n") * OPEN_LINES}
    r = x86run.run(jobs, Path(tempfile.mkdtemp()) / "w", 60000, file=True, screen=True, keyboard=True,
                   files={"BASIC.HLP": (x86run.ROOT / "src/ui/basic.hlp").read_bytes()},
                   before=("AUTOTYPE -w 2 f5",))
    return (r["b"].instructions - r["a"].instructions) / OPEN_LINES


def use(exe: str | None) -> Path:
    if exe:
        x86run.COM = Path(exe)
    return x86run.COM


def main() -> None:
    exe = use(sys.argv[1] if len(sys.argv) > 1 else os.environ.get("X86_COM"))
    jobs = {}
    for name, (stmt, base, _, _, (head, tail)) in CASES.items():
        jobs[name] = LOOP.format(head, N, stmt, tail)
        jobs[name + "_base"] = LOOP.format(head, N, base, tail)
    res = x86run.run(jobs, Path(tempfile.mkdtemp()) / "w", 60000, file=True, screen=True)
    bad, report = [], []
    for name, (_, _, parts, budget, _) in CASES.items():
        got, base = res[name].instructions, res[name + "_base"].instructions
        if not got or not base:
            bad.append(f"FAIL {name}: no count from {exe}")
            continue
        each = (got - base) / (N * parts)
        report.append(f"{name} {each:.1f}/{budget}")
        if each > budget:
            bad.append(f"FAIL {name}: {each:.1f} instructions each, budget {budget}")
    ide = exe.parent / "ide32.exe"
    if ide.exists():
        each = per_pass(ide, True) - per_pass(exe, False)
        report.append(f"ide_stmt {each:.1f}/{IDE_BUDGET}")
        if each > IDE_BUDGET:
            bad.append(f"FAIL ide_stmt: {each:.1f} instructions a statement, budget {IDE_BUDGET}")
        line = open_per_line(ide)
        report.append(f"ide_open {line:.0f}/{OPEN_BUDGET}")
        if line > OPEN_BUDGET:
            bad.append(f"FAIL ide_open: {line:.0f} instructions a line, budget {OPEN_BUDGET}")
    print("\n".join(bad) or f"ok {exe}: {' '.join(report)}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
