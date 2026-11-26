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
    lexr_line(&S->lexr, buf);
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
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, "x(0).a = 2: print x(0).a");
}

/* A name in the casing it was first written with: each was listed as typed there. */
TEST(list_name_as_first_written)
{
    unit_store(S, program);
    listed(S, "x(0).A = TOTAL: print r.A%");
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, "x(0).a = Total: print r.A%");
}

/* A statement's keyword before '(' a word, not a function, and a '-' between two points close:
   PSET (1, 1) was PSET(1, 1), LINE's points (0, 0) - (2, 2). */
TEST(list_statement_before_paren)
{
    listed(S, "pset (1, 1): line (0, 0)-step(2, 2): x = cint(2)-(1)");
    CHECK_TEXT(
        S->cons.sbuf,
        S->cons.sblen,
        "pset (1, 1): line (0, 0)-step (2, 2): x = cint(2) - (1)"
    );
}

/* a(:), ours, an array whole by its elements, listed as typed, close to its name. */
TEST(list_whole_array)
{
    listed(S, "x(:) = y (:): m.a(:)=n.b(:)");
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, "x(:) = y(:): m.a(:) = n.b(:)");
}

/* BYTE, DEFBYTE and CBYTE, ours, keywords: they were names, listed as typed. */
TEST(list_byte_keywords)
{
    listed(S, "DefByte b: Dim c As Byte: c = CByte(2.5)");
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, "defbyte b: dim c as byte: c = cbyte(2.5)");
}

/* A procedure's name where it is declared, a space before its '(' as QBasic's: SUB count (n%) was
   SUB count(n%); a call's stays close. */
TEST(list_proc_name_before_paren)
{
    listed(S, "declare sub hello (): sub count (n%): call count(1)");
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, "declare sub hello (): sub count (n%): call count(1)");
    listed(S, "function f% (a): x = f%(1)");
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, "function f% (a): x = f%(1)");
}

/* Keywords lower case, however typed; the tables hold them in capitals. */
TEST(list_keywords_lower_case)
{
    listed(S, "PRINT x MOD 2: Line Input a$: IF x THEN GOTO 10 ELSE END");
    CHECK_TEXT(
        S->cons.sbuf,
        S->cons.sblen,
        "print x mod 2: line input a$: if x then goto 10 else end"
    );
}

/* CONTINUE, ours, a keyword only at a statement's start before FOR, DO or WHILE; elsewhere a
   name, listed as first written. */
TEST(list_continue_keyword)
{
    listed(S, "IF x THEN Continue For ELSE CONTINUE DO: Continue = 1: print CONTINUE");
    CHECK_TEXT(
        S->cons.sbuf,
        S->cons.sblen,
        "if x then continue for else continue do: Continue = 1: print Continue"
    );
}

/* '_' in a name, ours: my_var was my _ var, do_it the keyword DO and _ it. */
TEST(list_underscore_names)
{
    listed(S, "My_Var = 1: print MY_VAR: gosub do_it");
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, "My_Var = 1: print My_Var: gosub do_it");
}
