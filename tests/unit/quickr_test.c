/* Quickr's records as the VM lays them. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "session.h"
#include "comp/declare.h"
#include "qr/qr.h"
#include "unit.h"

/* src built as a .qr file: its compile errors' count. */
static uint32_t build(Session *const S, const char *const src)
{
    char path[] = "/tmp/qb_unit_XXXXXX.qr";
    FILE *const f = fdopen(mkstemps(path, 3), "wb");
    CHECK(f && fputs(src, f) >= 0);
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
    CHECK(S->decl.ntypes == 1);
    CHECK(decl_type(&S->decl, 1)->size == 12);
}
