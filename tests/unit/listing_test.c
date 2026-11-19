/* LIST's spelling of a stored body (comp/listing.h). */
#include "session.h"
#include "comp/lexer.h"
#include "comp/listing.h"
#include "unit.h"

static const char *const program[] = {
    "10 TYPE foo",
    "20 a AS INTEGER",
    "30 END TYPE",
    "40 DIM x(10) AS foo, r AS foo",
    "50 Total = 1",
    nullptr,
};

/* text lexed, then listed: in cons.sbuf. */
static void listed(Session *const S, const char *const text)
{
    uint8_t buf[100];
    strcpy((char *)buf, text);
    lexr_line(&S->lexr, buf, S->comp.cbuf);
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    list_body(&S->comp, S->comp.cbuf);
    S->cons.capture = 0;
}

/* An element's field close to its ')': it was x(0) . a. */
TEST(list_field_close)
{
    unit_store(S, program);
    listed(S, "x(0).a = 2: print x(0) . a");
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, "x(0).a = 2: PRINT x(0).a");
}

/* A name in the casing it was first written with: each was listed as typed there. */
TEST(list_name_as_first_written)
{
    unit_store(S, program);
    listed(S, "x(0).A = TOTAL: print r.A%");
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, "x(0).a = Total: PRINT r.A%");
}

/* A statement's keyword before '(' a word, not a function, and a '-' between two points close:
   PSET (1, 1) was PSET(1, 1), LINE's points (0, 0) - (2, 2). */
TEST(list_statement_before_paren)
{
    listed(S, "pset (1, 1): line (0, 0)-step(2, 2): x = cint(2)-(1)");
    CHECK_TEXT(
        S->cons.sbuf,
        S->cons.sblen,
        "PSET (1, 1): LINE (0, 0)-STEP (2, 2): x = CINT(2) - (1)"
    );
}

/* BYTE, DEFBYTE and CBYTE, ours, in capitals: they were names, listed as typed. */
TEST(list_byte_keywords)
{
    listed(S, "defbyte b: dim c as byte: c = cbyte(2.5)");
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, "DEFBYTE b: DIM c AS BYTE: c = CBYTE(2.5)");
}

/* A procedure's name where it is declared, a space before its '(' as QBasic's: SUB count (n%) was
   SUB count(n%); a call's stays close. */
TEST(list_proc_name_before_paren)
{
    listed(S, "declare sub hello (): sub count (n%): call count(1)");
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, "DECLARE SUB hello (): SUB count (n%): CALL count(1)");
    listed(S, "function f% (a): x = f%(1)");
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, "FUNCTION f% (a): x = f%(1)");
}
