# Making music and sound effects with PLAY

This guide starts from zero. It explains notes and sound first, then every `PLAY` command, then
builds a three-voice piece and a set of game sound effects. Every example runs: paste it into the
IDE and press Shift+F5. [sound.md](sound.md) is the short reference.

## 1. Sound in five ideas

**Pitch.** A sound is air shaking. The faster it shakes, the higher it sounds. The speed is the
frequency, in hertz (Hz): shakes a second. The A that orchestras tune to is 440 Hz.

**Notes.** Music names pitches with seven letters, `C D E F G A B`, then starts again at the next
`C`. Between most neighbours sits one more pitch: a sharp (`C#`, a little above `C`) or a flat
(`D-`, a little below `D`; the same pitch as `C#`). That makes twelve steps, called semitones.

**Octaves.** The next `C` up is twice the frequency of the last. The twelve steps from one `C` to
the next are an octave. `PLAY` has octaves 0 to 6; middle C is the `C` of octave 3, 262 Hz.

**Length and tempo.** A whole note is the longest usual note. A half note lasts half as long, a
quarter note a quarter, down to a 64th. The tempo says how many quarter notes play in a minute. A
dot after a note makes it half as long again. A rest is a note of silence.

**Melody and chords.** A melody is one note after another. A chord is several notes at once, such as
`C E G` (C major). An arpeggio plays a chord's notes one after another, fast.

## 2. What the synthesizer adds

QBasic had one sound: the PC speaker's square wave. QuickrBasic adds three voices after the
Commodore 64's SID chip. Each voice has its own wave, envelope and filter.

**Waves.** The shape of the shaking sets the colour of the sound.

- *Pulse* (`W0`): a square that is high for part of each cycle. At 50% it is the speaker's hollow
  beep; thinner pulses (`Q10`) sound nasal and buzzy.
- *Triangle* (`W1`): soft and round, like a flute. Good for bass.
- *Sawtooth* (`W2`): bright and brassy. Good for leads.
- *Noise* (`W3`): random hiss. Good for drums, wind and explosions.

**Envelope (ADSR).** How loud a note is over its life:

- *Attack*: how fast it rises from silence to full.
- *Decay*: how fast it then falls to the sustain level.
- *Sustain*: the level it holds while the note lasts.
- *Release*: how fast it fades after the note ends.

A piano has a fast attack and no sustain. An organ has a fast attack and full sustain. A violin
swells: a slow attack.

**Filter.** A filter takes part of the sound away. A low-pass keeps the low part: the sound gets
darker and softer. A high-pass keeps the high part: thin and crisp. A band-pass keeps the middle.
Resonance boosts the sound right at the cutoff, for a whistling, "wah" edge.

## 3. One voice: QBasic's commands

These work as in QBasic 4.5. Spaces between commands are allowed.

| Command | Does |
|---|---|
| `A` to `G` | Plays a note. Add `#` or `+` for sharp, `-` for flat. A number after it is its length, e.g. `C8`. |
| `N`n | Plays note n, 1 to 84 (seven octaves of 12); `N0` is a rest. |
| `O`n | Sets the octave, 0 to 6. The default is 4. |
| `>` `<` | Up or down one octave. |
| `L`n | Sets the length, 1 to 64: 1 a whole note, 4 a quarter. The default is 4. |
| `.` | After a note or rest: half as long again. Two dots add a quarter more. |
| `P`n | A rest of length n. |
| `T`n | Sets the tempo, 32 to 255 quarter notes a minute. The default is 120. |
| `MN` `ML` `MS` | Normal (7/8 of the note sounds), legato (all of it), staccato (3/4). |
| `MF` `MB` | Foreground (the program waits for the music) or background (it goes on). |
| `X` + `VARPTR$(a$)` | Plays the commands in string `a$`. |
| `=` + `VARPTR$(n)` | Any number above can come from a variable instead. |

A scale, up and down:

```basic
PLAY "T120 O3 L8 C D E F G A B > C"
PLAY "C < B A G F E D C"
```

Lengths, dots and rests: a quarter, an eighth, a dotted quarter, a rest and a half note.

```basic
PLAY "T100 O3 C4 D8 E4. P8 G2"
```

Sharps and flats, and notes by number (`N37` is middle C):

```basic
PLAY "O3 C C# D D# E" + "N37 N39 N41"
```

Legato and staccato:

```basic
PLAY "T140 O4 ML C D E F G2 MS C D E F G2 MN"
```

A string reused with `X`, and a tempo from a variable:

```basic
riff$ = "O3 L16 C E G E"
speed% = 180
PLAY "T=" + VARPTR$(speed%) + "X" + VARPTR$(riff$) + "X" + VARPTR$(riff$)
```

Background music lets the program go on while it plays. `PLAY(0)` counts the notes still waiting.
A rest in the foreground waits for them all:

```basic
PLAY "MB T150 O3 L8 C E G > C"
PRINT "notes waiting:"; PLAY(0)
PLAY "MF P64"
PRINT "done"
```

## 4. The synthesizer's commands

Any of these, or a second string, moves a voice from the speaker to the synthesizer.

| Command | Sets | Range | Default |
|---|---|---|---|
| `V`n | volume | 0 to 15 | 8 |
| `W`n | wave: 0 pulse, 1 triangle, 2 sawtooth, 3 noise | 0 to 3 | 0 |
| `Q`n | pulse width, percent of the cycle that is high | 1 to 99 | 50 |
| `H`a`,`d`,`s`,`r | attack, decay, sustain level, release | 0 to 15 each | `H0,0,15,0` |
| `Y`m`,`f`,`r | filter: m 0 off, 1 low-pass, 2 band-pass, 3 high-pass; cutoff f in Hz; resonance r | m 0 to 3, f 30 to 12000, r 0 to 15 | off, 1000, 0 |

`Y` can stop early: `Y0` turns the filter off, `Y1,500` keeps the resonance.

Envelope times use the SID's scale. Attack runs from 2 ms (0) through 100 ms (8) to 8 s (15).
Decay and release are three times as long: 6 ms (0), 300 ms (8), 24 s (15). Sustain 15 is full,
0 silent.

`PLAY a$, b$, c$` plays two or three strings at once, a voice each. Each voice keeps its own octave,
length, tempo and sound, so set them in each string. `MF` and `MB` apply to all three. `PLAY(n)`
counts the first voice's notes.

The four waves, one after another:

```basic
PLAY "T120 O3 L4 W0 C E G P4 W1 C E G P4 W2 C E G P4 W3 C E G"
```

Pulse width, from hollow to thin:

```basic
PLAY "T120 O3 L8 W0 Q50 C C Q25 C C Q10 C C Q5 C C"
```

Envelopes: an organ, a pluck, a swell and a bell.

```basic
PLAY "T90 O3 L2 W2 H0,0,15,0 C H0,6,0,4 C H10,0,15,8 C W1 H0,9,0,9 > C"
```

A low-pass filter closing on a saw, then resonance making it whistle:

```basic
PLAY "T120 O2 L4 W2 Y1,3000 C Y1,1200 C Y1,500 C Y1,200 C"
PLAY "Y1,800,0 C Y1,800,8 C Y1,800,15 C Y0 C"
```

Volume:

```basic
PLAY "T120 O3 L8 W2 V15 C V11 C V7 C V3 C"
```

## 5. Building a piece, step by step

The piece uses four chords, C, A minor, F and G, a bar each. Every string sets `T130` and the
lengths add up to the same four bars, so the voices stay together.

**A melody.** A sawtooth with a short pluck, the chord's notes in eighths:

```basic
melody$ = "T130 O4 L8 W2 H0,5,9,3 V9 E G > C < G E G E C A > C E C < A > C < A E F A > C < A F A F C G B > D < B G B > D4"
PLAY melody$
```

**A bass line.** A triangle, the chord's root in half notes, two octaves down:

```basic
bass$ = "T130 O2 L2 W1 H0,8,10,4 V11 C C O1 A A O2 F F G G"
PLAY bass$, "", ""
```

**Together.** Two strings, two voices:

```basic
melody$ = "T130 O4 L8 W2 H0,5,9,3 V9 E G > C < G E G E C A > C E C < A > C < A E F A > C < A F A F C G B > D < B G B > D4"
bass$ = "T130 O2 L2 W1 H0,8,10,4 V11 C C O1 A A O2 F F G G"
PLAY melody$, bass$
```

**Chords.** Three voices, a note of the chord each, held for a bar:

```basic
PLAY "T130 L1 W0 Q30 H4,0,12,6 V7 O3 E E F G", "T130 L1 W0 Q30 H4,0,12,6 V7 O3 G A A B", "T130 L1 W0 Q30 H4,0,12,6 V7 O4 C C C D"
```

**An arpeggio.** One voice runs up and down each chord in sixteenths, the SID's classic way to
sound a chord with a single voice:

```basic
up$ = "T130 L16 W0 Q20 H0,2,8,1 V6 O4 "
c$ = "C E G > C < G E C E G > C < G E C E G E "
am$ = "< A > C E A E C < A > C E A E C < A > C E C "
f$ = "< F A > C F C < A F A > C F C < A F A > C < A > "
g$ = "< G B > D G D < B G B > D G D < B G B > D < B > "
PLAY up$ + c$ + am$ + f$ + g$
```

**Drums from noise.** Noise with a quick decay is a drum. Low and filtered is a kick, middle is a
snare, high and thin is a hi-hat. Each hit sets its own envelope and filter:

```basic
kick$ = "H0,4,0,3 Y1,300,4 O1 C"
snare$ = "H0,4,0,4 Y0 O4 C"
hat$ = "H0,1,0,1 Y3,6000 O6 C"
bar$ = kick$ + hat$ + snare$ + hat$ + kick$ + hat$ + snare$ + hat$
PLAY "T130 L8 W3 V8 " + bar$ + bar$, "", ""
```

**The whole piece.** Melody, bass and drums on the three voices, played twice:

```basic
melody$ = "T130 O4 L8 W2 H0,5,9,3 V9 E G > C < G E G E C A > C E C < A > C < A E F A > C < A F A F C G B > D < B G B > D4"
bass$ = "T130 O2 L2 W1 H0,8,10,4 V11 C C O1 A A O2 F F G G"
kick$ = "H0,4,0,3 Y1,300,4 O1 C"
snare$ = "H0,4,0,4 Y0 O4 C"
hat$ = "H0,1,0,1 Y3,6000 O6 C"
bar$ = kick$ + hat$ + snare$ + hat$ + kick$ + hat$ + snare$ + hat$
drums$ = "T130 L8 W3 V7 " + bar$ + bar$ + bar$ + bar$
FOR i = 1 TO 2
    PLAY melody$, bass$, drums$
NEXT
```

## 6. Sound effects

A game effect is a short burst: a few fast notes, a sharp envelope, often a pitch that slides. The
music's clock ticks 18.2 times a second, so a note lasts at least a tick, about 55 ms. At `T255` an
`L16` note is one tick; shorter notes may be skipped. A slide is a run of such notes.

**Jump:** a thin pulse sliding up.

```basic
PLAY "T255 L16 W0 Q25 H0,3,0,1 V10 O3 C E G > C E"
```

**Coin:** two bright notes, the second ringing out.

```basic
PLAY "T255 W0 Q50 H0,6,0,6 V10 O5 L16 B > E4"
```

**Laser:** a sawtooth falling fast.

```basic
PLAY "T255 L16 W2 H0,3,0,1 V10 N76 N70 N64 N58 N52 N46"
```

**Explosion:** low noise through a low-pass, a long fade.

```basic
PLAY "T255 W3 H0,10,0,10 V15 Y1,900,6 O1 C4 P4"
```

**Hit:** a short burst of noise.

```basic
PLAY "T255 W3 H0,3,0,2 V12 O4 C16 P8"
```

**Power-up:** a triangle arpeggio climbing two octaves.

```basic
PLAY "T255 L16 W1 H0,2,12,2 V11 O3 C E G > C E G > C E G"
```

**Game over:** slow, falling, and sad.

```basic
PLAY "T90 W0 Q30 H2,6,8,8 V10 O3 L8 G F# F E2"
```

**Effects over music.** Give the music voices 1 and 2 in the background, and keep voice 3 for
effects. An empty string leaves a voice alone, so `PLAY "", "", fx$` plays an effect without
touching the music:

```basic
tune$ = "T130 O4 L8 W2 H0,5,9,3 V8 E G > C < G E G E C"
bass$ = "T130 O2 L2 W1 V10 C C"
jump$ = "T255 L16 W0 Q25 H0,3,0,1 V10 O3 C E G > C E"
coin$ = "T255 W0 Q50 H0,6,0,6 V10 O5 L16 B > E4"
PLAY "MB" + tune$, bass$
FOR i = 1 TO 3
    PLAY "", "", jump$
    PLAY "", "", coin$
NEXT
PLAY "MF P64"
```

## 7. On DOS

DOS has only the PC speaker. It plays voice 1, as a plain square wave at one volume; `V`, `W`, `Q`,
`H` and `Y` change nothing there, and voices 2 and 3 are silent. Put the melody in the first string
so a piece still makes sense on DOS. Effects meant for DOS too belong in voice 1, or use `SOUND`:

```basic
FOR f = 2000 TO 400 STEP -200
    SOUND f, 1
NEXT
```
