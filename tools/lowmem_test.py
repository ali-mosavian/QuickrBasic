#!/usr/bin/env python3
"""lowmem_test.py: the IDE (IDE_EXE) in a 2 MB machine without EMS, a 486's, runs a program with SCREEN 13's
two pages, PLAY, a 100 KB array and 250-deep expressions. Boot took 805 KB of a 514 KB heap and said Out of
memory: the IDE's screen copy, the POKE memory, the file records, 64 KB code cache, store and stack segment, symbol
tables for 1024 names, 56 KB table blocks and a 128 KB C stack, all up front."""

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
DEEP = "(" * 250 + "1" + " + 1)" * 250
PROGRAM = "\r\n".join(
    [
        "screen 13, , 1, 0",
        "line (0, 0)-(319, 199), 4, bf",
        "screen , , 1, 1",
        'play "T200 L16 CDE"',
        "screen 0",
        "redim a&(25000): a&(25000) = 7",
        f"x = {DEEP}",
        "y = 1" + " + 1" * 250,
        'print "done"; a&(25000); x; y',
        "",
    ]
)


def main() -> None:
    work = Path(tempfile.mkdtemp())
    (work / "BASIC.EXE").write_bytes(IDE.read_bytes())
    (work / "DOS32A.EXE").write_bytes((IDE.parent / "DOS32A.EXE").read_bytes())
    (work / "BASIC.HLP").write_bytes(mkhelp.compiled())
    (work / "T.BAS").write_bytes(PROGRAM.encode())
    conf = dosplay.CONF.replace("memsize=32", "memsize=2").replace("ems=true", "ems=false")
    s = dosplay.Session(work, conf)
    try:
        s.launch(3000, "BASIC T.BAS")
        s.resume(3000)
        for scan, ascii_ in ((0x01, 0x1B), (F5, 0)):  # the welcome box, then the run
            s.send({"cmd": "key", "scancode": scan, "ascii": ascii_, "stop": True})
            s.run_to_key(20000)
            s.resume(300)
        DONE = "done 7  251  251"
        s.run_until(lambda: DONE in s.screen() or "Out of memory" in s.screen(), 30000, "the run's end")
        text = s.screen()
        assert DONE in text and "Out of memory" not in text, text
    finally:
        s.close()
    print("lowmem_test: ok")


if __name__ == "__main__":
    main()
