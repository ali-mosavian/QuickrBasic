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
