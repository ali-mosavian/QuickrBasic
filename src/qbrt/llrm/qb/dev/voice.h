/* A voice of the synthesizer, as PLAY sets it (play.c) and a platform's synthesizer plays it
   (plat/synth): its own header, with no other, so the platform's side includes it alone. */
#ifndef QB_DEV_VOICE_H
#define QB_DEV_VOICE_H

enum { DEV_VOICES = 32, PAN_MIDDLE = 8, DEV_LAYERS = 4 };
enum DevWave { DEV_PULSE, DEV_TRIANGLE, DEV_SAW, DEV_NOISE, DEV_FM };
enum DevFilter { DEV_UNFILTERED, DEV_LOW_PASS, DEV_BAND_PASS, DEV_HIGH_PASS };

typedef struct DevVoice {
    unsigned hertz;          /* 0 silences the voice at once */
    unsigned short width;    /* the pulse's high part, 4096ths of a period */
    unsigned short envelope; /* attack, decay, sustain, release from the high nibble: SID's 0 to 15 */
    unsigned short cutoff;   /* the filter's, Hz */
    unsigned char wave;      /* DevWave */
    unsigned char volume;    /* 0 to 15 */
    unsigned char filter;    /* DevFilter */
    unsigned char resonance; /* 0 to 15 */
    unsigned char gate;      /* 1 while the note sounds; 0 starts its release */
    unsigned char trigger;   /* changes at each note: its attack starts again */
    unsigned char ratio;     /* FM: the modulator's pitch, 0 to 15 times the note's (0 with a fine part) */
    unsigned char depth;     /* FM: how far it bends the carrier, 0 to 15, at the modulator's full level */
    unsigned char fall;      /* FM: the modulator's level falls from full at each note: its decay (SID's 0 to 15) in the
                                high nibble, the level it holds (0 to 15) in the low */
    unsigned char feedback;  /* FM: the modulator bends itself, 0 to 7: towards a saw, then noise */
    unsigned char vibrato;   /* the pitch wavers 6 times a second once the note has sounded a while, 0 to 15 */
    unsigned char glide;     /* a note slides to its pitch, 0 to 15: at once to 800 ms */
    unsigned char from;      /* where it slides from: 0 the last note, else its own pitch and n quarters more */
    unsigned char spread;    /* a second voice in unison, n times 2 cents sharp, 0 to 15: the two beat, as a section */
    unsigned char reverb;    /* how much of the voice the room answers, 0 to 15 */
    unsigned char under;     /* it slides from n semitones under its own pitch instead, 1 to 24 */
    unsigned char pan;       /* where it is in stereo: 1 the left to 15 the right, 8 (and 0) the middle */
    unsigned char fine;      /* FM: hundredths added to the ratio, 0 to 99: the modulator off the note's harmonics */
    unsigned char layers;    /* a note's own: the DevVoices after this one that sound with it, each its own wave, pitch,
                                envelope and level, to DEV_LAYERS - 1; their hertz, gate, trigger, vibrato and unison
                                are this one's */
    unsigned short pitch;    /* hundredths of the note's pitch, 1 to 1600; 0 the note's */
} DevVoice;

#endif
