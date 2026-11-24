/* Quickr's records as the VM lays them. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "session.h"
#include "comp/declare.h"
#include "comp/link.h"
#include "qr/qr.h"
#include "unit.h"

/* src built as a Quickr file, the dialect's line first: its compile errors' count. */
static uint32_t build(Session *const S, const char *const src)
{
    char path[] = "/tmp/qb_unit_XXXXXX.bas";
    FILE *const f = fdopen(mkstemps(path, 4), "wb");
    CHECK(f && fputs("' OPTION DIALECT QUICKR\n", f) >= 0 && fputs(src, f) >= 0);
    fclose(f);
    session_new(S);
    snprintf(S->repl.file, sizeof S->repl.file, "%s", path);
    const uint32_t errors = qr_build(S);
    remove(path);
    return errors;
}

/* The ops op in the program built, as linked. */
static uint32_t ops(const Session *const S, const uint8_t op)
{
    uint32_t n = 0;

    for (uint32_t l = PROGBASE, len; (len = prog_line(&S->prog, l)->len); l += len) {
        const uint8_t *b = at(S->allo.cache + prog_line(&S->prog, l)->code);

        for (; *b != O_EOL && *b != O_EOL_TO; b += link_opsize(b))
            n += *b == op;
    }

    return n;
}

/* A byte or a ubyte takes 1 byte and a short or a ushort 2, in a field and in a fixed array's
   element. A byte took 2 and a ushort 4: this record 22 bytes, not 12. */
TEST(quickr_narrow_fields_take_their_size)
{
    CHECK(!build(
        S,
        "type P\n    a as byte\n    b as ubyte\n    c as short\n    d as ushort\n"
        "    xs as byte[2]\n    ys as ushort[2]\nend type\nvar p = P()\n"
    ));
    CHECK(S->decl.ntypes == 1);
    CHECK(decl_type(&S->decl, 1)->size == 12);
}

/* A narrow field of a record in cells is a variable of its own, a frame's and a module's: N.LD and
   N.ST. Through the record's address, a short's += ran L.CELL, L.CELL, FL.LD, op, FL.ST. */
TEST(quickr_narrow_fields_are_variables)
{
    CHECK(!build(
        S,
        "type P\n    b as byte\n    s as short\n    u as ushort\nend type\n"
        "fn f() as int\n    var p = P(b: 1, s: 2, u: 3)\n    p.s += p.b\n"
        "    return int(p.s) + int(p.u)\nend fn\n"
        "var g = P(b: 1, s: 2, u: 3)\ng.u += 1\nprint(f(), g.u)\n"
    ));
    CHECK(!ops(S, O_FL_LD) && !ops(S, O_FL_ST) && !ops(S, O_V_CELL) && !ops(S, O_L_CELL));
    CHECK(ops(S, O_N_LD_L) == 4 && ops(S, O_N_ST_L) == 4);
    CHECK(ops(S, O_N_LD_V) == 2 && ops(S, O_N_ST_V) == 4);
}
