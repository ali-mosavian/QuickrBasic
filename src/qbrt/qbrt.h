/* llrm's QB runtime (graphics, sound, the console in a graphics mode): the entries the core calls,
 * by their C names, and its start and end. How it works: qbrt.c. */
#ifndef QBRT_QBRT_H
#define QBRT_QBRT_H
#include "fault.h"
#include "plat/pclk.h"

/* A string as the runtime takes it (its SD): length, then the bytes. */
typedef struct {
    uintptr_t len;
    char *ptr;
} QbrtStr;

enum { QBRT_HEAP = 0x4000 }; /* the runtime's strings and local heap: DRAW's and PLAY's pieces */

/* The runtime started, once; its errors raised through f as QBasic's; BREAK (stop's SF_BRK) ends
   a wait for the music; heap its strings' and local heap's bytes, carved from the arena. */
void qbrt_init(Fault *f, volatile uint8_t *stop, void *heap, uint32_t bytes);
/* The next PAINT's queue: room of bytes, free until it ends (the BASIC heap's free part). */
void qbrt_lend(void *room, uint32_t bytes);
/* The program's end: text mode, the speaker quiet, the runtime's tick out (before the platform's: DOS32's
   chains to it). */
void qbrt_done(void);
/* The target's device ready (qbrt_host.c, ...): text mode, nothing typed or sounding. */
void qbrt_device(void);
/* The music's tick in real time (the default), or only while the program waits for it, a tick a
   wait: the same notes every time, for tests. */
void qbrt_realtime(bool yes);
/* A wait for the music (QB_IDLE): BREAK drops what is queued; else the device waits a while or,
   not in real time, ticks. */
void qbrt_idle(void);
void qbrt_tick_wait(void);

/* Standard output is the screen: the console's text goes through the runtime's (cn_*), which keeps
   the cursor, the colours and the PRINT window, in text and graphics modes alike. */
bool qbrt_screen(void);
/* The printer is ready: LPT1's status. */
bool qbrt_printer_ready(void);
/* A graphics mode is on; back to text (SCREEN 0), the runtime's state with it. */
bool qbrt_graphics(void);
void qbrt_text(void);
/* The program's graphics screen, kept while a front end shows its own in text mode: the bytes it
   takes at most; taken (its visible page, palette and display start); shown again, its mode set
   without clearing and the runtime's state as the program left it; dropped, by a new run's text
   mode. DOS32's (qbrt_dos.c). */
uint32_t qbrt_shot_size(void);
void qbrt_shot_take(void *shot);
void qbrt_shot_show(const void *shot);
void qbrt_shot_drop(void);
/* A front end's (the IDE's): the console takes the screen again (its active page, the cursor
   there) and clears it; its cursor, from 0; moved there. */
void qbrt_console_reset(void);
void qbrt_where(uint8_t *row, uint8_t *col);
void qbrt_move(uint8_t row, uint8_t col);
void cn_putc(char c);
void cn_write(const char *s, unsigned n);
void cn_crlf(void);
uint8_t cn_pos(void);
uint8_t cn_line(void);

/* COLOR's, LOCATE's arguments as a block, as screen_mode's: foreground, background, border; row,
   column, cursor, start, stop. */
void screen_color(const int *block);
void screen_locate(const int *block);

/* CLS; VIEW PRINT top TO bottom, -1 -1 the whole screen; WIDTH columns, rows, -1 one left out;
   BEEP. */
void B_SCLS(short selector);
void B_VWPT(short top, short bottom);
void B_WIDT(short columns, short rows);
void B_BEEP(void);

/* SCREEN mode: 0 text; not a mode it has, Illegal function call. */
void gfx_screen(int mode);
/* SCREEN's arguments as a block: its count of words, then from the top down each argument's flag
   and, when set, its value. */
void screen_mode(const int *block);

/* The statement's points: first (N1), second (N2); STEP's from the last point (S1) or the first (S2). */
void B_N1I2(short x, short y);
void B_N1R4(float x, float y);
void B_N2I2(short x, short y);
void B_N2R4(float x, float y);
void B_S1I2(short x, short y);
void B_S1R4(float x, float y);
void B_S2I2(short x, short y);
void B_S2R4(float x, float y);

/* PSET or PRESET with a colour; PSET, PRESET without. */
void B_PSTC(short color);
void B_PSET(void);
void B_PRST(void);

/* LINE between the points: color -1 the foreground, style -1 solid, how 0 a line, 1 B, 2 BF. */
void B_LINE(short color, short style, short how);

/* CIRCLE round the first point; the start and end angles and the aspect before it, if given. */
void B_CSTT(float start);
void B_CSTO(float end);
void B_CASP(float aspect);
void B_CIRC(float radius, short color);

/* PAINT from the first point: fill and border -1 the foreground, the fill's; or with a tile. */
void B_PAIN(short fill, short border);
void B_PNTC(QbrtStr *tile, short border, short background);

/* DRAW's commands. */
void B_DRAW(QbrtStr *commands);

/* VIEW [SCREEN] (x1, y1)-(x2, y2), fill and border -1 none; VIEW alone. */
void B_VIEW(short x1, short y1, short x2, short y2, short fill, short border, short screen);
void B_VEW0(void);

/* WINDOW [SCREEN] (x1, y1)-(x2, y2); WINDOW alone. */
void B_WIND(float x1, float y1, float x2, float y2, short screen);
void B_WIN0(void);

/* PALETTE attribute, color. */
void B_PAL2(short attribute, long color);

/* An array's elements from data on, bytes of them, each elem bytes: PALETTE USING's colours, GET's
   and PUT's picture (how 0 OR, 1 AND, 2 PRESET, 3 PSET, 4 XOR). */
void qbrt_palette_using(void *data, uint32_t bytes, uint32_t elem);
void qbrt_get(void *data, uint32_t bytes, uint32_t elem);
void qbrt_put(void *data, uint32_t bytes, uint32_t elem, short how);

/* POINT(x, y): the colour, -1 off the screen. */
int B_PNI2(short x, short y);
int B_PNR4(float x, float y);

/* PMAP(value, function): function 0, 1 a logical x, y to a pixel; 2, 3 back. */
float *B_PMAP(float value, short function);

/* TIMER: the seconds since midnight, as finely as the device keeps them (DOS: the timer chip's
   count within the tick). */
float *B_TIMR(void);
/* TIME$ set: the runtime's clock with it. The host's is its own, in real time from t; DOS's the
   BIOS's, which DOS sets. */
void qbrt_clock_set(const PclkTime *t);

/* SOUND hertz, ticks; PLAY's commands; the notes waiting in the background music; nothing queued
   or sounding. */
void B_SOND(short hertz, float ticks);
void B_SPLY(QbrtStr *commands);
unsigned music_waiting(void);
/* VARPTR$ of the variable at address of QBasic's type byte type: the type, then the address. */
enum { QBRT_VARPTR = 5 };
void qbrt_varptr(uint32_t address, uint8_t type, char out[static QBRT_VARPTR]);
/* SCREEN(row, column[, f]): the character there, f nonzero its attribute. */
short B_FSCN(short row, short column, short attribute);
/* STICK(n), STRIG(n), PEN(n). */
short B_STIK(short which);
short B_FSTG(short which);
short B_FPEN(short which);
void music_clear(void);
#endif
