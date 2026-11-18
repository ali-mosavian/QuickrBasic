"""bench.py: what a program costs on a build, and builds against each other.

    bench.py run FILE.bas | -e CODE [--dos]      one program: its output and cost
    bench.py iter -e BODY [--setup CODE] [--dos] one loop iteration's cost
    bench.py cmp [REV...] [-e CODE] [--file F] [--dos]  the suite, or these programs, per build (HEAD .)
    bench.py size [REV] [--functions]            the DOS executables' loaded bytes, REV's (default HEAD) and ours
    bench.py gate [--save]                       the suite and sizes against tools/bench/baseline.json
    bench.py qbasic [--json F]                   qbasic/BENCH.BAS in QBasic 1.1 and our IDE under DOSBox
    bench.py profile FILE.bas | -e CODE [-n N] [--link | --ide]  DOS: the functions a program spends its
                                                 instructions in; --link: RUN's link alone
    bench.py fps [--real] [--profile | --parts]  MOTHLITE.BAS played by an autopilot on a 486 at 10k cycles/ms
                                                 with the Sound Blaster: its frame rate in each mode;
                                                 --parts: each step of its loop's ms a frame

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
from dataclasses import dataclass

sys.path.insert(0, str(Path(__file__).resolve().parent))  # run with -I too
import doslink
import dosrun

ROOT = Path(__file__).resolve().parent.parent
SUITE = ROOT / "tools/bench"
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
    res = [run_dos(b, progs) for b in builds] if a.dos else run_natives(builds, progs)
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
        shutil.copy(ROOT / "src/basic.hlp", work / "BASIC.HLP")
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


def nm(o: Path) -> list[tuple[int, str, str]]:
    """Object o's defined symbols: offset, kind (t local, T global, ...), name."""
    out = subprocess.run([os.environ.get("NM", "nm"), "-n", "--defined-only", o], capture_output=True, text=True).stdout
    return [(int(f[0], 16), f[1], f[2]) for f in (ln.split() for ln in out.splitlines()) if len(f) == 3]


def with_statics(listing: str, objs: list[Path]) -> str:
    """The link map, each module's static functions added: the map lists globals alone, so the
    profiler counted a static's instructions under the global before it. A static's address is its
    module's code base, from a global the map and the object both place, plus its offset."""
    lines = listing.splitlines()
    by_path = {str(o): o for o in objs}
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
    if not m:
        sys.exit(f"profile: make printed no {var}")
    objs = [b.root / o for o in m.group(1).split()]
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
        dosrun.COM = profiled(build(".", True), Path(d), a.ide)
        r = dosrun.run(
            jobs,
            Path(d) / "w",
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
    p.add_argument("--dos", action="store_true")
    p = sub.add_parser("size")
    p.add_argument("rev", nargs="?", default="HEAD")
    p.add_argument("--functions", action="store_true", help="each function whose bytes changed")
    p = sub.add_parser("gate")
    p.add_argument("--save", action="store_true")
    sub.add_parser("qbasic").add_argument("--json", type=Path, help="the timings to this file too")
    p = sub.add_parser("profile")
    p.add_argument("file", nargs="?")
    p.add_argument("-e")
    p.add_argument("-n", type=int, default=25)
    p.add_argument("--link", action="store_true", help="RUN's link alone, the lines typed")
    p.add_argument("--ide", action="store_true", help="the IDE's open of the file")
    p.add_argument("--inclusive", action="store_true", help="each function's instructions with its callees'")
    p.add_argument("--ms", type=int, default=30000, help="--ide: the emulated time each run is given")
    p.add_argument("--mem", type=int, default=4, help="DOSBox's MB")
    p = sub.add_parser("fps")
    p.add_argument("--real", action="store_true", help="the host's clock at cycles=fixed 10000, not virtual time")
    p.add_argument("--profile", action="store_true", help="the functions it spends its instructions in (normal core)")
    p.add_argument("--parts", action="store_true", help="each step of the loop's ms a frame")
    p.add_argument("--each", action="store_true", help="each FPS_EACH call's ms on its own")
    p.add_argument("--nosb", action="store_true", help="no Sound Blaster: the PC speaker's music")
    p.add_argument("-n", type=int, default=30)
    a = ap.parse_args()
    return {
        "run": cmd_run,
        "iter": cmd_iter,
        "cmp": cmd_cmp,
        "size": cmd_size,
        "gate": cmd_gate,
        "qbasic": cmd_qbasic,
        "profile": cmd_profile,
        "fps": cmd_fps,
    }[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
