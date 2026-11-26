/* PLAY (QB rt/gwplays.asm B$SPLY): music written as a string of commands.

   A note is a letter A to G, a sharp (# or +) or a flat (-), a length and dots;
   the other commands set the octave (O, < and >), the length (L), the tempo (T)
   and the style (M).  The notes go to a queue that the clock tick plays through
   the speaker; music in the foreground makes the program wait until the queue
   is empty (the last note has begun), music in the background only while it is
   full.

   PLAY a$, b$, ... (after PCjr and Tandy BASIC's three) plays up to DEV_VOICES voices
   through the synthesizer (dev_voice), each from its own queue; so does one string with a
   command QB never had (synth_commands; docs/sound.md).
   Without them a voice is the speaker, as in QB. */
#include <string.h>

#include "music.h"
#include "nhstutil.h"
#include "varptr.h"
#include "rtinit.h"

#pragma aux B_SPLY "B$SPLY"
#pragma aux B_SPLYV "B$SPLYV"
enum {
    LOWEST_OCTAVE = 0,
    HIGHEST_OCTAVE = 6,
    DEFAULT_OCTAVE = 4,
    DEFAULT_LENGTH = 4,
    DEFAULT_TEMPO = 120,
    LOWEST_TEMPO = 32,
    HIGHEST_TEMPO = 255,
    LONGEST_NOTE = 64,
    HIGHEST_NOTE = 84,
    SEMITONES = 12,
    /* the synthesizer's: SID's nibbles, a pulse width in hundredths, a cutoff in Hz */
    HIGHEST_NIBBLE = 15,
    DEFAULT_VOLUME = 8,
    DEFAULT_ENVELOPE = 0x00F0, /* at once to full, held, at once to silence: an organ, as the speaker */
    WIDTH_SCALE = 4096,
    DEFAULT_WIDTH = WIDTH_SCALE / 2,
    LOWEST_CUTOFF = 30,
    HIGHEST_CUTOFF = 12000,
    DEFAULT_CUTOFF = 1000,
    DEFAULT_DEPTH = 8,
    DEFAULT_FALL = 0x0F, /* the modulator held at full */
    HIGHEST_FEEDBACK = 7,
    MOST_PITCH = 1600 /* hundredths of a note's pitch */
};

/* How much of a note sounds before the silence that ends it, in eighths. */
enum Style { NORMAL = 7, LEGATO = 8, STAGGER = 6 };

typedef struct Player {
    Macro m;
    /* where the X commands left their strings: X runs one inside the next */
    Macro outer[MACRO_NESTING + 1];
    int octave, length, tempo;
    enum Style style;
    byte synth; /* the first voice is the synthesizer's, else the speaker; the others always are */
    byte layer; /* the layer the sound's commands set (J) */
    DevVoice sound[DEV_LAYERS]; /* what its next note sounds like: its own sound and sound[0].layers more */
} Player;

static void illegal(void)
{
    qb_error(BE_ILLFUN);
}

/* The layer J chose: the sound its commands set. */
static DevVoice *layer_of(Player *p)
{
    return &p->sound[p->layer];
}

/* The number that follows a command, or -1 if there is none. */
static long number(Player *p)
{
    long value = -1;

    while (p->m.at < p->m.end && *p->m.at == ' ')
        p->m.at++;
    while (p->m.at < p->m.end && *p->m.at >= '0' && *p->m.at <= '9') {
        value = (value < 0 ? 0 : value) * 10 + (*p->m.at++ - '0');
        if (value > 30000)
            illegal();
    }
    if (value < 0)
        macro_equals(&p->m, &value);
    return value;
}

/* A number from `low` to `high`, else an Illegal function call. */
static unsigned within(Player *p, long low, long high)
{
    const long n = number(p);

    if (n < low || n > high)
        illegal();
    return (unsigned)n;
}

/* Whether a comma follows, passed. */
static int comma(Player *p)
{
    while (p->m.at < p->m.end && *p->m.at == ' ')
        p->m.at++;
    if (p->m.at < p->m.end && *p->m.at == ',') {
        p->m.at++;
        return 1;
    }
    return 0;
}

/* The hundredths after a number's point: ".5" 50, ".05" 5; 0 with no point. */
static unsigned hundredths(Player *p)
{
    unsigned value = 0, digits = 0;

    if (p->m.at >= p->m.end || *p->m.at != '.')
        return 0;
    p->m.at++;
    while (p->m.at < p->m.end && *p->m.at >= '0' && *p->m.at <= '9') {
        if (++digits > 2)
            illegal();
        value = value * 10 + (unsigned)(*p->m.at++ - '0');
    }
    if (!digits)
        illegal();
    return digits == 1 ? value * 10 : value;
}

/* A note of `length`th of a whole note and `dots` dots, in thousandths of a
   tick. */
static long duration(const Player *p, int length, int dots)
{
    long whole = 4L * TICKS_PER_MINUTE / p->tempo;
    long time = whole / length, extra = time;

    while (dots--) {
        extra /= 2;
        time += extra;
    }
    return time;
}

static unsigned frequency(int note)
{
    static const unsigned octave_three[SEMITONES] = {262, 277, 294, 311, 330, 349,
                                                     370, 392, 415, 440, 466, 494};
    int octave = (note - 1) / SEMITONES - 3;
    unsigned hertz = octave_three[(note - 1) % SEMITONES];

    return octave >= 0 ? hertz << octave : hertz >> -octave;
}

unsigned play_hertz(int note)
{
    return frequency(note);
}

/* The voice of an instrument's commands (play_sound): a note there is wrong. */
enum { NO_VOICE = DEV_VOICES };

/* Queues a note, or a rest with `note` 0, `time` long, in `voice`. */
static void play_note(Player *p, unsigned voice, int note, long time)
{
    const long sounding = note ? time * p->style / LEGATO : time;
    DevVoice v[DEV_LAYERS];

    if (voice == NO_VOICE)
        illegal();
    memcpy(v, p->sound, sizeof v);
    v[0].hertz = note ? frequency(note) : 0;
    music_queue(voice, p->synth || voice > 0, v, sounding, time - sounding);
}

static int dots(Player *p)
{
    int count = 0;

    while (p->m.at < p->m.end && *p->m.at == '.') {
        p->m.at++;
        count++;
    }
    return count;
}

static void style(Player *p)
{
    switch (macro_next(&p->m)) {
    case 'F':
        music_foreground = 1;
        break;
    case 'B':
        music_foreground = 0;
        break;
    case 'N':
        p->style = NORMAL;
        break;
    case 'L':
        p->style = LEGATO;
        break;
    case 'S':
        p->style = STAGGER;
        break;
    default:
        illegal();
    }
}

/* H attack, decay, sustain, release[, fade]: SID's 0 to 15 each; the fall on from the sustain, on the decay's scale. */
static void envelope(Player *p)
{
    DevVoice *const v = layer_of(p);
    unsigned value = 0;
    int i;

    for (i = 0; i < 4; i++) {
        if (i && !comma(p))
            illegal();
        value = value << 4 | within(p, 0, HIGHEST_NIBBLE);
    }
    v->envelope = (unsigned short)value;
    v->fade = (unsigned char)(comma(p) ? within(p, 0, HIGHEST_NIBBLE) + 1 : 0);
}

/* Y mode[, cutoff[, resonance[, from[, time]]]]: the filter off (0), low-pass, band-pass, high-pass; the cutoff a note
   starts at (0 none) and how long it takes to the cutoff. */
static void filter(Player *p)
{
    DevVoice *const v = layer_of(p);

    v->filter = (unsigned char)within(p, DEV_UNFILTERED, DEV_HIGH_PASS);
    if (!comma(p))
        return;
    v->cutoff = (unsigned short)within(p, LOWEST_CUTOFF, HIGHEST_CUTOFF);
    if (!comma(p))
        return;
    v->resonance = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
    if (!comma(p))
        return;
    v->sweep = (unsigned short)within(p, 0, HIGHEST_CUTOFF);
    if (v->sweep && v->sweep < LOWEST_CUTOFF)
        illegal();
    if (comma(p))
        v->swept = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
}

/* V volume[.hundredths]: 0 to 15. */
static void volume(Player *p)
{
    DevVoice *const v = layer_of(p);

    v->volume = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
    v->loud = (unsigned char)hundredths(p);
    if (v->loud && v->volume == HIGHEST_NIBBLE)
        illegal();
}

static void wave(Player *p)
{
    layer_of(p)->wave = (unsigned char)within(p, DEV_PULSE, DEV_FM);
}

/* Z ratio[.hundredths][, depth[, decay[, level[, feedback[, start]]]]]: FM's modulator, its pitch in the note's, how
   far it bends the carrier, how fast that moves at each note, from where (start, 15 full) to what, and how far it
   bends itself. */
static void modulator(Player *p)
{
    DevVoice *const v = layer_of(p);
    const unsigned whole = within(p, 0, HIGHEST_NIBBLE), part = hundredths(p);

    if (!whole && !part)
        illegal();
    v->ratio = (unsigned char)whole;
    v->fine = (unsigned char)part;
    if (!comma(p))
        return;
    v->depth = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
    if (!comma(p))
        return;
    v->fall = (unsigned char)(within(p, 0, HIGHEST_NIBBLE) << 4 | (v->fall & HIGHEST_NIBBLE));
    if (!comma(p))
        return;
    v->fall = (unsigned char)((v->fall & ~HIGHEST_NIBBLE) | within(p, 0, HIGHEST_NIBBLE));
    if (!comma(p))
        return;
    v->feedback = (unsigned char)within(p, 0, HIGHEST_FEEDBACK);
    if (comma(p))
        v->dip = (unsigned char)(HIGHEST_NIBBLE - within(p, 0, HIGHEST_NIBBLE));
}

/* U, I, R and S are the voice's, every layer's. */
/* U depth[, rate[, tremolo[, wobble]]]: the pitch wavers, tenths of a hertz a time round; the loudness and the cutoff
   with it. */
enum { MOST_RATE = 200 };
static void vibrato(Player *p)
{
    DevVoice *const v = &p->sound[0];

    v->vibrato = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
    if (!comma(p))
        return;
    v->rate = (unsigned char)within(p, 1, MOST_RATE);
    if (!comma(p))
        return;
    v->tremolo = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
    if (comma(p))
        v->wobble = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
}

/* I n: a second voice in unison, n times 2 cents sharp. */
static void unison(Player *p)
{
    p->sound[0].spread = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
}

/* R n: how much of the voice the room answers. */
static void reverb(Player *p)
{
    p->sound[0].reverb = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
}

/* S n: where the voice is in stereo, 1 the left to 15 the right, 8 the middle. */
static void side(Player *p)
{
    p->sound[0].pan = (unsigned char)within(p, 1, HIGHEST_NIBBLE);
}

/* J n[, pitch]: the commands after it set layer n, 0 the voice's own sound, up to the next J or the string's end, and
   it sounds with the voice. A pitch is hundredths of the note's, the last one given without; 0 silences the layer and
   those after it. Layers sound from 1 up, none missed: J0,0 is the voice alone, at its note. */
static void layer(Player *p)
{
    const unsigned n = within(p, 0, DEV_LAYERS - 1);
    DevVoice *const voice = &p->sound[0];

    p->layer = (byte)n;
    const int given = comma(p);
    const unsigned pitch = given ? within(p, 0, MOST_PITCH) : 0;

    if (given && !pitch) {
        if (!n)
            voice->pitch = 0;
        if (voice->layers >= n)
            voice->layers = (unsigned char)(n ? n - 1 : 0);
        return;
    }
    if (n > voice->layers + 1u)
        illegal();
    if (given)
        p->sound[n].pitch = (unsigned short)pitch;
    if (n > voice->layers)
        voice->layers = (unsigned char)n;
}

/* K time[, from[, under[, hundredths]]]: a note slides to its pitch, from the last note's, from so many quarters over
   its own, from so many semitones under it, or from so many hundredths of it. */
enum { MOST_UNDER = 24 };
static void glide(Player *p)
{
    DevVoice *const v = layer_of(p);

    v->glide = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
    if (comma(p))
        v->from = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
    if (!comma(p))
        return;
    v->under = (unsigned char)within(p, 0, MOST_UNDER);
    if (comma(p))
        v->over = (unsigned short)within(p, 0, MOST_PITCH);
}

/* Q percent: the pulse's high part. */
static void width(Player *p)
{
    layer_of(p)->width = (unsigned short)(within(p, 1, 99) * WIDTH_SCALE / 100);
}

/* The commands QB never had: each sets the voice's sound, and makes it the synthesizer's. */
static const struct {
    char letter;
    void (*set)(Player *p);
} synth_commands[] = {{'V', volume}, {'W', wave}, {'Q', width}, {'H', envelope}, {'Y', filter}, {'Z', modulator},
                    {'U', vibrato}, {'K', glide}, {'I', unison}, {'R', reverb}, {'S', side}, {'J', layer}};

static void synth_command(Player *p, char command)
{
    unsigned i;

    for (i = 0; i < sizeof synth_commands / sizeof synth_commands[0]; i++)
        if (synth_commands[i].letter == command) {
            synth_commands[i].set(p);
            p->synth = 1;
            return;
        }
    illegal();
}

/* A letter note: its semitone above C, with a sharp or a flat. */
static int named_note(Player *p, char letter)
{
    static const int semitone[7] = {9, 11, 0, 2, 4, 5, 7};
    int note = p->octave * SEMITONES + semitone[letter - 'A'] + 1;

    if (p->m.at < p->m.end && (*p->m.at == '#' || *p->m.at == '+')) {
        p->m.at++;
        note++;
    } else if (p->m.at < p->m.end && *p->m.at == '-') {
        p->m.at++;
        note--;
    }
    return note;
}

/* The commands up to the next note or rest, queued in `v`: whether there was one before the end of the string. */
static int step(Player *p, unsigned voice)
{
    char command;
    long n;

    for (;;) {
        command = macro_next(&p->m);
        if (!command) {
            /* the end of a string X ran: on with the one around it */
            if (!p->m.depth) {
                p->layer = 0;
                return 0;
            }
            macro_leave(&p->m, &p->outer[p->m.depth - 1]);
            continue;
        }
        switch (command) {
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G': {
            int note = named_note(p, command);

            n = number(p);
            if (n == 0 || n > LONGEST_NOTE)
                illegal();
            play_note(p, voice, note, duration(p, n < 0 ? p->length : (int)n, dots(p)));
            return 1;
        }
        case 'N':
            n = number(p);
            if (n < 0 || n > HIGHEST_NOTE)
                illegal();
            play_note(p, voice, (int)n, duration(p, p->length, dots(p)));
            return 1;
        case 'P':
            n = number(p);
            if (n < 1 || n > LONGEST_NOTE)
                illegal();
            play_note(p, voice, 0, duration(p, (int)n, dots(p)));
            return 1;
        case 'O':
            n = number(p);
            if (n < LOWEST_OCTAVE || n > HIGHEST_OCTAVE)
                illegal();
            p->octave = (int)n;
            break;
        case '<':
        case '>':
            p->octave += command == '>' ? 1 : -1;
            if (p->octave < LOWEST_OCTAVE || p->octave > HIGHEST_OCTAVE)
                illegal();
            break;
        case 'L':
            n = number(p);
            if (n < 1 || n > LONGEST_NOTE)
                illegal();
            p->length = (int)n;
            break;
        case 'T':
            n = number(p);
            if (n < LOWEST_TEMPO || n > HIGHEST_TEMPO)
                illegal();
            p->tempo = (int)n;
            break;
        case 'M':
            style(p);
            break;
        case 'X':
            /* the commands of a string variable, then on with these */
            macro_enter(&p->m, &p->outer[p->m.depth]);
            break;
        default:
            synth_command(p, command);
        }
    }
}

/* What PLAY keeps from one use to the next, a voice each: the octave, the length, the tempo, the style, and the sound;
   the first is what SOUND reads. */
#define FIRST_SOUND {0, DEFAULT_WIDTH, DEFAULT_ENVELOPE, DEFAULT_CUTOFF, DEV_PULSE, DEFAULT_VOLUME, DEV_UNFILTERED, 0, 0, 0, 1, DEFAULT_DEPTH, DEFAULT_FALL, 0, 0, 0, 0, 0, 0, 0, 0}
static const DevVoice first_sound = FIRST_SOUND;
static const Player first_player = {
    {0, 0, 0},
    {{0, 0, 0}},
    DEFAULT_OCTAVE,
    DEFAULT_LENGTH,
    DEFAULT_TEMPO,
    NORMAL,
    0,
    0,
    {[0 ... DEV_LAYERS - 1] = FIRST_SOUND}
};
static Player *players[DEV_VOICES]; /* taken at the voice's first PLAY */

static Player *player(unsigned voice)
{
    if (!players[voice]) {
        players[voice] = dev_take(sizeof *players[voice]);
        *players[voice] = first_player;
    }

    return players[voice];
}

/* B$SPLY: PLAY with the string of commands.  What the commands set (the octave,
   the length, the tempo, the style and foreground or background) stays set for
   the next PLAY.  Music in the foreground has each note wait for the one
   sounding to end. */
void B_SPLY(SD *commands)
{
    Player *const p = player(0);

    macro_begin(&p->m, commands);
    p->layer = 0;
    music_align(0);

    while (step(p, 0))
        ;
    music_begun(0);
    str_free_if_tmp(commands);
}

/* B$SPLYV: PLAY with `count` strings, a voice each, through the synthesizer; a null one leaves its voice alone.  The
   voices' notes are queued in the order they start, so no voice waits for room while another runs dry. */
void B_SPLYV(SD *const *commands, unsigned count)
{
    byte more[DEV_VOICES];
    unsigned i;

    for (i = 0; i < count; i++) {
        more[i] = commands[i] != 0;
        if (!more[i])
            continue;

        Player *const p = player(i);

        p->synth = 1;
        p->layer = 0;
        macro_begin(&p->m, commands[i]);
        music_align(i);
    }
    for (;;) {
        unsigned next = count;

        for (i = 0; i < count; i++)
            if (more[i] && (next == count || music_end(i) < music_end(next)))
                next = i;
        if (next == count)
            break;

        more[next] = (byte)step(players[next], next);
    }
    for (i = 0; i < count; i++)
        if (commands[i]) {
            music_begun(i);
            str_free_if_tmp(commands[i]);
        }
}

void play_sound(SD *commands, DevVoice sound[DEV_LAYERS])
{
    Player p = first_player;

    macro_begin(&p.m, commands);

    while (step(&p, NO_VOICE))
        ;
    str_free_if_tmp(commands);
    memcpy(sound, p.sound, sizeof p.sound);
}

/* RUN: what PLAY set of the synthesizer is undone, so a program sounds as QB's until it sets it again. */
void music_reset(void)
{
    unsigned i;

    for (i = 0; i < DEV_VOICES; i++) {
        Player *const p = players[i];
        unsigned k;

        if (!p)
            continue;

        p->synth = 0;
        p->layer = 0;

        for (k = 0; k < DEV_LAYERS; k++)
            p->sound[k] = first_sound;
    }
}

/* The program has ended. */
static void play_stop(void)
{
    music_halt();
    music_reset();
}

static Comp comp = {0, C_SN, {0, 0, 0, 0, play_stop}};

#define XI_FN play_xinit
#include "xi.h"

void play_xinit(void)
{
    qb_comp_add(&comp);
}
