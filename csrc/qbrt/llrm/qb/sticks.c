/* B$STIK, B$FSTG and B$FPEN: STICK(n), STRIG(n) and PEN(n) of a machine without a joystick or a
   light pen: nothing moves and nothing is pressed. Past QBasic 1.1's n, Illegal function call
   (measured). */
#include "error.h"
#include "qb.h"

#pragma aux B_STIK "B$STIK"
#pragma aux B_FSTG "B$FSTG"
#pragma aux B_FPEN "B$FPEN"

enum { STICK_LAST = 3, STRIG_LAST = 7, PEN_LAST = 9, PEN_FIRST_CELL = 6 };

static void in_range(short which, short last)
{
    if (which < 0 || which > last)
        qb_error(BE_ILLFUN);
}

short B_STIK(short which)
{
    in_range(which, STICK_LAST);
    return 0;
}

short B_FSTG(short which)
{
    in_range(which, STRIG_LAST);
    return 0;
}

/* 6 to 9: the character row and column of the last press and of now, from 1. */
short B_FPEN(short which)
{
    in_range(which, PEN_LAST);
    return which >= PEN_FIRST_CELL;
}
