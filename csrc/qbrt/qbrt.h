/* llrm's QB runtime (graphics, sound, the console in a graphics mode): the entries the core calls,
 * by their C names, and its start and end. How it works: qbrt.c. */
#ifndef QBRT_QBRT_H
#define QBRT_QBRT_H
#include "fault.h"

/* A string as the runtime takes it (its SD): length, then the bytes. */
typedef struct {
    uintptr_t len;
    char *ptr;
} QbrtStr;

/* The runtime started, once; its errors raised through f as QBasic's. */
void qbrt_init(Fault *f);
/* The target's device ready (qbrt_host.c, ...): text mode, nothing typed or sounding. */
void qbrt_device(void);

/* SCREEN mode: 0 text; not a mode it has, Illegal function call. */
void gfx_screen(int mode);

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

/* POINT(x, y): the colour, -1 off the screen. */
int B_PNI2(short x, short y);
#endif
