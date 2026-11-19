#!/usr/bin/env python3
"""mid2play.py FILE.mid [--voices N] [--bars N] [--grid N] [--transpose V=S,...] [--volume V=N,...] [--bend N]

A Standard MIDI File as a BASIC program of PLAY statements.

Each track and channel with notes is a voice, the ten with the most notes kept; a voice is one note at a time, a
chord's notes a voice each. Notes are put on a grid (sixteenths), a bar a PLAY statement, played in the
background: in the foreground each statement waited for the last, and 24 bars took 8% longer than they should. A
General MIDI program picks the voice's sound; channel 10 is the kit. A note longer than PLAY's lengths is its longest
piece, then a rest."""

from __future__ import annotations

import sys
import math
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
KICK = ("W4", "Z1,0", "H0,4,0,1", "Y0", "K2,8", "V11", "O1", "C")
SNARE = ("W3", "H0,5,0,1", "Y0", "K0", "V15", "O6", "C")
HAT = ("W3", "H0,3,0,1", "Y3,3000,0", "K0", "V15", "O6", "C")
KIT = {35: KICK, 36: KICK, 38: SNARE, 40: SNARE} | {
    key: ("W4", "Z1,0", "H0,5,0,3", "Y0", "K5,6", "V14", "O2", note)
    for key, note in ((41, "C"), (43, "E"), (45, "G"), (47, "B"))
}
LONGEST = 255  # a string's characters
STYLES = (("ML", 1.0), ("MN", 7 / 8), ("MS", 3 / 4))  # how much of a note's length sounds
KIT_SOUND = "Z1,0L16"
# a General MIDI program's family (program // 8) as a sound
SOUNDS = {
    0: "W0Q15H2,5,2,1",
    1: "W4Z7,6,7,0H0,8,0,6",
    2: "W4Z1,4,3,13,4H0,4,11,1U9",
    3: "W4Z3,9,3,1,3H0,6,3,3",
    4: "W4Z1,6,3,3,2H0,5,5,1",
    5: "W2Y1,2400,4H3,5,11,4U4I6",
    6: "W2Y1,2000,3H4,6,11,5U3I8",
    7: "W4Z1,8,3,11,4H0,4,12,1U11I2",
    8: "W4Z1,9,3,15,3H5,4,10,1U5I4",
    9: "W1H1,3,12,3U4",
    10: "W0Q20H3,4,9,1U5I4",
}
DEFAULT_SOUND = "W0Q50H0,5,9,3"


class Note(NamedTuple):
    start: int  # ticks
    length: int
    pitch: int
    velocity: int
    bend: int = 0  # the wheel at its start, 8192 a side
    back: int = 0  # ticks until the wheel is back


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
        sounding: dict[tuple[int, int], list[int]] = {}  # start, velocity, bend, back
        wheel: dict[int, int] = {}
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
            if event == 0xE0:
                wheel[channel] = (pitch | velocity << 7) - 8192
                for (of, _), note in sounding.items():
                    if of == channel and note[0] == tick:  # the wheel set as the note starts
                        note[2] = wheel[channel]
                    elif of == channel and note[2] < 0 and not note[3] and wheel[channel] >= 0:
                        note[3] = tick - note[0]
            elif event == 0x90 and velocity:
                sounding[channel, pitch] = [tick, velocity, wheel.get(channel, 0), 0]
            elif event in (0x80, 0x90) and (channel, pitch) in sounding:
                start, loud, bend, back = sounding.pop((channel, pitch))
                voices[track, channel].append(Note(start, tick - start, pitch, loud, bend, back or tick - start))
    return Song(division, tempo, voices, programs)


GLIDES = (0, 10, 20, 30, 40, 60, 80, 100, 120, 150, 200, 250, 300, 400, 500, 800)  # K's times, ms


def glide(note: Note, tick_ms: float, wheel: int) -> str:
    """A note's slide as K's command: one bent under at its start slides up into its pitch as the wheel comes back,
    `wheel` semitones at the wheel's end."""
    under = min(24, round(-note.bend * wheel / 8192))
    if under < 1:
        return "K0,0,0"
    time = min(range(1, 16), key=lambda k: abs(GLIDES[k] - note.back * tick_ms))
    return f"K{time},0,{under}"


def line(
    notes: list[Note], unit: int, kit: bool, full: int = 15, tick_ms: float = 0, wheel: int = 2
) -> dict[int, tuple[int, float, int, str]]:
    """A voice on the grid: at each place a note starts, its pitch, its length in places, its volume (`full` at
    velocity 127 and as the velocity squared under it) and its slide; the highest of those struck together (the kit:
    the lowest, the drum over the hat), cut short where the next starts."""
    places: dict[int, int] = {}
    lengths: dict[int, float] = {}
    volumes: dict[int, int] = {}
    glides: dict[int, str] = {}
    for note in notes:
        place = (note.start + unit // 2) // unit
        better = min if kit else max
        if place not in places or better(places[place], note.pitch) == note.pitch:
            places[place], lengths[place] = note.pitch, note.length / unit
            volumes[place] = max(1, min(15, round(full * (note.velocity / 127) ** 2)))
            glides[place] = glide(note, tick_ms, wheel)
    order = sorted(places)
    return {
        place: (
            places[place],
            min(lengths[place], (order[k + 1] - place) if k + 1 < len(order) else lengths[place]),
            volumes[place],
            glides[place],
        )
        for k, place in enumerate(order)
    }


def rest(places: int) -> str:
    out = ""
    while places:
        piece = max(p for p in PIECES if p <= places)
        out, places = out + "P" + PIECES[piece], places - piece
    return out


def sounded(voice: dict[int, tuple[int, float, int, str]], at: int) -> list[tuple[int, str]]:
    """The lengths and styles that sound nearest a note's own length, in the room before the voice's next: one, or
    where none is near (nine sixteenths) one sounding all its length tied to another (ML holds a pitch run into
    itself). Not all of a length that ends where its own pitch is struck again, the two run into one."""
    pitch, length = voice[at][:2]
    room = min((place for place in voice if place > at), default=at + max(PIECES)) - at
    again = voice.get(at + room, (None,))[0] == pitch

    def one(length: float, room: int) -> tuple[float, int, str]:
        return min(
            (abs(piece * part - length), piece, style)
            for piece in sorted(PIECES)
            if piece <= room
            for style, part in STYLES
            if not (again and piece == room and style == "ML")
        )

    alone = one(length, room)
    held = max((piece for piece in PIECES if piece < room and piece <= length - 3 / 4), default=0)
    if alone[0] > 0.3 and held and not again:
        tail = one(length - held, room - held)
        if tail[0] < alone[0] - 0.2:
            return [(held, "ML"), tail[1:]]
    return [alone[1:]]


def bar_of(
    voice: dict[int, tuple[int, float, int, str]],
    first: int,
    places: int,
    drum: str | None,
    set_to: dict[str, int | str],
    up: int = 0,
) -> str:
    """A bar of a voice as PLAY's commands, its volume where it changes from what the voice is `set_to`; a drum's is
    its one note, at its own volume. A note runs over the bar's end if it is longer, and the next bar starts where
    it ends (`set_to`'s "at")."""
    out, at = "", max(first, int(set_to.get("at", first)))
    while at < first + places:
        if at not in voice:
            gap = next((p for p in range(at, first + places) if p in voice), first + places) - at
            out, at = out + rest(gap), at + gap
            continue
        pitch, _, volume, slide = voice[at]
        if drum is not None:
            out, at = out + drum, at + 1
        else:
            note = pitch - 23 + up
            while note < 1:
                note += 12
            while note > 84:
                note -= 12
            if set_to.get("V") != volume:
                out, set_to["V"] = out + f"V{volume}", volume
            if set_to.get("K", "K0,0,0") != slide:
                out, set_to["K"] = out + slide, slide
            for piece, style in sounded(voice, at):
                dot = "." if PIECES[piece].endswith(".") else ""
                if set_to.get("M", "MN") != style:
                    out, set_to["M"] = out + style, style
                out, at = out + f"L{PIECES[piece].rstrip('.')}N{note}{dot}", at + piece
    set_to["at"] = at
    return out


def sound(family: int) -> str:
    """A program family's sound."""
    return SOUNDS.get(family, DEFAULT_SOUND)


class Part(NamedTuple):
    name: str
    notes: list[Note]
    sound: str
    drum: str | None  # a drum's note


def parts(song: Song, unit: int | None = None) -> list[Part]:
    """A voice a track, one more for each note under the highest of a chord (those struck at one place of `unit`
    ticks), and one a drum of the kit: the kit in one voice lost the hat under every kick and snare, 784 of a song's
    1600."""
    unit = unit or song.division // 4
    out: list[Part] = []
    for key in sorted(song.voices):
        name = f"track {key[0]} channel {key[1] + 1}"
        if key[1] != KIT_CHANNEL:
            struck: dict[int, list[Note]] = defaultdict(list)
            for note in song.voices[key]:
                struck[(note.start + unit // 2) // unit].append(note)
            chords = [sorted(chord, key=lambda note: -note.pitch) for _, chord in sorted(struck.items())]
            for under in range(max(map(len, chords), default=0)):
                notes = [chord[under] for chord in chords if under < len(chord)]
                label = name + (f" note {under + 1}" if under else "")
                out.append(Part(label, notes, sound(song.programs.get(key, -8) // 8), None))
            continue
        drums: dict[tuple[str, ...], list[Note]] = defaultdict(list)
        for note in song.voices[key]:
            drums[KIT.get(note.pitch, HAT)].append(note)
        for drum, notes in drums.items():
            out.append(Part(f"{name} drum {notes[0].pitch}", notes, KIT_SOUND + "".join(drum[:-1]), drum[-1]))
    return out


def program(
    song: Song,
    voices: int,
    bars: int | None,
    grid: int,
    up: dict[int, int] | None = None,
    loud: dict[int, int] | None = None,
    wheel: int = 2,
) -> str:
    unit = song.division * 4 // grid
    every = parts(song, unit)
    tick_ms = 60_000 / song.tempo / song.division
    kept = sorted(sorted(range(len(every)), key=lambda k: -len(every[k].notes))[:voices])
    keys = [every[k] for k in kept]
    lines = [
        line(part.notes, unit, part.drum is not None, (loud or {}).get(k + 1, 15), tick_ms, wheel)
        for k, part in enumerate(keys)
    ]
    set_to: list[dict[str, int | str]] = [{} for _ in keys]
    last = max(math.ceil(place + length) for voice in lines for place, (_, length, _, _) in voice.items())
    count = min(bars or sys.maxsize, -(-last // grid))
    tempo = max(32, min(255, round(song.tempo)))
    out = ["' " + ", ".join(f"voice {k + 1}: {part.name}" for k, part in enumerate(keys))]
    out.append("PLAY " + ", ".join(f'"{"MB" if k == 0 else ""}T{tempo}{part.sound}"' for k, part in enumerate(keys)))
    for bar in range(count):
        strings = [
            bar_of(lines[k], bar * grid, grid, part.drum, set_to[k], (up or {}).get(k + 1, 0))
            for k, part in enumerate(keys)
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
    parser.add_argument("--bend", type=int, default=2, help="semitones at the pitch wheel's end")
    parser.add_argument("--transpose", default="", help="voices moved by semitones: 4=-12,5=-12")
    parser.add_argument(
        "--volume", default="", help="voices' volumes at velocity 127, 15 unless given, over it for a quiet file: 1=30"
    )
    args = parser.parse_args()
    up, loud = (
        {int(voice): int(by) for voice, by in (pair.split("=") for pair in text.split(",") if pair)}
        for text in (args.transpose, args.volume)
    )
    sys.stdout.write(program(read(args.file.read_bytes()), args.voices, args.bars, args.grid, up, loud, args.bend))


if __name__ == "__main__":
    main()
