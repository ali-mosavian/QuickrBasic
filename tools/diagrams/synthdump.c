/* The synthesizer's samples (plat/synth) for tools/diagrams/sound.py, one a line at 44100 a second:
   synthdump HERTZ WAVE WIDTH ENVELOPE FILTER CUTOFF RESONANCE VOLUME GATE_MS TOTAL_MS
   One voice, its note from 0 to GATE_MS, rendered to TOTAL_MS. ENVELOPE is SID's four nibbles, in hex. */
#include <stdio.h>
#include <stdlib.h>

#include "plat/synth/synth.h"

enum { RATE = 44100, ARGS = 11 };

int main(const int argc, char **const argv)
{
    if (argc != ARGS) {
        fprintf(stderr, "synthdump HERTZ WAVE WIDTH ENVELOPE FILTER CUTOFF RESONANCE VOLUME GATE_MS TOTAL_MS\n");
        return 2;
    }
    DevVoice v = {
        .hertz = (unsigned)atoi(argv[1]),
        .wave = (unsigned char)atoi(argv[2]),
        .width = (unsigned short)atoi(argv[3]),
        .envelope = (unsigned short)strtol(argv[4], nullptr, 16),
        .filter = (unsigned char)atoi(argv[5]),
        .cutoff = (unsigned short)atoi(argv[6]),
        .resonance = (unsigned char)atoi(argv[7]),
        .volume = (unsigned char)atoi(argv[8]),
        .gate = 1,
        .trigger = 1,
    };
    const int gate = atoi(argv[9]) * RATE / 1000, total = atoi(argv[10]) * RATE / 1000;
    static Synth s;
    synth_init(&s, RATE);
    synth_voice(&s, 0, &v);
    for (int i = 0; i < total; i++) {
        int16_t sample;
        if (i == gate) {
            v.gate = 0;
            synth_voice(&s, 0, &v);
        }
        synth_render(&s, &sample, 1);
        printf("%d\n", sample);
    }
    return 0;
}
