#!/usr/bin/env python3
"""qbx_test.py: tools/qbx.py's glue hands each argument to the author's C function as its C type,
and refuses an interface it cannot glue. tools/extproto/run.sh runs the glue end to end."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import qbx

FACE = """' every kind the glue passes
type pt
    x as short
    y as short
end type
type box
    corner as pt
    name as string * 6
    scale as double
end type
declare function add% cdecl (byval a%, byval b&)
declare sub kinds cdecl (byval s as short, byval u as byte, byval f!, byval d#, byval t$)
declare sub refs cdecl (n%, f!, b as box, all() as box)
declare function shout$ cdecl alias "shout_text" (s$)
declare sub wrapped (n%)
declare function default% (byval n%)

sub wrapped (n%)
    n% = n% + 1
end sub
"""


def refused(text: str, why: str) -> str | None:
    try:
        qbx.read(text)
    except qbx.QbxError as e:
        return None if why in str(e) else f"wrong reason: {e}"
    return "was accepted"


if __name__ == "__main__":
    bad = []
    face = qbx.read(FACE)
    h, c = qbx.header("t", face), qbx.source("t", face)

    # each kind reaches the C function as its own type: a SHORT or BYTE cast from the cell's INT
    for want in (
        "c->result.i = add(c->arg[0].i, c->arg[1].i);",
        "kinds((int16_t)c->arg[0].i, (uint8_t)c->arg[1].i, c->arg[2].f, c->arg[3].d, c->arg[4].s);",
        "void kinds(int16_t s, uint8_t u, float f, double d, QxStr t);",
    ):
        if want not in c + h:
            bad.append(f"a value's kind not passed as its type: {want}")

    # by reference: a number's pointer, a record's struct, an array's QxArray; a string stays read-only
    for want in (
        "refs((int32_t *)c->arg[0].ref, (float *)c->arg[1].ref, (t_box *)c->arg[2].ref, c->arg[3].a);",
        "void refs(int32_t *n, float *f, t_box *b, const QxArray *all);",
        "QxStr shout_text(QxStr s);",
    ):
        if want not in c + h:
            bad.append(f"a reference not passed as a pointer: {want}")

    # the entries' kinds, as the link checks them against the DECLARE
    for want in ('{"add", "II", qx_add}', '{"kinds", "WUFDS", qx_kinds}', '{"refs", "&I&FTR", qx_refs}'):
        if want not in c:
            bad.append(f"an entry's kinds wrong: {want}")

    # a string result goes back through the host, an ALIAS names the C function and the entry
    if "qx_host->result_str(c, r.ptr, r.len);" not in c or '{"shout_text", "S", qx_shout_text}' not in c:
        bad.append("a string result not returned through result_str, or ALIAS not the entry's name")

    # a record is packed, its bytes asserted as BASIC lays it out; a record in a record by its struct
    for want in (
        "typedef struct __attribute__((packed)) {",
        "    t_pt corner;",
        "    char name[6];",
        'static_assert(sizeof(t_box) == 18, "TYPE box\'s bytes");',
        '{"box", sizeof(t_box)}',
    ):
        if want not in c + h:
            bad.append(f"a record's struct wrong: {want}")

    # a DECLARE whose procedure the interface defines is BASIC's, not native; one with no CDECL is
    if "wrapped" in c or "qx_default" not in c:
        bad.append("a BASIC procedure glued, or a DECLARE without CDECL left out")

    for text, why in (
        ("declare sub s cdecl (byval n)", "no type"),
        ("type r\n  n as short\nend type\ndeclare sub s cdecl (byval x as r)", "BYVAL on a record"),
        ("declare sub s cdecl (x as nothing)", "no TYPE nothing"),
        ("type r\n  n as short\nend type\ndeclare function f cdecl () as r", "a record result"),
        ("declare sub s% cdecl ()", "a SUB with a type"),
        ("type r\n  s as string\nend type", "a variable-length string"),
        ("type r\n  n as short", "without END TYPE"),
    ):
        if why := refused(text, why):
            bad.append(f"{text!r}: {why}")

    if bad:
        sys.exit("FAIL " + "\nFAIL ".join(bad))
    print("ok")
