/* PLAY (QB rt/gwplays.asm B$SPLY): music written as a string of commands.

   A note is a letter A to G, a sharp (# or +) or a flat (-), a length and dots;
   the other commands set the octave (O, < and >), the length (L), the tempo (T)
   and the style (M).  The notes go to a queue that the clock tick plays through
   the speaker; music in the foreground makes the program wait until the queue
   is empty (the last note has begun), music in the background only while it is
   full.

   PLAY a$, b$, ... (after PCjr and Tandy BASIC's three) plays up to six voices
   through the synthesizer (dev_voice), each from its own queue; so does one string with a
   command QB never had: V volume, W wave, Q pulse width, H envelope, Y filter.
   Without them a voice is the speaker, as in QB. */
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
    HIGHEST_FEEDBACK = 7
};

/* How much of a note sounds before the silence that ends it, in eighths. */
enum Style { NORMAL = 7, LEGATO = 8, STAGGER = 6 };

typedef struct Player {
    Macro m;
    /* where the X commands left their strings: X runs one inside the next */
    Macro outer[MACRO_NESTING + 1];
    int octave, length, tempo;
    enum Style style;
    byte synth;     /* the first voice is the synthesizer's, else the speaker; the others always are */
    DevVoice sound; /* what its next note sounds like */
} Player;

static void illegal(void)
{
    qb_error(BE_ILLFUN);
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

/* Queues a note, or a rest with `note` 0, `time` long, in `voice`. */
static void play_note(Player *p, unsigned voice, int note, long time)
{
    const long sounding = note ? time * p->style / LEGATO : time;
    DevVoice v = p->sound;

    v.hertz = note ? frequency(note) : 0;
    music_queue(voice, p->synth || voice > 0, &v, sounding, time - sounding);
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

/* H attack, decay, sustain, release: SID's 0 to 15 each. */
static void envelope(Player *p)
{
    unsigned value = 0;
    int i;

    for (i = 0; i < 4; i++) {
        if (i && !comma(p))
            illegal();
        value = value << 4 | within(p, 0, HIGHEST_NIBBLE);
    }
    p->sound.envelope = (unsigned short)value;
}

/* Y mode[, cutoff[, resonance]]: the filter off (0), low-pass, band-pass, high-pass. */
static void filter(Player *p)
{
    p->sound.filter = (unsigned char)within(p, DEV_UNFILTERED, DEV_HIGH_PASS);
    if (comma(p)) {
        p->sound.cutoff = (unsigned short)within(p, LOWEST_CUTOFF, HIGHEST_CUTOFF);
        if (comma(p))
            p->sound.resonance = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
    }
}

static void volume(Player *p)
{
    p->sound.volume = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
}

static void wave(Player *p)
{
    p->sound.wave = (unsigned char)within(p, DEV_PULSE, DEV_FM);
}

/* Z ratio[, depth[, decay[, level[, feedback]]]]: FM's modulator, its pitch in the note's, how far it bends the
   carrier, how fast that falls at each note and to what, and how far it bends itself. */
static void modulator(Player *p)
{
    p->sound.ratio = (unsigned char)within(p, 1, HIGHEST_NIBBLE);
    if (!comma(p))
        return;
    p->sound.depth = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
    if (!comma(p))
        return;
    p->sound.fall = (unsigned char)(within(p, 0, HIGHEST_NIBBLE) << 4 | (p->sound.fall & HIGHEST_NIBBLE));
    if (!comma(p))
        return;
    p->sound.fall = (unsigned char)((p->sound.fall & ~HIGHEST_NIBBLE) | within(p, 0, HIGHEST_NIBBLE));
    if (comma(p))
        p->sound.feedback = (unsigned char)within(p, 0, HIGHEST_FEEDBACK);
}

static void vibrato(Player *p)
{
    p->sound.vibrato = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
}

/* I n: a second voice in unison, n times 2 cents sharp. */
static void unison(Player *p)
{
    p->sound.spread = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
}

/* R n: how much of the voice the room answers. */
static void reverb(Player *p)
{
    p->sound.reverb = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
}

/* S n: where the voice is in stereo, 1 the left to 15 the right, 8 the middle. */
static void side(Player *p)
{
    p->sound.pan = (unsigned char)within(p, 1, HIGHEST_NIBBLE);
}

/* K time[, from[, under]]: a note slides to its pitch, from the last note's, from so many quarters over its own, or
   from so many semitones under it. */
enum { MOST_UNDER = 24 };
static void glide(Player *p)
{
    p->sound.glide = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
    if (comma(p))
        p->sound.from = (unsigned char)within(p, 0, HIGHEST_NIBBLE);
    if (comma(p))
        p->sound.under = (unsigned char)within(p, 0, MOST_UNDER);
}

/* Q percent: the pulse's high part. */
static void width(Player *p)
{
    p->sound.width = (unsigned short)(within(p, 1, 99) * WIDTH_SCALE / 100);
}

/* The commands QB never had: each sets the voice's sound, and makes it the synthesizer's. */
static const struct {
    char letter;
    void (*set)(Player *p);
} synth_commands[] = {{'V', volume}, {'W', wave}, {'Q', width}, {'H', envelope}, {'Y', filter}, {'Z', modulator},
                    {'U', vibrato}, {'K', glide}, {'I', unison}, {'R', reverb}, {'S', side}};

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
            if (!p->m.depth)
                return 0;
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
static Player players[DEV_VOICES] = {
    [0 ... DEV_VOICES - 1] = {{0, 0, 0}, {{0, 0, 0}}, DEFAULT_OCTAVE, DEFAULT_LENGTH, DEFAULT_TEMPO, NORMAL, 0, FIRST_SOUND}};

/* B$SPLY: PLAY with the string of commands.  What the commands set (the octave,
   the length, the tempo, the style and foreground or background) stays set for
   the next PLAY.  Music in the foreground has each note wait for the one
   sounding to end. */
void B_SPLY(SD *commands)
{
    macro_begin(&players[0].m, commands);
    while (step(&players[0], 0))
        ;
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
        players[i].synth = 1;
        macro_begin(&players[i].m, commands[i]);
        music_align(i);
    }
    for (;;) {
        unsigned next = count;

        for (i = 0; i < count; i++)
            if (more[i] && (next == count || music_end(i) < music_end(next)))
                next = i;
        if (next == count)
            break;
        more[next] = (byte)step(&players[next], next);
    }
    for (i = 0; i < count; i++)
        if (commands[i])
            str_free_if_tmp(commands[i]);
}

/* RUN: what PLAY set of the synthesizer is undone, so a program sounds as QB's until it sets it again. */
void music_reset(void)
{
    unsigned i;

    for (i = 0; i < DEV_VOICES; i++) {
        players[i].synth = 0;
        players[i].sound = first_sound;
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
