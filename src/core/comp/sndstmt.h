/* The speaker's statements' compiler's interface. How it works: sndstmt.c. */
#ifndef COMP_SNDSTMT_H
#define COMP_SNDSTMT_H
#include "comp/comp.h"

/* SOUND or PLAY at comp.p (FFh n) compiled; false not one: PLAY ON, OFF, STOP are events'. */
bool snds_stmt(Comp *C);
/* instrument, music or note at comp.p, a name not the program's, compiled; false not one. */
bool snds_named(Comp *C);
/* beat() at comp.p: comp.p past it. */
bool snds_beat(Comp *C);
#endif
