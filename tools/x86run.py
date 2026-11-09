#!/usr/bin/env python3
"""x86run.py: run DOS jobs of the x86 port under llrm's headless DOSBox-X."""

import json
import os
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LLRM = Path("/Users/alim/work/personal/llrm-qbmac/target/release")
COM = ROOT / "build/x86/basic.com"

CONF = """[sdl]
priority=higher,normal
output=surface
[dosbox]
memsize=32
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


def run(jobs: dict[str, str], work: Path, limit_ms: int = 60000) -> dict[str, Result]:
    """Run each program text as stdin of BASIC.COM; one DOSBox, one job per program."""
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    shutil.copy(COM, work / "BASIC.COM")
    names = list(jobs)
    lines = []
    for i, name in enumerate(names):
        (work / f"J{i}.IN").write_bytes(jobs[name].encode("latin-1"))
        lines += [f":ms {limit_ms}", f"mount c {work}", "c:", f"BASIC.COM < J{i}.IN > J{i}.TXT", "."]
    (work / "job.conf").write_text(CONF)
    (work / "jobs.txt").write_text("\n".join(lines) + "\n")
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", DOSRUN_FD="9")
    with open(work / "events.txt", "wb") as ev, open(work / "jobs.txt", "rb") as jin:
        subprocess.run(
            [str(LLRM / "dosbox-x"), "-nolog", "-conf", str(work / "job.conf")],
            stdin=jin,
            pass_fds=(9,) if False else (),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            preexec_fn=lambda: os.dup2(ev.fileno(), 9),
            close_fds=False,
            timeout=600,
        )
    ends = [json.loads(x) for x in (work / "events.txt").read_text().splitlines() if '"ev":"end"' in x]
    out: dict[str, Result] = {}
    for i, name in enumerate(names):
        e = ends[i] if i < len(ends) else {}
        f = work / f"J{i}.TXT"
        text = f.read_bytes().decode("latin-1") if f.exists() else ""
        out[name] = Result(name, text, e.get("reason", "none"), e.get("instructions", 0), e.get("ms", 0))
    return out
