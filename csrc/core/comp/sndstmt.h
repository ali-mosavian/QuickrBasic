/* The speaker's statements' compiler's interface. How it works: sndstmt.c. */
#ifndef COMP_SNDSTMT_H
#define COMP_SNDSTMT_H
#include "comp/comp.h"

/* SOUND or PLAY at comp.p (FFh n) compiled; false not one: PLAY ON, OFF, STOP are events'. */
bool snds_stmt(Comp *C);
#endif
