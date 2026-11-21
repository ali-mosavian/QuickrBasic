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
| `V`n | volume, with up to two decimals: `V4.5` | 0–15 | 8 |
| `W`n | wave: 0 pulse, 1 triangle, 2 sawtooth, 3 noise, 4 FM | 0–4 | 0 |
| `Q`n | pulse width, percent high (wave 0 only) | 1–99 | 50 |
| `H`a`,`d`,`s`,`r[`,`f] | envelope: attack, decay, sustain level, release, all four needed; fade (below) | 0–15 each | `H0,0,15,0` |
| `Y`m[`,`f[`,`r[`,`s[`,`t]]]] | filter: 0 off, 1 low-pass, 2 band-pass, 3 high-pass; cutoff in Hz; resonance; sweep (below) | m 0–3, f 30–12000, r t 0–15, s 0 or 30–12000 | off, 1000, 0, 0, 0 |
| `Z`r[`,`d[`,`f[`,`l[`,`b[`,`s]]]]] | FM's modulator: ratio, depth, fall, level, feedback, start (below) | r 0.01–15.99, d f l s 0–15, b 0–7 | `Z1,8,0,15,0,15` |
| `U`n[`,`r[`,`a[`,`c]]] | vibrato: depth, rate, tremolo, the cutoff's wobble (below) | n a c 0–15, r 1–200 | `U0,60,0,0` |
| `K`t[`,`q[`,`u[`,`p]]] | glide: time, from over, from under, from hundredths (below) | t q 0–15, u 0–24, p 0–1600 | `K0,0,0,0` |
| `S`n | stereo place: 1 the left, 8 the middle, 15 the right | 1–15 | 8 |
| `R`n | reverb: how much of the voice the room answers | 0–15 | 0 |
| `I`n | unison: a second copy of the voice, n × 2 cents sharp | 0–15 | 0 |
| `J`n[`,`p] | layer: the one the commands after it set; its pitch (below) | n 0–3, p 0–1600 | `J0` |

A number left off the end of `Y`, `Z`, `K` or `U` keeps its last value.

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

A fifth number lets the level fall on from the sustain while the note is held, at the decay scale's
f: `H0,4,1,7,10` falls to a sustain of 1, then on, slower as it gets quieter, as a drum's ring. Left
off, the level holds.

### Filter: `Y`

A low-pass removes what is over the cutoff (darker), a high-pass what is under it (thinner), a
band-pass both. Resonance raises what is at the cutoff: at 15 it whistles.

A sweep starts each note's cutoff at s Hz and moves it to f in the time t, on the envelope's decay
scale: `Y1,800,6,6000,4` closes from 6000 Hz to 800 in 114 ms, a pluck's brightness falling. From
under f it opens. s 0 is no sweep.

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
| start, 0–15 | the part of the depth the bend starts at, at each note | 15 full; under the level it rises: `Z1,8,4,15,7,0` swells into a hiss |

The ratio takes up to two decimals: `Z1.41`, `Z0.5`.

`Z1,12,4,4` plucks a bass that starts bright and ends round. `Z1,9,3,11,5` holds a brass bright.
`Z7,6,7,0` rings a bell. `Z1,0` is a plain sine: with `K1,8` it is a kick drum.

### Vibrato: `U`

The pitch wavers 6 times a second, once the note has sounded a fifth of a second: short notes stay
steady, held ones sing. The depth is n × 0.2% of the pitch each way: `U5` is gentle, `U15` (3%, half
a semitone) wide.

- r: how often it goes round, tenths of a hertz: 60 is 6 times a second, 5 once in two seconds.
- a: tremolo, the loudness wavering with it, a 16ths of itself each way.
- c: the cutoff wavering with it, c tones each way: `Y1,800 U0,20,0,12` sweeps a filter from 200 to
  3200 Hz and back twice a second.

The tremolo and the wobble wait as the vibrato does. `U` is the voice's, every layer's.

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
- p, 1–1600: from p hundredths of its own pitch, whatever q and u are. `K4,0,0,140` falls from 1.4
  times the pitch in 40 ms, a tom's drop.

### Stereo, reverb, unison: `S`, `R`, `I`

- `S` places the voice between the left (1) and the right (15). A Sound Blaster is mono.
- `R` sends that much of the voice to one shared room, whose tail lasts about a second. Keep drums
  and bass dry.
- `I` plays the voice twice, the copy n × 2 cents sharp: the two beat against each other, as a
  section of players. Not on DOS, nor for noise.

### Layers: `J`

A voice sounds up to three layers with its own sound, each its own wave, envelope, filter, FM, glide
and volume, at its own pitch: a drum's thump and its hiss, or sines at a bell's partials.

- `J`n chooses the layer the commands after it set, and sounds it: 0 the voice's own sound, 1 to 3 its
  layers. It holds until the next `J` or the end of the string.
- `J`n`,`p sounds layer n at p hundredths of the note's pitch: 100 the note, 200 an octave over, 159
  a drum's second ring. Without p a layer keeps its last pitch, the note's at first. Layers come on in
  order, 1 before 2.
- p 0 silences layer n and those after it. `J0,0` is the voice alone again, at its note.
- `U`, `I`, `R` and `S` are the voice's, whatever layer `J` chose.
- Each note starts its layers together, every wave from its beginning, so every hit sounds the same.
  A layer costs as much as a voice.

`W4 Z1,0 H0,4,0,3 K1,4 V12 J1,250 W3 H2,4,0,4 Y2,3000,4 V9 O2 E` is a snare: a sine's thump falling,
and a hiss.

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

## Songs

A song is an array of timed notes that the music's clock plays by itself. Nothing is fed while it
plays, and each note starts at its time from the song's start, so a long song does not drift.

- `instrument n, commands$` sets sound n, 1 to 64, by the commands of a `PLAY` string, layers too.
  A note in commands$ is an error.
- `music events()` plays a song from now. events() is an array of a `TYPE` whose first five fields
  are `INTEGER` or `LONG`: at, length, kind, pitch, loud, by any names.
  - at and length are ticks, 96 a quarter: a 16th is 24, an 8th triplet 32.
  - kind is the instrument. 0 is a tempo: pitch is then quarters a minute, 32 to 255, from at on.
    Until the first, 120.
  - pitch is `PLAY`'s `N`, 1 to 84; 0 a rest.
  - loud is hundredths of the instrument's volume, 0 to 100.
- Events may come in any order. Events on one tick start together: a chord is three notes at one at.
- `music events(), from` plays the song again from tick from each time it ends.
- `music stop` ends the song now; `music stop, ticks` fades it out over that many ticks.
- `note n, pitch, length, loud` strikes instrument n at once, beside a song or without one. length
  is ticks at the song's tempo, else 120's.
- `beat()` is the quarters since the song began, counting on through its loops; 0 when none plays.

Each note takes a voice of its own, from voice 32 down, where `PLAY` takes them from 1 up, so notes
that overlap ring on. A note takes a voice `PLAY` is not using whose last note has died away, or else
the one whose sound ends first, cutting it. With every voice `PLAY`'s, the note is not played.

`note`, `music`, `beat` and `instrument` stay a program's own names wherever QBasic allows a name: a
variable, an array, a `SUB` or a `FUNCTION` called that works as before.

```basic
type event
    at as integer
    length as integer
    kind as integer
    pitch as integer
    loud as integer
end type

instrument 1, "W4 Z1,0 H0,4,0,4 V12"
instrument 2, "W3 Y2,3000,2 H0,2,0,2 V10"
dim ev(3) as event
for i = 0 to 3
    read ev(i).at, ev(i).length, ev(i).kind
    read ev(i).pitch, ev(i).loud
next i
data 0, 0, 0, 140, 0
data 0, 48, 1, 25, 100
data 0, 24, 2, 61, 80
data 48, 24, 2, 61, 60
music ev(), 0
```

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
