/* The built-ins of devices this machine has not: QBasic 1.1's answers. */
#include "session.h"
#include "unit.h"
#include "plat/host/host.h"
#include <stdio.h>

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

/* STICK, STRIG and PEN were Syntax errors, STRIG ON too: no joystick nor mouse, nothing moves and
   nothing is pressed; PEN's character row and column are 1. Past QBasic's n (PEN's: 19, the right
   button's 10 to 19), Illegal function call. */
TEST(joystick_and_pen_of_a_machine_without)
{
    static const char *const lines[] = {
        "10 ON ERROR GOTO 90",
        "20 STRIG ON: STRIG OFF",
        "30 FOR k = -1 TO 4: PRINT STICK(k);: NEXT: PRINT \"|\";",
        "40 FOR k = -1 TO 8: PRINT STRIG(k);: NEXT: PRINT \"|\";",
        "50 FOR k = -1 TO 20: PRINT PEN(k);: NEXT",
        "60 END",
        "90 PRINT \"e\"; ERR;: RESUME NEXT",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(
        S->cons.sbuf,
        S->cons.sblen,
        "e 5  0  0  0  0 e 5 |e 5  0  0  0  0  0  0  0  0 e 5 |e 5  0  0  0  0  0  0  1  1 "
        " 1  1  0  0  0  0  0  0  1  1  1  1 e 5 "
    );
}

void vdev_pen(Session *S);

static int32_t pen(Session *const S, const int32_t n)
{
    exec_pushi(&S->vm, n);
    vdev_pen(S);
    return exec_popi(&S->vm);
}

/* PEN was a pen nobody has: it is the mouse, the left button its switch, PEN(n + 10) the right's.
   A click while the editor had the mouse is no press of the program's; a click between two PENs is
   one, where it was, though the button is up again. */
TEST(pen_is_the_mouse)
{
    static const char *const lines[] = {
        "10 PRINT PEN(0); PEN(3); PEN(4); PEN(5); PEN(8); PEN(9); PEN(13);",
        nullptr,
    };

    host_mouse_on();
    host_mouse_set(2, 3, 24, 16, 1);
    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 0 -1  24  16  3  4  0 ");

    host_mouse_set(2, 3, 24, 16, 0);
    host_mouse_set(12, 39, 319, 99, 2);
    host_mouse_set(5, 10, 80, 40, 0);
    host_mouse_set(5, 10, 80, 40, 1);
    host_mouse_set(5, 10, 80, 40, 0);
    CHECK(pen(S, 3) == 0);
    CHECK(pen(S, 0) == -1 && pen(S, 0) == 0);
    CHECK(pen(S, 1) == 80 && pen(S, 2) == 40 && pen(S, 6) == 6 && pen(S, 7) == 11);
    CHECK(pen(S, 10) == -1 && pen(S, 10) == 0);
    CHECK(pen(S, 11) == 319 && pen(S, 12) == 99 && pen(S, 16) == 13 && pen(S, 17) == 40);
    host_mouse_set(0, 0, 0, 0, 0);
}

void headless_set_printer_ready(int yes);

/* What a program run uncaptured (its LPRINTs the printer's) wrote to LPOS.TMP. */
static void printed(
    Session *const S,
    const char *const *const lines,
    char *const out,
    const size_t cap
)
{
    unit_run_shown(S, lines);
    FILE *const f = fopen("LPOS.TMP", "rb");
    const size_t n = f ? fread(out, 1, cap - 1, f) : 0;
    out[n] = 0;

    if (f)
        fclose(f);

    remove("LPOS.TMP");
    remove("LPT1.OUT");
}

/* LPRINT, WIDTH LPRINT and LPOS were Syntax errors. The printer's column as QBasic 1.1 counts it
   with a printer: zones, USING, TAB and SPC as the screen's, WIDTH's wrap, LPOS(1) LPT1's;
   without one, Device unavailable. */
TEST(lprint_counts_the_printers_column)
{
    static const char *const lines[] = {
        "10 ON ERROR GOTO 90: OPEN \"LPOS.TMP\" FOR OUTPUT AS #9",
        "20 PRINT #9, LPOS(0);: LPRINT \"abc\";: PRINT #9, LPOS(0); LPOS(1); LPOS(2); LPOS(3);",
        "30 LPRINT \"x\", \"y\";: PRINT #9, LPOS(0);: LPRINT USING \"##.#\"; 2.25;: PRINT #9, "
        "LPOS(0);",
        "40 LPRINT: PRINT #9, LPOS(0);: WIDTH LPRINT 10: LPRINT STRING$(12, \"z\");: PRINT #9, "
        "LPOS(0);",
        "50 LPRINT TAB(5); \"q\";: PRINT #9, LPOS(0);: LPRINT SPC(3); \"q\";: PRINT #9, LPOS(0);",
        "60 OPEN \"LPT1:\" FOR OUTPUT AS #3: PRINT #3, \"ab\";: PRINT #9, LPOS(0);: CLOSE #3",
        "70 WIDTH LPRINT 0: WIDTH LPRINT 256: PRINT #9, LPOS(4);",
        "80 CLOSE #9",
        "85 END",
        "90 PRINT #9, \"e\"; ERR;: RESUME NEXT",
        nullptr,
    };

    char out[200];

    headless_set_printer_ready(1);
    printed(S, lines, out, sizeof out);
    CHECK(!strcmp(out, " 1  4  4  1  1  16  20  1  3  6  10  3 e 5 e 5 e 5 "));

    static const char *const none[] = {
        "10 ON ERROR GOTO 90: OPEN \"LPOS.TMP\" FOR OUTPUT AS #9",
        "20 LPRINT \"x\": OPEN \"LPT1:\" FOR OUTPUT AS #3",
        "30 CLOSE #9",
        "35 END",
        "90 PRINT #9, ERR;: RESUME NEXT",
        nullptr,
    };

    headless_set_printer_ready(0);
    printed(S, none, out, sizeof out);
    CHECK(!strcmp(out, " 68  68 "));
}

/* VARPTR$ was a Syntax error: QBasic's type byte, then the address, 4 bytes here and 2 there; a
   LONG's, which DRAW takes not: Illegal function call. */
TEST(varptr_string_is_the_type_and_address)
{
    static const char *const lines[] = {
        "10 ON ERROR GOTO 90",
        "20 a% = 1: b$ = \"x\": c# = 1: d& = 1: e! = 1",
        "30 PRINT LEN(VARPTR$(a%)); ASC(VARPTR$(a%)); ASC(VARPTR$(b$)); ASC(VARPTR$(c#));",
        "40 PRINT ASC(VARPTR$(d&)); ASC(VARPTR$(e!));",
        "50 SCREEN 13: DRAW \"R=\" + VARPTR$(d&)",
        "60 END",
        "90 PRINT \"e\"; ERR;: RESUME NEXT",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 5  2  3  8  20  4 e 5 ");
}

/* Port 60h read FFh on a Mac: a game that reads held keys there fell back to INKEY$'s repeats, and a key held
   was forgotten as a second went down. A byte a look, in order, the last one kept. */
TEST(port_60h_is_the_keys_down_and_up)
{
    static const char *const lines[] = {
        "10 PRINT INP(&H60); INP(&H60); INP(&H60); INP(&H60);",
        nullptr,
    };

    host_scan_push(0x4D);
    host_scan_push(0x39);
    host_scan_push(0xCD);
    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 77  57  205  205 ");
}
