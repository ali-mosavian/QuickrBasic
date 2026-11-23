#!/usr/bin/env python3
"""qbx.py: native extensions' files (docs/native-extensions-design.md).

  qbx.py glue NAME OUTDIR INTERFACE...          the glue, from the BASIC interfaces
  qbx.py pack OUT INTERFACE... [TAG=SLICE]...  the .qbx: the interfaces and each platform's slice

One interface is the module's own. Several are a package's parts, each named by its file: gfx's
display.bas is gfx.display.

glue: numeric CONSTs become C's, GFX_DRAW_FLIPX; the TYPEs packed C structs; each native DECLARE
(one with CDECL, as the host binds it) a C prototype and a wrapper that lays out QxCall's arguments
for it. The author writes plain C functions, named with the module and part: gfx's draw.sprite is
gfx_draw_sprite, its TYPE scene.vertex gfx_scene_vertex; an ALIAS is the name as it is.
NAME_qx.h declares them and NAME_qx.c holds the wrappers, the entry table and qx_init_NAME. An
entry is named as BASIC calls it: draw.sprite, or sprite in a module's own interface.

pack: a .qbx is little-endian: "QBX" 1Ah; the format's version, the ABI's major and the minor it
needs, the slices' and the parts' counts (u16 each), 2 bytes 0; then a slice each, its tag (16
bytes, 0-padded), offset and bytes (u32 each); then a part each, its name ("" the module's own)
the same way. The slices follow, each at a multiple of 16, then the parts, each followed by 1Ah:
the loader reads one as a source file from its offset. A dos-i386 slice given as its DLL is packed
as the image util/image.c reads.
"""

import re
import sys
import struct
from pathlib import Path
from dataclasses import dataclass

C_KEYWORDS = frozenset(
    [
        "auto",
        "bool",
        "break",
        "case",
        "char",
        "const",
        "continue",
        "default",
        "do",
        "double",
        "else",
        "enum",
        "extern",
        "float",
        "for",
        "goto",
        "if",
        "inline",
        "int",
        "long",
        "register",
        "restrict",
        "return",
        "short",
        "signed",
        "sizeof",
        "static",
        "struct",
        "switch",
        "typedef",
        "union",
        "unsigned",
        "void",
        "volatile",
        "while",
    ]
)


class QbxError(Exception):
    """The interface cannot be glued: the line and why."""


@dataclass(frozen=True)
class Kind:
    """A BASIC type as the ABI passes it: its kinds letter, its C type, its QxArg member, its bytes."""

    letter: str
    c: str
    member: str
    bytes: int


SCALARS = {
    "integer": Kind("I", "int32_t", "i", 4),
    "long": Kind("I", "int32_t", "i", 4),
    "short": Kind("W", "int16_t", "i", 2),
    "byte": Kind("U", "uint8_t", "i", 1),
    "single": Kind("F", "float", "f", 4),
    "double": Kind("D", "double", "d", 8),
    "string": Kind("S", "QxStr", "s", 0),
}
SUFFIXES = {"%": "integer", "&": "long", "!": "single", "#": "double", "$": "string"}


@dataclass(frozen=True)
class Field:
    name: str
    c: str  # a record's "": its TYPE in record
    count: int  # STRING * n's bytes; 0 a scalar or a record
    bytes: int
    record: str = ""


@dataclass(frozen=True)
class Record:
    name: str
    fields: tuple[Field, ...]

    @property
    def bytes(self) -> int:
        return sum(f.bytes for f in self.fields)


@dataclass(frozen=True)
class Param:
    name: str
    kind: Kind | None  # None: a record
    record: str  # a record's TYPE
    byval: bool
    array: bool

    @property
    def letters(self) -> str:
        if self.array:
            return "R"
        if self.kind is None:
            return "T"
        if self.byval or self.kind.letter == "S":  # a string is passed as a QxStr either way
            return self.kind.letter
        return "&" + self.kind.letter


@dataclass(frozen=True)
class Native:
    name: str  # the BASIC name, lower case
    alias: str  # ALIAS's: the C function's and the entry's name as it is; "" none
    params: tuple[Param, ...]
    result: Kind | None  # a SUB's None


@dataclass(frozen=True)
class Interface:
    records: tuple[Record, ...]
    natives: tuple[Native, ...]
    consts: tuple[tuple[str, int], ...] = ()


@dataclass(frozen=True)
class Part:
    """An interface glued into a module: its part's name, "" the module's own."""

    module: str
    name: str
    face: Interface

    @property
    def prefix(self) -> str:
        return f"{self.module}_{self.name}" if self.name else self.module

    def entry(self, n: Native) -> str:
        return n.alias or (f"{self.name}.{n.name}" if self.name else n.name)

    def c(self, n: Native) -> str:
        return n.alias or f"{self.prefix}_{n.name.replace('.', '_')}"

    def record(self, name: str) -> str:
        return f"{self.prefix}_{name}"

    def const(self, name: str) -> str:
        return f"{self.prefix}_{name}".upper()


NAME = r"[a-z][a-z0-9_.]*"
DECLARE = re.compile(
    rf"declare\s+(sub|function)\s+({NAME})([%&!#$]?)\s*(cdecl\s*)?"
    rf'(?:alias\s*"([^"]*)"\s*)?(?:\((.*)\))?\s*(?:as\s+({NAME}))?$',
    re.IGNORECASE,
)
PARAM = re.compile(rf"(byval\s+)?({NAME})([%&!#$]?)\s*(\(\s*\))?\s*(?:as\s+({NAME}))?$", re.IGNORECASE)
FIELD = re.compile(rf"({NAME})\s+as\s+({NAME})(?:\s*\*\s*(\d+))?$", re.IGNORECASE)
DEFINED = re.compile(rf"(?:private\s+)?(?:sub|function)\s+({NAME})", re.IGNORECASE)
CONST = re.compile(rf"const\s+({NAME})\s*=\s*(-?)(?:&h([0-9a-f]+)|(\d+))[%&]?$", re.IGNORECASE)


def code(line: str) -> str:
    """line without its comment, its blanks trimmed. A ' inside a string is not one."""
    out, quoted = [], False
    for c in line:
        if c == '"':
            quoted = not quoted
        elif c == "'" and not quoted:
            break
        out.append(c)
    text = "".join(out).strip()
    return "" if re.match(r"rem(\s|$)", text, re.IGNORECASE) else text


def cname(name: str) -> str:
    """A BASIC name as C's: dots to underscores, and not a C keyword."""
    name = name.lower().replace(".", "_")
    return name + "_" if name in C_KEYWORDS else name


def kind_of(at: int, suffix: str, as_type: str | None, records: dict[str, Record]) -> tuple[Kind | None, str]:
    """A name's type, from its suffix or AS: a scalar's Kind, or None and a record's TYPE."""
    if suffix and as_type:
        raise QbxError(f"line {at}: a suffix and AS both")
    if suffix:
        return SCALARS[SUFFIXES[suffix]], ""
    if not as_type:
        raise QbxError(f"line {at}: no type: give a suffix or AS")
    t = as_type.lower()
    if t in SCALARS:
        return SCALARS[t], ""
    if t not in records:
        raise QbxError(f"line {at}: no TYPE {as_type} in the interface")
    return None, t


def field(at: int, text: str, records: dict[str, Record]) -> Field:
    m = FIELD.match(text)
    if not m:
        raise QbxError(f"line {at}: not a field the glue knows: {text}")
    name, t, count = m.group(1), m.group(2).lower(), m.group(3)
    if t == "string":
        if not count:
            raise QbxError(f"line {at}: a variable-length string in a TYPE")
        return Field(cname(name), "char", int(count), int(count))
    if count:
        raise QbxError(f"line {at}: * on a {t}")
    if t in SCALARS:
        k = SCALARS[t]
        return Field(cname(name), k.c, 0, k.bytes)
    if t not in records:
        raise QbxError(f"line {at}: no TYPE {t} before this one")
    return Field(cname(name), "", 0, records[t].bytes, t)


def params(at: int, text: str | None, records: dict[str, Record]) -> tuple[Param, ...]:
    out = []
    for one in filter(None, (p.strip() for p in (text or "").split(","))):
        m = PARAM.match(one)
        if not m:
            raise QbxError(f"line {at}: not a parameter the glue knows: {one}")
        kind, record = kind_of(at, m.group(3), m.group(5), records)
        byval, array = bool(m.group(1)), bool(m.group(4))
        if byval and (record or array):
            raise QbxError(f"line {at}: BYVAL on a record or an array")
        out.append(Param(cname(m.group(2)), kind, record, byval, array))
    return tuple(out)


def joined(text: str) -> list[tuple[int, str]]:
    """The text's lines without comments, each numbered by its first: a line ending in " _" goes
    on in the next."""
    out: list[tuple[int, str]] = []
    going = False
    for at, raw in enumerate(text.splitlines(), 1):
        line = code(raw)
        if going:
            out[-1] = (out[-1][0], f"{out[-1][1]} {line}")
        else:
            out.append((at, line))
        going = bool(re.search(r"(^|\s)_$", out[-1][1]))
        if going:
            out[-1] = (out[-1][0], out[-1][1][:-1].rstrip())
    return out


def read(text: str) -> Interface:
    """The interface's records and native procedures."""
    lines = joined(text)
    defined = {m.group(1).lower() for _, line in lines if (m := DEFINED.match(line))}
    records: dict[str, Record] = {}
    natives: list[Native] = []
    consts: list[tuple[str, int]] = []
    open_type: tuple[str, list[Field]] | None = None

    for at, line in lines:
        if open_type:
            if re.fullmatch(r"end\s+type", line, re.IGNORECASE):
                records[open_type[0]] = Record(open_type[0], tuple(open_type[1]))
                open_type = None
            elif line:
                open_type[1].append(field(at, line, records))
        elif m := CONST.match(line):
            sign, hexa, decimal = m.group(2), m.group(3), m.group(4)
            value = int(hexa, 16) if hexa else int(decimal)
            consts.append((m.group(1).lower(), -value if sign else value))
        elif m := re.fullmatch(rf"type\s+({NAME})", line, re.IGNORECASE):
            open_type = (m.group(1).lower(), [])
        elif m := DECLARE.match(line):
            what, name, suffix, cdecl, alias, plist, as_type = m.groups()
            if not cdecl:  # BASIC's own
                continue
            if name.lower() in defined:
                raise QbxError(f"line {at}: {name}: a CDECL DECLARE of the interface's own procedure")
            result = None
            if what.lower() == "function":
                result, record = kind_of(at, suffix, as_type, records)
                if record:
                    raise QbxError(f"line {at}: a record result")
            elif suffix or as_type:
                raise QbxError(f"line {at}: a SUB with a type")
            if (alias or "").lower().startswith("qx_"):
                raise QbxError(f"line {at}: {alias}: names starting qx_ are the glue's")
            natives.append(Native(name.lower(), alias or "", params(at, plist, records), result))
        elif line.lower().startswith("declare"):
            raise QbxError(f"line {at}: not a DECLARE the glue knows: {line}")

    if open_type:
        raise QbxError(f"TYPE {open_type[0]} without END TYPE")
    return Interface(tuple(records.values()), tuple(natives), tuple(consts))


def c_param(part: Part, p: Param) -> str:
    if p.array:
        return f"const QxArray *{p.name}"
    if p.kind is None:
        return f"{part.record(p.record)} *{p.name}"
    if p.byval or p.kind.letter == "S":  # a string is read-only, by reference or not
        return f"{p.kind.c} {p.name}"
    return f"{p.kind.c} *{p.name}"


def c_arg(part: Part, i: int, p: Param) -> str:
    a = f"qx_c->arg[{i}]"
    if p.array:
        return f"{a}.a"
    if p.kind is None:
        return f"({part.record(p.record)} *){a}.ref"
    if p.byval or p.kind.letter == "S":
        cast = f"({p.kind.c})" if p.kind.letter in "WU" else ""
        return f"{cast}{a}.{p.kind.member}"
    return f"({p.kind.c} *){a}.ref"


def prototype(part: Part, n: Native) -> str:
    ps = ", ".join(c_param(part, p) for p in n.params) or "void"
    return f"{n.result.c if n.result else 'void'} {part.c(n)}({ps});"


def natives(parts: list[Part]) -> list[tuple[Part, Native]]:
    return [(part, n) for part in parts for n in part.face.natives]


def header(module: str, parts: list[Part]) -> str:
    own = {f"{module}_{hook}" for hook in ("init", "start", "stop")}
    own |= {part.record(r.name) for part in parts for r in part.face.records}
    if taken := sorted(own & {part.c(n) for part, n in natives(parts)}):
        raise QbxError(f"{', '.join(taken)}: the glue's names for the hooks and the TYPEs")
    out = [
        f"/* {module}'s glue: generated by tools/qbx.py from its interface. Do not edit. */",
        f"#ifndef {module.upper()}_QX_H",
        f"#define {module.upper()}_QX_H",
        '#include "qx.h"',
        "",
        "extern const QxHost *qx_host;",
        "/* The running call's error: raised at its line once the entry returns. */",
        "void qx_fail(uint8_t code);",
        "/* The author's: at load, at each run's start, at its end. */",
        f"void {module}_init(void);",
        f"void {module}_start(void);",
        f"void {module}_stop(void);",
        "/* The host's call at load: the module. */",
        f"QX_EXPORT const QxModule *QX_CALL qx_init_{module}(const QxHost *host);",
    ]
    for part in parts:
        if part.face.consts:
            out += ["", "enum {", *(f"    {part.const(n)} = {v}," for n, v in part.face.consts), "};"]
        for r in part.face.records:
            out += ["", "typedef struct __attribute__((packed)) {"]
            for f in r.fields:
                c = f.c or part.record(f.record)
                out.append(f"    {c} {f.name}[{f.count}];" if f.count else f"    {c} {f.name};")
            out += [
                f"}} {part.record(r.name)};",
                f'static_assert(sizeof({part.record(r.name)}) == {r.bytes}, "TYPE {r.name}\'s bytes");',
            ]
    out += ["", *(prototype(part, n) for part, n in natives(parts)), "#endif", ""]
    return "\n".join(out)


def wrapper(part: Part, i: int, n: Native) -> str:
    call = f"{part.c(n)}({', '.join(c_arg(part, k, p) for k, p in enumerate(n.params))})"
    if not n.result:
        body = f"{call};"
    elif n.result.letter == "S":
        body = f"const QxStr qx_r = {call};\n    qx_host->result_str(qx_c, qx_r.ptr, qx_r.len);"
    else:
        body = f"qx_c->result.{n.result.member} = {call};"
    return f"static void QX_CALL qx_entry_{i}(QxCall *const qx_c)\n{{\n    qx_now = qx_c;\n    {body}\n}}\n"


def source(module: str, parts: list[Part]) -> str:
    entries = "".join(
        f'\n    {{"{part.entry(n)}", "{"".join(p.letters for p in n.params)}", qx_entry_{i}}},'
        for i, (part, n) in enumerate(natives(parts))
    )
    records = [(part, r) for part in parts for r in part.face.records]
    types = ", ".join(
        f'{{"{f"{part.name}." if part.name else ""}{r.name}", sizeof({part.record(r.name)})}}' for part, r in records
    )
    out = [
        f"/* {module}'s glue: generated by tools/qbx.py from its interface. Do not edit. */",
        f'#include "{module}_qx.h"',
        "",
        "const QxHost *qx_host;",
        "static QxCall *qx_now;",
        "#if defined(_MSC_VER)",
        "int _fltused = 0; /* the Windows target's mark of floating point, a C library's: a slice links none */",
        "#endif",
        "",
        "void qx_fail(const uint8_t code) { qx_now->error = code; }",
        "",
        *(wrapper(part, i, n) for i, (part, n) in enumerate(natives(parts))),
        f"static void QX_CALL qx_start(void) {{ {module}_start(); }}",
        f"static void QX_CALL qx_stop(void) {{ {module}_stop(); }}",
        "",
        f"static const QxEntry qx_entries[] = {{{entries}\n}};",
        f"static const QxType qx_types[] = {{{types}}};" if records else "",
        "static const QxModule qx_module = {",
        "    .size = sizeof qx_module,",
        "    .abi = QX_ABI,",
        "    .entries = qx_entries,",
        "    .nentries = sizeof qx_entries / sizeof *qx_entries,",
        *(["    .types = qx_types,", "    .ntypes = sizeof qx_types / sizeof *qx_types,"] if records else []),
        "    .start = qx_start,",
        "    .stop = qx_stop,",
        "};",
        "",
        f"QX_EXPORT const QxModule *QX_CALL qx_init_{module}(const QxHost *const host)",
        "{",
        "    qx_host = host;",
        f"    {module}_init();",
        "    return &qx_module;",
        "}",
        "",
    ]
    return "\n".join(out)


def part_names(interfaces: list[Path]) -> list[str]:
    """One interface is the module's own, "": several are its parts, each named by its file."""
    return [path.stem.lower() if len(interfaces) > 1 else "" for path in interfaces]


def glue(module: str, out: Path, interfaces: list[Path]) -> None:
    parts = [
        Part(module, name, read(path.read_text(encoding="utf-8")))
        for name, path in zip(part_names(interfaces), interfaces, strict=True)
    ]
    out.mkdir(parents=True, exist_ok=True)
    (out / f"{module}_qx.h").write_text(header(module, parts), encoding="utf-8")
    (out / f"{module}_qx.c").write_text(source(module, parts), encoding="utf-8")


MAGIC = b"QBX\x1a"
FORMAT, ABI, MINOR = 2, 1, 0
TAGS = frozenset(["dos-i386", "macos", "linux-x86_64", "wasm32"])
HEAD = struct.Struct("<4sHHHHH2x")
ENTRY = struct.Struct("<16sII")  # a slice's tag or a part's name, 0-padded; its offset and bytes
EOF_MARK = b"\x1a"  # ends a part: the loader reads a source file to it


def align(n: int) -> int:
    return (n + 15) & ~15


IMAGE = struct.Struct("<4sIIII")
I386, PE32, HIGHLOW, ABSOLUTE, DISCARDABLE = 0x14C, 0x10B, 3, 0, 0x02000000


def image(dll: bytes) -> bytes:
    """A DOS slice from an i386 DLL (lld-link /dll /noentry /nodefaultlib), as util/image.c reads
    it: its sections where they lie in memory, rebased to 0, its trailing zeros left to the loader;
    the offsets of its 32-bit words to relocate; its entry, qx_init_NAME. It must import nothing."""
    if dll[:2] != b"MZ" or len(dll) < 0x40:
        raise QbxError("dos-i386: not a DLL")
    pe = struct.unpack_from("<I", dll, 0x3C)[0]
    if dll[pe : pe + 4] != b"PE\0\0":
        raise QbxError("dos-i386: not a DLL")
    machine, nsections, _, _, _, optsize, _ = struct.unpack_from("<HHIIIHH", dll, pe + 4)
    opt = pe + 24
    if machine != I386 or struct.unpack_from("<H", dll, opt)[0] != PE32:
        raise QbxError("dos-i386: not an i386 DLL")
    base, size, ndirs = (struct.unpack_from("<I", dll, opt + at)[0] for at in (28, 56, 92))
    dirs = [struct.unpack_from("<II", dll, opt + 96 + 8 * i) for i in range(min(ndirs, 16))]
    if len(dirs) < 6:
        raise QbxError("dos-i386: a DLL without its exports and relocations")
    if dirs[1][1]:
        raise QbxError("dos-i386: the DLL imports: a slice imports nothing")
    mem, discarded = bytearray(size), []
    for i in range(nsections):
        _, vsize, va, rawsize, rawptr = struct.unpack_from("<8sIIII", dll, opt + optsize + 40 * i)
        n = min(vsize, rawsize) if vsize else rawsize
        mem[va : va + n] = dll[rawptr : rawptr + n]
        if struct.unpack_from("<I", dll, opt + optsize + 40 * i + 36)[0] & DISCARDABLE:
            discarded.append((va, n))  # .reloc: read here, not kept

    at, _ = dirs[0]
    nnames, funcs, names, ords = struct.unpack_from("<IIII", mem, at + 24)
    entry = None
    for i in range(nnames):
        name = mem[struct.unpack_from("<I", mem, names + 4 * i)[0] :].split(b"\0")[0]
        if name.lstrip(b"_").startswith(b"qx_init_"):
            entry = struct.unpack_from("<I", mem, funcs + 4 * struct.unpack_from("<H", mem, ords + 2 * i)[0])[0]
    if entry is None:
        raise QbxError("dos-i386: the DLL exports no qx_init_NAME")

    relocs, at, end = [], dirs[5][0], dirs[5][0] + dirs[5][1]
    while at < end:
        page, n = struct.unpack_from("<II", mem, at)
        for (word,) in struct.iter_unpack("<H", mem[at + 8 : at + n]):
            if word >> 12 == ABSOLUTE:
                continue
            if word >> 12 != HIGHLOW:
                raise QbxError(f"dos-i386: a relocation of type {word >> 12}")
            rva = page + (word & 0xFFF)
            struct.pack_into("<I", mem, rva, (struct.unpack_from("<I", mem, rva)[0] - base) & 0xFFFFFFFF)
            relocs.append(rva)
        at += n

    for va, n in discarded:
        mem[va : va + n] = bytes(n)
    stored = bytes(mem).rstrip(b"\0")
    padded = stored + bytes(-len(stored) % 4)
    return (
        IMAGE.pack(b"QXI\x01", len(stored), size, entry, len(relocs))
        + padded
        + struct.pack(f"<{len(relocs)}I", *relocs)
    )


def pack(parts: dict[str, bytes], slices: dict[str, bytes]) -> bytes:
    """The .qbx of its parts ("" the module's own interface) and its slices, by tag. A DOS slice
    given as its DLL is made its image."""
    for tag in slices:
        if tag not in TAGS:
            raise QbxError(f"no platform {tag}: one of {', '.join(sorted(TAGS))}")
    for name in parts:
        if len(name) >= ENTRY.size - 8:
            raise QbxError(f"a part's name of {ENTRY.size - 9} letters at most: {name}")
    if slices.get("dos-i386", b"")[:2] == b"MZ":
        slices = {**slices, "dos-i386": image(slices["dos-i386"])}
    at = align(HEAD.size + ENTRY.size * (len(slices) + len(parts)))
    table, body = [], bytearray()
    for tag, data in slices.items():
        table.append(ENTRY.pack(tag.encode(), at + len(body), len(data)))
        body.extend(data)
        body.extend(bytes(align(len(body)) - len(body)))
    for name, text in parts.items():
        table.append(ENTRY.pack(name.encode(), at + len(body), len(text)))
        body.extend(text + EOF_MARK)
    head = HEAD.pack(MAGIC, FORMAT, ABI, MINOR, len(slices), len(parts)) + b"".join(table)
    return head + bytes(at - len(head)) + bytes(body)


def unpack(data: bytes) -> tuple[dict[str, bytes], dict[str, bytes]]:
    """A .qbx's parts and slices: pack's inverse."""
    magic, _, _, _, nslices, nparts = HEAD.unpack_from(data)
    if magic != MAGIC:
        raise QbxError("not a .qbx")
    tables: list[dict[str, bytes]] = [{}, {}]
    for i in range(nslices + nparts):
        name, where, n = ENTRY.unpack_from(data, HEAD.size + i * ENTRY.size)
        tables[i >= nslices][name.rstrip(b"\0").decode()] = data[where : where + n]
    return tables[1], tables[0]


def main(argv: list[str]) -> int:
    try:
        if len(argv) >= 5 and argv[1] == "glue":
            glue(argv[2], Path(argv[3]), [Path(one) for one in argv[4:]])
            return 0
        if len(argv) >= 4 and argv[1] == "pack":
            faces = [Path(one) for one in argv[3:] if "=" not in one]
            parts = {name: face.read_bytes() for name, face in zip(part_names(faces), faces, strict=True)}
            pairs = [one.split("=", 1) for one in argv[3:] if "=" in one]
            slices = {tag: Path(file).read_bytes() for tag, file in pairs}
            Path(argv[2]).write_bytes(pack(parts, slices))
            return 0
    except QbxError as e:
        print(f"qbx.py {argv[1]}: {e}", file=sys.stderr)
        return 1
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
