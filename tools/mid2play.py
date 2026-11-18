#!/usr/bin/env python3
"""mid2play.py FILE.mid [--voices N] [--bars N] [--grid N]: a Standard MIDI File as a BASIC program of PLAY statements.

Each track and channel with notes is a voice, the six with the most notes kept; a voice is one note at a time, the
highest of those struck together. Notes are put on a grid (sixteenths), a bar a PLAY statement. A General MIDI
program picks the voice's sound; channel 10 is the kit. A note longer than PLAY's lengths is its longest piece, then
a rest."""

from __future__ import annotations

import sys
import argparse
from typing import NamedTuple
from pathlib import Path
from collections import defaultdict

VOICES = 6
KIT_CHANNEL = 9
# a note of so many sixteenths: its PLAY length and dot
PIECES = {16: "1", 12: "2.", 8: "2", 6: "4.", 4: "4", 3: "8.", 2: "8", 1: "16"}
KIT = {
    35: "W4Z1,0Y0K4,8O1C", 36: "W4Z1,0Y0K4,8O1C",
    38: "W3K0Y2,1800,3O4C", 40: "W3K0Y2,1800,3O4C",
    41: "W4Z1,0Y0K5,6O2C", 43: "W4Z1,0Y0K5,6O2E", 45: "W4Z1,0Y0K5,6O2G", 47: "W4Z1,0Y0K5,6O2B",
}
HAT = "W3K0Y3,7000,0O6C"
# a General MIDI program's family (program // 8) as a sound
SOUNDS = {
    0: "W4Z2,6,6,2,1H0,7,3,3", 1: "W4Z7,6,7,0H0,8,0,6", 2: "W4Z2,5,3,6,2H0,5,8,2", 3: "W4Z3,9,3,1,3H0,6,3,3",
    4: "W4Z1,12,4,4,4H0,5,6,1", 5: "W2Y1,2400,4H3,5,11,4U4", 6: "W2Y1,2000,3H4,6,11,5U3", 7: "W4Z1,5,3,11,5H1,4,12,3U5",
    8: "W0Q42H1,4,10,2U4", 9: "W1H1,3,12,3U4", 10: "W0Q30H0,4,9,2U4",
}
DEFAULT_SOUND = "W0Q50H0,5,9,3"


class Note(NamedTuple):
    start: int  # ticks
    length: int
    pitch: int


class Song(NamedTuple):
    division: int  # ticks a quarter
    tempo: float  # quarters a minute
    voices: dict[tuple[int, int], list[Note]]  # by track and channel
    programs: dict[tuple[int, int], int]


def number(data: bytes, at: int) -> tuple[int, int]:
    """A variable-length number at `at`, and where it ends."""
    value = 0
    while True:
        byte = data[at]
        at += 1
        value = value << 7 | byte & 0x7F
        if not byte & 0x80:
            return value, at


def read(data: bytes) -> Song:
    if data[:4] != b"MThd":
        sys.exit("mid2play: not a Standard MIDI File")
    tracks, division = int.from_bytes(data[10:12], "big"), int.from_bytes(data[12:14], "big")
    if division & 0x8000:
        sys.exit("mid2play: SMPTE time is not read")
    voices: dict[tuple[int, int], list[Note]] = defaultdict(list)
    programs: dict[tuple[int, int], int] = {}
    tempo, at = 120.0, 14
    for track in range(tracks):
        size = int.from_bytes(data[at + 4 : at + 8], "big")
        chunk, at = data[at + 8 : at + 8 + size], at + 8 + size
        i, tick, status = 0, 0, 0
        sounding: dict[tuple[int, int], int] = {}
        while i < len(chunk):
            delta, i = number(chunk, i)
            tick += delta
            if chunk[i] & 0x80:
                status, i = chunk[i], i + 1
            if status in (0xFF, 0xF0, 0xF7):
                kind = chunk[i] if status == 0xFF else 0
                size, i = number(chunk, i + (status == 0xFF))
                if kind == 0x51 and tick == 0:
                    tempo = 60_000_000 / int.from_bytes(chunk[i : i + size], "big")
                i += size
                continue
            event, channel = status & 0xF0, status & 0x0F
            if event in (0xC0, 0xD0):
                if event == 0xC0:
                    programs[track, channel] = chunk[i]
                i += 1
                continue
            pitch, velocity, i = chunk[i], chunk[i + 1], i + 2
            if event == 0x90 and velocity:
                sounding[channel, pitch] = tick
            elif event in (0x80, 0x90) and (channel, pitch) in sounding:
                start = sounding.pop((channel, pitch))
                voices[track, channel].append(Note(start, tick - start, pitch))
    return Song(division, tempo, voices, programs)


def line(notes: list[Note], unit: int, kit: bool) -> dict[int, tuple[int, int]]:
    """A voice on the grid: at each place a note starts, its pitch and its length in places; the highest of those
    struck together (the kit: the lowest, the drum over the hat), cut short where the next starts."""
    places: dict[int, int] = {}
    lengths: dict[int, int] = {}
    for note in notes:
        place = (note.start + unit // 2) // unit
        better = min if kit else max
        if place not in places or better(places[place], note.pitch) == note.pitch:
            places[place], lengths[place] = note.pitch, max(1, round(note.length / unit))
    order = sorted(places)
    return {
        place: (places[place], min(lengths[place], (order[k + 1] - place) if k + 1 < len(order) else lengths[place]))
        for k, place in enumerate(order)
    }


def rest(places: int) -> str:
    out = ""
    while places:
        piece = max(p for p in PIECES if p <= places)
        out, places = out + "P" + PIECES[piece], places - piece
    return out


def bar_of(voice: dict[int, tuple[int, int]], first: int, places: int, kit: bool) -> str:
    """A bar of a voice as PLAY's commands."""
    out, at = "", first
    while at < first + places:
        if at not in voice:
            gap = next((p for p in range(at, first + places) if p in voice), first + places) - at
            out, at = out + rest(gap), at + gap
            continue
        pitch, length = voice[at]
        length = min(length, first + places - at)
        piece = max(p for p in PIECES if p <= length)
        if kit:
            out += KIT.get(pitch, HAT) + rest(piece - 1)
        else:
            note = pitch - 23
            while note < 1:
                note += 12
            while note > 84:
                note -= 12
            dot = "." if PIECES[piece].endswith(".") else ""
            out += f"L{PIECES[piece].rstrip('.')}N{note}{dot}"
        out, at = out + rest(length - piece), at + length
    return out


def program(song: Song, voices: int, bars: int | None, grid: int) -> str:
    unit = song.division * 4 // grid
    keys = sorted(song.voices, key=lambda key: -len(song.voices[key]))[:voices]
    keys.sort()
    lines = {key: line(song.voices[key], unit, key[1] == KIT_CHANNEL) for key in keys}
    last = max(place + length for voice in lines.values() for place, (_, length) in voice.items())
    count = min(bars or sys.maxsize, -(-last // grid))
    tempo = max(32, min(255, round(song.tempo)))
    setup = [
        "H0,3,0,1L16" if key[1] == KIT_CHANNEL else SOUNDS.get(song.programs.get(key, -8) // 8, DEFAULT_SOUND)
        for key in keys
    ]
    out = ["' " + ", ".join(f"voice {k + 1}: track {key[0]} channel {key[1] + 1}" for k, key in enumerate(keys))]
    out.append("PLAY " + ", ".join(f'"{"MF" if k == 0 else ""}T{tempo}{text}"' for k, text in enumerate(setup)))
    for bar in range(count):
        strings = (bar_of(lines[key], bar * grid, grid, key[1] == KIT_CHANNEL) for key in keys)
        out.append("PLAY " + ", ".join(f'"{text}"' for text in strings))
    return "\n".join(out) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description="A Standard MIDI File as a BASIC program of PLAY statements.")
    parser.add_argument("file", type=Path)
    parser.add_argument("--voices", type=int, default=VOICES, choices=range(1, VOICES + 1))
    parser.add_argument("--bars", type=int)
    parser.add_argument("--grid", type=int, default=16, choices=(8, 16), help="places a bar (4/4)")
    args = parser.parse_args()
    sys.stdout.write(program(read(args.file.read_bytes()), args.voices, args.bars, args.grid))


if __name__ == "__main__":
    main()
