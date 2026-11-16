"""bench.py: what a program costs on a build, and builds against each other.

    bench.py run FILE.bas | -e CODE [--dos]      one program: its output and cost
    bench.py iter -e BODY [--setup CODE] [--dos] one loop iteration's cost
    bench.py cmp [REV...] [--dos]                tools/bench/*.bas per build (default HEAD .; . the tree)
    bench.py size [REV] [--functions]            the DOS executables' bytes, REV's (default HEAD) and ours
    bench.py gate [--save]                       the suite and sizes against tools/bench/baseline.json
    bench.py qbasic                              qbasic/BENCH.BAS in QBasic 1.1 and our IDE under DOSBox

Native (the default) runs build/hostperf/basic for retired instructions, the least of RUNS runs less
an empty program's, builds compared run interleaved; and build/host/basic for the ops dispatched,
which are exact. --dos runs build/c/basic32.exe under x86run, whose instruction count is exact.
A REV is built once in its own worktree under build/bench/.
"""

import os
import re
import sys
import json
import time
import shutil
import argparse
import tempfile
import subprocess
from pathlib import Path
from dataclasses import dataclass

sys.path.insert(0, str(Path(__file__).resolve().parent))  # run with -I too
import x86run

ROOT = Path(__file__).resolve().parent.parent
SUITE = ROOT / "tools/bench"
BASELINE = SUITE / "baseline.json"
REVS = ROOT / "build/bench"
EXES = ("build/c/basic32.exe", "build/c/ide32.exe")
INSTRUMENT_SRC = ("csrc/plat/host/app/insns.c", "csrc/plat/host/pfpu.c", "csrc/plat/pfpu.h")  # arm64 too
RUNS = 3
INSNS_SLACK = 0.01  # on the native CPU a count repeats to 0.1%

type Programs = dict[str, str]


@dataclass(frozen=True)
class Cost:
    out: str
    insns: int  # native: retired instructions; DOS: instructions run
    ops: int = 0  # native only: the VM's ops dispatched


@dataclass(frozen=True)
class Build:
    root: Path

    @property
    def perf(self) -> Path:
        return self.root / "build/hostperf/basic"

    @property
    def counted(self) -> Path:
        return self.root / "build/host/basic"

    @property
    def dos(self) -> Path:
        return self.root / "build/c/basic32.exe"

    def make(self, dos: bool) -> None:
        """The builds, by our Makefile and instruments: an older rev may lack them, or count otherwise."""
        if self.root != ROOT:
            for src in INSTRUMENT_SRC:
                shutil.copy(ROOT / src, self.root / src)
        targets = ["chostperf", "build/host/basic"] + ["c"] * dos
        p = subprocess.run(
            ["nice", "-n", "10", "make", "-f", str(ROOT / "Makefile"), "-j4", *targets],
            cwd=self.root,
            capture_output=True,
            text=True,
        )
        if p.returncode:
            sys.exit(f"make in {self.root} failed\n{p.stdout[-2000:]}{p.stderr[-2000:]}")


def git(*args: str) -> str:
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()


def build(rev: str, dos: bool) -> Build:
    """The working tree's build (rev ".") or rev's, made up to date."""
    if rev == ".":
        b = Build(ROOT)
    else:
        sha = git("rev-parse", "--short", rev)
        b = Build(REVS / sha)
        if not b.root.exists():
            git("worktree", "add", "-q", "--detach", str(b.root), sha)
    b.make(dos)
    return b


def source(prog: str) -> bytes:
    return (prog.rstrip("\n") + "\nSYSTEM\n").replace("\r\n", "\n").replace("\n", "\r\n").encode("latin-1")


def count(exe: Path, bas: Path, env: str, key: bytes) -> tuple[int, str]:
    """exe's count of key for program bas, and its output."""
    p = subprocess.run(
        [str(exe), bas.name],
        cwd=bas.parent,
        input=b"",
        capture_output=True,
        env=os.environ | {env: "1"},
        timeout=600,
    )
    m = re.search(rb"\n" + key + rb" (\d+)\n", p.stderr)
    if not m:
        sys.exit(f"{bas.stem}: no {key.decode()} count from {exe}\n{p.stderr.decode(errors='replace')}")
    return int(m[1]), p.stdout.decode("latin-1").strip()


def run_natives(builds: list[Build], progs: Programs, runs: int = RUNS) -> list[dict[str, Cost]]:
    """Each build's costs, their runs interleaved: the machine's load drifts alike for all."""
    res: list[dict[str, Cost]] = [{} for _ in builds]
    with tempfile.TemporaryDirectory() as d:
        for name, prog in {"": "", **progs}.items():
            bas = Path(d) / f"{name[:8].upper() or 'EMPTY'}.BAS"
            bas.write_bytes(source(prog))
            least = [1 << 62] * len(builds)
            for _ in range(runs):
                for i, b in enumerate(builds):
                    least[i] = min(least[i], count(b.perf, bas, "QB_INSNS", b"insns")[0])
            for i, b in enumerate(builds):
                ops, out = count(b.counted, bas, "QB_OPS", b"ops")
                res[i][name] = Cost(out, least[i], ops)
    return [{k: Cost(c.out, c.insns - r[""].insns, c.ops - r[""].ops) for k, c in r.items() if k} for r in res]


def run_native(b: Build, progs: Programs, runs: int = RUNS) -> dict[str, Cost]:
    return run_natives([b], progs, runs)[0]


def run_dos(b: Build, progs: Programs) -> dict[str, Cost]:
    x86run.COM = b.dos
    with tempfile.TemporaryDirectory() as d:
        r = x86run.run(
            {"": "", **{k: source(p).decode("latin-1") for k, p in progs.items()}},
            Path(d) / "w",
            limit_ms=600000,
            file=True,
        )
    for k, v in r.items():
        if v.reason != "exit":
            sys.exit(f"{k or 'empty'}: {v.reason} under DOS")
    return {k: Cost(v.out.strip(), v.instructions - r[""].instructions) for k, v in r.items() if k}


def measure(b: Build, progs: Programs, dos: bool) -> dict[str, Cost]:
    return run_dos(b, progs) if dos else run_native(b, progs)


def suite() -> Programs:
    return {p.stem: p.read_text() for p in sorted(SUITE.glob("*.bas"))}


def program(a: argparse.Namespace) -> str:
    return a.e if a.e is not None else Path(a.file).read_text()


def sizes(b: Build) -> dict[str, int]:
    return {e: (b.root / e).stat().st_size for e in EXES}


def cmd_run(a: argparse.Namespace) -> int:
    c = measure(build(".", a.dos), {"prog": program(a)}, a.dos)["prog"]
    print(c.out)
    print(f"insns {c.insns:,}" + (f"  ops {c.ops:,}" if c.ops else ""))
    return 0


def cmd_iter(a: argparse.Namespace) -> int:
    n = a.n

    def loop(k: int) -> str:
        return f"{a.setup}\nFOR i& = 1 TO {k}: {a.e}: NEXT"

    r = measure(build(".", a.dos), {"one": loop(n), "two": loop(2 * n)}, a.dos)
    print(
        f"insns/iter {(r['two'].insns - r['one'].insns) / n:.1f}"
        + (f"  ops/iter {(r['two'].ops - r['one'].ops) / n:.2f}" if not a.dos else "")
    )
    return 0


def cmd_cmp(a: argparse.Namespace) -> int:
    names: list[str] = a.revs or ["HEAD", "."]
    builds = [build(r, a.dos) for r in names]
    res = [run_dos(b, suite()) for b in builds] if a.dos else run_natives(builds, suite())
    heads = ["tree" if n == "." else n for n in names]
    print(f"{'':10}" + "".join(f"{n:>14}" for n in heads) + "".join(f"{n:>8}" for n in heads[1:]))
    for k in res[0]:
        row = [r[k] for r in res]
        flag = "" if len({c.out for c in row}) == 1 else "  OUTPUT DIFFERS"
        print(
            f"{k:10}"
            + "".join(f"{c.insns:>14,}" for c in row)
            + "".join(f"{c.insns / row[0].insns:>8.3f}" for c in row[1:])
            + flag
        )
    return 0


def functions(b: Build) -> dict[str, int]:
    """Each DOS object's functions' bytes, by object and name: COFF gives no sizes, so each runs to
    the next function, the last to .text's end."""
    llvm = Path(shutil.which("llvm-nm") or "/opt/homebrew/opt/llvm/bin/llvm-nm").parent
    objs = b.root / "build/c/obj"
    res: dict[str, int] = {}
    for o in sorted(objs.rglob("*.obj")):
        heads = subprocess.run([llvm / "llvm-objdump", "-h", o], capture_output=True, text=True).stdout
        text = re.search(r"^\s*\d+ \.text\s+([0-9a-f]+)", heads, re.M)
        syms = subprocess.run([llvm / "llvm-nm", "-n", "--defined-only", o], capture_output=True, text=True).stdout
        fns = [(int(f[0], 16), f[2]) for f in (ln.split() for ln in syms.splitlines()) if len(f) == 3 and f[1] in "tT"]
        if not fns or not text:
            continue
        ends = [a for a, _ in fns[1:]] + [int(text[1], 16)]
        for (start, name), end in zip(fns, ends, strict=True):
            res[f"{o.relative_to(objs)}:{name}"] = end - start
    return res


def cmd_size(a: argparse.Namespace) -> int:
    was, now = build(a.rev, True), build(".", True)
    old, new = sizes(was), sizes(now)
    for e in EXES:
        print(f"{Path(e).name:12} {old[e]:>10,} {new[e]:>10,} {new[e] - old[e]:>+8,}")
    if a.functions:
        fo, fn = functions(was), functions(now)
        for k in sorted(fo.keys() | fn.keys()):
            if fo.get(k, 0) != fn.get(k, 0):
                print(f"{k:60} {fo.get(k, 0):>8} {fn.get(k, 0):>8} {fn.get(k, 0) - fo.get(k, 0):>+6}")
    return 0


def cmd_gate(a: argparse.Namespace) -> int:
    """ops, outputs and sizes against the baseline's record, exact; retired instructions against
    the baseline's rev built and run now, beside ours."""
    b = build(".", True)
    if a.save:
        now = run_native(b, suite())
        record = {
            "rev": git("rev-parse", "--short", "HEAD"),
            "size": sizes(b),
            "cost": {k: vars(c) for k, c in now.items()},
        }
        BASELINE.write_text(json.dumps(record, indent=1) + "\n")
        print(f"saved {BASELINE.relative_to(ROOT)}")
        return 0

    base = json.loads(BASELINE.read_text())
    then, now = run_natives([build(base["rev"], False), b], suite())
    fail = False
    print(f"against {base['rev']}{'':4}{'ops':>14}{'':8}{'insns':>14}")
    for k, c in now.items():
        was = Cost(**base["cost"][k])
        ratio = c.insns / then[k].insns
        bad = [
            why
            for why, hit in (("ops", c.ops > was.ops), ("insns", ratio > 1 + INSNS_SLACK), ("output", c.out != was.out))
            if hit
        ]
        fail |= bool(bad)
        print(
            f"{k:16}{c.ops:>14,}{c.ops / was.ops:>8.3f}{c.insns:>14,}{ratio:>8.3f}"
            + (f"  FAIL {' '.join(bad)}" if bad else "")
        )
    for e, s in sizes(b).items():
        was_s = base["size"][e]
        fail |= s > was_s
        print(f"{Path(e).name:16}{was_s:>14,}{s:>14,}{s - was_s:>+8,}" + ("  FAIL" if s > was_s else ""))
    return int(fail)


DOSBOX_CONF = """[sdl]
output=surface
[dosbox]
memsize=16
startbanner=false
quit warning=false
[cpu]
core=normal
cputype=386
cycles=fixed 5000
[dos]
xms=true
ems=true
[mixer]
nosound=true
[autoexec]
mount c {work}
c:
{cmds}
exit
"""


def qbasic_run(which: str, work: Path) -> dict[str, float]:
    """BENCH.BAS's R.TXT timings, run by QBasic 1.1 (QBASIC_EXE) or our IDE, at a fixed 5000 cycles."""
    work.mkdir(parents=True)
    shutil.copy(SUITE / "qbasic/BENCH.BAS", work / "BENCH.BAS")
    if which == "qbasic":
        exe = os.environ.get("QBASIC_EXE")
        if not exe:
            sys.exit("set QBASIC_EXE to QBasic 1.1's QBASIC.EXE")
        shutil.copy(exe, work / "QBASIC.EXE")
        cmds = "QBASIC /RUN BENCH.BAS"
    else:
        for f in ("ide32.exe", "DOS32A.EXE"):
            shutil.copy(ROOT / "build/c" / f, work / f.upper())
        shutil.copy(ROOT / "src/ui/basic.hlp", work / "BASIC.HLP")
        cmds = "AUTOTYPE -w 4 f5\nIDE32 BENCH.BAS"
    (work / "b.conf").write_text(DOSBOX_CONF.format(work=work, cmds=cmds))
    p = subprocess.Popen(
        [str(x86run.DOSBOX), "-nolog", "-conf", str(work / "b.conf")],
        env=os.environ | {"SDL_VIDEODRIVER": "dummy"},
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    r, end = work / "R.TXT", time.time() + 1500
    while p.poll() is None and time.time() < end and "check" not in (r.read_text() if r.exists() else ""):
        time.sleep(2)
    time.sleep(1)
    p.kill()
    lines = r.read_text(errors="replace").splitlines() if r.exists() else []
    return {ln.split()[0]: float(ln.split()[1]) for ln in lines if len(ln.split()) == 2}


def cmd_qbasic(a: argparse.Namespace) -> int:
    build(".", True)
    with tempfile.TemporaryDirectory() as d:
        from concurrent.futures import ThreadPoolExecutor

        with ThreadPoolExecutor(2) as ex:
            qb, us = ex.map(lambda w: qbasic_run(w, Path(d) / w), ("qbasic", "ours"))
    print(f"{'':10}{'QBasic s':>10}{'ours s':>10}{'speed-up':>10}")
    for k, t in qb.items():
        print(f"{k:10}{t:>10.2f}{us.get(k, float('nan')):>10.2f}{t / us[k] if us.get(k) else float('nan'):>9.1f}x")
    tq, tu = sum(qb.values()), sum(us.get(k, 0) for k in qb)
    print(f"{'total':10}{tq:>10.2f}{tu:>10.2f}{tq / tu if tu else float('nan'):>9.1f}x")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("run")
    p.add_argument("file", nargs="?")
    p.add_argument("-e")
    p.add_argument("--dos", action="store_true")
    p = sub.add_parser("iter")
    p.add_argument("-e", required=True)
    p.add_argument("--setup", default="")
    p.add_argument("-n", type=int, default=200000)
    p.add_argument("--dos", action="store_true")
    p = sub.add_parser("cmp")
    p.add_argument("revs", nargs="*")
    p.add_argument("--dos", action="store_true")
    p = sub.add_parser("size")
    p.add_argument("rev", nargs="?", default="HEAD")
    p.add_argument("--functions", action="store_true", help="each function whose bytes changed")
    p = sub.add_parser("gate")
    p.add_argument("--save", action="store_true")
    sub.add_parser("qbasic")
    a = ap.parse_args()
    return {"run": cmd_run, "iter": cmd_iter, "cmp": cmd_cmp, "size": cmd_size, "gate": cmd_gate, "qbasic": cmd_qbasic}[
        a.cmd
    ](a)


if __name__ == "__main__":
    sys.exit(main())
