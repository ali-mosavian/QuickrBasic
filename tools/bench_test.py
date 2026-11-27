#!/usr/bin/env python3
"""bench_test.py: demos --card --profile dropped --set, so SSTV2_CMDFIFO=1 profiled the Voodoo 2's direct
writes as if they were its command FIFO's. A driver's function named as one of the program's, tdfx.c's drawn as
player.c's, read the program's source lines by line."""

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
    bench.driver_images = lambda b, card, exe: ()
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

listing = """
 0001:00000100       _drawn                     0000000010001100     tdfx.obj
 0001:00000200       _put                       0000000010001200     sst.obj
 0001:00000300       _put                       0000000010001300     cvg.obj
 0001:00000400       _tdfx_setup                0000000010001400     setup.obj
"""
assert bench.dll_functions(listing, frozenset({"_drawn"})) == [
    (0x100, "_drawn (tdfx)"),
    (0x200, "_put (sst)"),
    (0x300, "_put (cvg)"),
    (0x400, "_tdfx_setup"),
]
print("bench_test ok")
