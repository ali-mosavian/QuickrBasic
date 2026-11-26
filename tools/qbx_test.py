#!/usr/bin/env python3
"""qbx_test.py: tools/qbx.py's glue hands each argument to the author's C function as its C type,
and refuses an interface it cannot glue. tools/extproto/run.sh runs the glue end to end."""

import sys
import shutil
import struct
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


def dll(imports: int = 0, machine: int = 0x14C, export: bytes = b"qx_init_t") -> bytes:
    """An i386 DLL as lld-link lays one out, based at 10000000h: one section at 1000h, 300h bytes
    stored of 2000h. Its word at 1000h points to its export (qx_init_t) at 1010h, relocated; its
    exports at 1100h, its relocations at 1200h."""
    base, va, opt = 0x10000000, 0x1000, 0x58
    sec = bytearray(0x300)
    struct.pack_into("<I", sec, 0, base + 0x1010)
    sec[0x10:0x14] = b"\x31\xc0\xc3\x90"
    struct.pack_into("<IIIIIIIIIII", sec, 0x100, 0, 0, 0, 0, 1, 1, 1, va + 0x140, va + 0x144, va + 0x148, 0)
    struct.pack_into("<IIH", sec, 0x140, va + 0x10, va + 0x150, 0)
    sec[0x150 : 0x151 + len(export)] = export + b"\0"
    struct.pack_into("<IIHH", sec, 0x200, va, 12, 0x3000, 0)
    head = bytearray(0x400)
    head[0:2] = b"MZ"
    struct.pack_into("<I", head, 0x3C, 0x40)
    head[0x40:0x44] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", head, 0x44, machine, 1, 0, 0, 0, 224, 0x2102)
    struct.pack_into("<H", head, opt, 0x10B)
    struct.pack_into("<I", head, opt + 28, base)
    struct.pack_into("<I", head, opt + 56, 0x3000)
    struct.pack_into("<I", head, opt + 92, 16)
    struct.pack_into("<II", head, opt + 96, va + 0x100, 0x60)
    struct.pack_into("<II", head, opt + 96 + 8, imports, 40 if imports else 0)
    struct.pack_into("<II", head, opt + 96 + 40, va + 0x200, 12)
    struct.pack_into("<8sIIIIIIHHI", head, opt + 224, b".data", 0x2000, va, 0x300, 0x400, 0, 0, 0, 0, 0)
    return bytes(head) + bytes(sec)


def coff_symbols(obj: bytes) -> tuple[set[str], set[str]]:
    """A COFF object's external symbols: those it defines, those it needs."""
    table, count = struct.unpack_from("<II", obj, 8)
    strings = table + 18 * count
    defined, needed, i = set(), set(), 0
    while i < count:
        raw, value, section, _, storage, aux = struct.unpack_from("<8sIhHBB", obj, table + 18 * i)
        if raw[:4] == b"\0\0\0\0":
            raw = obj[strings + struct.unpack_from("<I", raw, 4)[0] :]
        name = raw.split(b"\0")[0].decode()
        if storage == 2:  # external
            (defined if section > 0 or value else needed).add(name)
        i += 1 + aux
    return defined, needed


def refused(text: str, why: str) -> str | None:
    try:
        qbx.read(text)
    except qbx.QbxError as e:
        return None if why in str(e) else f"wrong reason: {e}"
    return "was accepted"


if __name__ == "__main__":
    bad = []
    own = [qbx.Part("t", "", qbx.read(FACE))]
    h, c = qbx.header("t", own), qbx.source("t", own)

    # each kind reaches the C function as its own type: a SHORT or BYTE cast from the cell's INT
    for want in (
        "qx_c->result.i = t_add(qx_c->arg[0].i, qx_c->arg[1].i);",
        "t_kinds((int16_t)qx_c->arg[0].i, (uint8_t)qx_c->arg[1].i, qx_c->arg[2].f, qx_c->arg[3].d, qx_c->arg[4].s);",
        "void t_kinds(int16_t s, uint8_t u, float f, double d, QxStr t);",
    ):
        if want not in c + h:
            bad.append(f"a value's kind not passed as its type: {want}")

    # by reference: a number's pointer, a record's struct, an array's QxArray; a string stays read-only
    for want in (
        "t_refs((int32_t *)qx_c->arg[0].ref, (float *)qx_c->arg[1].ref, (t_box *)qx_c->arg[2].ref, qx_c->arg[3].a);",
        "void t_refs(int32_t *n, float *f, t_box *b, const QxArray *all);",
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

    # a .qbx gives back its parts and slices, each slice at a multiple of 16 (the loaders' alignment)
    faces = {"draw": b"declare sub s cdecl ()\n", "pen": b"declare sub t cdecl ()\n"}
    data = qbx.pack(faces, {"macos": b"\xcf\xfa" * 9, "wasm32": b"\0asm"})
    faces_back, slices = qbx.unpack(data)
    table = [qbx.ENTRY.unpack_from(data, qbx.HEAD.size + i * qbx.ENTRY.size) for i in range(4)]
    if faces_back != faces or slices != {"macos": b"\xcf\xfa" * 9, "wasm32": b"\0asm"}:
        bad.append("a .qbx did not give back its parts and slices")
    if any(data[at + n : at + n + 1] != b"\x1a" for _, at, n in table[2:]):
        bad.append("a part not followed by 1Ah: the loader reads it from its offset to the next part")
    if any(at % 16 for _, at, _ in table[:2]) or data[:4] != b"QBX\x1a":
        bad.append(f"a slice not at a multiple of 16, or no magic: {table[:2]}")
    try:
        qbx.pack({}, {"amiga": b""})
        bad.append("a slice for no platform was packed")
    except qbx.QbxError:
        pass

    # pack --minor: the host's minor the .qbx needs, in its header (gfx.qbx's 1, as the Makefile packs it)
    with tempfile.TemporaryDirectory() as tmp:
        face, out = Path(tmp) / "t.bas", Path(tmp) / "t.qbx"
        face.write_text("declare sub s cdecl ()\n")
        qbx.main(["qbx.py", "pack", str(out), "--minor", "1", str(face), f"macos={face}"])
        if qbx.HEAD.unpack_from(out.read_bytes())[3] != 1 or qbx.unpack(out.read_bytes())[0] != {"": face.read_bytes()}:
            bad.append("pack --minor 1 wrote another minor, or took the flag for an interface")

    # a DOS slice is the DLL as an image at 0 (util/image.c): its sections where they lie, its words rebased
    img = qbx.image(dll())
    magic, stored, size, entry, nrelocs = qbx.IMAGE.unpack_from(img)
    words = img[qbx.IMAGE.size :]
    padded = (stored + 3) & ~3
    relocs = struct.unpack_from(f"<{nrelocs}I", words, padded)
    if (magic, size, entry, relocs) != (b"QXI\x01", 0x3000, 0x1010, (0x1000,)):
        bad.append(f"a DLL's image wrong: {(magic, size, entry, relocs)}")
    if struct.unpack_from("<I", words, 0x1000)[0] != 0x1010 or words[0x1010:0x1013] != b"\x31\xc0\xc3":
        bad.append("a DLL's image not rebased to 0, or its code not where it lies")
    if stored > 0x1300 or len(img) != qbx.IMAGE.size + padded + 4 * nrelocs:
        bad.append(f"a DLL's image stored its zeros: {stored} bytes")
    if qbx.unpack(qbx.pack({}, {"dos-i386": dll()}))[1]["dos-i386"] != img:
        bad.append("pack kept a DOS slice's DLL, not its image")
    for why, data in (("imports", dll(imports=0x1250)), ("x86-64", dll(machine=0x8664)), ("not a DLL", b"MZ")):
        try:
            qbx.image(data)
            bad.append(f"a DLL that {why} made an image")
        except qbx.QbxError:
            pass

    # a driver file: "QXD" 1, the ABI, its cards' IDs, then the image of its DLL, entered at accel_init
    data = qbx.driver(dll(export=b"accel_init"), [qbx.pci_id("121a:0001"), qbx.pci_id("121A:2")])
    if data[:16] != b"QXD\x01" + struct.pack("<III", qbx.ACCEL_ABI, 2, 0x121A0001):
        bad.append(f"a driver's header wrong: {data[:16].hex()}")
    if data[16:20] != struct.pack("<I", 0x121A0002) or data[20:] != qbx.image(dll(export=b"accel_init"), b"accel_init"):
        bad.append("a driver's second ID or its image wrong")
    for why, call in (
        ("exports no accel_init", lambda: qbx.driver(dll(), [1])),
        ("drives no card", lambda: qbx.driver(dll(export=b"accel_init"), [])),
        ("has an ID not in hex", lambda: qbx.pci_id("3dfx:0001")),
    ):
        try:
            call()
            bad.append(f"a driver that {why} was made")
        except qbx.QbxError:
            pass

    # natives named as the glue's own names compile: a SUB fail clashed with qx_fail
    clash = "".join(f"declare sub {n} cdecl ()\n" for n in ("fail", "module", "now", "entries", "host", "qx_init_c"))
    if (cc := shutil.which("clang") or shutil.which("cc")) is not None:
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "c.bas").write_text(clash, encoding="utf-8")
            qbx.glue("c", out, [out / "c.bas"])
            qx = Path(__file__).parents[1] / "src/qx"
            done = subprocess.run(
                [cc, "-std=c23", "-fsyntax-only", f"-I{qx}", f"-I{out}", str(out / "c_qx.c")],
                capture_output=True,
                text=True,
                check=False,
            )
            if done.returncode:
                bad.append(f"natives named as the glue's own did not compile: {done.stderr.strip()[:300]}")
    # a DOS slice links with no library: a double in it needed the Windows target's __fltused
    if cc is not None:
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "f.bas").write_text("declare function half# cdecl (byval d#)\n", encoding="utf-8")
            (out / "f.c").write_text(
                '#include "f_qx.h"\nvoid f_init(void) {}\nvoid f_start(void) {}\nvoid f_stop(void) {}\n'
                "double f_half(const double d) { return d / 2; }\n",
                encoding="utf-8",
            )
            qbx.glue("f", out, [out / "f.bas"])
            qx = Path(__file__).parents[1] / "src/qx"
            defined, needed = set(), set()
            for c in ("f.c", "f_qx.c"):
                done = subprocess.run(
                    [
                        cc,
                        "--target=i386-pc-win32",
                        "-march=i386",
                        "-ffreestanding",
                        "-std=c23",
                        "-O2",
                        f"-I{qx}",
                        f"-I{out}",
                        "-c",
                        str(out / c),
                        "-o",
                        str(out / f"{c}.obj"),
                    ],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                if done.returncode:
                    bad.append(f"a DOS slice's {c} did not compile: {done.stderr.strip()[:300]}")
                    break
                mine, theirs = coff_symbols((out / f"{c}.obj").read_bytes())
                defined |= mine
                needed |= theirs
            if missing := needed - defined:
                bad.append(f"a DOS slice needs what no library gives it: {sorted(missing)}")

    # QX_CALL is one convention on i386 whatever the build's -mregparm: the DOS core is built with 3 and
    # its cdecl calls passed the extension's arguments in registers; add% gave 0, a string crashed it
    if cc is not None:
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "f.c").write_text(
                '#include "qx.h"\nint32_t QX_CALL f(const int32_t a, const int32_t b) { return a - b; }\n',
                encoding="utf-8",
            )
            qx = Path(__file__).parents[1] / "src/qx"
            code = []
            for regparm in ("-mregparm=3", "-mregparm=0"):
                done = subprocess.run(
                    [
                        cc,
                        "--target=i386-pc-win32",
                        "-march=i386",
                        regparm,
                        "-O2",
                        "-ffreestanding",
                        f"-I{qx}",
                        "-S",
                        "-o",
                        "-",
                        str(out / "f.c"),
                    ],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                code.append(
                    [line for line in done.stdout.splitlines() if line.startswith("\t") and "." not in line[:3]]
                )
            if done.returncode or code[0] != code[1]:
                bad.append(f"QX_CALL changed with -mregparm: {code[0]} against {code[1]}")

    if refused('declare sub x cdecl alias "qx_init_x" ()', "qx_"):
        bad.append("a native aliased qx_... was accepted: the glue's names start so")

    # a native named as a hook is the hook's C name: refused, not a duplicate definition
    try:
        qbx.header("t", [qbx.Part("t", "", qbx.read("declare sub start cdecl ()"))])
        bad.append("a native named start glued: its C name t_start is the start hook's")
    except qbx.QbxError:
        pass

    # a package's parts: entries named as BASIC calls them, C names with the module and the part, so
    # open and free meet no C library's; a DECLARE's _ lines are one
    draw = 'const FLIPX = 1\nconst CANVAS = &H10000000\nconst LOW = -3\nconst NAME = "x"\n'
    draw += "type vec2\n    x as single\n    y as single\nend type\n"
    draw += "declare sub sprite cdecl (byval s&, _\n    byval x%, _ ' where\n    byval y%)\n"
    draw += "declare function open% cdecl (p as vec2)\n"
    pkg = [
        qbx.Part("gfx", "draw", qbx.read(draw)),
        qbx.Part("gfx", "surface", qbx.read("declare sub free cdecl (byval s&)")),
    ]
    h, c = qbx.header("gfx", pkg), qbx.source("gfx", pkg)
    for want in (
        "void gfx_draw_sprite(int32_t s, int32_t x, int32_t y);",
        "int32_t gfx_draw_open(gfx_draw_vec2 *p);",
        "void gfx_surface_free(int32_t s);",
        '{"draw.sprite", "III", qx_entry_0}',
        '{"surface.free", "I", qx_entry_2}',
        '{"draw.vec2", sizeof(gfx_draw_vec2)}',
        "    GFX_DRAW_FLIPX = 1,\n    GFX_DRAW_CANVAS = 268435456,\n    GFX_DRAW_LOW = -3,\n};",
    ):
        if want not in c + h:
            bad.append(f"a package's names wrong: {want}")
    own_name = qbx.part_names([Path("gfx/draw.bas")])
    if own_name != [""] or qbx.part_names([Path("a/Draw.bas"), Path("pen.bas")]) != ["draw", "pen"]:
        bad.append("one interface is not the module's own, or several not named by their files")

    if bad:
        sys.exit("FAIL " + "\nFAIL ".join(bad))
    print("ok")
