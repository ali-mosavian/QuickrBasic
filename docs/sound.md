# Sound

The reference for `PLAY`'s synthesizer. New to music or sound effects? Start with the
[tutorial](play-tutorial.md).

A program that uses none of this sounds as it did in QBasic: one square wave, the same notes at the
same ticks.

## Voices

`PLAY a$, b$, ...` plays up to 32 voices at once, a string each, after PCjr and Tandy BASIC's three.

- Strings go to voices 1, 2, 3, ... in order. A string left out leaves its voice playing what it
  was: `PLAY , , fx$` plays voice 3 alone.
- Each voice keeps its own octave, length, tempo, style and sound from one `PLAY` to the next. Set
  the tempo in every string.
- `MF` and `MB` apply to all voices.
- A voice goes through the synthesizer when its string has any command of the table below, or when
  `PLAY` has a second string. Otherwise voice 1 is the speaker's square wave, as in QBasic.
- `RUN` puts every voice back to its defaults.

## The commands

Each sets the sound of the notes that follow it in that voice, until it is set again. Each number
may be `=var;` as in QBasic's commands. A number out of range is Illegal function call.

| Command | Sets | Range | Default |
|---|---|---|---|
| `V`n | volume | 0–15 | 8 |
| `W`n | wave: 0 pulse, 1 triangle, 2 sawtooth, 3 noise, 4 FM | 0–4 | 0 |
| `Q`n | pulse width, percent high (wave 0 only) | 1–99 | 50 |
| `H`a`,`d`,`s`,`r | envelope: attack, decay, sustain level, release; all four are needed | 0–15 each | `H0,0,15,0` |
| `Y`m[`,`f[`,`r]] | filter: 0 off, 1 low-pass, 2 band-pass, 3 high-pass; cutoff in Hz; resonance | m 0–3, f 30–12000, r 0–15 | off, 1000, 0 |
| `Z`r[`,`d[`,`f[`,`l[`,`b]]]] | FM's modulator: ratio, depth, fall, level, feedback (below) | r 0.01–15.99, d f l 0–15, b 0–7 | `Z1,8,0,15,0` |
| `U`n | vibrato depth | 0–15 | 0 |
| `K`t[`,`q[`,`u]] | glide: time, from over, from under (below) | t q 0–15, u 0–24 | `K0,0,0` |
| `S`n | stereo place: 1 the left, 8 the middle, 15 the right | 1–15 | 8 |
| `R`n | reverb: how much of the voice the room answers | 0–15 | 0 |
| `I`n | unison: a second copy of the voice, n × 2 cents sharp | 0–15 | 0 |

A number left off the end of `Y`, `Z` or `K` keeps its last value.

### Waves: `W`, `Q`

| `W` | Wave | Sounds |
|---|---|---|
| 0 | pulse | hollow at `Q50` (a square), thin and nasal as `Q` falls: the PC speaker, chip leads |
| 1 | triangle | soft: flutes, sub bass |
| 2 | sawtooth | bright and buzzy: strings and brass once filtered |
| 3 | noise | a hiss whose colour follows the note's pitch: drums, wind, explosions |
| 4 | FM | a sine bent by a second sine: basses, brass, organs, bells, kicks |

### Envelope: `H`

A note rises to full in the attack, falls to the sustain level in the decay, holds there until the
note's sounding part ends, then fades in the release.

| n | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| attack, ms | 2 | 8 | 16 | 24 | 38 | 56 | 68 | 80 | 100 | 250 | 500 | 800 | 1000 | 3000 | 5000 | 8000 |
| decay, release, ms | 6 | 24 | 48 | 72 | 114 | 168 | 204 | 240 | 300 | 750 | 1500 | 2400 | 3000 | 9000 | 15000 | 24000 |

The decay and release times are from full to silence; a fall to a sustain of 8 takes less. Sustain
0 is silent, 15 full. With sustain 0 the note is a pluck: it dies in the decay however long it is
held.

### Filter: `Y`

A low-pass removes what is over the cutoff (darker), a high-pass what is under it (thinner), a
band-pass both. Resonance raises what is at the cutoff: at 15 it whistles.

### FM: `W4`, `Z`

The note is a sine (the carrier). A second sine (the modulator) bends its phase, which adds
harmonics: the further it bends, the brighter the sound. The bend has its own fall at each note,
apart from the volume's envelope, so a note can start bright and end round, as a plucked string.

| `Z`'s number | Is | Effect |
|---|---|---|
| ratio, 0.01–15.99 | the modulator's pitch, so many times the note's | 1 a full, saw-like tone; 2 hollow, as a square; 3 and up thinner; 7 and other high odd ones bells; one with hundredths, as 1.41, rings off the note's harmonics: drums, metal |
| depth, 0–15 | how far the modulator bends the carrier, at its full level | 0 a plain sine; 4 warm; 8 bright; 12 and over harsh |
| fall, 0–15 | how fast the bend falls at each note: the envelope's decay scale | 0 at once to its level; 3 a 72 ms pluck; 7 a 240 ms ring |
| level, 0–15 | the part of the depth the bend falls to and holds | 15 no fall; 11 a held brass; 0 down to a sine |
| feedback, 0–7 | how far the modulator bends itself | towards a sawtooth, then noise: grit for brass and organs |

The ratio takes up to two decimals: `Z1.41`, `Z0.5`.

`Z1,12,4,4` plucks a bass that starts bright and ends round. `Z1,9,3,11,5` holds a brass bright.
`Z7,6,7,0` rings a bell. `Z1,0` is a plain sine: with `K1,8` it is a kick drum.

### Vibrato: `U`

The pitch wavers 6 times a second, once the note has sounded a fifth of a second: short notes stay
steady, held ones sing. The depth is n × 0.2% of the pitch each way: `U5` is gentle, `U15` (3%, half
a semitone) wide.

### Glide: `K`

A note slides to its pitch in the time t:

| t | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| ms | 0 | 10 | 20 | 30 | 40 | 60 | 80 | 100 | 120 | 150 | 200 | 250 | 300 | 400 | 500 | 800 |

Where it starts:

- `K`t alone, or q and u 0: from the last note's pitch (portamento).
- q, 1–15: from over its own pitch, at 1 + q/4 times it. `K1,4` falls an octave in 10 ms; `K1,8`
  falls from three times the pitch, a drum's thump.
- u, 1–24: from u semitones under its own pitch, whatever q is. `K8,0,2` scoops up two semitones in
  120 ms, as a singer or a guitar's bend.

### Stereo, reverb, unison: `S`, `R`, `I`

- `S` places the voice between the left (1) and the right (15). A Sound Blaster is mono.
- `R` sends that much of the voice to one shared room, whose tail lasts about a second. Keep drums
  and bass dry.
- `I` plays the voice twice, the copy n × 2 cents sharp: the two beat against each other, as a
  section of players. Not on DOS, nor for noise.

## Time

- `T` is quarter notes a minute, 32 to 255. A length is 1 (whole) to 64, with dots.
- `MN` sounds 7/8 of each note, `ML` all of it, `MS` 3/4; the rest of its time is silence. The
  release fades into that silence and on into the next note.
- Under `ML`, a note followed by one of the same pitch is held, not struck again: `ML L2 C L16 C` is
  one C nine sixteenths long. This is how a note is tied over a bar line.
- A synthesizer voice's notes are timed to a 16th of a clock tick (3.4 ms), counted in the
  synthesizer's own samples on the Mac, in the browser and on a Sound Blaster. The speaker's notes
  keep QBasic's whole ticks (55 ms).
- A voice's notes follow what that voice already has queued; an idle voice starts at once. Voices
  stay together when their strings add up to the same time, bar after bar.
- Each voice's queue holds 18 notes. In the foreground (`MF`) `PLAY` waits for all it queued. In the
  background (`MB`) it waits only while a queue is full.
- `PLAY(n)` counts the notes waiting in voice 1, whatever n is. A rest counts.

## Loudness

Every voice at `V15` is as loud as it gets; the voices are added. When the sum would pass full
scale it is turned down at once and climbs back over a fifth of a second, so many loud voices do
not clip, but the loudest one ducks the rest. Balance with `V`: lead 10 to 12, chords 6 to 9 each,
bass 10 to 12, drums 10 to 14.

## Where it plays

| Where | Plays |
|---|---|
| Mac, Linux, the browser | every voice, in stereo |
| DOS with a Sound Blaster (DSP 2.00 or later, named by `BLASTER`) | every voice, mono, 15873 samples a second; no unison; FM with a ratio of hundredths costs more |
| DOS without one | voice 1 only, on the PC speaker, as a square wave: put the melody first |

`SOUND`, `BEEP` and a `PLAY` with one string and none of these commands always use the speaker.

## Example

```basic
lead$ = "T140 W4 Z1,9,3,11,4 H0,4,12,1 U8 S10 R4 V11 L8 O4 C E G > C < G E C4"
bass$ = "T140 W4 Z1,12,4,4 H0,5,5,1 V11 L4 O2 C G C G"
drum$ = "T140 L8 W4 Z1,0 H0,4,3,3 K1,8 V12 O1 C W3 K0 H0,3,0,1 Y3,2500 O6 B W4 Y0 H0,4,3,3 K1,8 O1 C W3 K0 H0,3,0,1 Y3,2500 O6 B"
PLAY lead$, bass$, drum$ + drum$
```
