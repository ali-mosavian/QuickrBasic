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
            # an eighth's rest, then a G of three sixteenths, the wheel half way down as it starts (a semitone under,
            # at two a side) and back in an eighth: an eighth of a second at this tempo
            (six * 2 - 1, bytes([0xE0, 0, 32])),
            (1, bytes([on, 67, 64])),
            (six * 2, bytes([0xE0, 0, 64])),
            (six, bytes([off, 67, 0])),
        ]
    )
    kit = track(
        [
            (0, bytes([on | 9, 36, 100])),  # a kick with a hat, then a hat on the second beat
            (0, bytes([on | 9, 42, 100])),
            (1, bytes([off | 9, 36, 0])),
            (0, bytes([off | 9, 42, 0])),
            (QUARTER - 1, bytes([on | 9, 42, 100])),
            (1, bytes([off | 9, 42, 0])),
        ]
    )
    head = (6).to_bytes(4, "big") + (1).to_bytes(2, "big") + (2).to_bytes(2, "big") + QUARTER.to_bytes(2, "big")
    return b"MThd" + head + lead + kit


def main() -> None:
    exe = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build/host/basic").resolve()
    text = mid2play.program(mid2play.read(song()), 10, None, 16)
    # a voice moved an octave: the E an octave down
    low = mid2play.program(mid2play.read(song()), 10, None, 16, {1: -12})
    assert 'PLAY "V9L4N29P8V4K8,0,1L8N32.' in low, low
    # a voice's volume as asked is its loudest note's: these, at 100 of 127, are 0.62 of it
    quiet = mid2play.program(mid2play.read(song()), 10, None, 16, None, {1: 6})
    assert 'PLAY "V4L4N41' in quiet, quiet
    lines = text.splitlines()
    assert lines[1].startswith('PLAY "MBT240R5W0Q20'), lines[1]
    # E above middle C is N41, G N44: a quarter, an eighth's rest, a dotted eighth, the bar's seven sixteenths of rest
    # a drum whose pitch slides is a sine alone: a kick's click, a modulator, slid with it, a chirp from 5 kHz down at
    # every kick, which no drum of a recording had
    for drum in mid2play.KIT.values():
        assert "K0" in drum or "Z1,0" in drum, drum
    # a voice a drum, set once: the kick, then the hat
    # with the hat under the kick, which the kit in one voice lost
    assert lines[1].endswith(
        '"T240Z1,0H0,5,0,3L16W4Z1,0H0,3,0,2Y0K1,8R3V15O1", "T240Z1,0H0,5,0,3L16W3Y3,3000,0K0R5V11O6"'
    )
    # a note is as loud as its velocity, squared: every track at one volume was not a recording's balance. The
    # volume is set where it changes, the G at 64 of 127 a quarter of full
    # a note bent under at its start slides up into its pitch: the wheel was not read, and a lead's every note sat
    # where a recording's scooped up four semitones
    assert lines[2] == 'PLAY "V9L4N41P8V4K8,0,1L8N44.P4.P16", "CP2.P8.", "CP8.CP2P8."', lines[2]
    wide = mid2play.program(mid2play.read(song()), 10, None, 16, None, None, 8)
    assert "K8,0,4L8N44." in wide, wide
    hats = mid2play.bar_of(dict.fromkeys(range(16), (42, 1, 15, "")), 0, 16, "C", {})
    assert hats == "C" * 16, hats
    with tempfile.TemporaryDirectory() as tmp:
        (Path(tmp) / "T.BAS").write_text(text + 'PRINT "played"\n')
        run = subprocess.run([exe, "T.BAS"], cwd=tmp, capture_output=True, timeout=60)
    assert b"played" in run.stdout, run.stdout[-300:] + run.stderr[-300:]
    print("mid2play ok")


if __name__ == "__main__":
    main()
