/* A voice of the synthesizer, as PLAY sets it (play.c) and a platform's synthesizer plays it
   (plat/synth): its own header, with no other, so the platform's side includes it alone. */
#ifndef QB_DEV_VOICE_H
#define QB_DEV_VOICE_H

enum { DEV_VOICES = 6 };
enum DevWave { DEV_PULSE, DEV_TRIANGLE, DEV_SAW, DEV_NOISE };
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
} DevVoice;

#endif
