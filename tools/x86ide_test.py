#!/usr/bin/env python3
"""x86ide_test.py: the QBasic-style editor (make ide) played on the emulated console: free-form lines, F5 on
the output page, the immediate window, menus, clipboard, the file and search dialogs. Screenshots go to docs/img."""

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
ALT_F, ALT_S, F5 = 0x21, 0x1F, 0x3F


def play() -> None:
    work = Path(tempfile.mkdtemp())
    (work / "BASIC.COM").write_bytes((ROOT / "build/x86/basic-ide.com").read_bytes())
    (work / "SQ.BAS").write_bytes(b"for i=1 to 3\r\n  print i*i\r\nnext\r\n")
    (work / "SUB").mkdir()
    s = qbplay.Session(work)
    s.launch(3000, "BASIC.COM")
    s.keys_in_lockstep(["esc"], 3000)  # the first key after the launch is lost
    s.resume(100)
    s.keys_in_lockstep(["esc"], 3000)  # the welcome box
    s.resume(100)

    def press(*names: str) -> None:
        for k in names:
            if k.startswith("0x"):
                s.send({"cmd": "key", "scancode": int(k, 16), "ascii": 0, "stop": True})
                s.run_to_key(3000)
            else:
                s.keys_in_lockstep([k], 3000)
            s.resume(60)  # the key's work, drawn

    def keys(text: str) -> None:
        names = {" ": "space", "\n": "enter"}
        for c in text:
            press(names.get(c, c))

    def shift(on: bool) -> None:
        """The BIOS shift flags, which ui_getkey reads: the key socket sends no modifiers."""
        s.send({"cmd": "mem_write_linear", "addr": 0x417, "data": "02" if on else "00"})

    def look(name: str = "") -> list[str]:
        s.resume(100)
        if name:
            s.snap(IMG / f"ide-{name}.png")
        return s.screen().upper().split("\n")

    def text() -> list[str]:
        """The program window's lines."""
        return [r[1:79].rstrip() for r in look()[2:20]]

    def output() -> list[str]:
        """Page 1, where the program writes: text_screen shows page 0 only."""
        data = bytes.fromhex(s.send({"cmd": "mem_read_linear", "addr": 0xB9000, "len": 4000})["data"])
        return [data[r * 160 : r * 160 + 160 : 2].decode("latin-1").rstrip().upper() for r in range(25)]

    # Free-form lines, capitalised on leaving; F5 runs them.
    keys("for i=1 to 3\n  print i*i\n")
    press("home")  # past the indent Enter kept
    keys("next\n")
    assert text()[:3] == ["FOR I=1 TO 3", "  PRINT I*I", "NEXT"], text()
    look("editor")
    press(f"0x{F5:x}")
    look("output")
    out = output()
    assert out[:3] == [" 1", " 4", " 9"] and out[24] == "PRESS ANY KEY TO CONTINUE", out
    press("space")

    # The immediate window: output on page 1. The prompt once stayed there,
    # in front of the next output.
    press("f6")
    keys("print 6*7\n")
    press("space")
    assert output()[24] == "", output()
    keys("print 8\n")
    out = output()
    assert " 42" in out and " 8" in out and out.count("PRESS ANY KEY TO CONTINUE") == 1, out
    press("space", "f6")

    # Every File item drops down: a separator once ended the menu after Save As.
    press(f"0x{ALT_F:x}")
    scr = "\n".join(look("menu"))
    for want in ("SAVE AS...", "PRINT...", "EXIT"):
        assert want in scr, (want, scr)
    press("esc")

    # Lines to the clipboard and back at the cursor. The paste once lost its
    # line number to the text it copied, and appended at the end.
    press("up", "up", "up", "up")
    shift(True)
    press("down")
    shift(False)
    press("0x92", "down", "down", "down")  # Ctrl+Ins; to the empty line after NEXT
    shift(True)
    press("0x52")  # Shift+Ins
    shift(False)
    assert text()[:5] == ["FOR I=1 TO 3", "  PRINT I*I", "NEXT", "FOR I=1 TO 3", ""], text()
    assert look()[24].endswith("00005:001"), look()[24]

    # A runtime error: the cursor on its line, the error in a box.
    press("ctrl+y")
    keys("goto 99\n")
    press(f"0x{F5:x}")
    scr = look("error")
    assert any("UNDEF'D STATEMENT ERROR" in r for r in scr) and scr[24].endswith("00005:001"), scr
    press("enter")

    # New: the unsaved text asks first; N, its button's letter, answers No.
    press(f"0x{ALT_F:x}", "n")
    assert any("NOT SAVED" in r for r in look()), look()
    press("n")
    assert text()[0] == "" and "UNTITLED" in look()[1], look()

    # Open. Its header was two bytes too long: controls drawn from garbage.
    press(f"0x{ALT_F:x}", "o")
    scr = look("open")
    assert "FILE NAME:  *.BAS" in "\n".join(scr) and any(r.split() == ["SQ.BAS", ".."] for r in scr), scr
    press("tab", "enter")
    assert "SQ.BAS" in look()[1] and text()[:3] == ["FOR I=1 TO 3", "  PRINT I*I", "NEXT"], look()

    # Find selects the match; Change All.
    press(f"0x{ALT_S:x}", "f")
    keys("i*i\n")
    assert look()[24].endswith("00002:012"), look()[24]
    press(f"0x{ALT_S:x}", "c")
    keys("3")
    press("tab")
    keys("4")
    press("tab", "tab", "tab", "tab")
    look("change")
    press("enter")
    assert any("CHANGE COMPLETE" in r for r in look()), look()
    press("enter")
    assert text()[0] == "FOR I=1 TO 4", look()
    # Change All finding nothing once left the cursor at column -1, the
    # window scrolled off the text.
    press(f"0x{ALT_S:x}", "c")
    keys("zz")
    press("tab", "tab", "tab", "tab", "tab", "enter", "enter")
    assert text()[0] == "FOR I=1 TO 4", look()

    # Save As, then Exit to DOS.
    press(f"0x{ALT_F:x}", "a")
    keys("t1\n")
    assert "T1.BAS" in look()[1], look()
    saved = (work / "T1.BAS").read_bytes()
    assert saved.startswith(b"FOR I=1 TO 4\r\n  PRINT I*I\r\nNEXT\r\n"), saved
    press(f"0x{ALT_F:x}", "x")
    assert "C:\\>" in look(), look()
    s.close()
    print("ok")


if __name__ == "__main__":
    play()
