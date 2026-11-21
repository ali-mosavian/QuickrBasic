#!/usr/bin/env python3
"""qbx.py: native extensions' files (docs/native-extensions-design.md).

  qbx.py glue INTERFACE NAME OUTDIR         the glue, from the BASIC interface
  qbx.py pack OUT INTERFACE [TAG=SLICE]...  the .qbx: the interface and each platform's slice

glue: the interface's TYPEs become packed C structs, and each native DECLARE (one with CDECL, as
the host binds it) a C prototype and a wrapper that lays out QxCall's arguments for it. The author
writes plain C functions; NAME_qx.h declares them and NAME_qx.c holds the wrappers, the entry
table and qx_init_NAME.

pack: a .qbx is little-endian: "QBX" 1Ah; the format's version, the ABI's major and the minor it
needs, the slices' count (u16 each); the interface's offset and bytes (u32 each); then a slice
each, its tag (16 bytes, 0-padded), offset and bytes (u32 each). The slices follow, each at a
multiple of 16, then the interface, last: the loader reads it as a source file from its offset.
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
    name: str  # the C function's and the entry's: ALIAS's, else the BASIC name
    params: tuple[Param, ...]
    result: Kind | None  # a SUB's None


@dataclass(frozen=True)
class Interface:
    records: tuple[Record, ...]
    natives: tuple[Native, ...]


NAME = r"[a-z][a-z0-9_.]*"
DECLARE = re.compile(
    rf"declare\s+(sub|function)\s+({NAME})([%&!#$]?)\s*(cdecl\s*)?"
    rf'(?:alias\s*"([^"]*)"\s*)?(?:\((.*)\))?\s*(?:as\s+({NAME}))?$',
    re.IGNORECASE,
)
PARAM = re.compile(rf"(byval\s+)?({NAME})([%&!#$]?)\s*(\(\s*\))?\s*(?:as\s+({NAME}))?$", re.IGNORECASE)
FIELD = re.compile(rf"({NAME})\s+as\s+({NAME})(?:\s*\*\s*(\d+))?$", re.IGNORECASE)
DEFINED = re.compile(rf"(?:private\s+)?(?:sub|function)\s+({NAME})", re.IGNORECASE)


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


def read(text: str) -> Interface:
    """The interface's records and native procedures."""
    lines = [code(line) for line in text.splitlines()]
    defined = {m.group(1).lower() for line in lines if (m := DEFINED.match(line))}
    records: dict[str, Record] = {}
    natives: list[Native] = []
    open_type: tuple[str, list[Field]] | None = None

    for at, line in enumerate(lines, 1):
        if open_type:
            if re.fullmatch(r"end\s+type", line, re.IGNORECASE):
                records[open_type[0]] = Record(open_type[0], tuple(open_type[1]))
                open_type = None
            elif line:
                open_type[1].append(field(at, line, records))
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
            native = alias or cname(name)
            if native.lower().startswith("qx_"):
                raise QbxError(f"line {at}: {native}: names starting qx_ are the glue's")
            natives.append(Native(native, params(at, plist, records), result))
        elif line.lower().startswith("declare"):
            raise QbxError(f"line {at}: not a DECLARE the glue knows: {line}")

    if open_type:
        raise QbxError(f"TYPE {open_type[0]} without END TYPE")
    return Interface(tuple(records.values()), tuple(natives))


def c_param(module: str, p: Param) -> str:
    if p.array:
        return f"const QxArray *{p.name}"
    if p.kind is None:
        return f"{module}_{p.record} *{p.name}"
    if p.byval or p.kind.letter == "S":  # a string is read-only, by reference or not
        return f"{p.kind.c} {p.name}"
    return f"{p.kind.c} *{p.name}"


def c_arg(module: str, i: int, p: Param) -> str:
    a = f"qx_c->arg[{i}]"
    if p.array:
        return f"{a}.a"
    if p.kind is None:
        return f"({module}_{p.record} *){a}.ref"
    if p.byval or p.kind.letter == "S":
        cast = f"({p.kind.c})" if p.kind.letter in "WU" else ""
        return f"{cast}{a}.{p.kind.member}"
    return f"({p.kind.c} *){a}.ref"


def prototype(module: str, n: Native) -> str:
    ps = ", ".join(c_param(module, p) for p in n.params) or "void"
    return f"{n.result.c if n.result else 'void'} {n.name}({ps});"


def header(module: str, face: Interface) -> str:
    own = {f"{module}_{hook}" for hook in ("init", "start", "stop")} | {f"{module}_{r.name}" for r in face.records}
    if taken := sorted(own & {n.name for n in face.natives}):
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
    ]
    for r in face.records:
        out += ["", "typedef struct __attribute__((packed)) {"]
        for f in r.fields:
            c = f.c or f"{module}_{f.record}"
            out.append(f"    {c} {f.name}[{f.count}];" if f.count else f"    {c} {f.name};")
        out += [
            f"}} {module}_{r.name};",
            f'static_assert(sizeof({module}_{r.name}) == {r.bytes}, "TYPE {r.name}\'s bytes");',
        ]
    out += ["", *(prototype(module, n) for n in face.natives), "#endif", ""]
    return "\n".join(out)


def wrapper(module: str, i: int, n: Native) -> str:
    call = f"{n.name}({', '.join(c_arg(module, i, p) for i, p in enumerate(n.params))})"
    if not n.result:
        body = f"{call};"
    elif n.result.letter == "S":
        body = f"const QxStr qx_r = {call};\n    qx_host->result_str(qx_c, qx_r.ptr, qx_r.len);"
    else:
        body = f"qx_c->result.{n.result.member} = {call};"
    return f"static void QX_CALL qx_entry_{i}(QxCall *const qx_c)\n{{\n    qx_now = qx_c;\n    {body}\n}}\n"


def source(module: str, face: Interface) -> str:
    entries = "".join(
        f'\n    {{"{n.name}", "{"".join(p.letters for p in n.params)}", qx_entry_{i}}},'
        for i, n in enumerate(face.natives)
    )
    types = ", ".join(f'{{"{r.name}", sizeof({module}_{r.name})}}' for r in face.records)
    out = [
        f"/* {module}'s glue: generated by tools/qbx.py from its interface. Do not edit. */",
        f'#include "{module}_qx.h"',
        "",
        "const QxHost *qx_host;",
        "static QxCall *qx_now;",
        "",
        "void qx_fail(const uint8_t code) { qx_now->error = code; }",
        "",
        *(wrapper(module, i, n) for i, n in enumerate(face.natives)),
        f"static void QX_CALL qx_start(void) {{ {module}_start(); }}",
        f"static void QX_CALL qx_stop(void) {{ {module}_stop(); }}",
        "",
        f"static const QxEntry qx_entries[] = {{{entries}\n}};",
        f"static const QxType qx_types[] = {{{types}}};" if face.records else "",
        "static const QxModule qx_module = {",
        "    .size = sizeof qx_module,",
        "    .abi = QX_ABI,",
        "    .entries = qx_entries,",
        "    .nentries = sizeof qx_entries / sizeof *qx_entries,",
        *(["    .types = qx_types,", "    .ntypes = sizeof qx_types / sizeof *qx_types,"] if face.records else []),
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


def glue(interface: Path, module: str, out: Path) -> None:
    face = read(interface.read_text(encoding="utf-8"))
    out.mkdir(parents=True, exist_ok=True)
    (out / f"{module}_qx.h").write_text(header(module, face), encoding="utf-8")
    (out / f"{module}_qx.c").write_text(source(module, face), encoding="utf-8")


MAGIC = b"QBX\x1a"
FORMAT, ABI, MINOR = 1, 1, 0
TAGS = frozenset(["dos-i386", "macos", "linux-x86_64", "wasm32"])
HEAD = struct.Struct("<4sHHHHII")
SLICE = struct.Struct("<16sII")


def align(n: int) -> int:
    return (n + 15) & ~15


def pack(interface: bytes, slices: dict[str, bytes]) -> bytes:
    """The .qbx of an interface and its slices, by tag: the interface last, so the loader reads it
    as a source file from its offset to the end."""
    for tag in slices:
        if tag not in TAGS:
            raise QbxError(f"no platform {tag}: one of {', '.join(sorted(TAGS))}")
    at = align(HEAD.size + SLICE.size * len(slices))
    table, body = [], bytearray()
    for tag, data in slices.items():
        table.append(SLICE.pack(tag.encode(), at + len(body), len(data)))
        body.extend(data)
        body.extend(bytes(align(len(body)) - len(body)))
    head = HEAD.pack(MAGIC, FORMAT, ABI, MINOR, len(slices), at + len(body), len(interface)) + b"".join(table)
    return head + bytes(at - len(head)) + bytes(body) + interface


def unpack(data: bytes) -> tuple[bytes, dict[str, bytes]]:
    """A .qbx's interface and slices: pack's inverse."""
    magic, _, _, _, n, at, size = HEAD.unpack_from(data)
    if magic != MAGIC:
        raise QbxError("not a .qbx")
    slices = {}
    for i in range(n):
        tag, where, bytes_ = SLICE.unpack_from(data, HEAD.size + i * SLICE.size)
        slices[tag.rstrip(b"\0").decode()] = data[where : where + bytes_]
    return data[at : at + size], slices


def main(argv: list[str]) -> int:
    try:
        if len(argv) == 5 and argv[1] == "glue":
            glue(Path(argv[2]), argv[3], Path(argv[4]))
            return 0
        if len(argv) >= 4 and argv[1] == "pack":
            pairs = [one.split("=", 1) for one in argv[4:]]
            if any(len(pair) != 2 for pair in pairs):
                raise QbxError("a slice is TAG=FILE")
            slices = {tag: Path(file).read_bytes() for tag, file in pairs}
            Path(argv[2]).write_bytes(pack(Path(argv[3]).read_bytes(), slices))
            return 0
    except QbxError as e:
        print(f"qbx.py {argv[1]}: {e}", file=sys.stderr)
        return 1
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
