"""bench.py: what a program costs on a build, and builds against each other.

    bench.py run FILE.bas | -e CODE [--dos]      one program: its output and cost
    bench.py iter -e BODY [--setup CODE] [--dos] one loop iteration's cost
    bench.py cmp [REV...] [-e CODE] [--file F] [--runs N] [--dos]  the suite, or these programs, per
                                                 build (HEAD .)
    bench.py size [REV] [--functions]            the DOS executables' loaded bytes, REV's (default HEAD) and ours
    bench.py gate [--save]                       the suite and sizes against tools/bench/baseline.json
    bench.py qr [REV...] [--ops]                 tools/bench/qr: each quickr/NAME.bas against its QBasic twin NAME.bas;
                                                 --ops: each Quickr case's ops dispatched, exact, per build
    bench.py qbasic [--json F]                   qbasic/BENCH.BAS in QBasic 1.1 and our IDE under DOSBox
    bench.py ops                                 each op's runs over the suite, the corpus, conformance's
                                                 cases and Quickr's: which are cold
    bench.py profile FILE.bas | -e CODE [-n N] [--link | --ide]  DOS: the functions a program spends its
                                                 instructions in; --link: RUN's link alone
    bench.py engine                              docs/engine-design.md's Today table, measured on CHARLIE's source
    bench.py fps [--real] [--profile | --parts]  MOTHLITE.BAS played by an autopilot on a 486 at 10k cycles/ms
                                                 with the Sound Blaster: its frame rate in each mode;
                                                 --parts: each step of its loop's ms a frame
    bench.py gfx [--depth D] [--seconds S] [--dos]  gfx's frames a second in the Mac app, drawn in pages
                                                 against played on the GPU, for each of GFX_WORK's frames;
                                                 --dos: in pages on DOS

Native (the default) runs build/hostperf/basic for retired instructions, the least of RUNS runs less
an empty program's, builds compared run interleaved; and build/host/basic for the ops dispatched,
which are exact. --dos runs build/c/basic32.exe under dosrun, whose instruction count is exact.
A REV is built once in its own worktree under build/bench/.
"""

import os
import re
import sys
import json
import time
import shutil
import struct
import hashlib
import argparse
import tempfile
import subprocess
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import mkhelp  # noqa: E402
from dataclasses import dataclass

sys.path.insert(0, str(Path(__file__).resolve().parent))  # run with -I too
import doslink
import dosrun
import basic_src

ROOT = Path(__file__).resolve().parent.parent
SUITE = ROOT / "tools/bench"
QUICKR = "' OPTION DIALECT QUICKR\n"  # a Quickr file's first line
QR = SUITE / "qr"  # QBasic twins, NAME.bas, of Quickr's cases, quickr/NAME.bas
BASELINE = SUITE / "baseline.json"
REVS = ROOT / "build/bench"
EXES = ("build/c/basic32.exe", "build/c/ide32.exe")
INSTRUMENT_SRC = ("src/plat/host/app/insns.c", "src/plat/host/pfpu.c", "src/plat/pfpu.h")  # arm64 too
RUNS = 3
INSNS_SLACK = 0.01  # on the native CPU a count repeats to 0.1%

type Programs = dict[str, str]


class NoCounter(RuntimeError):
    """The machine will not count retired instructions (insns.c said so): no count, not a count of 0."""


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
        targets = ["build/hostperf/basic", "build/host/basic"] + ["all"] * dos
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
        rules = hashlib.sha1((ROOT / "Makefile").read_bytes()).hexdigest()[:8]
        b = Build(REVS / f"{sha}-{rules}")  # our Makefile builds it: another's objects mislink
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
    if re.search(rb" in line .* in module ", p.stdout):  # stopped by an error: its cost is not the program's
        sys.exit(f"{bas.stem}: {p.stdout.decode('latin-1').strip()}")
    if re.search(rb"\n" + key + rb" unavailable\n", p.stderr):
        raise NoCounter(f"{exe}: no {key.decode()} counter on this machine")
    m = re.search(rb"\n" + key + rb" (\d+)\n", p.stderr)
    if not m:
        sys.exit(f"{bas.stem}: no {key.decode()} count from {exe}\n{p.stderr.decode(errors='replace')}")
    return int(m[1]), p.stdout.decode("latin-1").strip()


def run_natives(builds: list[Build], progs: Programs, runs: int = RUNS, quickr: bool = False) -> list[dict[str, Cost]]:
    """Each build's costs, their runs interleaved: the machine's load drifts alike for all. quickr: the
    programs are Quickr's, as they are; the baseline the dialect's line alone."""
    res: list[dict[str, Cost]] = [{} for _ in builds]
    with tempfile.TemporaryDirectory() as d:
        for name, prog in {"": QUICKR if quickr else "", **progs}.items():
            bas = Path(d) / f"{name[:8].upper() or 'EMPTY'}.BAS"
            bas.write_bytes(prog.encode("latin-1") if quickr else source(prog))
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
    dosrun.COM = b.dos
    with tempfile.TemporaryDirectory() as d:
        r = dosrun.run(
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


def image(exe: Path) -> int:
    """The bytes an LE executable loads: its objects' sizes, code, data and BSS; not its fixups or
    the pages' padding, which move with layout."""
    b = exe.read_bytes()
    le = b.find(b"LE\0\0")
    if le < 0:
        sys.exit(f"{exe}: not an LE executable")
    table, n = struct.unpack_from("<II", b, le + 0x40)
    return sum(struct.unpack_from("<I", b, le + table + 24 * i)[0] for i in range(n))


def sizes(b: Build) -> dict[str, int]:
    return {e: image(b.root / e) for e in EXES}


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
    progs = {f"e{i}": e for i, e in enumerate(a.e)} | {Path(f).stem: Path(f).read_text() for f in a.file} or suite()
    builds = [build(r, a.dos) for r in names]
    res = [run_dos(b, progs) for b in builds] if a.dos else run_natives(builds, progs, a.runs)
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


def cmd_qr(a: argparse.Namespace) -> int:
    """Per build, each Quickr case's instructions and ops against its QBasic twin's: Quickr's over
    QBasic's. The two must print the same values."""
    names: list[str] = a.revs or ["."]
    cases = sorted((QR / "quickr").glob("*.bas"))
    builds = [build(r, False) for r in names]
    qr = run_natives(builds, {p.stem: p.read_text() for p in cases}, quickr=True)
    qb = run_natives(builds, {p.stem: (QR / p.name).read_text() for p in cases})
    pairs = list(zip(qr, qb, strict=True))
    if a.ops:
        print(f"{'':10}" + "".join(f"{'tree' if n == '.' else n:>16}" for n in names))
        for k in qr[0]:
            print(f"{k:10}" + "".join(f"{q[k].ops:>16,}" for q in qr))
        return 0
    print(f"{'':10}" + "".join(f"{'tree' if n == '.' else n:>40}" for n in names))
    print(f"{'':10}" + f"{'insns':>14}{'QBasic':>14}{'x':>6}{'ops x':>6}" * len(names))
    for k in qr[0]:
        row = "".join(
            f"{q[k].insns:>14,}{b[k].insns:>14,}{q[k].insns / b[k].insns:>6.2f}{q[k].ops / b[k].ops:>6.2f}"
            for q, b in pairs
        )
        same = all(q[k].out.split() == b[k].out.split() for q, b in pairs)
        print(f"{k:10}{row}" + ("" if same else "  OUTPUT DIFFERS"))
    return 0


def each_op(jobs: dict[str, str | tuple], files: dict[str, bytes] | None = None) -> dict[str, int]:
    """Each op's runs over the jobs (dosrun's) on the host build, by name; an X op's "X.NAME"."""
    os.environ["QB_EACH_OP"] = "1"
    dosrun.COM = Build(ROOT).counted
    with tempfile.TemporaryDirectory() as d:
        batches = [{"jobs": b, "limit_ms": 60000, "file": True, "files": files} for b in dosrun.split(jobs)]
        res = dosrun.run_all(batches, Path(d))
    total: dict[str, int] = {}
    for r in res.values():
        for kind, name, n in re.findall(r"^(x?op) (\w+) (\d+)$", r.screen, re.M):
            key = f"X.{name}" if kind == "xop" else name
            total[key] = total.get(key, 0) + int(n)
    return total


def cmd_ops(a: argparse.Namespace) -> int:
    """Each op's runs per corpus, the least run by QBasic's programs first."""
    import conformance

    Build(ROOT).make(False)
    bcg = sorted((ROOT / "tests/corpus/bcg").glob("*.bas"))
    conf = conformance.cases()
    quickr = sorted((QR / "quickr").glob("*.bas")) + sorted((ROOT / "tests/qr").glob("*.bas"))
    cols = {
        "suite": each_op({k: source(v).decode("latin-1") for k, v in suite().items()}),
        "bcg": each_op({p.stem: (conformance.text(p), stdin(p)) for p in bcg}),
        "conformance": each_op({k: v[0] for k, v in conf.items()}, conformance.COMPANIONS),
        "quickr": each_op({p.stem: p.read_text() for p in quickr}),
    }

    def qbasic(n: str) -> int:
        return sum(cols[c].get(n, 0) for c in cols if c != "quickr")

    print(f"{'':14}" + "".join(f"{c:>16}" for c in cols))
    for n in sorted(set().union(*cols.values()), key=lambda n: (n.startswith("X."), qbasic(n), n)):
        print(f"{n:14}" + "".join(f"{cols[c].get(n, 0):>16,}" for c in cols))
    return 0


def stdin(bas: Path) -> str:
    """The input beside bas, NAME.in, its lines ended by CR LF; none, ""."""
    f = bas.with_suffix(".in")
    return f.read_text(encoding="latin-1").replace("\r\n", "\n").replace("\n", "\r\n") if f.exists() else ""


def functions(b: Build) -> dict[str, int]:
    """Each DOS object's functions' bytes, by object and name: COFF gives no sizes, so each runs to
    the next function, the last to .text's end."""
    objs = b.root / "build/c/obj"
    res: dict[str, int] = {}
    for o in sorted(objs.rglob("*.obj")):
        heads = subprocess.run([os.environ.get("OBJDUMP", "objdump"), "-h", o], capture_output=True, text=True).stdout
        text = re.search(r"^\s*\d+ \.text\s+([0-9a-f]+)", heads, re.M)
        fns = [(off, n) for off, k, n in nm(o) if k in "tT"]
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
    """ops and outputs against the baseline's record, exact; retired instructions and loaded bytes
    against the baseline's rev built and run now, beside ours."""
    b = build(".", True)
    if a.save:
        now = run_native(b, suite())
        record = {"rev": git("rev-parse", "--short", "HEAD"), "cost": {k: vars(c) for k, c in now.items()}}
        BASELINE.write_text(json.dumps(record, indent=1) + "\n")
        print(f"saved {BASELINE.relative_to(ROOT)}")
        return 0

    base = json.loads(BASELINE.read_text())
    base_build = build(base["rev"], True)
    then, now = run_natives([base_build, b], suite())
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
    old = sizes(base_build)
    for e, s in sizes(b).items():
        was_s = old[e]
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
        (work / "BASIC.HLP").write_bytes(mkhelp.compiled())
        cmds = "AUTOTYPE -w 4 f5\nIDE32 BENCH.BAS"
    (work / "b.conf").write_text(DOSBOX_CONF.format(work=work, cmds=cmds))
    p = subprocess.Popen(
        [str(dosrun.DOSBOX), "-nolog", "-conf", str(work / "b.conf")],
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
    if a.json:
        a.json.write_text(json.dumps({"qbasic": qb, "ours": us}, indent=1) + "\n")
    print(f"{'total':10}{tq:>10.2f}{tu:>10.2f}{tq / tu if tu else float('nan'):>9.1f}x")
    return 0


OBJCOPY = os.environ.get("OBJCOPY", shutil.which("llvm-objcopy") or "/opt/homebrew/opt/llvm/bin/llvm-objcopy")


def nm(o: Path) -> list[tuple[int, str, str]]:
    """Object o's defined symbols: offset, kind (t local, T global, ...), name."""
    out = subprocess.run([os.environ.get("NM", "nm"), "-n", "--defined-only", o], capture_output=True, text=True).stdout
    return [(int(f[0], 16), f[1], f[2]) for f in (ln.split() for ln in out.splitlines()) if len(f) == 3]


def with_statics(listing: str, objs: list[Path]) -> str:
    """The link map, each module's static functions added: the map lists globals alone, so the
    profiler counted a static's instructions under the global before it. A static's address is its
    module's code base, from a global the map and the object both place, plus its offset."""
    lines = listing.splitlines()
    by_path = {str(o.resolve()): o for o in objs}  # the map's paths are resolved
    out: list[str] = []
    i = 0
    while i < len(lines):
        out.append(lines[i])
        m = re.match(r"Module: (.*?)\(", lines[i])
        i += 1
        if not m or m[1] not in by_path:
            continue
        placed: dict[str, int] = {}
        while i < len(lines) and re.match(r"[0-9a-f]{4}:", lines[i]):
            at = re.match(r"0001:([0-9a-f]{8})\*?\s+(\S+)", lines[i])
            if at:
                placed[at[2]] = int(at[1], 16)
            out.append(lines[i])
            i += 1
        syms = nm(by_path[m[1]])
        bases = {placed[n] - off for off, k, n in syms if k == "T" and n in placed}
        if len(bases) == 1:
            base = bases.pop()
            out += [f"0001:{base + off:08x}  {n}" for off, k, n in syms if k == "t"]
    return "\n".join(out) + "\n"


def apart(objs: list[Path], d: Path) -> list[Path]:
    """objs copied into d, each name they define but the entry renamed in all of them: jwlink binds a static by its
    name, so the slice's call to its gfx_init ran cngfx.c's static one, and display.open failed."""
    clash = {n for o in objs for _, _, n in nm(o) if n != "_qx_init_gfx" and not n.startswith(("@", ".", "__real@", "??_C@"))}
    renames = [a for n in sorted(clash) for a in ("--redefine-sym", f"{n}=_slice{n}")]
    (d / "slice").mkdir(exist_ok=True)
    out = []
    for i, o in enumerate(objs):
        out.append(d / "slice" / f"{i}_{o.name}")
        subprocess.run([OBJCOPY, *renames, str(o), str(out[-1])], check=True)
    return out


def profiled(b: Build, d: Path, ide: bool = False) -> Path:
    """b's basic32 (ide: ide32) relinked in d with a map that names its static functions too."""
    var = "IDE_OBJ" if ide else "BASIC_OBJ"
    db = subprocess.run(
        ["make", "-pnq", "build/c/ide32.exe" if ide else "build/c/basic32.exe"],
        cwd=b.root,
        capture_output=True,
        text=True,
    ).stdout
    m = re.search(rf"^{var} := (.*)$", db, re.M)
    g = re.search(r"^GFX_DOS_OBJ := (.*)$", db, re.M)
    if not m or not g:
        sys.exit(f"profile: make printed no {var} or GFX_DOS_OBJ")
    # gfx's slice linked in, its loader's PEXT_LINKED pext.c taking it: the map names gfx's code too. Its own C library
    # bits are the program's.
    linked = b.root / "build/c/prof/pext.obj"
    subprocess.run(["make", str(linked.relative_to(b.root))], cwd=b.root, check=True, capture_output=True)
    objs = [linked if o.endswith("plat/dos32/pext.obj") else b.root / o for o in m.group(1).split()]
    objs += apart([b.root / o for o in g.group(1).split()], d)
    exe = d / ("ide32.exe" if ide else "basic32.exe")
    listing = exe.with_suffix(".map")
    doslink.link(exe, objs, listing)
    listing.write_text(with_statics(listing.read_text(), objs))
    shutil.copy(b.dos.parent / "DOS32A.EXE", d / "DOS32A.EXE")
    return exe


def stopped(r: dosrun.Result) -> str:
    """Why a profiled run stopped short, if it printed or did not exit: its profile is the stop's."""
    return f"{r.reason}: {r.out.strip()[-300:]}" if r.out.strip() or r.reason != "exit" else ""


def cycles_line(ms: int) -> str:
    """ms of dosrun.CONF's virtual time as cycles, and seconds at 10k cycles/ms: a rep's every
    iteration counted, which the instruction count takes as one."""
    rate = int(re.search(r"virtual time rate=(\d+)", dosrun.CONF)[1])
    return f"{ms * rate:,} cycles: {ms * rate / 10_000_000:.2f} s at 10k cycles/ms"


def cmd_profile(a: argparse.Namespace) -> int:
    """The DOS build relinked with a map, DOSBox's profiler naming each function's own instructions:
    the program's less an empty one's. --link: its lines typed, then RUN, less them typed alone: the
    link before the run (a numbered program that ENDs at once). --ide: the IDE's open of the file,
    left waiting for keys till --ms, less an empty file's."""
    text = program(a)
    jobs: dict[str, str | tuple] = {"": text, "prog": text + "\nRUN\n"} if a.link else {"": "", "prog": text}
    with tempfile.TemporaryDirectory() as d:
        b = build(".", True)
        gfx = b.dos.parent / "lib/gfx.qbx"
        dosrun.COM = profiled(b, Path(d), a.ide)
        r = dosrun.run(
            jobs,
            Path(d) / "w",
            files={"lib/gfx.qbx": gfx.read_bytes()} if gfx.exists() else None,
            limit_ms=a.ms if a.ide else 600000,
            file=not a.link,
            profile=True,
            mem=a.mem,
            keyboard=a.ide,
            screen=a.ide,
        )
    base, got = (r[""].incls, r["prog"].incls) if a.inclusive else (r[""].selfs, r["prog"].selfs)
    own = {f: n - base.get(f, 0) for f, n in got.items() if n > base.get(f, 0)}
    total = sum(v for f, v in own.items() if not a.inclusive or f in r["prog"].selfs) if not a.inclusive else (
        sum(n - r[""].selfs.get(f, 0) for f, n in r["prog"].selfs.items() if n > r[""].selfs.get(f, 0)))
    print(f"{total:,} instructions ({r['prog'].instructions - r[''].instructions:,} counted)")
    print(cycles_line(r["prog"].ms - r[""].ms))
    if stopped(r["prog"]):
        print(stopped(r["prog"]))
    for f, n in sorted(own.items(), key=lambda kv: -kv[1])[: a.n]:
        calls = r["prog"].calls.get(f, 0) - r[""].calls.get(f, 0)
        print(f"{n:>12,} {n / total:6.1%} {calls:>9,} calls  {f}")
    return 0


# fills: each case's draw, a statement on canvas& from tex& (64 by 64) or q(), and the pixels it covers. The quad is the
# whole screen, its texels a turned parallelogram inside tex&, so u and v both step along a row.
FILLS_CASES: dict[str, tuple[str, str, int]] = {
    "textured": ("", "gfx.draw.poly canvas&, q(), 0, 4, tex&, 0, 0", 64000),
    "lit": ("gfx.draw.set_light 31", "gfx.draw.poly canvas&, q(), 0, 4, tex&, 0, 0", 64000),
    "lit opaque": ("gfx.draw.set_light 31", "gfx.draw.poly canvas&, q(), 0, 4, tex&, 0, gfx.draw.OPAQUE", 64000),
    "lit persp": ("gfx.draw.set_light 31", "gfx.draw.poly canvas&, q(), 0, 4, tex&, 0, gfx.draw.PERSPECTIVE", 64000),
    "added": (
        "gfx.draw.set_alpha 32", "gfx.draw.rect canvas&, 0, 0, 319, 199, warm&, gfx.draw.FILL + gfx.draw.ADD", 64000
    ),
    "rect": ("", "gfx.draw.rect canvas&, 0, 0, 319, 199, warm&, gfx.draw.FILL", 64000),
    "sprites": ("", "for k = 0 to 14\ngfx.draw.sprite canvas&, (k mod 5) * 64, (k \\ 5) * 64, tex&, 0\nnext", 61440),
    "flipped": (
        "",
        "for k = 0 to 14\ngfx.draw.sprite canvas&, (k mod 5) * 64, (k \\ 5) * 64, tex&, gfx.draw.FLIP_X\nnext",
        61440,
    ),
    "scaled": (
        "",
        "for k = 0 to 5\ngfx.draw.sprite_turned canvas&, 1 + (k mod 3) * 100, 1 + (k \\ 3) * 100, 0, 1.5, tex&, 0"
        "\nnext",
        55296,
    ),
    "blended": ("gfx.draw.set_alpha 32", "gfx.draw.poly canvas&, q(), 0, 4, tex&, 0, gfx.draw.BLEND", 64000),
    "solid": ("gfx.draw.set_tint warm&", "gfx.draw.poly canvas&, q(), 0, 4, tex&, 0, gfx.draw.SOLID", 64000),
    "smooth": ("", "gfx.draw.poly canvas&, q(), 0, 4, tex&, 0, gfx.draw.SMOOTH", 64000),
    "depth": ("", "gfx.draw.poly canvas&, q(), 0, 4, tex&, 0, gfx.draw.DEPTH_WRITE", 64000),
}
FILLS_DRAWS = 4
# the texture's two colours and the shapes', by depth
FILLS_COLOURS = {8: (7, 15, 42), 16: (0x808080, 0xC0C0C0, 0xFF9030)}


def fills_program(bits: int, case: str, draws: int) -> str:
    """case's draw a warm-up and draws more times at bits; its z far at the foot for PERSPECTIVE."""
    setup, draw, _ = FILLS_CASES[case]
    plain, band, warm = FILLS_COLOURS[bits]
    corners = ((0, 0, 8, 0), (320, 0, 63, 8), (320, 200, 55, 63), (0, 200, 0, 55))
    lines = [
        "import gfx.display, gfx.surface, gfx.draw",
        "from gfx.draw import Vertex",
        "dim q(3) as Vertex",
        f"canvas& = gfx.display.open(320, 200, {bits}, 0)",
        "tex& = gfx.surface.new(64, 64)",
        f"gfx.draw.clear tex&, {plain}",
        f"gfx.draw.rect tex&, 8, 8, 55, 55, {band}, gfx.draw.FILL",
        f"warm& = {warm}",
    ]
    for i, (x, y, u, v) in enumerate(corners):
        lines += [f"q({i}).x = {x}", f"q({i}).y = {y}", f"q({i}).z = {1 + i // 2}"]
        lines += [f"q({i}).u = {u}", f"q({i}).v = {v}"]
    return "\n".join([*lines, setup, f"for i = 0 to {draws}", draw, "next", "gfx.display.close", ""])


def cmd_fills(a: argparse.Namespace) -> int:
    """Each FILLS_CASES draw's instructions a pixel on DOS, at 8 and 16 bits: all of them, and each function's own
    among the top. A case is run with FILLS_DRAWS more draws and with none, the same warm-up in both, so all else
    cancels."""
    cases, depths = a.case or list(FILLS_CASES), a.bits or list(FILLS_COLOURS)
    jobs = {f"{bits} {case} {n}": fills_program(bits, case, n) for bits in depths for case in cases
            for n in (0, FILLS_DRAWS)}
    with tempfile.TemporaryDirectory() as d:
        b = build(".", True)
        dosrun.COM = profiled(b, Path(d))
        qbx = (b.dos.parent / "lib/gfx.qbx").read_bytes()
        batches = [{"jobs": {k: v}, "limit_ms": 600000, "file": True, "files": {"lib/gfx.qbx": qbx}, "mem": 16,
                    "profile": True} for k, v in jobs.items()]
        r = dosrun.run_all(batches, Path(d))
    print(f"{'':16}{'a pixel':>8}  its own, the top {a.n}")
    for bits in depths:
        for case in cases:
            base, got = r[f"{bits} {case} 0"], r[f"{bits} {case} {FILLS_DRAWS}"]
            if stopped(got):
                print(f"{bits:>2} {case:13}{stopped(got)}")
                continue
            own = {f: n - base.selfs.get(f, 0) for f, n in got.selfs.items() if n > base.selfs.get(f, 0)}
            pixels = FILLS_CASES[case][2] * FILLS_DRAWS
            top = sorted(own.items(), key=lambda kv: -kv[1])[: a.n]
            calls = {f: got.calls.get(f, 0) - base.calls.get(f, 0) for f, _ in top}
            parts = "  ".join(f"{f.removeprefix('_slice_')} {n / pixels:.1f} ({calls[f]:,})" for f, n in top)
            print(f"{bits:>2} {case:13}{sum(own.values()) / pixels:8.1f}  {parts}")
    return 0


MOTH = ROOT / "src/plat/wasm/web/demos/MOTHLITE.BAS"
# a 486 at 10000 cycles a ms, the Sound Blaster on: by virtual time, or --real by the host's clock
FPS_CPU = "core=dynamic\ncputype=486\nvirtual time=true\nvirtual time rate=10000\n"
FPS_REAL = "core=dynamic\ncputype=486\ncycles=fixed 10000\n"
FPS_FRAMES = 4000
# flaps toward the next gap; quits 2 s after the game is over, stops flying after 40 s
AUTOPILOT = """
SUB Auto (w AS World, st() AS Stone, k AS Pad)
    DIM i AS INTEGER, best AS SINGLE, target AS SINGLE
    SELECT CASE w.g.mode
    CASE TITLE
        IF w.g.modeT > 3 AND w.g.toTitle = 0 AND w.g.quitting = 0 THEN k.flap = -1
    CASE PLAYING
        IF w.g.modeT > 40 THEN EXIT SUB
        best = 9999: target = 100
        FOR i = 0 TO NSTONE - 1
            IF st(i).x + STONEW + 12 > w.m.x AND st(i).x < best THEN best = st(i).x: target = st(i).gap + 10
        NEXT
        IF w.m.y > target AND w.m.vy > 0 THEN k.flap = -1
    CASE OVER
        IF w.g.modeT > 2 THEN w.g.quitting = -1: w.sky.fadeTo = 0
    END SELECT
END SUB
"""


# the loop's steps --parts times: Main's and Render's calls
FPS_PARTS = ("ReadPad joy", "Tick w, joy, stones(), sparks()", "Fly w, k, st(), sp()", "Orbit w, k, st(), sp()",
             "Trail w, sp()", "EaseSky w.sky, cur()", "FeedSong w.song", "DrawSky w.g", "DrawSun w.g.clock", "DrawTop w",
             "DrawLand w.g", "DrawStones st(), w.g.clock", "DrawMoth w.m, w.g.mode", "DrawLake w.g.clock", "DrawSparks sp()",
             "DrawHud w", "DrawFx w.fx", "Flip w.page")


def timed(s: str) -> str:
    """Each FPS_PARTS call's TIMER time added to PartT(its index), written to PARTS.TXT by Main; its own start, as
    one may be inside another."""
    s = s.replace("\nMain\nEND\n", "\nDIM SHARED PartT(30) AS SINGLE, PartAt(30) AS SINGLE\nMain\nEND\n")
    for i, call in enumerate(FPS_PARTS):
        n = s.count(f" {call}\n")
        assert n, call
        s = s.replace(f" {call}\n", f" PartAt({i}) = TIMER: {call}: PartT({i}) = PartT({i}) + TIMER - PartAt({i})\n")
    return s.replace('    CLOSE #1\nEND SUB\n', f'    CLOSE #1\n    OPEN "PARTS.TXT" FOR OUTPUT AS #1\n'
                     f'    FOR i = 0 TO {len(FPS_PARTS) - 1}: PRINT #1, PartT(i) * 1000 / fr: NEXT\n    CLOSE #1\nEND SUB\n')


def fps_program(parts: bool = False, one: bool = False) -> str:
    """MOTHLITE.BAS with the autopilot pressing its key, each frame's TIMER and mode kept, written to FPS.TXT at the end;
    one: a single frame, the start-up's cost."""
    s = MOTH.read_text(encoding="latin-1").replace("\r\n", "\n")
    edits = [
        ("DECLARE SUB Main ()\n", "DECLARE SUB Main ()\nDECLARE SUB Auto (w AS World, st() AS Stone, k AS Pad)\n"),
        ("    DIM now AS SINGLE, last AS SINGLE, acc AS SINGLE\n",
         f"    DIM now AS SINGLE, last AS SINGLE, acc AS SINGLE\n    DIM ft({FPS_FRAMES}) AS SINGLE, fm({FPS_FRAMES}) AS INTEGER, fr AS INTEGER\n"),
        ("        ReadPad joy\n", "        ReadPad joy\n        Auto w, stones(), joy\n"),
        ("        Render w, stones(), sparks()\n    LOOP",
         f"        Render w, stones(), sparks()\n        IF fr < {FPS_FRAMES} THEN fr = fr + 1: ft(fr) = TIMER: fm(fr) = w.g.mode\n    LOOP"),
        ("    CloseScreen\nEND SUB\n",
         '    CloseScreen\n    OPEN "FPS.TXT" FOR OUTPUT AS #1\n    FOR i = 1 TO fr: PRINT #1, ft(i); fm(i): NEXT\n    CLOSE #1\nEND SUB\n'),
    ]
    if one:
        edits.append(("    LOOP UNTIL w.g.quitting AND w.sky.fade <= 0\n", "    LOOP UNTIL -1\n"))
    for old, new in edits:
        assert s.count(old) == 1, old
        s = s.replace(old, new)
    return (timed(s) if parts else s) + AUTOPILOT


# --each: a call's ms on its own, in a game playing with a stone of each kind and sparks out, its music off
FPS_EACH = ("Tick w, joy, st(), sp()", "cur(0, 0) = (n + q) MOD 2 * 9: ApplyPalette w.sky, cur()", "DrawSky w.g",
            "DrawSun w.g.clock", "DrawTop w", "DrawLand w.g", "DrawStones st(), w.g.clock",
            "DrawStone st(), 0, w.g.clock", "DrawStone st(), 1, w.g.clock", "DrawStone st(), 2, w.g.clock",
            "DrawStone st(), 3, w.g.clock", "DrawMoth w.m, w.g.mode", "DrawLake w.g.clock", "DrawSparks sp()",
            "DrawHud w", "Flip w.page", "Render w, st(), sp()")
EACH = """OpenScreen
LoadTables
LoadTerrain
BuildGlyphs
LoadMoth
BuildSprites
BuildLand
SparkPaths
DIM w AS World, joy AS Pad, cur(NKEY - 1, 2) AS SINGLE
DIM st(NSTONE - 1) AS Stone, sp(NSPARK - 1) AS Spark
DIM i AS INTEGER, q AS INTEGER, n AS LONG, t0 AS SINGLE
NewWorld w, cur()
Launch w, st(), sp()
StopSong w.song
w.song.playing = 0
w.g.scroll = 1000: w.g.score = 12
OPEN "EACH.TXT" FOR OUTPUT AS #1
"""


# Calls between TIMER readings: on DOS a reading takes about a millisecond of the PIT's ports, and calls under that
# read as 0 or 1 ms.
EACH_REPS = 10


def each_program() -> str:
    """MOTHLITE.BAS's Main swapped for a loop of each FPS_EACH call (after an empty one) for 2 s, its ms a call to
    EACH.TXT; the scene set again each time round."""
    head, rest = MOTH.read_text(encoding="latin-1").replace("\r\n", "\n").split("\nMain\nEND\n", 1)
    rest = rest.replace("RANDOMIZE TIMER", "RANDOMIZE 5")
    reset = "    FOR i = 0 TO NSTONE - 1: st(i).x = 10 + i * 90: st(i).kind = 1 + i MOD 4: st(i).gap = 95: NEXT\n"
    reset += "    IF (n + q) MOD 60 = 0 THEN Emit sp(), w.m, BSCORE\n    w.m.y = 100: w.m.vy = 0: w.g.mode = PLAYING\n"
    loops = "".join(f"n = 0: t0 = TIMER\nDO\n    FOR q = 1 TO {EACH_REPS}\n    {call}\n{reset}    NEXT\n"
                    f"    n = n + {EACH_REPS}\nLOOP UNTIL TIMER - t0 >= 2\n"
                    "PRINT #1, (TIMER - t0) / n * 1000; n\n" for call in ("REM", *FPS_EACH))
    return head + "\n" + EACH + loops + "CLOSE #1\nEND\n" + rest


def cmd_fps(a: argparse.Namespace) -> int:
    """Frames a second in each mode (title, play, dying, over), with the longest frame, from the frames' own TIMER readings."""
    build(".", True)
    conf = dosrun.CONF
    dosrun.CONF = conf.replace("core=normal\ncputype=pentium\nvirtual time=true\nvirtual time rate=20000\n",
                               FPS_REAL if a.real else FPS_CPU)
    assert dosrun.CONF != conf
    if a.profile:
        dosrun.CONF = dosrun.CONF.replace("core=dynamic", "core=normal")
    if a.nosb:
        dosrun.CONF += "[sblaster]\nsbtype=none\n"
    with tempfile.TemporaryDirectory() as d:
        if a.profile:
            dosrun.COM = profiled(build(".", True), Path(d))
        if a.each:
            dosrun.run({"each": each_program()}, Path(d) / "w", limit_ms=600000, file=True, mem=16)
            rows = [ln.split() for ln in (Path(d) / "w/EACH.TXT").read_text().splitlines()]
            got = [(float(ms), int(n)) for ms, n in rows]
            dosrun.CONF = conf
            for call, (ms, n) in zip(FPS_EACH, got[1:]):
                print(f"{ms - got[0][0]:>8.2f} ms {n:>7} calls  {call}")
            return 0
        # the start-up's first: the game's FPS.TXT is the one left
        jobs = ({"start": fps_program(one=True)} if a.profile else {}) | {"moth": fps_program(a.parts)}
        r = dosrun.run(jobs, Path(d) / "w", limit_ms=600000, file=True, mem=16, profile=a.profile)
        log, parts = Path(d) / "w/FPS.TXT", Path(d) / "w/PARTS.TXT"
        rows = [ln.split() for ln in log.read_text().splitlines()] if log.exists() else []
        if a.parts and parts.exists():
            for call, ms in zip(FPS_PARTS, parts.read_text().split()):
                print(f"{float(ms):>8.1f} ms  {call}")
    dosrun.CONF = conf
    if not rows:
        print(f"no FPS.TXT: {r['moth'].reason} {r['moth'].out[:400]}")
        return 1
    frames = [(float(t), int(m)) for t, m in rows]
    print(f"{'mode':8}{'frames':>8}{'fps':>8}{'worst ms':>10}")
    for mode, name in enumerate(("title", "play", "dying", "over")):
        dts = [b[0] - a_[0] for a_, b in zip(frames, frames[1:]) if b[1] == mode and b[0] >= a_[0]]
        if dts:
            print(f"{name:8}{len(dts):>8}{len(dts) / sum(dts):>8.1f}{max(dts) * 1000:>10.0f}")
    if a.profile:  # the frames' own: less the start-up's
        got, base = r["moth"], r["start"]
        sel = {f: n - base.selfs.get(f, 0) for f, n in got.selfs.items() if n > base.selfs.get(f, 0)}
        total = sum(sel.values())
        print(f"{total:,} instructions over {len(frames)} frames: {total // max(len(frames), 1):,} a frame")
        for f, n in sorted(sel.items(), key=lambda kv: -kv[1])[: a.n]:
            calls = got.calls.get(f, 0) - base.calls.get(f, 0)
            print(f"{n:>12,} {n / total:6.1%} {calls:>9,} calls  {f}")
    return 0

CHARLIE = ROOT / "src/plat/wasm/web/demos/CHARLIE.BAS"
# the game's own pilot playing from the start; each call for 2 s, a second of play between them
CHARLIE_STEP = "Pilot wd.lv, wd.gr, wd.zn, wd.se, p, k, wd.pilot: Tick wd, g, p, k, lo(), pt()"
CHARLIE_EACH = (CHARLIE_STEP, "DrawBack wd.zn, wd.art, cx, cy",
                "DrawThings wd.lv, wd.zn, wd.art, wd.kinds, wd.se, cx, cy, -1", "DrawTiles wd.lv, wd.zn, wd.art, cx, cy",
                "DrawThings wd.lv, wd.zn, wd.art, wd.kinds, wd.se, cx, cy, 0", "DrawHero wd.gr, wd.art, wd.se, p, cx, cy",
                "DrawParts pt(), cx, cy", "Blit wd.art, 6, 5, IHUD, HUDW, HUDH", "Flip g.page", "Render wd, g, p, lo(), pt()")
CHARLIE_IMPORTS = "".join(f"FROM {'' if m.startswith('engine.') else 'barkley.'}{m} IMPORT {n}\n" for m, n in (
    ("engine.video", "Blit, Flip, OpenScreen"),
    ("model.images", "IHUD"),
    ("model.hero", "Hero, Pad"),
    ("model.game", "Game, Loose, NLOOSE, NPART, Part"),
    ("model.kinds", "LoadKinds"),
    ("assets.store", "BuildArt, HUDH, HUDW"),
    ("assets.tunes", "LoadSongs"),
    ("world.terrain.ground", "LoadTables"),
    ("world.level", "LoadLevel"),
    ("world.backdrop", "DrawBack, DrawTiles"),
    ("stage.hero.draw", "DrawHero"),
    ("stage.scene", "DrawParts, DrawThings, Render"),
    ("control.pilot", "Pilot"),
    ("flow.states", "StartGame, Tick"),
))
CHARLIE_SETUP = """OpenScreen
LoadTables wd.lv
BuildArt wd.art, wd.tn
LoadKinds wd.kinds, wd.dat
LoadLevel wd, 0
DIM g AS Game, p AS Hero, k AS Pad, lo(NLOOSE - 1) AS Loose, pt(NPART - 1) AS Part
DIM q AS INTEGER, n AS LONG, t0 AS SINGLE, cx AS INTEGER, cy AS INTEGER
LoadSongs wd.mu
wd.mu.muted = -1
g.page = 1
StartGame wd, g, p, lo(), pt()
OPEN "EACH.TXT" FOR OUTPUT AS #1
"""


# A frame's draws besides its clear, on a 640 by 480 canvas; f the frame, s& a 32 by 32 picture drawn, pic&() 16
# written, m& a colour's mask: 255 at 8 bits; p() a polygon's points, their texels s&'s corners, ox() and oy() an
# octagon's. calls: the shapes' loop with three calls that draw nothing, the interpreter's share.
GFX_WORK = {
    "flip": "",
    "calls": "for i = 0 to 299\n"
    "    x = (i * 37 + f) mod 600: y = (i * 53) mod 440\n"
    "    gfx.draw.set_light 63\n    gfx.draw.set_light 63\n    gfx.draw.set_light 63\n"
    "next",
    "fill": "for i = 0 to 9\n    gfx.draw.rect canvas&, i, i, 629 + i, 469 + i, (i * 4099) and m&, gfx.draw.FILL\nnext",
    "rects": "for i = 0 to 299\n"
    "    x = (i * 37 + f) mod 600: y = (i * 53) mod 440\n"
    "    gfx.draw.rect canvas&, x, y, x + 39, y + 39, (i * 4099) and m&, gfx.draw.FILL\n"
    "next",
    "lines": "for i = 0 to 299\n"
    "    x = (i * 37 + f) mod 600: y = (i * 53) mod 440\n"
    "    gfx.draw.line canvas&, x, y, 639 - x, 479 - y, (i * 977) and m&\n"
    "next",
    "circles": "for i = 0 to 299\n"
    "    x = (i * 37 + f) mod 600: y = (i * 53) mod 440\n"
    "    gfx.draw.circle canvas&, 639 - x, y, 12, (i * 131) and m&, gfx.draw.FILL\n"
    "next",
    "arcs": "for i = 0 to 299\n"
    "    x = (i * 37 + f) mod 600: y = (i * 53) mod 440\n"
    "    gfx.draw.arc canvas&, 639 - x, y, 20, 0.3, 4, (i * 131) and m&\n"
    "next",
    "sprites": "for i = 0 to 999\n    gfx.draw.sprite canvas&, (i * 37 + f) mod 608, (i * 53 + f) mod 448, s&, 0\nnext",
    "pictures": "for i = 0 to 999\n"
    "    gfx.draw.sprite canvas&, (i * 37 + f) mod 608, (i * 53 + f) mod 448, pic&(i mod 16), 0\n"
    "next",
    "turned": "for i = 0 to 99\n"
    "    x = (i * 37 + f) mod 600: y = (i * 53) mod 440\n"
    "    gfx.draw.sprite_turned canvas&, x + 16, y + 16, (i + f) * 0.05, 2, s&, 0\n"
    "next",
    "tris": "for i = 0 to 299\n"
    "    x = (i * 37 + f) mod 600: y = (i * 53) mod 440\n"
    "    p(0).x = x: p(0).y = y: p(1).x = x + 40: p(1).y = y + 8: p(2).x = x + 12: p(2).y = y + 40\n"
    "    gfx.draw.poly canvas&, p(), 0, 3, 0, (i * 4099) and m&, gfx.draw.FILL\n"
    "next",
    "octagons": "for i = 0 to 299\n"
    "    x = (i * 37 + f) mod 600 + 20: y = (i * 53) mod 440 + 20\n"
    "    for k = 0 to 7: p(k).x = x + ox(k): p(k).y = y + oy(k): next\n"
    "    gfx.draw.poly canvas&, p(), 0, 8, 0, (i * 4099) and m&, gfx.draw.FILL\n"
    "next",
    "mixed": "for i = 0 to 299\n"
    "    x = (i * 37 + f) mod 600: y = (i * 53) mod 440\n"
    "    gfx.draw.sprite canvas&, x, y, s&, 0\n"
    "    gfx.draw.rect canvas&, x, y, x + 7, y + 7, (i * 4099) and m&, gfx.draw.FILL\n"
    "next",
}
# draw.poly's modes: 300 squares turned, 40 pixels across, their far edge twice as far; flat, or s&'s texels affine
# or in perspective; with no depth, depth set, or tested and set, at five depths; or blended, or added, at half
# alpha. poly_loop: their loop, no poly, the interpreter's share.
POLY_LOOP = (
    "for i = 0 to 299\n"
    "    x = (i * 37 + f) mod 600 + 20: y = (i * 53) mod 440 + 20: zb = 1 + i mod 5\n"
    "    cs = cos((i + f) * 0.05) * 20: sn = sin((i + f) * 0.05) * 20\n"
    "    p(0).x = x + cs: p(0).y = y + sn: p(1).x = x - sn: p(1).y = y + cs\n"
    "    p(2).x = x - cs: p(2).y = y - sn: p(3).x = x + sn: p(3).y = y - cs\n"
    "    p(0).z = zb: p(1).z = zb: p(2).z = zb * 2: p(3).z = zb * 2\n"
    "{draw}"
    "next"
)
GFX_WORK["poly_loop"] = POLY_LOOP.format(draw="")
for kind, s_, how in (("flat", "0", "gfx.draw.FILL"), ("affine", "s&", "0"), ("persp", "s&", "gfx.draw.PERSPECTIVE")):
    for mode, z in (
        ("", ""),
        ("_zset", " + gfx.draw.DEPTH_WRITE"),
        ("_ztest", " + gfx.draw.DEPTH_TEST + gfx.draw.DEPTH_WRITE"),
        ("_blend", " + gfx.draw.BLEND"),
        ("_add", " + gfx.draw.ADD"),
    ):
        draw = f"    gfx.draw.poly canvas&, p(), 0, 4, {s_}, (i * 4099) and m&, {how}{z}\n"
        depth, blends = "DEPTH_WRITE" in z, "BLEND" in z or "ADD" in z
        first = "gfx.surface.clear_depth canvas&\n" if depth else "gfx.draw.set_alpha 32\n" if blends else ""
        GFX_WORK[f"poly_{kind}{mode}"] = first + POLY_LOOP.format(draw=draw)
GFX_PROGRAM = """import gfx.display, gfx.draw, gfx.surface
from gfx.display import FrameStats
from gfx.draw import Vertex
dim st as FrameStats
dim p(7) as Vertex
dim ox(7), oy(7)
for k = 0 to 7
    ox(k) = cos(k * 0.7853982) * 20: oy(k) = sin(k * 0.7853982) * 20: p(k).z = 1
next
p(1).u = 32: p(2).u = 32: p(2).v = 32: p(3).v = 32
canvas& = gfx.display.open(640, 480, {depth}, {flags})
m& = {mask}
s& = gfx.surface.new(32, 32)
gfx.draw.clear s&, 0
gfx.draw.circle s&, 15, 15, 14, 2, gfx.draw.FILL
dim px(32 * 32 * 4 - 1) as byte
dim pic&(15)
for k = 0 to 15
    for i = 0 to 32 * 32 * 4 - 1
        px(i) = (i * 7 + k * 40) and 255
    next
    pic&(k) = gfx.surface.new(32, 32)
    gfx.surface.write pic&(k), 0, 0, 32, 32, px()
next
do
    if f = {warm} then t# = gfx.display.clock
    gfx.draw.clear canvas&, 1
{work}
    gfx.display.flip
    f = f + 1
loop until f > {warm} and gfx.display.clock - t# >= {seconds}
t# = gfx.display.clock - t#
rate# = (f - {warm}) / t#
gfx.display.get_stats st
f = 0
gfx.draw.clear canvas&, 1
{work}
gfx.display.flip
for y = 0 to 479 step 16
    for x = 0 to 639 step 16
        sum& = (sum& * 31 + gfx.surface.point(canvas&, x, y)) and &HFFFFF
    next
next
{report}
"""
# the app's: a file it is killed after; DOS's: standard output, and the program ends
GFX_REPORT = 'open "GFX.TXT" for output as #1\nprint #1, {line}\nclose #1\ndo\nloop'
GFX_REPORT_DOS = "gfx.display.close\nprint {line}"
GFX_LINE = "rate#; sum&; st.batches; gfx.display.refresh"


def gfx_program(work: str, depth: int, seconds: float, vsync: bool, report: str, warm: int = 30) -> str:
    body = "\n".join("    " + line for line in GFX_WORK[work].splitlines())
    return GFX_PROGRAM.format(depth=depth, work=body, seconds=seconds, flags=int(vsync), warm=warm,
                              mask=255 if depth == 8 else "&HFFFFFF", report=report.format(line=GFX_LINE))


def gfx_rate(app: Path, work: str, depth: int, seconds: float, how: str, vsync: bool) -> tuple[float, int, int, float]:
    """Frames a second after 30 warming up, a checksum of frame 0's pixels drawn after (equal for both ways, or one of
    them drew something else), the last timed frame's batches, and the display's refresh."""
    with tempfile.TemporaryDirectory() as tmp:
        prog, out = Path(tmp) / "GFX.BAS", Path(tmp) / "GFX.TXT"
        prog.write_text(gfx_program(work, depth, seconds, vsync, GFX_REPORT))
        env = os.environ | {"QB_GFX": how, "QB_KEYS": "3F00", "SDL_AUDIO_DRIVER": "dummy"}
        p = subprocess.Popen([str(app), prog.name], env=env, cwd=tmp)
        try:
            until = time.monotonic() + seconds + 60
            while not out.exists() or not out.read_text().endswith("\n"):
                if time.monotonic() > until or p.poll() is not None:
                    raise SystemExit(f"gfx {work} {how}: no GFX.TXT")
                time.sleep(0.2)
        finally:
            p.kill()
            p.wait()
        rate, total, batches, hz = out.read_text().split()
        return float(rate), int(total), int(batches), float(hz)


def cmd_gfx_dos(a: argparse.Namespace) -> int:
    """Each GFX_WORK frame's rate in pages on DOS, --cpu (a 486) at 10k cycles/ms (as fps's) with an S3's VBE, after 3
    frames, a DOSBox a frame."""
    if not a.as_built:
        build(".", True)
    qbx = (dosrun.COM.parent / "lib/gfx.qbx").read_bytes()
    conf = dosrun.CONF
    dosrun.CONF = conf.replace("core=normal\ncputype=pentium\nvirtual time=true\nvirtual time rate=20000\n",
                               FPS_CPU.replace("cputype=486", f"cputype={a.cpu}"))
    assert dosrun.CONF != conf
    print(f"640x480 at {a.depth} bits on DOS ({a.cpu}), frames a second")
    print(f"{'':18}{'fps':>8}{'refresh':>9}")
    try:
        for work in a.work or GFX_WORK:
            job = (gfx_program(work, a.depth, a.seconds, a.vsync, GFX_REPORT_DOS, warm=3),)
            with tempfile.TemporaryDirectory() as d:
                r = dosrun.run({work: job}, Path(d) / "w", limit_ms=600000, file=True, mem=16,
                               files={"lib/gfx.qbx": qbx})[work]
            fields = r.out.split()
            if len(fields) == 4:
                print(f"{work:18}{float(fields[0]):8.1f}{float(fields[3]):9.2f}", flush=True)
            else:
                print(f"{work:18}  {r.out.strip().splitlines()[-1] if r.out.strip() else r.reason}", flush=True)
    finally:
        dosrun.CONF = conf
    return 0


def cmd_gfx(a: argparse.Namespace) -> int:
    """Each GFX_WORK frame's rate in pages and on the GPU, in the app's window (the Mac's, or Linux's, off-screen with
    SDL_VIDEO_DRIVER=offscreen); with --vsync, each flip waiting for the refresh. --dos: in pages on DOS."""
    if a.dos:
        return cmd_gfx_dos(a)
    mac = sys.platform == "darwin"
    subprocess.run(["make", "-j8", "mac" if mac else "linux-app"], cwd=ROOT, check=True, capture_output=True)
    app = ROOT / ("build/mac/QuickrBasic.app/Contents/MacOS/QuickrBasic" if mac else "build/linux/app/QuickrBasic")
    print(f"640x480 at {a.depth} bits, frames a second")
    print(f"{'':10}{'pages':>10}{'GPU':>10}{'GPU/pages':>11}{'batches':>9}{'refresh':>9}")
    for work in a.work or GFX_WORK:
        runs = (gfx_rate(app, work, a.depth, a.seconds, how, a.vsync) for how in ("soft", "gpu"))
        (soft, ssum, _, _), (gpu, gsum, batches, hz) = runs
        same = "" if ssum == gsum else "  frames differ"
        print(f"{work:10}{soft:10.0f}{gpu:10.0f}{gpu / soft:10.1f}x{batches:9}{hz:9.2f}{same}")
    return 0


def charlie_modules() -> dict[str, bytes]:
    """The game's package and the engine by the path each module sits at beside CHARLIE.BAS."""
    return {
        p.relative_to(CHARLIE.parent).as_posix(): p.read_bytes()
        for d in ("barkley", "engine")
        for p in sorted(CHARLIE.with_name(d).rglob("*.bas"))
    }


def charlie_program() -> str:
    """CHARLIE.BAS's Main swapped for a loop of each CHARLIE_EACH call (after an empty one), its ms a call to EACH.TXT."""
    head = CHARLIE.read_text().split("\nMain wd\nend\n", 1)[0]
    move = f"FOR q = 1 TO 60: {CHARLIE_STEP}: NEXT\ncx = INT(g.cam.x): cy = INT(g.cam.y)\n"
    loops = "".join(f"{move}n = 0: t0 = TIMER\nDO\n    FOR q = 1 TO {EACH_REPS}\n    {call}\n    NEXT\n"
                    f"    n = n + {EACH_REPS}\nLOOP UNTIL TIMER - t0 >= 2\n"
                    "PRINT #1, (TIMER - t0) / n * 1000; n\n" for call in ("REM", *CHARLIE_EACH))
    return head + "\n" + CHARLIE_IMPORTS + CHARLIE_SETUP + loops + "CLOSE #1\nEND\n"


def cmd_charlie(a: argparse.Namespace) -> int:
    """CHARLIE's calls, a ms each on a 486 at 10k cycles/ms, and the frames a second its Render and Flip allow."""
    build(".", True)
    conf = dosrun.CONF
    dosrun.CONF = conf.replace("core=normal\ncputype=pentium\nvirtual time=true\nvirtual time rate=20000\n", FPS_CPU)
    assert dosrun.CONF != conf
    with tempfile.TemporaryDirectory() as d:
        r = dosrun.run({"each": (charlie_program(), "", "", charlie_modules())}, Path(d) / "w", limit_ms=600000,
                       file=True, mem=16,
                       files={"CHARLIE.DAT": CHARLIE.with_suffix(".DAT").read_bytes()})
        log = Path(d) / "w/EACH.TXT"
        rows = [ln.split() for ln in log.read_text().splitlines()] if log.exists() else []
    dosrun.CONF = conf
    if len(CHARLIE_EACH) >= len(rows):
        print(f"no EACH.TXT: {r['each'].reason} {r['each'].out[-400:]}")
        return 1
    got = [(float(ms), int(n)) for ms, n in rows]
    cost = {call: ms - got[0][0] for call, (ms, _) in zip(CHARLIE_EACH, got[1:], strict=True)}
    for call, ms in cost.items():
        print(f"{ms:>8.2f} ms  {call}")
    frame = cost[CHARLIE_EACH[-1]] + cost["Flip g.page"]
    print(f"{1000 / frame:.1f} fps drawn; a tick's {cost[CHARLIE_EACH[0]]:.2f} ms sixty times a second on top")
    return 0


# The switches on a thing's kind: drawing, touch and each step's rule, by the routine each is in.
SWITCHES = {"draw": "drawthings", "touch": "touch", "think": "think"}


def case_arms(prog: basic_src.Program, routine: str) -> list[tuple[list[str], list[list[str]]]]:
    """Each case of the routine's switches on a thing's kind: its labels and its body's statements."""
    arms: list[tuple[list[str], list[list[str]]]] = []
    for mod in prog.modules:
        if routine not in mod.routines:
            continue
        depth, ours = 0, -1
        for st in mod.stmts:
            if st.routine != routine:
                continue
            low = [t.low for t in st.toks]
            if low[:2] == ["select", "case"]:
                depth += 1
                if low[-1] in ("kind", "k") and ours < 0:
                    ours = depth
                continue
            if low[:2] == ["end", "select"]:
                if depth == ours:
                    ours = -1
                depth -= 1
                continue
            if depth == ours and low[0] == "case":
                arms.append((low[1:], []))
            elif depth >= ours > 0 and arms:
                arms[-1][1].append(low)
    return arms


def kind_compares(prog: basic_src.Program, mod: basic_src.Module, st: basic_src.Stmt) -> int:
    """The comparisons of a thing's kind in a statement: not a kind assigned, nor one read into a variable."""
    ops = ("=", "<>", "<", ">", "<=", ">=")
    conditional = st.toks[0].low in ("if", "elseif", "while", "until", "do", "loop", "case")
    assign = next((i for i, t in enumerate(st.toks) if t.text == "="), -1)
    n = 0
    for ref in prog.refs(mod, st):
        if not ref.fields or ref.fields[-1][1:] not in (("kind", "thing"), ("k", "thing")):
            continue
        before = st.toks[ref.start - 1].text if ref.start > 0 else ""
        after = st.toks[ref.end].text if ref.end < len(st.toks) else ""
        target = ref.start in (0, 1) and after == "="
        read = ref.start - 1 == assign and after not in ops
        if not conditional and (target or read):
            continue
        n += before in ops or after in ops
    return n


def engine_measures() -> dict[str, str]:
    """docs/engine-design.md's Today table, measured on CHARLIE's source."""
    demos = CHARLIE.parent
    mods = sorted(p for d in ("barkley", "engine") for p in (demos / d).rglob("*.bas"))
    files = {p: p.read_text() for p in [*mods, CHARLIE]}
    prog = basic_src.Program(files)

    def where(mod: basic_src.Module) -> str:
        return mod.path.relative_to(demos).with_suffix("").as_posix().removeprefix("barkley/")

    kinds = {
        int(v)
        for k, v in re.findall(r"\b(K[A-Z0-9]+) = (\d+)", "\n".join(t for p, t in files.items() if "model" in p.parts))
        if k != "KFRONT"
    }
    arms = {k: case_arms(prog, r) for k, r in SWITCHES.items()}
    named = {k: [a for a in v if any(w.startswith("k") for w in a[0])] for k, v in arms.items()}
    blits = sum(1 for a in named["draw"] if len(a[1]) == 1 and a[1][0][0] == "blit")
    compares, compare_files, thing_sets, thing_files, sfx = 0, set(), 0, set(), 0
    takes_deck = takes_world = routines = 0
    for mod in prog.modules:
        for r in mod.routines.values():
            routines += 1
            takes_deck += any(v.type == "deck" for v in r.params.values())
            takes_world += any(v.type == "world" for v in r.params.values())
        for st in mod.stmts:
            switch = [t.low for t in st.toks[:2]] == ["select", "case"]
            for part in basic_src.clauses(st.toks):
                if part[0].low == "sfx" and not where(mod).startswith("assets/"):
                    sfx += 1
            if not switch and (n := kind_compares(prog, mod, st)):
                compares += n
                compare_files.add(where(mod))
            for ref in basic_src.assigned(prog, mod, st):
                if any(f[2] == "thing" for f in ref.fields):
                    thing_sets += 1
                    thing_files.add(where(mod))
    lines = [ln for p, t in files.items() if p != CHARLIE for ln in t.removesuffix("\n").split("\n")]
    notes = [ln for ln in lines if ln.lstrip().startswith("'")]
    heads = [ln for ln in lines if re.match(r"(private )?(sub|function) ", ln) and len(ln) > 70]
    up = [
        where(m)
        for m in prog.modules
        if where(m).startswith("engine/") and re.search(r"(?m)^from barkley\.(?!engine\.)", m.text)
    ]
    return {
        "Kinds of thing": str(len(kinds)),
        "`case` branches that name a kind: draw, touch, think": ", ".join(str(len(named[k])) for k in SWITCHES),
        "Draw branches that are one `Blit`": f"{blits} of {len(named['draw'])}",
        "`Meet` routines, in six zone modules": str(
            sum(
                1
                for m in prog.modules
                if where(m).startswith("stage/zones/")
                for r in m.routines
                if r.startswith("meet") and not re.search(rf"(?im)^private sub {r}\b", m.text)
            )
        ),
        "Comparisons of a thing's kind outside those switches": f"{compares}, in {len(compare_files)} files",
        "Files that assign a thing's field": f"{len(thing_files)}, in {thing_sets} assignments",
        "Routines that take the `Deck`": f"{takes_deck} of {routines}",
        "`Sfx` calls outside `assets`": str(sfx),
        "Routines that take the whole `World`": str(takes_world),
        "Game code that `engine` imports": ", ".join(up) or "none",
        "Lines over 70 columns": f"{sum(len(ln) > 70 for ln in lines):,} of {len(lines):,}",
        "Comment lines over 60 columns": f"{sum(len(ln) > 60 for ln in notes):,} of {len(notes):,}",
        "Routine headers over 70 columns": str(len(heads)),
    }


def cmd_engine(a: argparse.Namespace) -> int:
    for k, v in engine_measures().items():
        print(f"| {k} | {v} |")
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
    p.add_argument("-e", action="append", default=[], help="a program instead of the suite")
    p.add_argument("--file", action="append", default=[], help="a .bas file instead of the suite")
    p.add_argument("--runs", type=int, default=RUNS, help="native: the least of this many runs")
    p.add_argument("--dos", action="store_true")
    p = sub.add_parser("qr")
    p.add_argument("revs", nargs="*")
    p.add_argument("--ops", action="store_true", help="each Quickr case's ops, exact")
    p = sub.add_parser("size")
    p.add_argument("rev", nargs="?", default="HEAD")
    p.add_argument("--functions", action="store_true", help="each function whose bytes changed")
    p = sub.add_parser("gate")
    p.add_argument("--save", action="store_true")
    sub.add_parser("qbasic").add_argument("--json", type=Path, help="the timings to this file too")
    sub.add_parser("ops")
    p = sub.add_parser("profile")
    p.add_argument("file", nargs="?")
    p.add_argument("-e")
    p.add_argument("-n", type=int, default=25)
    p.add_argument("--link", action="store_true", help="RUN's link alone, the lines typed")
    p.add_argument("--ide", action="store_true", help="the IDE's open of the file")
    p.add_argument("--inclusive", action="store_true", help="each function's instructions with its callees'")
    p.add_argument("--ms", type=int, default=30000, help="--ide: the emulated time each run is given")
    p.add_argument("--mem", type=int, default=4, help="DOSBox's MB")
    p = sub.add_parser("fills")
    p.add_argument("--case", action="append", choices=list(FILLS_CASES), help="a draw to count; all when none")
    p.add_argument("--bits", type=int, action="append", choices=list(FILLS_COLOURS), help="a depth; 8 and 16 when none")
    p.add_argument("-n", type=int, default=3)
    p = sub.add_parser("fps")
    p.add_argument("--real", action="store_true", help="the host's clock at cycles=fixed 10000, not virtual time")
    p.add_argument("--profile", action="store_true", help="the functions it spends its instructions in (normal core)")
    p.add_argument("--parts", action="store_true", help="each step of the loop's ms a frame")
    p.add_argument("--each", action="store_true", help="each FPS_EACH call's ms on its own")
    p.add_argument("--nosb", action="store_true", help="no Sound Blaster: the PC speaker's music")
    p.add_argument("-n", type=int, default=30)
    p = sub.add_parser("gfx")
    p.add_argument("--depth", type=int, default=32)
    p.add_argument("--seconds", type=float, default=3)
    p.add_argument("--work", action="append", choices=list(GFX_WORK), help="a frame to time; all when none")
    p.add_argument("--vsync", action="store_true", help="each flip waits for the refresh")
    p.add_argument("--dos", action="store_true", help="pages under DOSBox-X")
    p.add_argument("--cpu", default="486", help="--dos: DOSBox's cputype")
    p.add_argument("--as-built", action="store_true", help="--dos: BASIC_EXE's build as it is, not made first")
    sub.add_parser("charlie")
    sub.add_parser("engine")
    a = ap.parse_args()
    return {
        "run": cmd_run,
        "iter": cmd_iter,
        "cmp": cmd_cmp,
        "size": cmd_size,
        "gate": cmd_gate,
        "qr": cmd_qr,
        "qbasic": cmd_qbasic,
        "ops": cmd_ops,
        "profile": cmd_profile,
        "fills": cmd_fills,
        "fps": cmd_fps,
        "gfx": cmd_gfx,
        "charlie": cmd_charlie,
        "engine": cmd_engine,
    }[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
