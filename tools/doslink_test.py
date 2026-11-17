#!/usr/bin/env python3
"""doslink_test.py: ide32 links from a deep checkout. jwlink copies its command line into 10 KiB; the
object paths overran it, and the heap corruption aborted ubuntu's link. Guard malloc stops macOS's at it."""

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import doslink

ROOT = doslink.ROOT


def objects(exe: str) -> list[Path]:
    """exe's objects, in the Makefile's order."""
    p = subprocess.run(
        ["make", "-n", "-W", "tools/doslink.py", exe], cwd=ROOT, capture_output=True, text=True, check=True
    )
    line = next(one for one in p.stdout.splitlines() if f"tools/doslink.py {exe} " in one)
    return [Path(one) for one in line.split()[3:]]


if __name__ == "__main__":
    if sys.platform == "darwin":
        os.environ["DYLD_INSERT_LIBRARIES"] = "/usr/lib/libgmalloc.dylib"
    built = ROOT / "build/c/ide32.exe"
    with tempfile.TemporaryDirectory() as t:
        deep = Path(t).joinpath(*["d" * 200] * 4)
        objs = []
        for obj in objects("build/c/ide32.exe"):
            objs.append(deep / obj)
            objs[-1].parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(ROOT / obj, objs[-1])
        exe = Path(t) / "ide32.exe"
        doslink.link(exe, objs)
        assert exe.read_bytes() == built.read_bytes(), "the deep link differs from build/c/ide32.exe"
    print("ok")
