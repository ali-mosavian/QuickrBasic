#!/usr/bin/env python3
"""dosrun.py: DOS jobs of the DOS build run under the headless DOSBox-X (tools/deps.sh dosbox)."""

import os
import re
import json
import shutil
import subprocess
from pathlib import Path
from dataclasses import field
from dataclasses import dataclass
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parents[1]
DOSBOX = Path(os.environ.get("DOSBOX_BIN", ROOT / "build/deps/bin/dosbox-x"))
COM = Path(os.environ.get("BASIC_EXE", ROOT / "build/c/basic32.exe"))

CONF = """[sdl]
priority=higher,normal
output=surface
[dosbox]
memsize={mem}
startquiet=true
startbanner=false
quit warning=false
[cpu]
core=normal
cputype=pentium
virtual time=true
virtual time rate=20000
[dos]
xms=true
ems=true
[mixer]
nosound=true
"""


@dataclass
class Result:
    name: str
    out: str
    reason: str
    instructions: int
    ms: int
    screen: str = ""  # the console's lines: what standard error wrote, with DOSBox's own
    calls: dict[str, int] = field(default_factory=dict)  # profile: each function run, its calls
    selfs: dict[str, int] = field(default_factory=dict)  # profile: each function's own instructions
    incls: dict[str, int] = field(default_factory=dict)  # profile: and its callees' (a recursion's twice)


def messages() -> list[str]:
    """The errors' messages, as src/core/io/errmsg.c spells them."""
    msgs = re.findall(r'^\s*\{\d+, "([^"]+)"\},', (ROOT / "src/core/io/errmsg.c").read_text(), re.M)
    assert len(msgs) > 50, "errmsg.c's messages not read"
    return msgs + ["Unprintable error"]


def run(jobs: dict[str, str | tuple], work: Path, limit_ms: int = 60000, file: bool = False,
        files: dict[str, bytes] | None = None, mem: int = 4, screen: bool = False,
        before: tuple[str, ...] = (), keyboard: bool = False, profile: bool = False) -> dict[str, Result]:
    """Run each program text as stdin of BASIC.COM, or with file as the file BASIC.COM runs, its
    stdin a job's (program, input[, arguments]) second, the arguments after the file's name; files
    are put beside them. One DOSBox, one job per program. screen: standard output not redirected,
    the screen's path run; out is then empty. before: DOS commands ahead of each job's; keyboard: standard input not redirected
    either (the IDE's own run). profile: each function's calls counted, by the map beside COM."""
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    shutil.copy(COM, work / "BASIC.COM")
    if profile:
        shutil.copy(COM.with_suffix(".map"), work / "BASIC.MAP")
    # stub32a loads DOS32A.EXE from the program's directory
    if (COM.parent / "DOS32A.EXE").exists() and COM.suffix.lower() == ".exe":
        shutil.copy(COM.parent / "DOS32A.EXE", work / "DOS32A.EXE")
    for n, data in (files or {}).items():  # companions the programs read
        (work / n).write_bytes(data)
    if is_host(COM):
        return run_host(jobs, work, limit_ms, file)
    names = list(jobs)
    lines = []
    for i, name in enumerate(names):
        prog, stdin, args = (jobs[name], "", "") if isinstance(jobs[name], str) else (*jobs[name], "")[:3]
        # the file named as the job, its first 8 letters: BC's module name in an error's message
        bas = f"J{i}\\{re.sub(r'[^A-Za-z0-9_]', '', name)[:8].upper() or 'PROG'}.BAS"
        if file:
            (work / f"J{i}").mkdir()
            (work / bas.replace("\\", "/")).write_bytes(prog.encode("latin-1"))
        (work / f"J{i}.IN").write_bytes((stdin if file else prog).encode("latin-1"))
        run = f"BASIC.COM {bas} {args}".rstrip() + ("" if keyboard else f" < J{i}.IN") if file else f"BASIC.COM < J{i}.IN"
        lines += [":profile"] * profile + [f":ms {limit_ms}", f"mount c {work}", "c:", *before, run if screen else f"{run} > J{i}.TXT", "."]
    fpu = "" if os.environ.get("DOS_FPU", "1") == "1" else "fpu=false\n"
    # mem: DOSBox's MB. DOS/32A's start costs about 0.35 s a MB, the heap takes what is free
    (work / "job.conf").write_text(CONF.format(mem=mem).replace("[dos]", f"{fpu}[dos]"))
    (work / "jobs.txt").write_text("\n".join(lines) + "\n")
    with open(work / "events.txt", "wb") as ev, open(work / "jobs.txt", "rb") as jin:
        env = dict(os.environ, SDL_VIDEODRIVER="dummy", DOSRUN_FD=str(ev.fileno()))
        subprocess.run(
            [str(DOSBOX), "-nolog", "-conf", str(work / "job.conf")],
            stdin=jin,
            pass_fds=(ev.fileno(),),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=max(600, 20 * len(names)),  # about 9 s a job: a fixed 600 cut a long batch
        )
    evs = [json.loads(x) for x in (work / "events.txt").read_text().splitlines()]
    ends = [e for e in evs if e.get("ev") == "end"]
    screens = [[] for _ in names]
    calls: list[dict[str, int]] = [{} for _ in names]
    selfs: list[dict[str, int]] = [{} for _ in names]
    incls: list[dict[str, int]] = [{} for _ in names]
    k = 0
    for e in evs:
        if e.get("ev") == "profile" and k < len(names):
            calls[k] = {f["name"]: f["calls"] for f in e["functions"] if f["calls"] or f["self"]}
            selfs[k] = {f["name"]: f["self"] for f in e["functions"] if f["self"]}
            incls[k] = {f["name"]: f["inclusive"] for f in e["functions"] if f.get("inclusive")}
        if e.get("ev") == "end":
            k += 1
        elif e.get("ev") == "line" and k < len(names):
            screens[k].append(e.get("text", ""))
    out: dict[str, Result] = {}
    for i, name in enumerate(names):
        e = ends[i] if i < len(ends) else {}
        f = work / f"J{i}.TXT"
        text = f.read_bytes().decode("latin-1") if f.exists() else ""
        out[name] = Result(name, text, e.get("reason", "none"), e.get("instructions", 0), e.get("ms", 0),
                           "\n".join(screens[i]), calls[i], selfs[i], incls[i])
    return out


WORKERS = (os.cpu_count() or 4) * 2  # DOSBoxes at once


def is_host(com: Path) -> bool:
    """com is this machine's program (the host build), not a DOS one."""
    return com.suffix.lower() not in (".exe", ".com")


def run_host(jobs: dict[str, str | tuple], work: Path, limit_ms: int, file: bool) -> dict[str, Result]:
    """run's jobs with the host build, natively: the same files, names and streams, no DOSBox."""
    out: dict[str, Result] = {}
    for i, name in enumerate(jobs):
        prog, stdin, args = (jobs[name], "", "") if isinstance(jobs[name], str) else (*jobs[name], "")[:3]
        bas = f"J{i}\\{re.sub(r'[^A-Za-z0-9_]', '', name)[:8].upper() or 'PROG'}.BAS"
        argv = [str(COM.resolve())]
        if file:
            (work / f"J{i}").mkdir()
            (work / bas.replace("\\", "/")).write_bytes(prog.encode("latin-1"))
            argv += [bas, *args.split()]
        data = (stdin if file else prog).encode("latin-1")
        try:
            p = subprocess.run(argv, cwd=work, input=data, capture_output=True, timeout=limit_ms / 1000,
                               env=os.environ | {"QB_OPS": "1"})
            reason, text, err = "exit", p.stdout, p.stderr
        except subprocess.TimeoutExpired as e:
            reason, text, err = "timeout", e.stdout or b"", e.stderr or b""
        m = re.search(rb"\nops (\d+)\n$", err)  # the ops run: the instruction count
        count, err = (int(m[1]), err[: m.start()]) if m else (0, err)
        out[name] = Result(name, text.decode("latin-1"), reason, count, 0, err.decode("latin-1").replace("\r\n", "\n"))
    return out


def split(jobs: dict, n: int = WORKERS) -> list[dict]:
    """jobs dealt into at most n batches, none empty."""
    items = list(jobs.items())
    return [dict(items[i::n]) for i in range(min(n, len(items)))]


def run_all(batches: list[dict], work: Path) -> dict[str, Result]:
    """Each batch, run's arguments less work, in its own DOSBox, WORKERS at once."""
    res: dict[str, Result] = {}
    with ThreadPoolExecutor(WORKERS) as ex:
        for r in ex.map(lambda ib: run(work=work / f"b{ib[0]}", **ib[1]), enumerate(batches)):
            res |= r
    return res
