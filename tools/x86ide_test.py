#!/usr/bin/env python3
"""x86ide_test.py: the QBasic-style editor (make ide) played on the emulated console: free-form lines, F5 on
the output page, the immediate window, menus, clipboard, the file and search dialogs. Screenshots go to docs/img."""

import os
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
# X86_IDE=build/x86/ide32.exe plays the flat build, DOS32A.EXE beside it
IDE = Path(os.environ.get("X86_IDE", ROOT / "build/x86/basic-ide.com"))


def play() -> None:
    work = Path(tempfile.mkdtemp())
    exe = "BASIC" + IDE.suffix.upper()
    (work / exe).write_bytes(IDE.read_bytes())
    if IDE.suffix == ".exe":
        (work / "DOS32A.EXE").write_bytes((IDE.parent / "DOS32A.EXE").read_bytes())
    (work / "BASIC.HLP").write_bytes((ROOT / "src/x86/ui/basic.hlp").read_bytes())
    (work / "SQ.BAS").write_bytes(b"for i=1 to 3\r\n  print i*i\r\nnext\r\n")
    (work / "SUB").mkdir()
    s = qbplay.Session(work)
    s.launch(3000, exe)
    s.keys_in_lockstep(["esc"], 3000)  # the first key after the launch is lost
    s.resume(100)
    s.run_until(lambda: "PRESS ESC" in s.screen().upper(), 5000, "the welcome box")  # DOS/32A's start is slower
    # The welcome box's button, of no set width, once ran past its frame.
    scr = s.screen().upper().split("\n")
    r = next(i for i, line in enumerate(scr) if "PRESS ESC" in line)
    raw = bytes.fromhex(s.send({"cmd": "mem_read_linear", "addr": 0xB8000 + r * 160, "len": 160})["data"])[0::2]
    assert raw.index(b">") < raw.rindex(b"\xb3") - 1, raw
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

    def ctrl_y() -> None:
        s.send({"cmd": "key", "scancode": 0x15, "ascii": 0x19, "stop": True})
        s.run_to_key(3000)
        s.resume(60)

    def flags(value: int) -> None:
        """The BIOS shift flags, 02h Shift, 08h Alt."""
        s.send({"cmd": "mem_write_linear", "addr": 0x417, "data": f"{value:02x}"})

    def mouse_stub() -> None:
        """INT 33h replaced: AX=3 answers from 0:0540h, the rest return. The key socket has no mouse."""
        stub = "3d03007510" "2e8b1e4005" "2e8b0e4205" "2e8b164405" "cfcf"
        s.send({"cmd": "mem_write_linear", "addr": 0x520, "data": stub})
        s.send({"cmd": "mem_write_linear", "addr": 0x33 * 4, "data": "20050000"})

    def mouse(row: int, col: int, down: bool) -> None:
        state = bytes([int(down), 0, col * 8 % 256, col * 8 // 256, row * 8, 0])
        s.send({"cmd": "mem_write_linear", "addr": 0x540, "data": state.hex()})
        s.resume(60)

    def click(row: int, col: int) -> None:
        mouse(row, col, True)
        mouse(row, col, False)

    def attrs(row: int) -> bytes:
        """The attributes of a row of page 0."""
        data = bytes.fromhex(s.send({"cmd": "mem_read_linear", "addr": 0xB8000 + row * 160, "len": 160})["data"])
        return data[1::2]

    def chars(row: int) -> bytes:
        """The characters of a row of page 0."""
        data = bytes.fromhex(s.send({"cmd": "mem_read_linear", "addr": 0xB8000 + row * 160, "len": 160})["data"])
        return data[0::2]

    def look(name: str = "") -> list[str]:
        s.resume(100)
        if name and IDE.suffix == ".com":  # the docs' pictures are the .COM's
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
    # QBasic's status lines (qbasmsgs.txt), less the keys we lack.
    assert look("editor")[24].startswith(" <SHIFT+F1=HELP> <F6=WINDOW> <F5=RUN>  "), look()[24]
    press(f"0x{F5:x}")
    look("output")
    out = output()
    assert out[:3] == [" 1", " 4", " 9"] and out[24] == "PRESS ANY KEY TO CONTINUE", out
    press("space")

    # The immediate window: output on page 1. The prompt once stayed there,
    # in front of the next output.
    # The WordStar keys of COW's edit manager (edittbls.inc).
    def ctrl(c: str) -> None:
        code = ord(c.upper()) - 64
        s.send({"cmd": "key", "scancode": 0, "ascii": code, "stop": True})
        s.run_to_key(3000)
        s.resume(60)

    press("up")  # to NEXT, 3
    ctrl("d")
    ctrl("d")
    assert look()[24].endswith("00003:003"), look()[24]
    ctrl("s")
    ctrl("q")
    assert look()[24][64:66] == "^Q", look()[24]
    keys("d")
    assert look()[24].endswith("00003:005") and look()[24][64:66] == "  ", look()[24]
    ctrl("q")
    keys("s")
    ctrl("d")
    ctrl("q")
    ctrl("y")  # Ctrl+Q,Y: the rest of the line out
    assert text()[2] == "N", text()
    keys("ext")
    press("down", "home")
    # Caps Lock and Num Lock: C at 67, N at 68 (uiwind.c).
    flags(0x60)
    press("right")
    flags(0)
    assert look()[24][67:69] == "CN", look()[24]
    press("left")
    assert look()[24][67:69] == "  ", look()[24]
    # QBasic's title bar (uiwind.c): the active title and the maximise
    # arrow in the window's colours swapped, ┤↑├ ending at ─┐.
    assert chars(1)[75:80] == b"\xb4\x18\xc3\xc4\xbf" and attrs(1)[76] == 0x71, (chars(1)[74:], attrs(1)[74:])
    assert attrs(1)[34:44] == bytes([0x71] * 10) and attrs(1)[44] == 0x17, attrs(1)[32:46]  # " Untitled ", at (80-8-5)/2+1
    press("f6")
    assert look()[24].startswith(" <SHIFT+F1=HELP> <F6=WINDOW> <ENTER=EXECUTE LINE>  "), look()[24]
    # Scroll bars on the active window only.
    assert chars(20)[1] == 0x20 and chars(5)[79] == 0xB3, (chars(20)[:4], chars(5)[78:])
    keys("print 6*7\n")
    press("space")
    assert output()[24] == "", output()
    keys("print 8\n")
    out = output()
    assert " 42" in out and " 8" in out and out.count("PRESS ANY KEY TO CONTINUE") == 1, out
    press("space", "f6")

    # Alt+F with Alt still held: letting Alt go once counted as an Alt tap
    # and closed the menu, so it could not be moved in.
    flags(0x08)
    press(f"0x{ALT_F:x}")
    flags(0)
    s.resume(100)  # Alt let go while the menu waits
    press("down")
    scr = look()
    assert any("OPEN..." in r for r in scr) and scr[24].startswith(" F1=HELP   LOADS NEW FILE INTO MEMORY"), scr
    bar = s.send({"cmd": "mem_read_linear", "addr": 0xB8000 + 24 * 160 + 18, "len": 1})["data"]
    assert bar.lower() == "b3", bar  # F1=Help │
    press("esc", "esc")
    # Alt held on: Alt+Down and Alt+letter come as other keys, which the menu
    # ignored, so it worked only once Alt was let go.
    flags(0x08)
    press(f"0x{ALT_F:x}", "0xa0")
    assert "LOADS NEW FILE INTO MEMORY" in look()[24], look()
    press("0x18")  # Alt+O: Open..., which asks about the unsaved text first
    flags(0)
    assert any("NOT SAVED" in r for r in look()), look()
    press("esc")

    # Every File item drops down: a separator once ended the menu after Save As.
    press(f"0x{ALT_F:x}")
    scr = "\n".join(look("menu"))
    for want in ("SAVE AS...", "PRINT...", "EXIT"):
        assert want in scr, (want, scr)
    # QBasic's title columns (uirsrcc.c rxTitle): ours sat one to the left.
    bar = look()[0]
    for title, col in (("FILE", 3), ("EDIT", 9), ("VIEW", 15), ("SEARCH", 21), ("RUN", 29), ("DEBUG", 34), ("OPTIONS", 41), ("HELP", 74)):
        assert bar[col : col + len(title)] == title, (title, bar)
    assert look()[2][3:6] == "NEW", look()[2]  # the box at title-2, items at +2
    assert attrs(0)[2:9] == b"\x07\x0f\x07\x07\x07\x07\x70", attrs(0)[:10]
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

    # A runtime error: its statement's word selected, the error in a box,
    # moved to the foot when it would cover that line (uiedit.asm).
    keys("rem\n" * 4 + "  goto 99\n")
    press(f"0x{F5:x}")
    scr = look("error")
    assert next(i for i, r in enumerate(scr) if "UNDEF'D" in r) == 19, scr
    assert attrs(10)[2:8] == bytes([0x17, 0x71, 0x71, 0x71, 0x71, 0x17]), attrs(10)[:9]
    assert any("UNDEF'D STATEMENT ERROR" in r for r in scr) and scr[24].startswith(" F1=HELP   ENTER=EXECUTE"), scr
    # QBasic's message box (msgbox.c): text +6 wide, OK and Help 8 wide, 3 apart.
    row = next(r for r in scr if "< HELP >" in r)
    assert row.ljust(80)[25:55] == "     <  OK  >   < HELP >      ", scr
    press("tab", "enter")  # Help: nothing to show, the box stays
    assert any("UNDEF'D STATEMENT ERROR" in r for r in look()), look()
    press("tab")  # back to OK
    press("enter")
    assert look()[24].endswith("00009:003") and attrs(10)[3] == 0x17, (look()[24], attrs(10)[:9])

    # New: the unsaved text asks first; N, its button's letter, answers No.
    press(f"0x{ALT_F:x}", "n")
    assert any("NOT SAVED" in r for r in look()), look()
    press("n")
    assert text()[0] == "" and "UNTITLED" in look()[1], look()

    # Open. Its header was two bytes too long: controls drawn from garbage.
    press(f"0x{ALT_F:x}", "o")
    scr = look("open")
    assert scr[24].rstrip() == " F1=HELP   ENTER=EXECUTE   ESC=CANCEL   TAB=NEXT FIELD   ARROW=NEXT ITEM", scr[24]
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
    press("tab", "tab", "tab", "tab", "tab", "enter")
    assert any("MATCH NOT FOUND" in r for r in look()), look()  # QBasic's, not "Change complete"
    press("enter")
    assert text()[0] == "FOR I=1 TO 4", look()
    # Find and Verify asks in QBasic's box: buttons only, at row 19 (uisearch.des).
    press("0x77", f"0x{ALT_S:x}", "c")
    keys("4")
    press("tab")
    keys("5\n")
    row = look("verify")[22].ljust(80)
    assert (row[17:27], row[29:37], row[41:51], row[54:62]) == ("< CHANGE >", "< SKIP >", "< CANCEL >", "< HELP >"), look()
    press("esc")  # Cancel ends it, no box after
    assert text()[0] == "FOR I=1 TO 4" and not any("COMPLETE" in r for r in look()), look()
    # Ctrl+L repeats the find, as QBasic.
    press("0x77", f"0x{ALT_S:x}", "f")
    keys("i\n")
    s.send({"cmd": "key", "scancode": 0x26, "ascii": 0x0C, "stop": True})
    s.run_to_key(3000)
    assert look()[24].endswith("00002:006"), look()[24]
    # Find starts from the selection, else the word at the cursor.
    press(f"0x{ALT_S:x}", "f")
    scr = [r.ljust(80) for r in look("find")]
    # QBasic's dlgSearchFind (uisearch.des), its places client-relative.
    assert scr[9][12:22] == "FIND WHAT:" and scr[9][24:27] == "I  ", scr
    assert (scr[16][19:25], scr[16][34:44], scr[16][52:60]) == ("< OK >", "< CANCEL >", "< HELP >"), scr[16]
    # Its letters lit once Alt is down (COW dlgcore.c); Alt+M checks Match.
    assert 0x7F not in attrs(9)[11:60], attrs(9)[11:60]
    flags(0x08)
    s.resume(100)
    flags(0)
    s.resume(100)
    assert attrs(9)[12] == 0x7F and attrs(9)[13] == 0x70, attrs(9)[11:16]
    flags(0x08)
    press("0x32")  # Alt+M
    flags(0)
    assert "[X] MATCH UPPER/LOWERCASE" in look()[13], look()
    flags(0x08)
    press("0x32")  # once more: off
    flags(0)
    press("esc", "0x77", *["right"] * 8, f"0x{ALT_S:x}", "f")
    scr = look(); assert scr[9].ljust(80)[24:27] == "TO ", scr
    press("esc")

    # Save As, then Exit to DOS.
    press(f"0x{ALT_F:x}", "a")
    keys("t1\n")
    assert "T1.BAS" in look()[1], look()
    saved = (work / "T1.BAS").read_bytes()
    assert saved.startswith(b"FOR I=1 TO 4\r\n  PRINT I*I\r\nNEXT\r\n"), saved
    # Save As onto another file asks first (uifile.c MSG_Overwrite); No asks for a name again.
    press(f"0x{ALT_F:x}", "a")
    keys("sq\n")
    assert any("FILE ALREADY EXISTS. OVERWRITE?" in r for r in look()), look()
    press("n")
    assert any("SAVE AS" in r for r in look()), look()
    press("esc")
    assert (work / "SQ.BAS").read_bytes().startswith(b"for i=1 to 3"), "SQ.BAS overwritten"
    # The mouse: a title, an item dragged to, a list's line twice.
    press("home")  # the found text's selection off
    mouse_stub()
    click(0, 9)
    assert any("CUT" in r for r in look()), look()
    # No selection: Copy is gray, Paste (the Clipboard has a line) not.
    assert attrs(3)[10] == 0x78 and attrs(4)[10] == 0x70, (attrs(3)[8:12], attrs(4)[8:12])
    # Help lines come from BASIC.HLP, counted across the menus.
    assert look()[24].startswith(" F1=HELP   DELETES SELECTED TEXT AND COPIES IT TO BUFFER"), look()[24]
    # A gray item's key lights it and beeps, the menu stays (COW menu.c).
    press("c")
    assert attrs(3)[10] == 0x07 and any("CUT" in r for r in look()), attrs(3)[8:12]
    click(12, 60)
    assert not any("CUT" in r for r in look()), look()
    mouse(0, 3, True)
    mouse(3, 5, True)  # Open...
    mouse(3, 5, False)
    scr = look()
    row = next(r for r, line in enumerate(scr) if "SQ.BAS" in line)
    click(row, 14)
    click(row, 14)
    assert "SQ.BAS" in look()[1] and text()[0] == "FOR I=1 TO 3", look()
    # A click places the cursor; a drag selects whole lines.
    click(3, 5)
    assert look()[24].endswith("00002:005"), look()[24]
    click(2, 1)
    mouse(2, 1, True)
    mouse(4, 1, True)
    mouse(4, 1, False)
    s.resume(100)
    assert attrs(2)[1] == attrs(3)[1] == 0x71 and attrs(4)[1] == 0x17, (attrs(2)[:4], attrs(4)[:4])
    # <F5=Run> on the status bar.
    click(24, 32)
    s.resume(300)
    assert output()[:3] == [" 1", " 4", " 9"], output()
    press("space")

    # Tab completes from what fits where the word is.
    def line() -> str:
        return text()[3]

    def complete(typed: str) -> list[str]:
        """Typed on an empty line 4, then Tab: the screen."""
        press("0x75", "0x4f", "enter")  # Ctrl+End, End: a new last line
        if line():
            ctrl_y()
        for c in typed:
            if c == '"':
                s.send({"cmd": "key", "scancode": 0x28, "ascii": 0x22, "stop": True})
                s.run_to_key(3000)
                s.resume(60)
            else:
                keys(c)
        press("tab")
        return look()

    scr = complete("pr")  # a statement's start: statements, the common part typed
    assert line() == "PRINT" and scr[7].strip() == "PRINT" and scr[8].strip() == "PRINT#", scr
    assert attrs(7)[3] == 0x07 and attrs(8)[3] == 0x70, attrs(7)[:6]
    press("down", "enter")
    assert line() == "PRINT#", line()
    # Ctrl+Y once checked the line first: PRINT# alone got a syntax error box,
    # which then took the keys, and the line stayed.
    ctrl_y()
    assert line() == "" and not any("SYNTAX" in r for r in look()), look()
    scr = complete("print le")  # an operand: functions and variables
    assert [r.split()[0] for r in scr[7:9] if r.split()] == ["LEFT$", "LEN"], scr
    press("esc")
    ctrl_y()
    scr = complete("print mi")  # one fits: put in, its ( and its form shown
    assert line() == "PRINT MID$(" and "MID$(S$, START[, N])" in scr[24], scr[24]
    ctrl_y()
    complete("for j=1 t")  # after an operand: the words that go on
    assert line() == "FOR J=1 TO", line()
    ctrl_y()
    complete("next i")  # NEXT names a variable: I is all there is, so Tab indents
    assert line() == "NEXT I" and look()[24].endswith(":009"), look()[24]
    ctrl_y()
    complete("rem pr")  # nothing in a remark
    assert line() == "REM PR" and look()[24].endswith(":009"), look()[24]
    ctrl_y()
    complete('print "pr')  # nor in a string
    assert line() == 'PRINT "PR' and look()[24].endswith(":017"), look()[24]
    ctrl_y()
    scr = complete("x$=")  # a string wanted: string names and functions only
    assert [r.split()[0] for r in scr[7:10]] == ["X$", "CHR$", "LEFT$"], scr
    assert not any(r.split() and r.split()[0] in ("I", "INT", "FN") for r in scr[7:16]), scr
    press("esc")
    ctrl_y()
    complete("n=m")  # a number wanted: no MID$, so Tab indents
    assert line() == "N=M" and look()[24].endswith(":009"), look()[24]
    ctrl_y()
    complete("for k=1 to 2:next ")  # NEXT: the open FOR's variable
    assert line() == "FOR K=1 TO 2:NEXT K", line()
    ctrl_y()
    press("0x75", "0x4f", "enter")  # GOSUB: a line after RETURN first, its text shown
    keys("10 return\n20 rem b\ngosub ")
    press("tab")
    scr = look()
    assert scr[9].split()[:3] == ["20", "REM", "B"] and scr[10].split()[:2] == ["10", "RETURN"], scr
    press("enter")
    assert text()[5] == "GOSUB 20", text()

    press(f"0x{ALT_F:x}", "x")
    if any("NOT SAVED" in r for r in look()):
        press("n")
    assert "C:\\>" in look(), look()
    s.close()
    print("ok")


if __name__ == "__main__":
    play()
