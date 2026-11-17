/* B$STIK and B$FSTG: STICK(n) and STRIG(n) of a machine without a joystick: nothing moves and
   nothing is pressed. Past QBasic 1.1's n, Illegal function call (measured). PEN is the mouse's
   (core/vm/devices.c). */
#include "error.h"
#include "qb.h"

#pragma aux B_STIK "B$STIK"
#pragma aux B_FSTG "B$FSTG"

enum { STICK_LAST = 3, STRIG_LAST = 7 };

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
