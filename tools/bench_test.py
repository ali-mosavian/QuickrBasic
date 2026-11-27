#!/usr/bin/env python3
"""bench_test.py: demos --card --profile dropped --set, so SSTV2_CMDFIFO=1 profiled the Voodoo 2's direct
writes as if they were its command FIFO's."""

import argparse
import sys
import tempfile
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).parent))
import bench


class Staged(Exception):
    pass


def staged(image: Path, d: Path, com: Path, job: tuple, files: dict, last: str, sets: tuple = ()) -> Path:
    raise Staged(sets)


with tempfile.TemporaryDirectory() as d:
    (Path(d) / "lib").mkdir()
    (Path(d) / "lib/gfx.qbx").write_bytes(b"")
    bench.build = lambda rev, dos: SimpleNamespace(dos=Path(d) / "basic32.exe")
    bench.card_vm = lambda card, machine: "qb-486-voodoo2"
    bench.drivers_of = lambda b: {}
    bench.profiled = lambda b, d: Path(d) / "hlprof.exe"
    bench.driver_images = lambda b, card: ()
    bench.driver_dll = lambda b, card: Path(d) / "voodoo2.dll"
    bench.hl86_module = lambda: None
    bench.vm_stage = staged
    a = argparse.Namespace(card="voodoo2", machine="486", profile=True, demo=["MEADOW"], frames=None, edits=None,
                           depth=16, n=20, at=None, lines=False, set=["SSTV2_CMDFIFO=1"])
    try:
        bench.demos_on_card(a, {})
        raise AssertionError("no job staged")
    except Staged as e:
        assert e.args[0] == ("SSTV2_CMDFIFO=1",), e.args
print("bench_test ok")
