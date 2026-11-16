#!/usr/bin/env python3
"""x86ide_test.py: the QBasic-style editor (make ide) played on the emulated console: free-form lines, F5 on
the output page, the immediate window, menus, clipboard, the file and search dialogs. Screenshots go to docs/img."""

import os
import sys
import tempfile

from PIL import Image
from pathlib import Path

LLRM = Path("/Users/alim/work/personal/llrm-qbmac/tools")
sys.path[:0] = [str(LLRM), str(LLRM / "dosbatch")]
import llrmbin  # noqa: E402

llrmbin.stale_binaries = lambda *a, **k: []  # the emulator is the stamped one; the llrm sources are not ours
import qbplay  # noqa: E402
import x86run  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
IMG = ROOT / "docs/img"
ALT_F, ALT_S, F4, F5 = 0x21, 0x1F, 0x3E, 0x3F
MSGS = x86run.messages()
# X86_IDE=build/x86/ide32.exe plays the flat build, DOS32A.EXE beside it
IDE = Path(os.environ.get("X86_IDE", ROOT / "build/x86/ide32.exe"))


def play() -> None:
    work = Path(tempfile.mkdtemp())
    exe = "BASIC" + IDE.suffix.upper()
    (work / exe).write_bytes(IDE.read_bytes())
    if IDE.suffix == ".exe":
        (work / "DOS32A.EXE").write_bytes((IDE.parent / "DOS32A.EXE").read_bytes())
    hlp = "src/ui/basic.hlp"
    (work / "BASIC.HLP").write_bytes((ROOT / hlp).read_bytes())
    (work / "SQ.BAS").write_bytes(b"for i=1 to _\r\n 3\r\n  print i*i\r\nnext\r\n")
    (work / "SUB").mkdir()
    s = qbplay.Session(work)
    s.launch(3000, exe)
    s.keys_in_lockstep(["esc"], 3000)  # the first key after the launch is lost
    s.resume(100)
    s.run_until(lambda: "PRESS ESC" in s.screen().upper(), 5000, "the welcome box")  # DOS/32A's start is slower
    # The welcome box's button, of no set width, once ran past its frame.
    scr = s.screen().upper().split("\n")
    # The box named the 6502 project it came from: Welcome to CBM BASIC V2 for DOS.
    assert any("WELCOME TO QUICKRBASIC" in line for line in scr), scr
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
        state = bytes([int(down), 0, col * 8 % 256, col * 8 // 256, row * 8 % 256, row * 8 // 256])
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

    flat = IDE.suffix == ".exe"
    if flat:  # the checks of colours before the highlighting's own are of plain text
        press(f"0x{0x18:x}", "h")

    # Free-form lines, capitalised on leaving; F5 runs them.
    keys("for i=1 to 3\n")
    press("backspace")  # the body's indent Enter made, a step back: 2 typed
    keys("  print i*i\n")
    press("home")  # past the indent Enter kept
    keys("next\n")
    assert text()[:3] == ["FOR I = 1 TO 3", "  PRINT I * I", "NEXT"], text()
    # QBasic's status lines (qbasmsgs.txt), less the keys we lack.
    assert look("editor")[24].startswith(" <SHIFT+F1=HELP> <F6=WINDOW> <F5=RUN> <F8=STEP>  "), look()[24]
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
    press("space")
    # A line that leaves SCREEN 13: the editor came back drawn on a screen still in it, unseen.
    # The lines short: the immediate window's last shows beside a box drawn over its row later.
    def peek(addr: int) -> int:
        return bytes.fromhex(s.send({"cmd": "mem_read_linear", "addr": addr, "len": 1})["data"])[0]

    keys("screen 13\n")
    press("space")
    assert peek(0x449) == 3 and "UNTITLED" in look()[1], (peek(0x449), look()[:2])
    # The program's screen kept: F4 shows it again, and a line goes on drawing in its mode.
    keys("pset (1,1),4\n")
    press("space")
    press(f"0x{F4:x}")
    assert peek(0x449) == 0x13 and peek(0xA0000 + 321) == 4, (peek(0x449), peek(0xA0000 + 321))
    press("space")
    assert peek(0x449) == 3 and "UNTITLED" in look()[1], (peek(0x449), look()[:2])
    press("f6")
    # RUN starts the program in text mode: F4 after shows its text, not the last SCREEN 13's.
    press(f"0x{F5:x}")
    press("space")
    press(f"0x{F4:x}")
    assert peek(0x449) == 3 and output()[:3] == [" 1", " 4", " 9"], (peek(0x449), output()[:3])
    press("space")

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
    assert text()[:5] == ["FOR I = 1 TO 3", "  PRINT I * I", "NEXT", "FOR I = 1 TO 3", ""], text()
    assert look()[24].endswith("00005:001"), look()[24]

    # A runtime error: its statement's word selected, the error in a box,
    # moved to the foot when it would cover that line (uiedit.asm).
    # ERROR 8 is Label not defined when run; a GOTO to no label is reported at RUN, before it runs.
    keys("next\n" + "rem\n" * 3 + "  error 8\n")  # the pasted FOR closed: QBasic pairs blocks first
    press(f"0x{F5:x}")
    scr = look("error")
    assert next(i for i, r in enumerate(scr) if "LABEL NOT DEFINED" in r) == 19, scr
    assert attrs(10)[2:9] == bytes([0x17, 0x71, 0x71, 0x71, 0x71, 0x71, 0x17]), attrs(10)[:9]  # ERROR
    assert any("LABEL NOT DEFINED" in r for r in scr) and scr[24].startswith(" F1=HELP   ENTER=EXECUTE"), scr
    # QBasic's message box (msgbox.c): text +6 wide, OK and Help 8 wide, 3 apart.
    row = next(r for r in scr if "< HELP >" in r)
    assert row.ljust(80)[25:55] == "     <  OK  >   < HELP >      ", scr
    press("tab", "enter")  # Help: nothing to show, the box stays
    assert any("LABEL NOT DEFINED" in r for r in look()), look()
    press("tab")  # back to OK
    press("enter")
    assert look()[24].endswith("00009:003") and attrs(10)[3] == 0x17, (look()[24], attrs(10)[:9])

    # New: the unsaved text asks first; N, its button's letter, answers No.
    press(f"0x{ALT_F:x}", "n")
    assert any("NOT SAVED" in r for r in look()), look()
    press("n")
    assert text()[0] == "" and "UNTITLED" in look()[1], look()

    # Open. Its header was two bytes too long: controls drawn from garbage. A line
    # continued with '_' was two lines, the first a syntax error.
    press(f"0x{ALT_F:x}", "o")
    scr = look("open")
    assert scr[24].rstrip() == " F1=HELP   ENTER=EXECUTE   ESC=CANCEL   TAB=NEXT FIELD   ARROW=NEXT ITEM", scr[24]
    assert "FILE NAME:  *.BAS" in "\n".join(scr) and any(r.split() == ["SQ.BAS", ".."] for r in scr), scr
    press("tab", "enter")
    assert "SQ.BAS" in look()[1] and text()[:3] == ["FOR I = 1 TO 3", "  PRINT I * I", "NEXT"], look()

    # Find selects the match; Change All.
    press(f"0x{ALT_S:x}", "f")
    keys("i * i\n")
    assert look()[24].endswith("00002:014"), look()[24]
    press(f"0x{ALT_S:x}", "c")
    keys("3")
    press("tab")
    keys("4")
    press("tab", "tab", "tab", "tab")
    look("change")
    press("enter")
    assert any("CHANGE COMPLETE" in r for r in look()), look()
    press("enter")
    assert text()[0] == "FOR I = 1 TO 4", look()
    # Change All finding nothing once left the cursor at column -1, the
    # window scrolled off the text.
    press(f"0x{ALT_S:x}", "c")
    keys("zz")
    press("tab", "tab", "tab", "tab", "tab", "enter")
    assert any("MATCH NOT FOUND" in r for r in look()), look()  # QBasic's, not "Change complete"
    press("enter")
    assert text()[0] == "FOR I = 1 TO 4", look()
    # Find and Verify asks in QBasic's box: buttons only, at row 19 (uisearch.des).
    press("0x77", f"0x{ALT_S:x}", "c")
    keys("4")
    press("tab")
    keys("5\n")
    row = look("verify")[22].ljust(80)
    assert (row[17:27], row[29:37], row[41:51], row[54:62]) == ("< CHANGE >", "< SKIP >", "< CANCEL >", "< HELP >"), look()
    press("esc")  # Cancel ends it, no box after
    assert text()[0] == "FOR I = 1 TO 4" and not any("COMPLETE" in r for r in look()), look()
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
    press("esc", "0x77", *["right"] * 10, f"0x{ALT_S:x}", "f")
    scr = look(); assert scr[9].ljust(80)[24:27] == "TO ", scr
    press("esc")

    # Save As, then Exit to DOS.
    press(f"0x{ALT_F:x}", "a")
    keys("t1\n")
    assert "T1.BAS" in look()[1], look()
    saved = (work / "T1.BAS").read_bytes()
    assert saved.startswith(b"FOR i = 1 TO 4\r\n  PRINT i * i\r\nNEXT\r\n"), saved
    # Save As onto another file asks first (uifile.c MSG_Overwrite); No asks for a name again.
    press(f"0x{ALT_F:x}", "a")
    keys("sq\n")
    assert any("FILE ALREADY EXISTS. OVERWRITE?" in r for r in look()), look()
    press("n")
    assert any("SAVE AS" in r for r in look()), look()
    press("esc")
    assert (work / "SQ.BAS").read_bytes().startswith(b"for i=1 to _"), "SQ.BAS overwritten"
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
    assert "SQ.BAS" in look()[1] and text()[0] == "FOR I = 1 TO 3", look()
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

    # Tab completes from what fits where the word is; a word's second letter lists it as typed.
    def line() -> str:
        return text()[3]

    def typed(chars: str) -> list[str]:
        """Typed on an empty line 4: the screen."""
        press("0x75", "0x4f", "enter")  # Ctrl+End, End: a new last line
        if line():
            ctrl_y()
        for c in chars:
            if c == '"':
                s.send({"cmd": "key", "scancode": 0x28, "ascii": 0x22, "stop": True})
                s.run_to_key(3000)
                s.resume(60)
            else:
                keys(c)
        return look()

    def complete(chars: str) -> list[str]:
        """typed, then Tab: the screen."""
        typed(chars)
        press("tab")
        return look()

    def dim(col: int) -> bool:
        """Line 4's column col (0 the first) in the likeliest's dim rest."""
        return attrs(5)[col + 1] & 0x0F == 0x08

    scr = typed("pr")  # the likeliest's rest dim past the cursor, no list
    assert line() == "PRINT" and dim(2) and not dim(1) and not scr[7].strip(), scr
    press("tab")  # Tab takes it
    assert line() == "PRINT" and not dim(2), line()
    # Ctrl+Y once checked the line first: PRINT# alone got a syntax error box,
    # which then took the keys, and the line stayed.
    ctrl_y()
    assert line() == "" and not any("SYNTAX" in r for r in look()), look()
    scr = complete("print mi")  # Tab takes the likeliest: its ( and its form shown
    assert line() == "PRINT MID$(" and "MID$(S$, START[, N])" in scr[24], scr[24]
    ctrl_y()
    typed("print lef")  # the rest follows the word
    assert line() == "PRINT LEFT$" and dim(9), line()
    press("enter")  # Enter is typed: the rest left
    assert text()[3:5] == ["PRINT LEF", ""], text()
    press("backspace")  # the lines joined again
    ctrl_y()
    typed("function sum% ")  # after a space, nothing it needs: no guess
    assert line() == "FUNCTION SUM%" and not dim(14), line()
    ctrl_y()
    typed("for j")  # what the statement needs next: FOR's '='
    assert line() == "FOR J=" and dim(5), line()
    ctrl_y()
    complete("for j=1")  # its TO
    assert line() == "FOR J=1 TO", line()
    ctrl_y()
    complete("for j=1 t")
    assert line() == "FOR J=1 TO", line()
    ctrl_y()
    scr = complete("next i")  # a whole name, nothing needed: Tab lists what goes on after it
    assert "I," in [r.strip() for r in scr[5:20]], scr
    press("escape")
    assert line() == "NEXT I", line()
    ctrl_y()
    complete("rem pr")  # nothing in a remark
    assert line() == "REM PR" and look()[24].endswith(":009"), look()[24]
    ctrl_y()
    complete('print "pr')  # nor in a string
    assert line() == 'PRINT "PR' and look()[24].endswith(":013"), look()[24]  # Tab: the next step of 4
    ctrl_y()
    # The editor's own reader took all after DATA as DATA's: no statement after its ':'.
    complete("data 1:pr")  # a statement after DATA's ':'
    assert line() == "DATA 1:PRINT", line()
    ctrl_y()
    scr = complete("x$=")  # a string wanted: string names and functions only
    assert "X$" in [r.split()[0] for r in scr[7:15] if r.split()], scr
    assert not any(r.split() and r.split()[0] in ("I", "INT", "FN", "LEN") for r in scr[7:16]), scr
    press("esc")
    ctrl_y()
    scr = complete("n=m")  # a number wanted: no MID$, what goes on after the new name
    assert "MID$" not in [r.strip() for r in scr[5:20]] and "M:" in [r.strip() for r in scr[5:20]], scr
    press("escape")
    assert line() == "N=M", line()
    ctrl_y()
    complete("for k=1 to 2:next ")  # NEXT: the open FOR's variable
    assert line() == "FOR K=1 TO 2:NEXT K", line()
    ctrl_y()
    # Auto indent: a block's body a step (4) in, its end back at its start; Tab to the next step,
    # Backspace in the indent to the one before.
    typed("for k = 1 to 2")
    press("enter")
    keys("print k")
    press("enter")
    keys("next")
    press("enter")
    assert text()[3:6] == ["FOR K = 1 TO 2", "    PRINT K", "NEXT"], text()
    press("tab")
    keys("x")
    assert text()[6].startswith("    X"), text()  # a step in; X='s '=' dim after it
    press("backspace", "backspace")
    assert text()[6] == "", text()
    for _ in range(4):
        ctrl_y()
    assert text()[3] == "", text()
    press("0x75", "0x4f", "enter")  # GOSUB: a line after RETURN first
    keys("10 return\n20 rem b\ngosub ")
    assert text()[5] == "GOSUB 20", text()
    press("tab")
    assert text()[5] == "GOSUB 20", text()

    # A typed SUB, then its call without CALL: each line, checked alone, was a syntax error (the
    # procedure table is the link's), and the SUB line stayed in small letters.
    press(f"0x{ALT_F:x}", "n")
    if any("NOT SAVED" in r for r in look()):
        press("n")
    # The SUB line opens the procedure's own view, its END SUB made (QBasic's); Ctrl+F2 the module again.
    press("home")
    keys("sub hi\n")
    assert chars(2)[1:7] == b"SUB hi" and text()[:3] == ["SUB HI", "", "END SUB"], look()
    assert look()[1].strip() == "UNTITLED:HI" and look()[24].endswith(":005"), look()  # its body a step in
    keys("print 7\n")
    assert text()[1] == "    PRINT 7", text()
    assert not any(m.upper() in r for m in MSGS for r in look()), look()
    press("0x5f")
    keys("hi\n")
    assert not any(m.upper() in r for m in MSGS for r in look()), look()
    press(f"0x{F5:x}")
    assert output()[0] == " 7", output()
    press("space")

    # A '$INCLUDE line's file was not read when the editor's lines were entered: A was 0, not 41.
    if flat:
        (work / "INC.BI").write_bytes(b"a = 41\r\n")
        (work / "INC.BAS").write_bytes(b"'$INCLUDE: 'INC.BI'\r\nPRINT a + 1\r\n")
        press(f"0x{ALT_F:x}", "o")
        if any("NOT SAVED" in r for r in look()):
            press("n")
        keys("inc.bas\n")
        press(f"0x{F5:x}")
        assert output()[0] == " 42", output()
        press("space")

    # A line edited in a SUB's view was checked as the module's, whichever procedure the file's last
    # line left: its record array parameter's field, b(1).x = 1, a syntax error.
    (work / "REC.BAS").write_bytes(b"TYPE P\r\nx AS INTEGER\r\nEND TYPE\r\nSUB s (b() AS P)\r\nEND SUB\r\n")
    press(f"0x{ALT_F:x}", "o")
    if any("NOT SAVED" in r for r in look()):
        press("n")
    keys("rec.bas\n")
    press("0x55")  # Shift+F2: the SUB's view
    assert look()[1].strip() == "REC.BAS:S", look()
    press("end")
    keys("\nb")
    for c, code in (("(", 0x0A), ("1", 0x02), (")", 0x0B)):
        s.send({"cmd": "key", "scancode": code, "ascii": ord(c), "stop": True})
        s.run_to_key(3000)
        s.resume(60)
    keys(".x = 1\n")
    assert not any(m.upper() in r for m in MSGS for r in look()), look()
    assert text()[1] == "    B(1).X = 1", text()  # LIST's spelling: not kept as typed

    # Views: the module first, each procedure its own, as QBasic loads a file. Shift+F2 the next,
    # Ctrl+F2 the one before, F2 the list; Edit, New FUNCTION.
    (work / "VW.BAS").write_bytes(b"SUB a\r\nPRINT 1\r\nEND SUB\r\na\r\nFUNCTION f\r\nf = 2\r\nEND FUNCTION\r\n")
    press(f"0x{ALT_F:x}", "o")
    if any("NOT SAVED" in r for r in look()):
        press("n")
    keys("vw.bas\n")
    assert look()[1].strip() == "VW.BAS" and text()[:2] == ["A", ""], look()
    press("0x55")
    assert look()[1].strip() == "VW.BAS:A" and text()[:4] == ["SUB A", "PRINT 1", "END SUB", ""], look()
    press("0x55")
    assert look()[1].strip() == "VW.BAS:F" and text()[0] == "FUNCTION F", look()
    press("0x55")
    assert look()[1].strip() == "VW.BAS", look()[1]
    press("0x5f")
    assert look()[1].strip() == "VW.BAS:F", look()[1]
    press("0x3c")
    scr = look()
    r = next(i for i, ln in enumerate(scr) if "VW.BAS" in ln and i > 1)
    assert [scr[r + k][11:20].strip() for k in range(3)] == ["VW.BAS", "A", "F"], scr
    press("up", "enter")
    assert look()[1].strip() == "VW.BAS:A", look()[1]
    press("0x12", "f")
    keys("g\n")
    assert look()[1].strip() == "VW.BAS:G" and text()[:3] == ["FUNCTION G", "", "END FUNCTION"], look()
    assert look()[24].endswith(":005"), look()[24]
    press("0x55")
    assert look()[1].strip() == "VW.BAS", look()[1]
    # A FUNCTION typed in a SUB's view was left there, missing from F2's list.
    press("0x55", "0x75")
    keys("\nfunction h\n")
    assert look()[1].strip() == "VW.BAS:H" and text()[:3] == ["FUNCTION H", "", "END FUNCTION"], look()
    # A number with an exponent, kept as text, was listed a character a word: 6 E 6, a syntax error.
    keys("h = 6e6 + 1.5e-2 + 2d3 + 1.5#\n")
    assert chars(3)[1:34] == b"    h = 6E6 + 1.5E-2 + 2D3 + 1.5#", chars(3)
    press("0x55")

    # Windows: Split, Alt+Plus and Alt+Minus a row, Ctrl+F10 the whole screen and back.
    def titles() -> list[int]:
        return [i for i, ln in enumerate(look()) if ln.strip() in ("VW.BAS", "IMMEDIATE")]

    assert titles() == [1, 21], titles()
    press(f"0x{0x2F:x}", "p")
    assert titles() == [1, 11, 21], titles()
    press("0x83")
    assert titles() == [1, 12, 21], titles()
    press("0x82", "0x82")
    assert titles() == [1, 10, 21], titles()
    press("0x67")
    assert titles() == [1], titles()
    press("0x67")
    assert titles() == [1, 10, 21], titles()
    press(f"0x{0x2F:x}", "p")
    assert titles() == [1, 21], titles()

    # Syntax highlighting from the compiler's lexer: keywords, strings, numbers, comments, a
    # declared name, its parameters, types, operators by the theme; Options' One Dark and QBasic
    # Colors, Syntax Highlighting off; Split Vertically.
    if flat:
        press(f"0x{ALT_F:x}", "n")
        if any("NOT SAVED" in r for r in look()):
            press("n")
        raw = {'"': 0x28, "'": 0x28, "(": 0x0A, ")": 0x0B, "+": 0x0D}  # keys() has none of these
        for c in 'PRINT "a"; 12: REM c\nDEF FNf(v) = v + 1: DIM x AS INTEGER\nx = 1 \' n\n':
            if c in raw:
                s.send({"cmd": "key", "scancode": raw[c], "ascii": ord(c), "stop": True})
                s.run_to_key(3000)
                s.resume(60)
            else:
                keys(c)
        press(f"0x{0x18:x}", "h")
        assert chars(2)[1:21] == b'PRINT "a"; 12: REM c', chars(2)
        assert chars(4)[1:10] == b"x = 1 ' n", chars(4)  # was listed as REM
        want = [0x1B] * 6 + [0x1E] * 3 + [0x17] * 2 + [0x1D] * 2 + [0x17] * 2 + [0x1A] * 5
        assert list(attrs(2)[1:21]) == want, attrs(2)[1:21].hex()
        # were all keyword or plain: DEF FNf(v) = v + 1: DIM x AS INTEGER
        want = [0x1B] * 4 + [0x1F] * 3 + [0x17, 0x1C] + [0x17] * 2 + [0x13] * 2 + [0x17] * 2 + [0x13] * 2
        want += [0x1D] + [0x17] * 2 + [0x1B] * 4 + [0x17] * 2 + [0x1B] * 3 + [0x13] * 7
        assert list(attrs(3)[1:37]) == want, attrs(3)[1:37].hex()
        # the themes, Options' Theme sub-menu: its box by its item, its help line; Left back
        press(f"0x{0x18:x}", "t")
        look()
        row = next(r for r in (2, 3, 4) if b"Theme" in chars(r))
        assert b"\x10" in chars(row) and b"QBasic Colors" in chars(row), chars(row)
        assert b"QBasic's colors" in chars(24), chars(24)
        press("0x4b")
        assert not any(b"One Dark" in chars(r) for r in range(1, 12))
        press("esc")
        press(f"0x{0x18:x}", "t", "d")
        assert list(attrs(2)[1:21:5]) == [0x15, 0x15, 0x17, 0x18], attrs(2)[1:21].hex()
        assert [attrs(3)[1 + i] for i in (0, 4, 8, 11, 29)] == [0x15, 0x19, 0x1D, 0x1A, 0x1B], attrs(3).hex()
        # Gruvbox Dark Soft: Zed's colours in the DAC, the editor's ground #32302F
        press(f"0x{0x18:x}", "t", "g")
        assert attrs(2)[1] == 0x15, attrs(2).hex()
        shot = Path(tempfile.mkdtemp()) / "th.png"
        s.snap(shot)
        px = Image.open(shot).convert("RGB").getpixel((364, 168))
        assert all(abs(a - b) <= 8 for a, b in zip(px, (0x32, 0x30, 0x2F))), px
        # a box's shadow darker than the box and the ground: it was the bars' colour, the box's own
        press(f"0x{ALT_F:x}")
        right = chars(1).index(0xBF, 1)
        s.snap(shot)
        img = Image.open(shot).convert("RGB")
        box, shade, ground = (img.getpixel((c * 9 + 8, 3 * 16 + 1)) for c in (right - 2, right + 1, right + 6))
        assert sum(shade) < min(sum(box), sum(ground)), (box, shade, ground)
        press("esc")
        press(f"0x{0x18:x}", "t", "q")
        press(f"0x{0x18:x}", "h")
        assert set(attrs(2)[1:21]) == {0x17}, attrs(2)[1:21].hex()
        press(f"0x{0x18:x}", "h")
        press(f"0x{0x2F:x}", "v")
        # One line between them, column 39 (there were two): a joint on top, ED1's scroll bar on it.
        assert chars(1)[0] == 0xDA and chars(1)[39] == 0xC2 and chars(2)[40:60] == chars(2)[1:21], (chars(1), chars(2))
        assert chars(2)[39] == 0x18 and attrs(2)[40] == 0x1B, (chars(2), attrs(2).hex())
        def lit_title() -> list[bool]:  # each window's title lit (the arrows always are)
            return [0x71 in attrs(1)[c : c + 30] for c in (5, 45)]

        assert lit_title() == [True, False], attrs(1).hex()
        click(2, 45)  # the window under the click by its column too
        assert lit_title() == [False, True], attrs(1).hex()
        assert chars(2)[39] == 0xB3 and chars(1)[39] == 0xC2, (chars(1), chars(2))  # ED2's bar at its right
        # Any line between windows drags: this one too, not the debug pane's alone.
        mouse(5, 39, True)
        mouse(5, 29, True)
        mouse(5, 29, False)
        assert chars(1)[29] == 0xC2 and chars(2)[30:50] == chars(2)[1:21], (chars(1), chars(2))
        press(f"0x{0x2F:x}", "v")
        assert chars(1)[60] == 0xC4 and look()[21].strip() == "IMMEDIATE", look()
        press(f"0x{0x18:x}", "h")

    # The debugger (QBasic's uidebug.asm). lit(): the lit statement's text line and text, the
    # breakpoints' lines.
    F7, F8, F9, F10, ALT_D, ALT_R = 0x41, 0x42, 0x43, 0x44, 0x20, 0x13

    def lit() -> tuple:  # the code's columns only: the debug pane is right of column 49 at a stop
        cur, bps = None, []
        for r in range(2, 20):
            a, c = attrs(r), chars(r)
            on = [i for i in range(1, 49) if a[i] == 0x1F]
            if on:
                cur = (r - 2, c[on[0] : on[-1] + 1].decode())
            if a[48] == 0x47:
                bps.append(r - 2)
        return cur, bps

    def step(key: int, want: tuple) -> None:
        press(f"0x{key:x}")
        assert lit()[0] == want, (want, lit(), look())

    def run_out() -> list[str]:
        """F5 to the run's Press any key: press() ends at the first key poll, and DOS's Ctrl+C check
        in a run's INT 21h is one."""
        press(f"0x{F5:x}")
        for _ in range(100):
            if output()[24]:
                break
            s.resume(100)
        return output()

    def open_file(name: str) -> None:
        press(f"0x{ALT_F:x}", "o")
        if any("NOT SAVED" in r for r in look()):
            press("n")
        keys(name + "\n")
        assert name.upper() in look()[1], look()

    def cursor(line: int) -> None:
        press("0x77")  # Ctrl+Home
        for _ in range(line):
            press("down")

    def pane() -> list[str]:
        """The debug pane's rows, from the line it shares with the code, column 49: to row 23, the
        immediate window hidden."""
        return [chars(r)[49:80].decode("latin-1") for r in range(1, 24)]

    def debug_pane() -> None:
        """At a stop: the code in columns 0-49; Watch, Locals, Call stack right of it; gone on a run."""
        fact = ["DECLARE SUB Fact (n%)", "DIM SHARED r&", "s$ = \"hi\"", "d# = 2.5#", "r& = 1", "Fact 4", "PRINT r&", "END",
                "SUB Fact (n%)", "t% = n% * 10", "IF n% > 3 THEN Fact n% - 1", "r& = r& * n%", "SHARED s$", "END SUB"]
        (work / "FACT.BAS").write_bytes("\r\n".join(fact).encode() + b"\r\n")
        open_file("fact.bas")
        assert "Watch" not in "".join(pane()), pane()
        press("0x55")  # Fact's view: the breakpoint at r& = r& * n%, the inner call's
        cursor(3)
        press(f"0x{F9:x}", "0x5f", f"0x{F5:x}")
        def frames() -> list[list[str]]:
            rows = pane()
            stack = rows[next(i for i, r in enumerate(rows) if "Call stack" in r) + 1 : -1]
            return [r[1:-1].split() for r in stack if r[1:-1].strip()]

        def locals_() -> list[str]:
            rows = pane()
            top = next(i for i, r in enumerate(rows) if "Locals" in r)
            end = next(i for i, r in enumerate(rows) if "Call stack" in r)
            return [r[2:-1].strip() for r in rows[top + 1 : end] if r[2:-1].strip()]

        rows = pane()
        assert rows[0].startswith("\xc2") and "Watch" in rows[0] and "\xbf" == rows[0][-1], rows
        # The immediate window took the stop's rows from the code: hidden while the pane shows.
        assert not any("IMMEDIATE" in r for r in look()), look()
        # Beside the pane the code has no scroll bars: one line between them, column 49, a square on
        # it the code's place.
        line = [chars(r)[49] for r in range(2, 23)]
        assert chars(1)[49] == 0xC2 and line.count(0xFE) == 1 and set(line) <= {0xB3, 0xFE, 0xC3}, line
        # A drag of the line moves the split.
        mouse(5, 49, True)
        mouse(5, 39, True)
        mouse(5, 39, False)
        assert chars(1)[39] == 0xC2 and chars(2)[39] in (0xB3, 0xFE), (chars(1), chars(2))
        mouse(5, 39, True)
        mouse(5, 49, True)
        mouse(5, 49, False)
        assert chars(1)[49] == 0xC2, chars(1)
        assert locals_() == ["N% = 3", "T% = 30"], pane()  # the parameter first

        def typed(text: str) -> None:  # keys() has no shifted symbols
            raw = {"(": 0x0A, ")": 0x0B, "+": 0x0D, "*": 0x09, "/": 0x35, "%": 0x06}
            for c in text:
                if c in raw:
                    s.send({"cmd": "key", "scancode": raw[c], "ascii": ord(c), "stop": True})
                    s.run_to_key(3000)
                    s.resume(60)
                else:
                    keys(c)

        def watches() -> list[str]:
            rows = pane()
            end = next(i for i, r in enumerate(rows) if "Locals" in r)
            return [r[2:-1].strip() for r in rows[1:end] if r[2:-1].strip()]

        # The cursor on a variable, or the mouse over one: its value under it; a SHARED module
        # one's too. The stop put the cursor on r&.
        assert b" R& = 1 " in chars(6), chars(6)
        press("end", "left")
        assert b" N% = 3 " in chars(6), chars(6)
        mouse(3, 1, False)
        assert b" T% = 30 " in chars(4), chars(4)
        mouse(6, 8, False)
        assert b' S$ = "hi" ' in chars(7), chars(7)
        mouse(0, 70, False)
        assert b"T% = 30" not in chars(4), chars(4)
        # Watch: Ins asks for an expression, evaluated in the stopped call's scope; an error is its
        # message, and the run goes on as it was; Del drops the selected one.
        press("0x40", "0x52")
        typed("n% * 2\n")
        press("0x52")
        typed("1 / 0\n")
        assert watches() == ["n% * 2 = 6", "1 / 0 = Division by zero"], pane()
        press("0x53")
        assert watches() == ["n% * 2 = 6"], pane()
        press("esc")
        assert frames() == [["\x10Fact", "line", "4"], ["Fact", "line", "3"], ["module", "line", "6"]], pane()
        assert lit()[0] == (3, "r& = r& * n%"), lit()
        # F6 to the pane, Tab to the call stack, the caller's frame: the cursor on its call
        press("0x40", "tab", "tab", "down", "enter")
        assert look()[24].endswith("00003:001"), look()[24]
        assert [f[0] for f in frames()] == ["Fact", "\x10Fact", "module"], pane()
        assert locals_() == ["N% = 4", "T% = 40"], pane()  # the outer call's: saved by the inner
        press("down", "enter")
        assert locals_() == ["R& = 1", 'S$ = "hi"', "D# = 2.5"], pane()
        press(f"0x{F5:x}")  # the outer call's stop: two frames
        assert [f[0] for f in frames()] == ["\x10Fact", "module"], pane()
        assert watches() == ["n% * 2 = 8"], pane()
        press(f"0x{F5:x}")
        assert output()[0] == " 12", output()
        press("space")
        assert "Watch" not in "".join(pane()), pane()
        assert look()[21].strip() == "IMMEDIATE", look()  # back, its height as it was
        press(f"0x{ALT_D:x}", "c")
        # Locals: the last changed first. B, changed after the first stop, came after A.
        (work / "CHG.BAS").write_bytes(b"a = 1: b = 2: STOP: b = 3: STOP: PRINT a\r\n")
        open_file("chg.bas")
        press(f"0x{F5:x}")
        assert locals_() == ["A = 1", "B = 2"], pane()
        press(f"0x{F5:x}")
        assert locals_() == ["B = 3", "A = 1", "N% = 0"], pane()  # N%: the watch's, made by it
        press(f"0x{F5:x}")
        press("space")

    dbg = ["DECLARE SUB s ()", "PRINT 1", "FOR i = 1 TO 2: PRINT i: NEXT", "GOSUB g", "s", "PRINT 9", "END",
           "g: PRINT 7", "RETURN", "SUB s", "PRINT 5", "END SUB"]
    (work / "DBG.BAS").write_bytes("\r\n".join(dbg).encode() + b"\r\n")
    (work / "LOOP.BAS").write_bytes(b"DO\r\nx = x + 1: y = y + 1\r\nLOOP\r\n")
    (work / "STP.BAS").write_bytes(b"PRINT 1: STOP: PRINT 2\r\n")
    (work / "LNG.BAS").write_bytes(b"x = 0: " + b"x = x + 1: " * 300 + b"PRINT x\r\n")
    open_file("dbg.bas")
    # F8 steps a statement at a time, into a GOSUB and a SUB; DECLARE makes no stop.
    for want in [(1, "PRINT 1"), (2, "FOR i = 1 TO 2"), (2, "PRINT i"), (2, "NEXT"), (2, "PRINT i"), (2, "NEXT"),
                 (3, "GOSUB g"), (7, "PRINT 7"), (8, "RETURN"), (4, "s"), (1, "PRINT 5"), (2, "END SUB"),
                 (5, "PRINT 9")]:
        step(F8, want)
    if flat:
        assert look()[24].startswith(" <F5=CONTINUE> <F7=OVER> <F8=STEP> <SHIFT+F8=OUT> <F9=BKPT>"), look()[24]
    else:
        assert look()[24].startswith(" <SHIFT+F1=HELP> <F5=CONTINUE> <F9=TOGGLE BKPT> <F8=STEP>"), look()[24]
    # F10, and F7 (step over, ours), run a GOSUB, a call through. Restart stops at the first
    # statement again. Shift+F8 (step out, ours) runs on till the SUB has returned.
    for over in [F10, F7] if flat else [F10]:
        press(f"0x{ALT_R:x}", "r")
        assert lit()[0] == (1, "PRINT 1"), lit()
        for want in [(2, "FOR i = 1 TO 2"), (2, "PRINT i"), (2, "NEXT"), (2, "PRINT i"), (2, "NEXT"),
                     (3, "GOSUB g"), (4, "s"), (5, "PRINT 9")]:
            step(over, want)
    if flat:
        press(f"0x{ALT_R:x}", "r")
        for _ in range(7):
            press(f"0x{F10:x}")
        step(F8, (1, "PRINT 5"))
        step(0x5B, (5, "PRINT 9"))
    # A breakpoint is its line's colour; F5 stops there. Clear All takes it away.
    press("0x55")  # the SUB's view
    cursor(1)
    press(f"0x{F9:x}")
    assert lit()[1] == [1], lit()
    press("0x5f", f"0x{ALT_R:x}", "r", f"0x{F5:x}")
    assert lit() == ((1, "PRINT 5"), [1]), lit()
    press(f"0x{ALT_D:x}", "c")
    assert lit()[1] == [], lit()
    # F7 (Ctrl+F7 in ours): on to the cursor's line. Set Next Statement: CONT goes on there.
    press("0x5f")
    cursor(6)
    step(0x64 if flat else F7, (6, "END"))
    press(f"0x{ALT_R:x}", "r")
    cursor(5)
    press(f"0x{ALT_D:x}", "n")
    assert lit()[0] == (5, "PRINT 9"), lit()
    press(f"0x{F5:x}")
    assert output()[0] == " 9", output()
    press("space")
    # Trace On lights each statement as it runs; the run ends as without it. Labels once failed
    # in the editor (UNDEF'D STATEMENT): the text was stored as numbered lines, not loaded.
    press(f"0x{ALT_D:x}", "t")
    press(f"0x{F5:x}")
    seen = set()
    for _ in range(40):
        s.resume(100)
        if lit()[0]:
            seen.add(lit()[0])
        if output()[24] == "PRESS ANY KEY TO CONTINUE":
            break
    assert {(2, "PRINT i"), (7, "PRINT 7"), (1, "PRINT 5")} <= seen, seen
    assert output()[:7] == [" 1", " 1", " 2", " 7", " 5", " 9", ""], output()
    press("space")
    press(f"0x{ALT_D:x}", "t")
    # STOP, then Continue: CONT was typed to the REPL, which QBasic's keywords lack: a syntax error.
    open_file("stp.bas")
    press(f"0x{F5:x}")
    assert lit()[0] == (0, "PRINT 2"), (lit(), look())
    press(f"0x{F5:x}")
    assert output()[:2] == [" 1", " 2"], output()
    press("space")
    # A line was cut at 255 characters on load and at 250 on Run: it printed nothing.
    open_file("lng.bas")
    assert run_out()[0] == " 300", output()
    press("space")
    # A graphics program stepped, then traced: the editor shown in text mode at each stop, the
    # program's screen kept. A trace drew the editor on SCREEN 13's page: the PSETs were lost.
    (work / "GFX.BAS").write_bytes(b"SCREEN 13\r\nPSET (1, 1), 4\r\nPSET (2, 1), 5\r\nEND\r\n")
    open_file("gfx.bas")
    for want in [(0, "SCREEN 13"), (1, "PSET (1, 1), 4"), (2, "PSET (2, 1), 5")]:
        step(F8, want)
        assert peek(0x449) == 3, peek(0x449)
    press(f"0x{F4:x}")
    assert peek(0x449) == 0x13 and peek(0xA0000 + 321) == 4, (peek(0x449), peek(0xA0000 + 321))
    press("space")
    press(f"0x{ALT_D:x}", "t", f"0x{ALT_R:x}", "r", f"0x{F5:x}")
    press("space")
    press(f"0x{F4:x}")
    assert peek(0x449) == 0x13 and [peek(0xA0000 + 320 + x) for x in (1, 2)] == [4, 5], look()
    press("space", f"0x{ALT_D:x}", "t")
    # READ past indented lines (GORILLAS.BAS): the editor keeps a line's indent after its 0, and the
    # scan took it for the next line's record. Out of DATA, or 0.
    (work / "DAT.BAS").write_bytes(b"RESTORE L2\r\nREAD a, b\r\nPRINT a; b\r\nEND\r\nL1:\r\n  DATA 1\r\n"
                                   b"L2:\r\n  'c\r\n  DATA 3\r\n  DATA 4\r\n")
    open_file("dat.bas")
    press(f"0x{F5:x}")
    assert output()[0] == " 3  4", output()
    press("space")
    # LOCATE and COLOR on the output's page (NIBBLES.BAS): they went to the runtime's console on the
    # editor's page, PRINT to the output's; the text ran on line after line.
    (work / "LOC.BAS").write_bytes(b'CLS\r\nLOCATE 5, 20: PRINT "A";\r\nCOLOR 14, 1: LOCATE 2, 10: PRINT "B"\r\n')
    open_file("loc.bas")
    press(f"0x{F5:x}")
    out = output()
    attr = bytes.fromhex(s.send({"cmd": "mem_read_linear", "addr": 0xB9000 + 160 + 18, "len": 2})["data"])[1]
    assert out[4][19:] == "A" and out[1][9:] == "B" and attr == 0x1E, (out[:6], hex(attr))
    press("space")
    # Targets past a statement's JMP (a CASE, ELSE, a one-line DEF FN) ignored the debugger's DBG
    # before it: they landed in the JMP. Every CASE but the first was skipped (GORILLAS.BAS drew one
    # gorilla of three); a call of FnR from a SUB ran the caller's line again for ever.
    (work / "CS.BAS").write_bytes(b'FOR a = 1 TO 3\r\nSELECT CASE a\r\nCASE 1\r\nPRINT "one";\r\nCASE 2\r\n'
                                  b'PRINT "two";\r\nCASE ELSE\r\nPRINT "else";\r\nEND SELECT\r\nNEXT\r\n')
    open_file("cs.bas")
    assert run_out()[0] == "ONETWOELSE", output()
    press("space")
    (work / "FN.BAS").write_bytes(b"DECLARE SUB s ()\r\n  DEF FnR (x) = x + 1\r\nPRINT \"a\";\r\ns\r\nEND\r\n"
                                  b"SUB s\r\nPRINT FnR(1)\r\nEND SUB\r\n")
    open_file("fn.bas")
    assert run_out()[0] == "A 2", output()
    press("space")
    if flat:  # past 64 KB of text: the text's buffer was 64 KB, the file cut short
        pad = b"x = x + 1 ' " + b"pad " * 12 + b"\r\n"
        (work / "BIG.BAS").write_bytes(b"x = 0\r\n" + pad * 2000 + b"PRINT x\r\n")
        open_file("big.bas")
        assert run_out()[0] == " 2000", output()
        press("space")
        # Past 65,535 lines: the line counters were words, the last lines out of reach.
        (work / "LONG.BAS").write_bytes(b"x = 0\r\n" + b"x = x + 1\r\n" * 66000 + b"PRINT x\r\n")
        press(f"0x{ALT_F:x}", "o")
        keys("long.bas")
        s.send({"cmd": "key", "scancode": 0x1C, "ascii": 13, "stop": False})  # a load of 30 emulated s
        for _ in range(120):
            s.resume(1000)
            if "LONG.BAS" in look()[1]:
                break
        press("0x75")  # Ctrl+End
        assert look()[24].endswith("66002:008"), look()[24]
        s.send({"cmd": "key", "scancode": F5, "ascii": 0, "stop": False})
        for _ in range(60):
            s.resume(1000)
            if output()[24]:
                break
        assert output()[0] == " 66000", output()
        press("space")
    # Ctrl+Break stops at a statement's start, lit; the Ctrl+C is no key for the editor (a page down).
    open_file("loop.bas")
    s.send({"cmd": "key", "scancode": F5, "ascii": 0, "stop": False})  # a loop reads no key
    s.resume(300)
    s.send({"cmd": "key", "scancode": 0x2E, "ascii": 3, "stop": False})
    for _ in range(40):  # the stop's drawing takes 0.5 s or more
        s.resume(100)
        if lit()[0]:
            break
    assert lit()[0] in [(0, "DO"), (1, "x = x + 1"), (1, "y = y + 1"), (2, "LOOP")], (lit(), look())

    def box() -> list[str]:
        """A box's title, then its lines past the frame's left edge, blank ones out; capitals."""
        s.resume(100)
        rows = [chars(r) for r in range(25)]
        top = max(i for i, r in enumerate(rows) if b"\xda" in r and b"\xbf" in r)  # over the window's
        left, right = rows[top].index(b"\xda"), rows[top].index(b"\xbf")
        title = rows[top][left + 1:right].strip(b"\xc4 ").decode("latin-1").upper()
        lines = []
        for r in rows[top + 1:]:
            if r[left] != 0xB3:
                break
            line = r[left + 1:right].rstrip().decode("latin-1").upper()
            if line.strip():
                lines.append(line)
        return [title] + lines

    # A compile error's box said its class alone; now the class its title, what is wrong, then what to do and the
    # statement's form on lines of their own.
    (work / "DX.BAS").write_bytes(b"x = 1\r\n")
    open_file("dx.bas")
    press("end", "enter")
    keys("print a(1\n")
    b = box()
    assert b[0] == "SYNTAX ERROR", b
    assert b[1].startswith("  THE PRINT STATEMENT IS INCOMPLETE"), b
    assert "  PRINT IS WRITTEN:" in b and any(r.startswith("    PRINT [#FILE,]") for r in b), b
    press("esc", "end", "enter")
    keys("for i = 1 3\n")
    b = " ".join(r.strip() for r in box())
    assert "EXPECTED `TO` IN THE FOR STATEMENT, BUT FOUND `3`." in b, b
    press("esc")
    # A block left open, a NEXT of another loop's variable: a Syntax error, or NEXT without FOR, in no words.
    (work / "BLK.BAS").write_bytes(b"FOR i = 1 TO 3\r\nPRINT i\r\nNEXT j\r\n")
    open_file("blk.bas")
    press(f"0x{F5:x}")
    b = box()
    assert b[0] == "NEXT WITHOUT FOR", b
    assert "WRITE `NEXT I`, OR JUST NEXT." in " ".join(r.strip() for r in b), b
    press("esc")
    (work / "BLK2.BAS").write_bytes(b"PRINT 1\r\nFOR i = 1 TO 3\r\nPRINT i\r\n")
    open_file("blk2.bas")
    press(f"0x{F5:x}")
    b = box()
    assert b[0] == "FOR WITHOUT NEXT", b
    assert "ADD `NEXT I` AFTER THE LAST LINE" in " ".join(r.strip() for r in b), b
    press("esc")

    # Names offered as first written, not in the small letters typed: they came all small, totsum.
    (work / "CASE.BAS").write_bytes(b"TYPE pair\r\nQty AS INTEGER\r\nEND TYPE\r\nDIM pt(1) AS pair\r\nTotSum = 1\r\n")
    open_file("case.bas")
    press("0x75", "0x4f", "enter")  # Ctrl+End, End: a new last line
    def as_is() -> str:  # look() is in capitals
        return chars(7)[1:79].decode("latin-1").rstrip()

    keys("print to")
    assert as_is() == "print totSum" and attrs(7)[9] & 0x0F == 0x08, as_is()
    press("tab")
    assert as_is() == "print TotSum", as_is()
    keys(" + pt(0).q")
    press("tab")
    assert as_is() == "print TotSum + pt(0).Qty", as_is()

    # The line buffers and the clipboard start small: a line past their first bytes typed, copied
    # and pasted whole.
    long = "rem " + "z" * 66
    before = text()
    press("end", "enter")
    keys(long + "\n")
    press("up")
    shift(True)
    press("down")
    shift(False)
    press("0x92", "up")  # Ctrl+Ins; the selection dropped
    shift(True)
    press("0x52")  # Shift+Ins: before it
    shift(False)
    assert [r for r in text() if "REM Z" in r] == [long.upper()] * 2, text()
    ctrl_y()
    press("up")
    ctrl_y()
    ctrl_y()
    assert text() == before, text()

    if flat:
        debug_pane()
        # Options, Display Size: 50 lines, the windows fitted; a run's output in them too. The exit
        # leaves DOS in 25.
        (work / "Z.BAS").write_bytes(b'PRINT "Z"\r\n')
        press(f"0x{ALT_F:x}", "o")
        if any("NOT SAVED" in r for r in look()):
            press("n")
        keys("z.bas\n")
        press(f"0x{0x18:x}", "t", "g")
        press(f"0x{0x18:x}", "z", "5")
        assert b"<Shift+F1=Help>" in chars(49), chars(49)
        # The theme's colours kept: the mode's change loaded VGA's, Gruvbox's ground came out blue.
        shot = Path(tempfile.mkdtemp()) / "rows.png"
        s.snap(shot)
        px = Image.open(shot).convert("RGB").getpixel((364, 168))
        assert all(abs(a - b) <= 8 for a, b in zip(px, (0x32, 0x30, 0x2F))), px
        press(f"0x{0x18:x}", "t", "q")
        assert any(b"Immediate" in chars(r) for r in range(40, 49)), [chars(r) for r in range(40, 49)]
        # A window's title row drags: the immediate window's up a row-count, the code's shorter.
        top = next(r for r in range(40, 49) if b"Immediate" in chars(r))
        mouse(top, 40, True)
        mouse(top - 5, 40, True)
        mouse(top - 5, 40, False)
        assert b"Immediate" in chars(top - 5), [chars(r) for r in range(top - 6, top + 1)]
        press(f"0x{F5:x}")

        def mem(addr: int, n: int) -> bytes:
            return bytes.fromhex(s.send({"cmd": "mem_read_linear", "addr": addr, "len": n})["data"])

        page = 0xB8000 + int.from_bytes(mem(0x44C, 2), "little")  # page 1: the BIOS's page size on
        assert b"Press any key" in mem(page + 49 * 160, 160)[0::2], mem(page, 50 * 160)[0::2]
        press("space")
        assert b"<Shift+F1=Help>" in chars(49), chars(49)

    press(f"0x{ALT_F:x}", "x")
    if any("NOT SAVED" in r for r in look()):
        press("n")
    assert "C:\\>" in look(), look()
    if flat:
        rows = bytes.fromhex(s.send({"cmd": "mem_read_linear", "addr": 0x484, "len": 1})["data"])[0] + 1
        assert rows == 25, rows
    s.close()
    print("ok")


if __name__ == "__main__":
    play()
