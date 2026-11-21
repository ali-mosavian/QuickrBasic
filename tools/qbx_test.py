#!/usr/bin/env python3
"""qbx_test.py: tools/qbx.py's glue hands each argument to the author's C function as its C type,
and refuses an interface it cannot glue. tools/extproto/run.sh runs the glue end to end."""

import sys
import shutil
import tempfile
import subprocess
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
        "qx_c->result.i = add(qx_c->arg[0].i, qx_c->arg[1].i);",
        "kinds((int16_t)qx_c->arg[0].i, (uint8_t)qx_c->arg[1].i, qx_c->arg[2].f, qx_c->arg[3].d, qx_c->arg[4].s);",
        "void kinds(int16_t s, uint8_t u, float f, double d, QxStr t);",
    ):
        if want not in c + h:
            bad.append(f"a value's kind not passed as its type: {want}")

    # by reference: a number's pointer, a record's struct, an array's QxArray; a string stays read-only
    for want in (
        "refs((int32_t *)qx_c->arg[0].ref, (float *)qx_c->arg[1].ref, (t_box *)qx_c->arg[2].ref, qx_c->arg[3].a);",
        "void refs(int32_t *n, float *f, t_box *b, const QxArray *all);",
        "QxStr shout_text(QxStr s);",
    ):
        if want not in c + h:
            bad.append(f"a reference not passed as a pointer: {want}")

    # the entries' kinds, as the link checks them against the DECLARE
    for want in ('{"add", "II", qx_entry_0}', '{"kinds", "WUFDS", qx_entry_1}', '{"refs", "&I&FTR", qx_entry_2}'):
        if want not in c:
            bad.append(f"an entry's kinds wrong: {want}")

    # a string result goes back through the host, an ALIAS names the C function and the entry
    if "qx_host->result_str(qx_c, qx_r.ptr, qx_r.len);" not in c or '{"shout_text", "S", qx_entry_3}' not in c:
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

    # native is CDECL, as the host binds it: glued without one, it was a call the host never made
    if "wrapped" in c or "default" in c:
        bad.append("a BASIC procedure glued, or a DECLARE without CDECL")

    for text, why in (
        ("declare sub s cdecl (byval n)", "no type"),
        ("type r\n  n as short\nend type\ndeclare sub s cdecl (byval x as r)", "BYVAL on a record"),
        ("declare sub s cdecl (x as nothing)", "no TYPE nothing"),
        ("type r\n  n as short\nend type\ndeclare function f cdecl () as r", "a record result"),
        ("declare sub s% cdecl ()", "a SUB with a type"),
        ("type r\n  s as string\nend type", "a variable-length string"),
        ("type r\n  n as short", "without END TYPE"),
        ("declare sub s cdecl ()\nsub s\nend sub", "a CDECL DECLARE of the interface's own procedure"),
    ):
        if why := refused(text, why):
            bad.append(f"{text!r}: {why}")

    # a .qbx gives back its interface and slices, each slice at a multiple of 16 (the loaders' alignment)
    data = qbx.pack(b"declare sub s cdecl ()\n", {"macos": b"\xcf\xfa" * 9, "wasm32": b"\0asm"})
    face_back, slices = qbx.unpack(data)
    offsets = [qbx.SLICE.unpack_from(data, qbx.HEAD.size + i * qbx.SLICE.size)[1] for i in range(2)]
    if face_back != b"declare sub s cdecl ()\n" or slices != {"macos": b"\xcf\xfa" * 9, "wasm32": b"\0asm"}:
        bad.append("a .qbx did not give back its interface and slices")
    if not data.endswith(b"declare sub s cdecl ()\n"):
        bad.append("the interface is not last: the loader reads it from its offset to the end")
    if any(at % 16 for at in offsets) or data[:4] != b"QBX\x1a":
        bad.append(f"a slice not at a multiple of 16, or no magic: {offsets}")
    try:
        qbx.pack(b"", {"amiga": b""})
        bad.append("a slice for no platform was packed")
    except qbx.QbxError:
        pass

    # natives named as the glue's own names compile: a SUB fail clashed with qx_fail, stop with the stop hook
    clash = "".join(
        f"declare sub {n} cdecl ()\n" for n in ("fail", "stop", "start", "module", "now", "entries", "host")
    )
    if (cc := shutil.which("clang") or shutil.which("cc")) is not None:
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "c.bas").write_text(clash, encoding="utf-8")
            qbx.glue(out / "c.bas", "c", out)
            qx = Path(__file__).parents[1] / "src/qx"
            done = subprocess.run(
                [cc, "-std=c23", "-fsyntax-only", f"-I{qx}", f"-I{out}", str(out / "c_qx.c")],
                capture_output=True,
                text=True,
                check=False,
            )
            if done.returncode:
                bad.append(f"natives named as the glue's own did not compile: {done.stderr.strip()[:300]}")
    if refused("declare sub qx_init_x cdecl ()", "qx_"):
        bad.append("a native named qx_... was accepted: the glue's names start so")

    if bad:
        sys.exit("FAIL " + "\nFAIL ".join(bad))
    print("ok")
