#!/usr/bin/env python3
"""x86kbd_test.py: a line typed on the console, not piped. A console read ends at CR alone, and
plt_getline waited for an LF that never came: READY, then nothing, whatever was typed."""

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import x86run

TYPED = "print 10/5\r"  # the BIOS buffer holds 15 keys


KEYS_ASM = """
        .model tiny
        .code
        org     100h
start:  mov     sp,offset stk
        mov     ah,4Ah                  ; keep 1K, for BASIC to load
        mov     bx,40h
        int     21h
        mov     si,offset keys
put:    lodsb
        test    al,al
        jz      run
        mov     cl,al
        mov     ch,0
        cmp     al,13
        jne     @F
        mov     ch,1Ch
@@:     mov     ah,5
        int     16h
        jmp     put
run:    mov     [pb+4],cs
        mov     [pb+8],cs
        mov     [pb+12],cs
        mov     ax,4B00h
        mov     dx,offset prog
        mov     bx,offset pb
        int     21h
        mov     ax,4C00h
        int     21h
prog    db      "BASIC.COM",0
tail    db      0,13
fcb     db      16 dup(0)
pb      dw      0,offset tail,0,offset fcb,0,offset fcb,0
keys    db      %s,0
        db      256 dup(?)
stk     label   byte
        end     start
"""


def stuffer(work: Path, text: str) -> None:
    """KEYS.COM: text in the keyboard buffer (INT 16h AH=05h), then BASIC.COM as its child."""
    (work / "keys.asm").write_text(KEYS_ASM % ",".join(str(b) for b in text.encode()))
    subprocess.run([str(x86run.LLRM / "jwasm"), "-q", "-bin", "-Fo", "KEYS.COM", "keys.asm"], cwd=work, check=True)


def main() -> None:
    work = Path(tempfile.mkdtemp())
    (work / "BASIC.COM").write_bytes(x86run.COM.read_bytes())
    stuffer(work, TYPED)
    (work / "job.conf").write_text(x86run.CONF)
    job = f":ms 2000\nmount c {work}\nc:\nKEYS.COM > OUT.TXT\n.\n"
    with open(work / "events.txt", "wb") as ev:
        subprocess.run(
            [str(x86run.LLRM / "dosbox-x"), "-nolog", "-conf", str(work / "job.conf")],
            input=job.encode(),
            pass_fds=(ev.fileno(),),
            env=dict(os.environ, SDL_VIDEODRIVER="dummy", DOSRUN_FD=str(ev.fileno())),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=120,
        )
    events = [json.loads(x) for x in (work / "events.txt").read_text().splitlines() if '"ev":"out"' in x]
    out = "".join(e["text"] for e in events)
    assert " 2 " in out, repr(out)
    print("ok")


if __name__ == "__main__":
    main()
