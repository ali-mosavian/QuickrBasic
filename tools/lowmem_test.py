#!/usr/bin/env python3
"""lowmem_test.py: the IDE (IDE_EXE) in a 3 MB machine without EMS runs a program with SCREEN 12's
two pages, SCREEN 13, PLAY and a 500 KB array. The array was Out of memory: boot took the IDE's
screen copy, the POKE memory and the file records (280 KB) up front, leaving FRE(-1) 419 KB."""

import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import mkhelp
import dosplay

ROOT = Path(__file__).resolve().parents[1]
IDE = Path(os.environ.get("IDE_EXE", ROOT / "build/c/ide32.exe"))
F5 = 0x3F
PROGRAM = "\r\n".join(
    [
        "screen 12, , 1, 0",
        "circle (320, 240), 100, 14",
        "screen , , 1, 1",
        "pcopy 1, 0",
        "screen 13, , 1, 0",
        "line (0, 0)-(319, 199), 4, bf",
        "screen , , 1, 1",
        'play "T200 L16 CDE"',
        "screen 0",
        "redim a&(125000): a&(125000) = 7",
        'print "done"; a&(125000)',
        "",
    ]
)


def main() -> None:
    work = Path(tempfile.mkdtemp())
    (work / "BASIC.EXE").write_bytes(IDE.read_bytes())
    (work / "DOS32A.EXE").write_bytes((IDE.parent / "DOS32A.EXE").read_bytes())
    (work / "BASIC.HLP").write_bytes(mkhelp.compiled())
    (work / "T.BAS").write_bytes(PROGRAM.encode())
    conf = dosplay.CONF.replace("memsize=32", "memsize=3").replace("ems=true", "ems=false")
    s = dosplay.Session(work, conf)
    try:
        s.launch(3000, "BASIC T.BAS")
        s.resume(3000)
        for scan, ascii_ in ((0x01, 0x1B), (F5, 0)):  # the welcome box, then the run
            s.send({"cmd": "key", "scancode": scan, "ascii": ascii_, "stop": True})
            s.run_to_key(20000)
            s.resume(300)
        s.run_until(lambda: "done" in s.screen() or "Out of memory" in s.screen(), 30000, "the run's end")
        text = s.screen()
        assert "done 7" in text and "Out of memory" not in text, text
    finally:
        s.close()
    print("lowmem_test: ok")


if __name__ == "__main__":
    main()
