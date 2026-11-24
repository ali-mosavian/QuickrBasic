#!/usr/bin/env python3
"""qr_test.py [-v] [NAME...]: Quickr's cases on the host build (BASIC_EXE, build/host/basic).

tests/qr/NAME.qr must print NAME.out; tests/qr/errors/NAME.qr its compile errors, NAME.out, and run
nothing. A folder NAME/ beside a case holds the modules it imports, put beside it. --check must
list the same errors, exit 1 on them and 0 on a clean program. NAME picks cases; -v prints a
failure's output."""

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
    found = sorted(CASES.glob("*.qr")) + sorted((CASES / "errors").glob("*.qr"))
    return {str(p.relative_to(CASES).with_suffix("")): p for p in found if p.with_suffix(".out").exists()}


def named(text: str, name: str) -> str:
    """The output, its job file's name (J0\\NAME.QR) as the case's own and its folder (J0\\) dropped
    from a module's, its lines ended by \\n."""
    text = re.sub(r"J\d+\\[A-Z0-9_]+\.QR", Path(name).name + ".qr", text.replace("\r\n", "\n"))
    return re.sub(r"J\d+\\", "", text)


def modules(prog: Path) -> dict[str, bytes]:
    """The files of the folder beside prog named as it, by their paths in it."""
    folder = prog.with_suffix("")
    found = sorted(folder.rglob("*")) if folder.is_dir() else []
    return {str(f.relative_to(folder)): f.read_bytes() for f in found if f.is_file()}


def check(name: str, prog: Path) -> str:
    """--check of prog, its modules beside it: "" if its output and exit status are right, else why."""
    work = Path(tempfile.mkdtemp())
    (work / prog.name).write_bytes(prog.read_bytes())
    for n, data in modules(prog).items():
        (work / n).parent.mkdir(parents=True, exist_ok=True)
        (work / n).write_bytes(data)
    p = subprocess.run([str(dosrun.COM.resolve()), "--check", prog.name], cwd=work, capture_output=True, timeout=10)
    shutil.rmtree(work)
    want = prog.with_suffix(".out").read_text() if name.startswith("errors/") else ""
    got = p.stdout.decode("latin-1").replace("\r\n", "\n")
    if got != want:
        return f"--check printed {got[:60]!r}"
    return "" if p.returncode == (1 if want else 0) else f"--check exited {p.returncode}"


def main() -> None:
    args = [a for a in sys.argv[1:] if a != "-v"]
    pick = {k: v for k, v in cases().items() if not args or k in args}
    jobs = {k: (v.read_text(), "", "", modules(v)) for k, v in pick.items()}
    res = dosrun.run_host(jobs, Path(tempfile.mkdtemp()), 10000, True, "QR")
    ok = 0
    for k, prog in pick.items():
        got = named(res[k].out, k)
        why = "" if got == prog.with_suffix(".out").read_text() else f"{res[k].reason} {got[:60]!r}"
        why = why or check(k, prog)
        ok += not why
        print(f"pass {k}" if not why else f"FAIL {k}: {why}")
        if why and "-v" in sys.argv:
            print(got)
    print(f"{ok}/{len(pick)}")
    sys.exit(ok != len(pick))


if __name__ == "__main__":
    main()
