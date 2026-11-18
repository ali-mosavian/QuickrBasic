#!/usr/bin/env python3
"""mid2play.py FILE.mid [--voices N] [--bars N] [--grid N] [--transpose V=S,...]

A Standard MIDI File as a BASIC program of PLAY statements.

Each track and channel with notes is a voice, the ten with the most notes kept; a voice is one note at a time, the
highest of those struck together. Notes are put on a grid (sixteenths), a bar a PLAY statement, played in the
background: in the foreground each statement waited for the last, and 24 bars took 8% longer than they should. A
General MIDI program picks the voice's sound; channel 10 is the kit. A note longer than PLAY's lengths is its longest
piece, then a rest."""

from __future__ import annotations

import sys
import argparse
from typing import NamedTuple
from pathlib import Path
from collections import defaultdict

VOICES = 32
KIT_CHANNEL = 9
# a note of so many sixteenths: its PLAY length and dot
PIECES = {16: "1", 12: "2.", 8: "2", 6: "4.", 4: "4", 3: "8.", 2: "8", 1: "16"}
# a drum: what its voice is set to, then its note. A snare is all the noise, a hat what is over 4 kHz: a
# recording's drums were in every band
KICK = ("W4", "Z5,15,2,0,7", "Y0", "K4,8", "V15", "O1", "C")
SNARE = ("W3", "Y0", "K0", "R5", "V11", "O6", "C")
HAT = ("W3", "Y3,4000,0", "K0", "R5", "V4", "O6", "C")
KIT = {35: KICK, 36: KICK, 38: SNARE, 40: SNARE} | {
    key: ("W4", "Z1,0", "Y0", "K5,6", "R5", "V14", "O2", note)
    for key, note in ((41, "C"), (43, "E"), (45, "G"), (47, "B"))
}
LONGEST = 255  # a string's characters
KIT_SOUND = "Z1,0H0,5,0,3L16"
# a General MIDI program's family (program // 8) as a sound
SOUNDS = {
    0: "W4Z2,6,6,2,1H0,7,3,3I2",
    1: "W4Z7,6,7,0H0,8,0,6",
    2: "W4Z1,3,3,8,2H0,4,11,4U6I6",
    3: "W4Z3,9,3,1,3H0,6,3,3",
    4: "W4Z1,6,4,8,3H0,7,11,2I2",
    5: "W2Y1,2400,4H3,5,11,4U4I6",
    6: "W2Y1,2000,3H4,6,11,5U3I8",
    7: "W4Z1,6,3,12,5H1,4,12,5U7I6",
    8: "W0Q42H1,4,10,3U5I4",
    9: "W1H1,3,12,3U4",
    10: "W0Q30H0,4,9,3U5I4",
}
DEFAULT_SOUND = "W0Q50H0,5,9,3"
BASS = 4


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


def bar_of(voice: dict[int, tuple[int, int]], first: int, places: int, drum: str | None, up: int = 0) -> str:
    """A bar of a voice as PLAY's commands; a drum's is its one note."""
    out, at = "", first
    while at < first + places:
        if at not in voice:
            gap = next((p for p in range(at, first + places) if p in voice), first + places) - at
            out, at = out + rest(gap), at + gap
            continue
        pitch, length = voice[at]
        length = min(length, first + places - at)
        piece = max(p for p in PIECES if p <= length)
        if drum is not None:
            out += drum + rest(piece - 1)
        else:
            note = pitch - 23 + up
            while note < 1:
                note += 12
            while note > 84:
                note -= 12
            dot = "." if PIECES[piece].endswith(".") else ""
            out += f"L{PIECES[piece].rstrip('.')}N{note}{dot}"
        out, at = out + rest(length - piece), at + length
    return out


def sound(family: int) -> str:
    """A program family's sound at its volume, in a room but for the bass: PLAY's own 8 of 15 left six voices 8 dB
    under a recording."""
    return SOUNDS.get(family, DEFAULT_SOUND) + ("V15" if family == BASS else "V11R5")


class Part(NamedTuple):
    name: str
    notes: list[Note]
    sound: str
    drum: str | None  # a drum's note


def parts(song: Song) -> list[Part]:
    """A voice a track, and one a drum of the kit: the kit in one voice lost the hat under every kick and snare, 784
    of a song's 1600."""
    out: list[Part] = []
    for key in sorted(song.voices):
        name = f"track {key[0]} channel {key[1] + 1}"
        if key[1] != KIT_CHANNEL:
            out.append(Part(name, song.voices[key], sound(song.programs.get(key, -8) // 8), None))
            continue
        drums: dict[tuple[str, ...], list[Note]] = defaultdict(list)
        for note in song.voices[key]:
            drums[KIT.get(note.pitch, HAT)].append(note)
        for drum, notes in drums.items():
            out.append(Part(f"{name} drum {notes[0].pitch}", notes, KIT_SOUND + "".join(drum[:-1]), drum[-1]))
    return out


def program(song: Song, voices: int, bars: int | None, grid: int, up: dict[int, int] | None = None) -> str:
    unit = song.division * 4 // grid
    every = parts(song)
    kept = sorted(sorted(range(len(every)), key=lambda k: -len(every[k].notes))[:voices])
    keys = [every[k] for k in kept]
    lines = [line(part.notes, unit, part.drum is not None) for part in keys]
    last = max(place + length for voice in lines for place, (_, length) in voice.items())
    count = min(bars or sys.maxsize, -(-last // grid))
    tempo = max(32, min(255, round(song.tempo)))
    out = ["' " + ", ".join(f"voice {k + 1}: {part.name}" for k, part in enumerate(keys))]
    out.append("PLAY " + ", ".join(f'"{"MB" if k == 0 else ""}T{tempo}{part.sound}"' for k, part in enumerate(keys)))
    for bar in range(count):
        strings = [
            bar_of(lines[k], bar * grid, grid, part.drum, (up or {}).get(k + 1, 0)) for k, part in enumerate(keys)
        ]
        if max(map(len, strings)) > LONGEST:
            sys.exit(f"mid2play: bar {bar + 1} is over {LONGEST} characters in a voice")
        out.append("PLAY " + ", ".join(f'"{text}"' for text in strings))
    # in the background, so a bar follows the last with no gap; then the program waits the queue out
    out.append("DO: LOOP WHILE PLAY(0) > 0")
    out.append("SLEEP 1")
    return "\n".join(out) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description="A Standard MIDI File as a BASIC program of PLAY statements.")
    parser.add_argument("file", type=Path)
    parser.add_argument("--voices", type=int, default=VOICES, choices=range(1, VOICES + 1))
    parser.add_argument("--bars", type=int)
    parser.add_argument("--grid", type=int, default=16, choices=(8, 16), help="places a bar (4/4)")
    parser.add_argument("--transpose", default="", help="voices moved by semitones: 4=-12,5=-12")
    args = parser.parse_args()
    up = {int(voice): int(by) for voice, by in (pair.split("=") for pair in args.transpose.split(",") if pair)}
    sys.stdout.write(program(read(args.file.read_bytes()), args.voices, args.bars, args.grid, up))


if __name__ == "__main__":
    main()
