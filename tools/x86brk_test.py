#!/usr/bin/env python3
"""x86brk_test.py: Ctrl+C breaks a running program. DOS calls INT 23h only while it reads the console, so
a loop that never did ran on, and INPUT, read by DOS's line input, restarted on ^C."""

import sys
import tempfile
from pathlib import Path

LLRM = Path("/Users/alim/work/personal/llrm-qbmac/tools")
sys.path[:0] = [str(LLRM), str(LLRM / "dosbatch")]
import llrmbin  # noqa: E402

llrmbin.stale_binaries = lambda *a, **k: []  # the emulator is the stamped one; the llrm sources are not ours
import qbplay  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
LOOPS = ["10 GOTO 10", "20 FOR I=1 TO 1E9:NEXT", "30 PRINT I;:I=I+1:GOTO 30", "40 INPUT A"]  # each its BREAK IN


def ctrl_c(s: qbplay.Session) -> None:
    """Ctrl+C put in the BIOS key buffer, as INT 9 does: a key sent by the socket would arm a key_wait that ends
    resume() while the guest runs on."""
    s.resume(300)
    tail = int.from_bytes(bytes.fromhex(s.send({"cmd": "mem_read_linear", "addr": 0x41C, "len": 2})["data"]), "little")
    s.send({"cmd": "mem_write_linear", "addr": 0x400 + tail, "data": "032E"})
    nxt = tail + 2 if tail + 2 < 0x3E else 0x1E
    s.send({"cmd": "mem_write_linear", "addr": 0x41C, "data": nxt.to_bytes(2, "little").hex()})


def session(com: str, args: str = "") -> qbplay.Session:
    work = Path(tempfile.mkdtemp())
    (work / "BASIC.COM").write_bytes((ROOT / "build/x86" / com).read_bytes())
    (work / "T.BAS").write_bytes(b"10 GOTO 10\r\n")
    s = qbplay.Session(work)
    s.launch(3000, f"BASIC.COM {args}".strip())
    s.keys_in_lockstep(["esc"], 3000)  # the first key after the launch is lost
    s.resume(100)
    return s


def plain() -> None:
    s = session("basic.com")
    for prog in LOOPS:
        for c in f"NEW\n{prog}\nRUN":
            s.keys_in_lockstep(["enter" if c == "\n" else "space" if c == " " else c.lower()], 3000)
        s.send({"cmd": "key", "scancode": 0x1C, "ascii": 13, "stop": False})  # RUN reads no key back
        ctrl_c(s)
        brk = f"BREAK IN {prog[:2]}"
        s.run_until(lambda: brk in s.screen().upper(), 3000, brk)
        s.keys_in_lockstep(["enter"], 3000)  # READY reads keys again: the break left none behind


def ide() -> None:
    s = session("basic-ide.com", "T.BAS")
    s.keys_in_lockstep(["esc"], 3000)  # the welcome box
    s.resume(100)
    s.send({"cmd": "key", "scancode": 0x3F, "ascii": 0, "stop": False})  # F5
    page = lambda: s.send({"cmd": "mem_read_linear", "addr": 0x462, "len": 1})["data"]  # noqa: E731
    s.run_until(lambda: page() == "01", 3000, "the output page")
    ctrl_c(s)
    s.run_until(lambda: "<F5=CONTINUE>" in s.screen().upper().split("\n")[24], 3000, "BREAK")


if __name__ == "__main__":
    plain()
    ide()
    print("ok")
