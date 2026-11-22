"""rtstatic_test.py: the runtime holds no static buffer.

    python3 tools/rtstatic_test.py NAMES_H SOURCE...   the DOS build's sources, compiled here
    python3 tools/rtstatic_test.py --objects OBJECT... the host build's objects (Mach-O or ELF)

A DOS source is compiled as DOS32's, to ELF for its symbols' sizes. A Mach-O symbol has no size: it is the distance
to the next symbol in its section, less the padding that symbol's alignment may need (a 256-byte array measured 260).
A variable over LIMIT bytes a 4-byte pointer (twice that at 8) in a writable data section fails. The graphics back
buffers (371 KB) and the sound tables (270 KB) were static once, and the DOS IDE did not start with 4 MB; the host's
graphics pages and sound logs were 5.6 MB."""

import os
import sys
import struct
import tempfile
import subprocess
from pathlib import Path
from itertools import pairwise
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parent.parent
CLANG = os.environ.get("CLANG", "clang")
LIMIT = 256  # bytes at a 4-byte pointer: a module's state, not a buffer
MOST_ALIGN = 16  # a data symbol's largest alignment, so the most padding before one is 15 bytes
QBRT = "src/qbrt/llrm/qb"
INCLUDES = ["src/qbrt/include", *(f"{QBRT}/{d}" for d in ("dos/m32", ".", "dev", "dev/dos/m32", "dev/dos"))]
FLAGS = [
    "--target=i386-unknown-linux-gnu",
    "-march=i386",
    "-mregparm=3",
    "-O2",
    "-ffreestanding",
    "-nostdlibinc",
    "-fno-asynchronous-unwind-tables",
    "-fno-stack-protector",
    "-w",
    "-D__cdecl=",
    "-DARENA_AT_ZERO=1",
    "-isystem",
    "src/lib/include",
    "-Isrc",
    "-Isrc/core",
    *(f"-I{d}" for d in INCLUDES),
    "-Isrc/qbrt/llrm/os/x86-m32",
]
ELF_SECTIONS = (".bss", ".data", ".tbss", ".tdata")
MACHO_SECTIONS = {"__data", "__bss", "__common", "__thread_data", "__thread_bss"}
SHN_COMMON = 0xFFF2
MH_MAGIC_64 = 0xFEEDFACF
LC_SEGMENT_64, LC_SYMTAB = 0x19, 0x2
N_STAB, N_TYPE, N_EXT, N_SECT = 0xE0, 0x0E, 0x01, 0x0E


def cstr(data: bytes, offset: int) -> str:
    return data[offset : data.index(b"\0", offset)].decode()


def padding(at: int) -> int:
    """The most padding before a symbol at `at`: its alignment, a power of two that divides it, less one."""
    return min(at & -at or MOST_ALIGN, MOST_ALIGN) - 1


def elf_statics(obj: bytes) -> list[tuple[str, int]]:
    """An ELF object's variables in .bss or .data, or common: name, bytes."""
    wide = obj[4] == 2  # ELFCLASS64
    if wide:
        shoff, shentsize, shnum, shstrndx = *struct.unpack_from("<Q", obj, 0x28), *struct.unpack_from("<HHH", obj, 0x3A)
        head, sym, symsize = "<IIQQQQIIQQ", "<IBBHQQ", 24
    else:
        shoff, shentsize, shnum, shstrndx = *struct.unpack_from("<I", obj, 0x20), *struct.unpack_from("<HHH", obj, 0x2E)
        head, sym, symsize = "<IIIIIIIIII", "<IIIBBH", 16
    heads = [struct.unpack_from(head, obj, shoff + i * shentsize) for i in range(shnum)]
    names = [cstr(obj, heads[shstrndx][4] + h[0]) for h in heads]
    symtab = next(h for h in heads if h[1] == 2)  # SHT_SYMTAB
    strtab = heads[symtab[6]][4]
    found = []
    for at in range(symtab[4], symtab[4] + symtab[5], symsize):
        if wide:
            name, info, _, shndx, _, size = struct.unpack_from(sym, obj, at)
        else:
            name, _, size, info, _, shndx = struct.unpack_from(sym, obj, at)
        data = shndx < len(names) and names[shndx].startswith(ELF_SECTIONS)
        if info & 0xF == 1 and (data or shndx == SHN_COMMON):  # STT_OBJECT
            found.append((cstr(obj, strtab + name), size))
    return found


def macho_statics(obj: bytes) -> list[tuple[str, int]]:
    """A Mach-O object's symbols in its writable data sections, each to the next or its section's end; common
    symbols, whose value is their size: name, bytes."""
    _, _, _, _, ncmds, _, _, _ = struct.unpack_from("<IiiIIIII", obj, 0)
    sections: list[tuple[str, int, int]] = []  # name, address, size; n_sect is 1 + the index
    symoff = nsyms = stroff = 0
    at = 32
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", obj, at)
        if cmd == LC_SEGMENT_64:
            nsects = struct.unpack_from("<I", obj, at + 64)[0]
            for i in range(nsects):
                sect = at + 72 + i * 80
                name = obj[sect : sect + 16].rstrip(b"\0").decode()
                addr, size = struct.unpack_from("<QQ", obj, sect + 32)
                sections.append((name, addr, size))
        elif cmd == LC_SYMTAB:
            symoff, nsyms, stroff, _ = struct.unpack_from("<IIII", obj, at + 8)
        at += cmdsize
    placed: dict[int, list[tuple[int, str]]] = {}  # section index: (address, name)
    found = []
    for i in range(nsyms):
        strx, kind, sect, _, value = struct.unpack_from("<IBBHQ", obj, symoff + i * 16)
        name = cstr(obj, stroff + strx)
        if kind & N_STAB:
            continue
        if kind & N_TYPE == 0 and kind & N_EXT and value:  # common
            found.append((name, value))
        elif kind & N_TYPE == N_SECT and sections[sect - 1][0] in MACHO_SECTIONS:
            placed.setdefault(sect - 1, []).append((value, name))
    for index, symbols in placed.items():
        _, addr, size = sections[index]
        starts = sorted({value for value, _ in symbols} | {addr + size})
        ends = dict(pairwise(starts))
        found += [
            (name, ends[value] - value - padding(ends[value]))
            for value, name in symbols
            if not name.startswith(("ltmp", "l_", "L"))
        ]
    return found


def statics(obj: bytes) -> tuple[list[tuple[str, int]], int]:
    """An object's variables, and the bytes of its pointer."""
    if struct.unpack_from("<I", obj, 0)[0] == MH_MAGIC_64:
        return macho_statics(obj), 8
    if obj[:4] == b"\x7fELF":
        return elf_statics(obj), 4 << (obj[4] == 2)
    raise ValueError("neither ELF nor 64-bit Mach-O")


def over(label: str, obj: Path) -> list[str]:
    found, pointer = statics(obj.read_bytes())
    return [f"{label}: {name} {size} bytes" for name, size in found if size > LIMIT * pointer // 4]


PROBE = """
__attribute__((used)) static int before;
__attribute__((used, aligned(4))) static char exact[LIMIT];
__attribute__((used)) static long after;
__attribute__((used)) static char big[LIMIT + 16];
__attribute__((used)) static char last[LIMIT];
"""


def probe(out: Path) -> None:
    """The measure on this machine's objects: an array of LIMIT bytes a 4-byte pointer passes, padded or not; one
    over it fails."""
    pointer = 8 if sys.maxsize > 2**32 else 4
    src, obj = out / "probe.c", out / "probe.o"
    src.write_text(PROBE.replace("LIMIT", str(LIMIT * pointer // 4)))
    subprocess.run([CLANG, "-O1", "-c", src, "-o", obj], check=True)
    found = {line.split()[1].lstrip("_") for line in over("probe", obj)}
    assert found == {"big"}, f"the probe measured {sorted(found)} over, not big alone: the measure is wrong"


def compiled(src: str, names_h: str, out: Path) -> list[str]:
    obj = out / (src.replace("/", "_") + ".o")
    std = "-std=gnu11" if src.startswith("src/qbrt/llrm/") else "-std=c23"  # llrm's C; the glue's
    subprocess.run([CLANG, *FLAGS, std, "-include", names_h, "-c", src, "-o", obj], cwd=ROOT, check=True)
    return over(src, obj)


def main() -> None:
    if sys.argv[1] == "--objects":
        objs = sys.argv[2:]
        with tempfile.TemporaryDirectory() as d:
            probe(Path(d))
        found = [line for obj in objs for line in over(obj, Path(obj))]
    else:
        names_h, objs = sys.argv[1], sys.argv[2:]
        with tempfile.TemporaryDirectory() as d, ThreadPoolExecutor() as pool:
            found = [line for lines in pool.map(lambda s: compiled(s, names_h, Path(d)), objs) for line in lines]
    if found:
        sys.exit("static buffers in the runtime:\n" + "\n".join(found))
    print(f"ok: {len(objs)} files")


if __name__ == "__main__":
    main()
