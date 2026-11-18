#!/usr/bin/env python3
"""mid2play_test.py [EXE]: a small MIDI file made here, as PLAY statements, and the program run on the host build."""

import sys
import tempfile
import subprocess
from pathlib import Path

import mid2play

ROOT = Path(__file__).resolve().parents[1]
QUARTER = 96


def track(events: list[tuple[int, bytes]]) -> bytes:
    """A track of (delta ticks, event)."""
    body = b"".join(bytes([delta]) + event for delta, event in events) + b"\x00\xff\x2f\x00"
    return b"MTrk" + len(body).to_bytes(4, "big") + body


def song() -> bytes:
    on, off = 0x90, 0x80
    six = QUARTER // 4
    lead = track(
        [
            (0, b"\xff\x51\x03" + (60_000_000 // 240).to_bytes(3, "big")),
            (0, bytes([0xC0, 81])),
            (0, bytes([on, 60, 100])),  # middle C, a quarter
            (0, bytes([on, 64, 100])),  # with an E over it: the E is kept
            (QUARTER, bytes([off, 60, 0])),
            (0, bytes([off, 64, 0])),
            (six * 2, bytes([on, 67, 100])),  # an eighth's rest, then a G of three sixteenths
            (six * 3, bytes([off, 67, 0])),
        ]
    )
    kit = track(
        [
            (0, bytes([on | 9, 36, 100])),  # a kick, then a hat on the second beat
            (1, bytes([off | 9, 36, 0])),
            (QUARTER - 1, bytes([on | 9, 42, 100])),
            (1, bytes([off | 9, 42, 0])),
        ]
    )
    head = (6).to_bytes(4, "big") + (1).to_bytes(2, "big") + (2).to_bytes(2, "big") + QUARTER.to_bytes(2, "big")
    return b"MThd" + head + lead + kit


def main() -> None:
    exe = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build/host/basic").resolve()
    text = mid2play.program(mid2play.read(song()), 6, None, 16)
    lines = text.splitlines()
    assert lines[1].startswith('PLAY "MBT240W0Q30'), lines[1]
    # E above middle C is N41, G N44: a quarter, an eighth's rest, a dotted eighth, the bar's seven sixteenths of rest
    kit = "W4Z1,0Y0K4,8O1CP8.W3K0Y3,7000,0O6CP2P8."
    assert lines[2] == f'PLAY "L4N41P8L8N44.P4.P16", "{kit}"', lines[2]
    with tempfile.TemporaryDirectory() as tmp:
        (Path(tmp) / "T.BAS").write_text(text + 'PRINT "played"\n')
        run = subprocess.run([exe, "T.BAS"], cwd=tmp, capture_output=True, timeout=60)
    assert b"played" in run.stdout, run.stdout[-300:] + run.stderr[-300:]
    print("mid2play ok")


if __name__ == "__main__":
    main()
