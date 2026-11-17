"""The PLAY tutorial's pictures: python3 tools/diagrams/sound.py SYNTHDUMP OUT_DIR

The waves, the envelope and the filter are drawn from the synthesizer's own samples (SYNTHDUMP,
tools/diagrams/synthdump.c). The piece and the effects are read from docs/play-tutorial.md, so the
pictures show what its examples play."""

import re
import sys
import math
import subprocess
from pathlib import Path

from svg import INK
from svg import Svg
from svg import LINE
from svg import MUTED
from svg import THEME

RATE = 44100
TUTORIAL = Path(__file__).parents[2] / "docs" / "play-tutorial.md"
BLUE, GREEN, ORANGE = THEME["front"][1], THEME["engine"][1], THEME["warn"][1]
GRID = "#e5e7eb"
NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
# play.c's octave 3, Hz: each octave up doubles them
OCTAVE_THREE = [262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494]
SEMITONE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def frequency(note: int) -> float:
    return OCTAVE_THREE[(note - 1) % 12] * 2.0 ** ((note - 1) // 12 - 3)


def samples(
    dump: str,
    hertz: int,
    wave: int,
    envelope: str = "00F0",
    width: int = 2048,
    filt: int = 0,
    cutoff: int = 1000,
    resonance: int = 0,
    gate_ms: int = 100,
    total_ms: int = 100,
) -> list[int]:
    args = [hertz, wave, width, envelope, filt, cutoff, resonance, 15, gate_ms, total_ms]
    out = subprocess.run([dump, *map(str, args)], capture_output=True, text=True, check=True).stdout
    return [int(x) for x in out.split()]


# ---- the tutorial's examples ----


def block(label: str) -> str:
    """The ```basic block that follows `label` in the tutorial."""
    text = TUTORIAL.read_text()
    m = re.search(re.escape(label) + r".*?```basic\n(.*?)```", text, re.S)
    assert m, label
    return m.group(1)


def evaluate(expr: str, names: dict[str, str]) -> str:
    """A string expression of literals and string variables joined by +."""
    return "".join(p[1:-1] if p.startswith('"') else names[p] for p in (x.strip() for x in expr.split("+")))


def plays(code: str) -> list[str]:
    """The strings of the block's PLAY statement, its string variables worked out."""
    names: dict[str, str] = {}
    for line in code.splitlines():
        line = line.strip()
        if m := re.fullmatch(r"(\w+\$) = (.*)", line):
            names[m.group(1)] = evaluate(m.group(2), names)
        elif line.startswith("PLAY "):
            return [evaluate(arg, names) for arg in re.split(r",(?=(?:[^\"]*\"[^\"]*\")*[^\"]*$)", line[5:])]
    raise ValueError("no PLAY")


type Note = tuple[float, float, int]  # start and length in seconds, note number (0 a rest)


def notes(commands: str) -> list[Note]:
    """A PLAY string's notes: the commands that set pitch and time; the sound's are passed over."""
    s, i, out = commands.upper(), 0, []
    octave, length, tempo, now = 4, 4, 120, 0.0

    def number() -> int | None:
        nonlocal i
        m = re.match(r" *(\d+)", s[i:])
        if not m:
            return None
        i += m.end()
        return int(m.group(1))

    def dotted(base: float) -> float:
        nonlocal i
        extra = base
        while i < len(s) and s[i] == ".":
            i += 1
            extra /= 2
            base += extra
        return base

    def add(note: int, n: int) -> None:
        nonlocal now
        t = dotted(4 * 60 / tempo / n)
        out.append((now, t, note))
        now += t

    while i < len(s):
        c, i = s[i], i + 1
        if c in SEMITONE:
            note = octave * 12 + SEMITONE[c] + 1
            if i < len(s) and s[i] in "#+-":
                note += -1 if s[i] == "-" else 1
                i += 1
            add(note, number() or length)
        elif c in "NP":
            n = number() or 0
            add(n, length) if c == "N" else add(0, n)
        elif c in "OLT":
            n = number() or 0
            octave, length, tempo = (
                (n, length, tempo) if c == "O" else (octave, n, tempo) if c == "L" else (octave, length, n)
            )
        elif c in "<>":
            octave += 1 if c == ">" else -1
        elif c == "M":
            i += 1
        elif c in "VWQHY":
            number()
            while i < len(s) and s[i] == ",":
                i += 1
                number()
    return out


# ---- the pictures ----


def keyboard(out: Path) -> None:
    s = Svg(760, 250, "One octave: octave 3", "Each octave up doubles every frequency. O3 C is middle C.")
    key_w, top, h = 100, 84, 140
    for k, name in enumerate("CDEFGAB"):
        x = 30 + k * key_w
        s.rect(x, top, key_w, h, "#ffffff", stroke=LINE, r=4)
        s.text(x + key_w / 2, top + h - 30, name, size=18, weight=700, anchor="middle")
        s.text(x + key_w / 2, top + h - 10, f"{OCTAVE_THREE[SEMITONE[name]]} Hz", size=12, fill=MUTED, anchor="middle")
    for k, name in enumerate("CDFGA"):
        x = 30 + ([1, 2, 4, 5, 6][k]) * key_w - 30
        semi = SEMITONE[name] + 1
        s.rect(x, top, 60, 82, INK, r=3)
        s.text(x + 30, top + 50, NAMES[semi], size=13, weight=700, fill="#ffffff", anchor="middle")
        s.text(x + 30, top + 68, f"{OCTAVE_THREE[semi]}", size=11, fill="#d1d5db", anchor="middle")
    s.save(out / "keyboard.svg")


def lengths(out: Path) -> None:
    rows = [
        ("C1", "whole", 4),
        ("C2", "half", 2),
        ("C4", "quarter", 1),
        ("C8", "eighth", 0.5),
        ("C16", "16th", 0.25),
        ("C4.", "dotted quarter", 1.5),
    ]
    s = Svg(760, 90 + 34 * len(rows), "Note lengths", "One bar of four beats. T120 plays 120 beats a minute.")
    x0, beat = 210, 130
    for b in range(5):
        s.line([(x0 + b * beat, 80), (x0 + b * beat, 84 + 34 * len(rows))], GRID, 1)
        if b < 4:
            s.text(x0 + b * beat + 4, 78, f"beat {b + 1}", size=11, fill=MUTED)
    for r, (cmd, name, beats) in enumerate(rows):
        y = 92 + 34 * r
        s.text(32, y + 16, cmd, size=14, weight=700, mono=True)
        s.text(90, y + 16, name, size=13, fill=MUTED)
        x = x0
        while x + beats * beat <= x0 + 4 * beat + 0.1:
            s.rect(x + 1, y, beats * beat - 2, 22, BLUE, r=4, opacity=0.85)
            x += beats * beat
    s.save(out / "lengths.svg")


def trace(s: Svg, values: list[int], x: float, y: float, w: float, h: float, color: str) -> None:
    s.line([(x, y + h / 2), (x + w, y + h / 2)], GRID, 1)
    step = w / (len(values) - 1)
    s.line([(x + i * step, y + h / 2 - v / 6000 * h / 2) for i, v in enumerate(values)], color, 1.6)


def waves(out: Path, dump: str) -> None:
    rows = [
        ("W0 Q50", "pulse 50%", 0, 2048),
        ("W0 Q25", "pulse 25%", 0, 1024),
        ("W0 Q10", "pulse 10%", 0, 410),
        ("W1", "triangle", 1, 2048),
        ("W2", "sawtooth", 2, 2048),
        ("W3", "noise", 3, 2048),
    ]
    s = Svg(760, 90 + 70 * len(rows), "Waves", "Three cycles of each at 441 Hz, as the synthesizer makes them.")
    for r, (cmd, name, wave, width) in enumerate(rows):
        y = 84 + 70 * r
        v = samples(dump, 441, wave, width=width, total_ms=40)[RATE // 50 :][:300]  # past the 2 ms attack
        s.text(32, y + 30, cmd, size=14, weight=700, mono=True)
        s.text(32, y + 48, name, size=12, fill=MUTED)
        trace(s, v, 170, y, 560, 56, BLUE)
    s.save(out / "waves.svg")


def adsr(out: Path, dump: str) -> None:
    gate_ms, total_ms = 700, 1500
    v = samples(dump, 1500, 2, envelope="8888", gate_ms=gate_ms, total_ms=total_ms)
    s = Svg(760, 330, "The envelope: H8,8,8,8", "A sawtooth's loudness over its life. The note ends at 700 ms.")
    x0, y0, w, h = 40, 100, 680, 180
    per = len(v) / w
    top = [max(abs(x) for x in v[int(i * per) : int((i + 1) * per)]) for i in range(w)]
    s.line(
        [(x0 + i, y0 + h - t / 6000 * h) for i, t in enumerate(top)] + [(x0 + w, y0 + h), (x0, y0 + h)],
        BLUE,
        1.5,
        fill=THEME["front"][3],
    )
    s.line([(x0, y0 + h), (x0 + w, y0 + h)], LINE, 1)
    # the stages' ends, as the samples show them
    peak = top.index(max(top))
    held = top[int(gate_ms / total_ms * w) - 2]
    settled = next(i for i in range(peak, w) if top[i] <= held * 1.02)
    attack_ms, decay_ms = peak / w * total_ms, settled / w * total_ms
    for start, end, label, color in [
        (0, attack_ms, "attack", GREEN),
        (attack_ms, decay_ms, "decay", ORANGE),
        (decay_ms, gate_ms, "sustain", BLUE),
        (gate_ms, total_ms, "release", MUTED),
    ]:
        a, b = x0 + start / total_ms * w, x0 + end / total_ms * w
        s.line([(b, y0 - 8), (b, y0 + h)], GRID, 1)
        s.text((a + b) / 2, y0 - 14, label, size=13, weight=700, fill=color, anchor="middle")
    for ms in range(0, total_ms + 1, 250):
        s.text(x0 + ms / total_ms * w, y0 + h + 18, f"{ms} ms", size=11, fill=MUTED, anchor="middle")
    s.save(out / "adsr.svg")


def harmonics(v: list[int], hertz: int, count: int) -> list[float]:
    """Each harmonic's level in dB, from whole cycles of `v`."""
    n = len(v) // (RATE // hertz) * (RATE // hertz)
    level = []
    for k in range(1, count + 1):
        w = 2 * math.pi * k * hertz / RATE
        re_, im = (
            sum(x * math.cos(w * i) for i, x in enumerate(v[:n])),
            sum(x * math.sin(w * i) for i, x in enumerate(v[:n])),
        )
        level.append(20 * math.log10(max(math.hypot(re_, im) * 2 / n, 1) / 6000))
    return level


def spectrum(out: Path, dump: str) -> None:
    hertz, count, cutoff = 441, 20, 1500
    plain = harmonics(samples(dump, hertz, 2, total_ms=150)[RATE // 20 :], hertz, count)
    low = harmonics(samples(dump, hertz, 2, filt=1, cutoff=cutoff, total_ms=150)[RATE // 20 :], hertz, count)
    s = Svg(
        760,
        330,
        "A low-pass filter: Y1,1500",
        "A 441 Hz sawtooth's harmonics: grey unfiltered, blue through the filter. Above the cutoff they fall away.",
    )
    x0, y0, w, h, floor = 50, 90, 680, 190, -60.0
    bar = w / count
    for k in range(count):
        for level, color, off in [(plain[k], LINE, 0), (low[k], BLUE, bar * 0.4)]:
            top = y0 + h * min(1.0, level / floor)
            s.rect(x0 + k * bar + off + 2, top, bar * 0.38, y0 + h - top, color, r=2)
    cx = x0 + (cutoff / hertz - 0.5) * bar
    s.line([(cx, y0 - 6), (cx, y0 + h)], ORANGE, 2)
    s.text(cx + 6, y0 + 8, "cutoff 1500 Hz", size=12, weight=700, fill=ORANGE)
    s.line([(x0, y0 + h), (x0 + w, y0 + h)], LINE, 1)
    for k in range(0, count, 4):
        s.text(x0 + k * bar + bar / 2, y0 + h + 18, f"{(k + 1) * hertz} Hz", size=11, fill=MUTED, anchor="middle")
    s.save(out / "filter.svg")


def roll(s: Svg, ns: list[Note], x0: float, y: float, h: float, scale: float, color: str, low: int, high: int) -> None:
    for start, length, note in ns:
        if note:
            ny = y + h - (note - low) / (high - low) * (h - 8) - 8
            s.rect(x0 + start * scale + 1, ny, max(length * scale - 2, 2), 7, color, r=2)


def voices(out: Path) -> None:
    melody, bass, drums = plays(block("**The whole piece.**"))
    lanes = [("voice 1", "melody", notes(melody), BLUE), ("voice 2", "bass", notes(bass), GREEN)]
    end = max(n[0] + n[1] for n in lanes[0][2])
    s = Svg(760, 380, "The whole piece, voice by voice", "Four bars: C, A minor, F, G. Height is pitch.")
    x0, w = 120, 610
    scale = w / end
    for b, chord in enumerate(["C", "Am", "F", "G"]):
        x = x0 + b * w / 4
        s.line([(x, 80), (x, 350)], GRID, 1)
        s.text(x + 6, 94, chord, size=13, weight=700, fill=MUTED)
    for r, (voice, name, ns, color) in enumerate(lanes):
        y = 104 + r * 90
        played = [n[2] for n in ns if n[2]]
        s.text(32, y + 40, voice, size=13, weight=700)
        s.text(32, y + 58, name, size=12, fill=MUTED)
        roll(s, ns, x0, y, 80, scale, color, min(played) - 1, max(played) + 1)
    y = 290
    s.text(32, y + 22, "voice 3", size=13, weight=700)
    s.text(32, y + 40, "drums", size=12, fill=MUTED)
    kind = {1: ("kick", INK), 4: ("snare", ORANGE), 6: ("hat", LINE)}
    for start, length, note in notes(drums):
        if note and start < end:
            name, color = kind.get((note - 1) // 12, ("", LINE))
            row = ["kick", "snare", "hat"].index(name)
            s.rect(x0 + start * scale + 1, y + 6 + row * 16, length * scale - 2, 12, color, r=2)
    for row, name in enumerate(["kick", "snare", "hat"]):
        s.text(x0 - 8, y + 16 + row * 16, name, size=10, fill=MUTED, anchor="end")
    s.save(out / "voices.svg")


def sweeps(out: Path) -> None:
    effects = ["Jump", "Laser", "Power-up", "Game over"]
    s = Svg(760, 330, "Effects as pitch over time", "Each step is a note; the clock makes each at least 55 ms.")
    cell_w, x0 = 170, 40
    for e, name in enumerate(effects):
        ns = [n for n in notes(plays(block(f"**{name}:**"))[0]) if n[2]]
        end = ns[-1][0] + ns[-1][1]
        x, y, w, h = x0 + e * cell_w, 100, cell_w - 30, 170
        hz = [math.log2(frequency(n[2])) for n in ns]
        low, high = min(hz) - 0.3, max(hz) + 0.3
        s.rect(x, y, w, h, "#f9fafb", stroke=GRID, r=6)
        pts = []
        for (start, length, _), f in zip(ns, hz, strict=True):
            ny = y + h - (f - low) / (high - low) * h
            pts += [(x + start / end * w, ny), (x + (start + length) / end * w, ny)]
        s.line(pts, BLUE, 2.5)
        s.text(x + w / 2, y - 12, name, size=14, weight=700, anchor="middle")
        s.text(x + w / 2, y + h + 18, f"{end * 1000:.0f} ms", size=11, fill=MUTED, anchor="middle")
    s.save(out / "sweeps.svg")


def main() -> None:
    dump, out = sys.argv[1], Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    keyboard(out)
    lengths(out)
    waves(out, dump)
    adsr(out, dump)
    spectrum(out, dump)
    voices(out)
    sweeps(out)


if __name__ == "__main__":
    main()
