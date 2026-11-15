/* The built-ins of devices this machine has not: QBasic 1.1's answers. */
#include "session.h"
#include "unit.h"

/* UEVENT was a Syntax error: QBasic compiles each form, and running one is Advanced feature
   unavailable. */
TEST(uevent_is_an_advanced_feature)
{
    static const char *const lines[] = {
        "10 ON ERROR GOTO 90",
        "20 UEVENT ON: UEVENT OFF: UEVENT STOP: ON UEVENT GOSUB 80",
        "30 END",
        "80 RETURN",
        "90 PRINT ERR;: RESUME NEXT",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 73  73  73  73 ");
}

/* IOCTL and IOCTL$ were Syntax errors: QBasic's take an open file, else Bad file name or number,
   and its device takes no control string: Illegal function call. */
TEST(ioctl_is_an_illegal_function_call)
{
    static const char *const lines[] = {
        "10 ON ERROR GOTO 90",
        "20 s$ = IOCTL$(9): IOCTL 9, \"x\"",
        "30 OPEN \"IOCTL.TMP\" FOR OUTPUT AS #2: s$ = IOCTL$(2): IOCTL #2, \"x\": CLOSE #2: KILL "
        "\"IOCTL.TMP\"",
        "40 END",
        "90 PRINT ERR;: RESUME NEXT",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 52  52  5  5 ");
}

/* STICK, STRIG and PEN were Syntax errors, STRIG ON too: no joystick nor pen, nothing moves and
   nothing is pressed; PEN's character row and column are 1. Past QBasic's n, Illegal function
   call. */
TEST(joystick_and_pen_of_a_machine_without)
{
    static const char *const lines[] = {
        "10 ON ERROR GOTO 90",
        "20 STRIG ON: STRIG OFF",
        "30 FOR k = -1 TO 4: PRINT STICK(k);: NEXT: PRINT \"|\";",
        "40 FOR k = -1 TO 8: PRINT STRIG(k);: NEXT: PRINT \"|\";",
        "50 FOR k = -1 TO 10: PRINT PEN(k);: NEXT",
        "60 END",
        "90 PRINT \"e\"; ERR;: RESUME NEXT",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(
        S->cons.sbuf,
        S->cons.sblen,
        "e 5  0  0  0  0 e 5 |e 5  0  0  0  0  0  0  0  0 e 5 |e 5  0  0  0  0  0  0  1  1 "
        " 1  1 e 5 "
    );
}
