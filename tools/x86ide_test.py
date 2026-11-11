#!/usr/bin/env python3
"""x86ide_test.py: the full-screen editor (make ide) played on the emulated console: lines typed, RUN
on the output page, the immediate line, the cursor between lines. Screenshots go to docs/img."""

import sys
import tempfile
from pathlib import Path

LLRM = Path("/Users/alim/work/personal/llrm-qbmac/tools")
sys.path[:0] = [str(LLRM), str(LLRM / "dosbatch")]
import llrmbin  # noqa: E402

llrmbin.stale_binaries = lambda *a, **k: []  # the emulator is the stamped one; the llrm sources are not ours
import qbplay  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
IMG = ROOT / "docs/img"


def play() -> None:
    work = Path(tempfile.mkdtemp())
    (work / "BASIC.COM").write_bytes((ROOT / "build/x86/basic-ide.com").read_bytes())
    s = qbplay.Session(work)
    s.launch(3000, "BASIC.COM")
    s.keys_in_lockstep(["esc"], 3000)  # the first key after the launch is lost
    s.resume(100)

    def press(*names: str) -> None:
        for k in names:
            s.keys_in_lockstep([k], 3000)
            s.resume(60)  # the key's work, drawn

    def keys(text: str) -> None:
        names = {" ": "space", "\n": "enter"}
        for c in text:
            press(names.get(c, c))

    def look(name: str = "") -> str:
        s.resume(100)
        if name:
            s.snap(IMG / f"ide-{name}.png")
        return s.screen().upper()

    def output() -> str:
        """Page 1, where the program writes: text_screen shows page 0 only."""
        data = bytes.fromhex(s.send({"cmd": "mem_read_linear", "addr": 0xB9000, "len": 4000})["data"])
        return "\n".join(data[r * 160 : r * 160 + 160 : 2].decode("latin-1").rstrip() for r in range(25)).upper()

    keys("for i=1 to 3\nprint i*i\nnext\n")
    scr = look("editor")
    for want in ("10 FOR I=1 TO 3", "20 PRINT I*I", "30 NEXT", "IMMEDIATE"):
        assert want in scr, (want, scr)
    s.keys_in_lockstep(["f5"], 3000)
    s.resume(300)
    look("output")
    scr = output()
    assert scr.split()[:3] == ["1", "4", "9"] and "PRESS ANY KEY" in scr, scr
    s.keys_in_lockstep(["space"], 3000)
    scr = look()
    assert "10 FOR I=1 TO 3" in scr and "PRESS ANY KEY" not in scr, scr
    # The immediate line.
    press("tab")
    keys("print 6*7\n")
    look("immediate")
    scr = output()
    assert "42" in scr.split(), scr
    press("space")
    # Up to line 10, change it; the cursor leaving stores it.
    press("tab")
    press("up", "up", "up", "end", "backspace", "4", "down", "f5")
    look()
    scr = output()
    assert "16" in scr.split(), scr  # FOR I=1 TO 4: the fourth square
    s.close()
    print("ok")


if __name__ == "__main__":
    play()
