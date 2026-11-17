#!/usr/bin/env python3
"""pgo.py OUT EXE [ide] -- OBJECT...: the functions a training run of the program calls, one a line.

The objects (lto.py --profiling's, then the asm's) are linked into EXE with a map, which DOSBox's
profiler names functions by. The training is the conformance programs (tools/conformance.py) and
tests/pgo's, the IDE's run by F5. lto.py --hot OUT makes every other function cold.
"""

import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import conformance
import dosrun

sys.path.insert(0, str(Path(os.environ.get("LLRM", Path.home() / "work/personal/llrm-next")) / "tools/dosbatch"))
import dosbatch

IDE_PROGRAMS = 40  # the IDE's training: its run of these many, each to the time limit


def link(exe: Path, objects: list[Path]) -> None:
    with tempfile.TemporaryDirectory() as work:
        dosbatch.link_target("x86-m32", objects[0], exe, Path(work), listing=exe.with_suffix(".map"),
                             objects_after=tuple(objects[1:]), defines=dosbatch.entry_defines("x86-m32", None, near=False))


def main() -> None:
    args = sys.argv[1:]
    cut = args.index("--")
    out, exe, ide, objects = Path(args[0]), Path(args[1]).resolve(), "ide" in args[2:cut], args[cut + 1:]
    link(exe, [Path(o).resolve() for o in objects])
    (exe.parent / "DOS32A.EXE").write_bytes((Path("build/c/DOS32A.EXE")).read_bytes())
    dosrun.COM = exe
    progs = {p.stem: p.read_text().replace("\n", "\r\n") for p in sorted(Path("tests/pgo").glob("*.bas"))}
    progs |= {k: v[0] for k, v in conformance.cases().items()}
    more = dict(files={**conformance.COMPANIONS, "BASIC.HLP": Path("src/basic.hlp").read_bytes()},
                before=("AUTOTYPE -w 2 f5",), keyboard=True, screen=True) if ide else dict(files=conformance.COMPANIONS)
    if ide:
        progs = dict(list(progs.items())[:IDE_PROGRAMS])
    batches = [dict(jobs=b, limit_ms=10000, file=True, profile=True, **more) for b in dosrun.split(progs)]
    ran = {f for r in dosrun.run_all(batches, Path(tempfile.mkdtemp())).values() for f in r.calls}
    names = sorted({n.removeprefix("_").removeprefix("q$") for n in ran})
    if len(names) < 100:
        sys.exit(f"pgo: {len(names)} functions run: no map read?")
    out.write_text("\n".join(names) + "\n")
    print(f"pgo: {len(names)} functions run, {out}")


if __name__ == "__main__":
    main()
