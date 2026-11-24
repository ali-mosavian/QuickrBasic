#!/usr/bin/env python3
"""qr_test.py [-v] [NAME...]: Quickr's cases on the host build (BASIC_EXE, build/host/basic).

tests/qr/NAME.bas, a Quickr file, must print NAME.out; tests/qr/errors/NAME.bas its compile errors, NAME.out, and run
nothing. A folder NAME/ beside a case holds the modules it imports, put beside it; NAME.in is its
standard input, NAME.args its command line. --check must
list the same errors, exit 1 on them and 0 on a clean program; --test must print NAME.test, if
there, and exit 1 when a test fails; --copies must print NAME.copies, if there. NAME picks cases; -v prints a failure's output."""

import os
import re
import sys
import shutil
import tempfile
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
os.environ.setdefault("BASIC_EXE", str(ROOT / "build/host/basic"))
import dosrun  # noqa: E402

CASES = ROOT / "tests/qr"


def cases() -> dict[str, Path]:
    """Each case's name (errors/NAME for an error case): its program."""
    found = sorted(CASES.glob("*.bas")) + sorted((CASES / "errors").glob("*.bas"))
    return {str(p.relative_to(CASES).with_suffix("")): p for p in found if p.with_suffix(".out").exists()}


def named(text: str, name: str) -> str:
    """The output, its job file's name (J0\\NAME.BAS) as the case's own and its folder (J0\\) dropped
    from a module's, its lines ended by \\n."""
    text = re.sub(r"J\d+\\[A-Z0-9_]+\.BAS", Path(name).name + ".bas", text.replace("\r\n", "\n"))
    return re.sub(r"J\d+\\", "", text)


def modules(prog: Path) -> dict[str, bytes]:
    """The files of the folder beside prog named as it, by their paths in it."""
    folder = prog.with_suffix("")
    found = sorted(folder.rglob("*")) if folder.is_dir() else []
    return {str(f.relative_to(folder)): f.read_bytes() for f in found if f.is_file()}


def beside(prog: Path, ext: str) -> str:
    """The file beside prog with extension ext, "" none."""
    f = prog.with_suffix(ext)
    return f.read_text() if f.exists() else ""


def flagged(flag: str, prog: Path) -> subprocess.CompletedProcess[bytes]:
    """prog run with flag, its modules beside it, in a folder of its own."""
    work = Path(tempfile.mkdtemp())
    (work / prog.name).write_bytes(prog.read_bytes())
    for n, data in modules(prog).items():
        (work / n).parent.mkdir(parents=True, exist_ok=True)
        (work / n).write_bytes(data)
    p = subprocess.run([str(dosrun.COM.resolve()), flag, prog.name], cwd=work, capture_output=True, timeout=10)
    shutil.rmtree(work)
    return p


def tested(prog: Path) -> str:
    """--test of prog: "" if it prints NAME.test and exits 1 just when a test failed, else why."""
    want = prog.with_suffix(".test")
    if not want.exists():
        return ""
    p = flagged("--test", prog)
    got = named(p.stdout.decode("latin-1"), prog.stem)
    if got != want.read_text():
        return f"--test printed {got[:60]!r}"
    return "" if p.returncode == ("FAIL" in got) else f"--test exited {p.returncode}"


def copied(prog: Path) -> str:
    """--copies of prog: "" if it prints NAME.copies, else why."""
    want = prog.with_suffix(".copies")
    if not want.exists():
        return ""
    got = named(flagged("--copies", prog).stdout.decode("latin-1"), prog.stem)
    return "" if got == want.read_text() else f"--copies printed {got[:60]!r}"


def check(name: str, prog: Path) -> str:
    """--check of prog, its modules beside it: "" if its output and exit status are right, else why."""
    p = flagged("--check", prog)
    want = prog.with_suffix(".out").read_text() if name.startswith("errors/") else ""
    got = p.stdout.decode("latin-1").replace("\r\n", "\n")
    if got != want:
        return f"--check printed {got[:60]!r}"
    return "" if p.returncode == (1 if want else 0) else f"--check exited {p.returncode}"


def main() -> None:
    args = [a for a in sys.argv[1:] if a != "-v"]
    pick = {k: v for k, v in cases().items() if not args or k in args}
    jobs = {k: (v.read_text(), beside(v, ".in"), beside(v, ".args").strip(), modules(v)) for k, v in pick.items()}
    res = dosrun.run_host(jobs, Path(tempfile.mkdtemp()), 10000, True)
    ok = 0
    for k, prog in pick.items():
        got = named(res[k].out, k)
        why = "" if got == prog.with_suffix(".out").read_text() else f"{res[k].reason} {got[:60]!r}"
        why = why or check(k, prog) or tested(prog) or copied(prog)
        ok += not why
        print(f"pass {k}" if not why else f"FAIL {k}: {why}")
        if why and "-v" in sys.argv:
            print(got)
    print(f"{ok}/{len(pick)}")
    sys.exit(ok != len(pick))


if __name__ == "__main__":
    main()
