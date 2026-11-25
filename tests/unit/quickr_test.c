/* Quickr's records as the VM lays them. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "session.h"
#include "comp/declare.h"
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

/* A byte or a ubyte takes 1 byte and a short or a ushort 2, in a field and in a fixed array's
   element. A byte took 2 and a ushort 4: this record 22 bytes, not 12. */
TEST(quickr_narrow_fields_take_their_size)
{
    CHECK(!build(
        S,
        "type P\n    a as byte\n    b as ubyte\n    c as short\n    d as ushort\n"
        "    xs as byte[2]\n    ys as ushort[2]\nend type\nvar p = P()\n"
    ));
    /* P first, its fixed arrays' boxes after */
    CHECK(S->decl.ntypes == 3 && decl_type(&S->decl, 1)->nfld == 6);
    CHECK(decl_type(&S->decl, 1)->size == 12);
}

/* A narrow field of a record in cells is a variable of its own, a frame's and a module's: N.LD and
   N.ST. Through the record's address, a short's += ran L.CELL, L.CELL, FL.LD, op, FL.ST. */
TEST(quickr_narrow_fields_are_variables)
{
    CHECK(!build(
        S,
        "type P\n    b as byte\n    s as short\n    u as ushort\nend type\n"
        "function f() as int\n    var p = P(b: 1, s: 2, u: 3)\n    p.s += p.b\n"
        "    return int(p.s) + int(p.u)\nend function\n"
        "var g = P(b: 1, s: 2, u: 3)\ng.u += 1\nprint f(); \" \"; g.u\n"
    ));
    CHECK(!unit_ops(S, O_FL_LD) && !unit_ops(S, O_FL_ST));
    CHECK(!unit_ops(S, O_V_CELL) && !unit_ops(S, O_L_CELL));
    CHECK(unit_ops(S, O_N_LD_L) == 4 && unit_ops(S, O_N_ST_L) == 4);
    CHECK(unit_ops(S, O_N_LD_V) == 2 && unit_ops(S, O_N_ST_V) == 4);
}

/* A record parameter's number field is read and written through its ref by one op, P.FL.LD or
   P.FL.ST, a narrow one's and a held record's too. Through R.ADDR, each took two. */
TEST(quickr_parameter_fields_by_ref)
{
    CHECK(!build(
        S,
        "type In\n    k as short\n    d as double\nend type\n"
        "type R\n    b as byte\n    n as int\n    i as In\nend type\n"
        "function bump(mut r as R)\n    r.n += int(r.b)\n    r.i.k += 1\n    r.i.d *= 2.0\nend "
        "function\n"
        "var a = R(b: 1, n: 2, i: In(k: 3, d: 4.0))\nbump(a)\nprint a.n; \" \"; a.i.k; \" \"; "
        "a.i.d\n"
    ));
    CHECK(!unit_ops(S, O_R_ADDR) && !unit_ops(S, O_FL_LD) && !unit_ops(S, O_FL_ST));
    CHECK(unit_ops(S, O_P_FL_LD) == 4 && unit_ops(S, O_P_FL_ST) == 3);
}

/* b = a copies a list only when a is used after it: the last use moves it, as it did before
   assignment copied, and copies nothing. */
TEST(quickr_assignment_moves_a_dead_source)
{
    CHECK(!build(S, "var a as int[]\na.push(1)\nvar b = a\nprint len(b)\n"));
    CHECK(unit_calls(S, K_AMOVE) == 1 && !unit_calls(S, K_ACOPY));
    CHECK(!build(S, "var a as int[]\na.push(1)\nvar b = a\nprint len(a); len(b)\n"));
    CHECK(!unit_calls(S, K_AMOVE) && unit_calls(S, K_ACOPY) == 1);
}

/* A method whose body is one return is inlined, a generic record's and an enum's too: p.dot(v)
   costs the field arithmetic. Called, it cost a PCALL, two refs and a frame. */
TEST(quickr_one_return_methods_inline)
{
    CHECK(!build(
        S,
        "type Vec\n    x as int\n    y as int\n"
        "    fn dot(self, o as Vec) as int\n        return self.x * o.x + self.y * o.y\n"
        "    end fn\nend type\n"
        "type Box[T]\n    v as T\n    fn get(self) as T\n        return self.v\n    end fn\nend "
        "type\n"
        "enum Mode\n    walk\n    run\n"
        "    fn speed(self) as int\n        return 1 if self = Mode.walk else 4\n    end fn\nend "
        "enum\n"
        "let p = Vec(1, 2)\nlet b = Box(3)\nlet m = Mode.run\n"
        "print p.dot(p); b.get(); m.speed()\n"
    ));
    CHECK(!unit_ops(S, O_PCALL));
}
