"""rtstatic_test.py: the runtime holds no static buffer: python3 tools/rtstatic_test.py NAMES_H SOURCE...

Each runtime source (src/qbrt, as the DOS build has it) is compiled as DOS32's, to ELF for its symbols' sizes; a
variable over LIMIT bytes in .bss or .data fails. The graphics back buffers (371 KB) and the sound tables (270 KB)
were static once, and the DOS IDE did not start with 4 MB."""

import os
import sys
import struct
import tempfile
import subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parent.parent
CLANG = os.environ.get("CLANG", "clang")
LIMIT = 256  # bytes: a module's state, not a buffer
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
SECTIONS = (".bss", ".data")


def statics(obj: bytes) -> list[tuple[str, int]]:
    """An ELF32 object's variables in .bss or .data: name, bytes."""
    shoff, _, _, _, _, shentsize, shnum, shstrndx = struct.unpack_from("<IIHHHHHH", obj, 0x20)
    heads = [struct.unpack_from("<IIIIIIIIII", obj, shoff + i * shentsize) for i in range(shnum)]

    def text(offset: int) -> str:
        return obj[offset : obj.index(b"\0", offset)].decode()

    names = [text(heads[shstrndx][4] + h[0]) for h in heads]
    symtab = next(h for h in heads if h[1] == 2)  # SHT_SYMTAB
    strtab = heads[symtab[6]]
    found = []
    for at in range(symtab[4], symtab[4] + symtab[5], 16):
        name, _, size, info, _, shndx = struct.unpack_from("<IIIBBH", obj, at)
        if info & 0xF == 1 and shndx < len(names) and names[shndx].startswith(SECTIONS):  # STT_OBJECT
            found.append((text(strtab[4] + name), size))
    return found


def over(src: str, names_h: str, out: Path) -> list[str]:
    obj = out / (src.replace("/", "_") + ".o")
    std = "-std=gnu11" if src.startswith("src/qbrt/llrm/") else "-std=c23"  # llrm's C; the glue's
    subprocess.run([CLANG, *FLAGS, std, "-include", names_h, "-c", src, "-o", obj], cwd=ROOT, check=True)
    return [f"{src}: {name} {size} bytes" for name, size in statics(obj.read_bytes()) if size > LIMIT]


def main() -> None:
    names_h, srcs = sys.argv[1], sys.argv[2:]
    with tempfile.TemporaryDirectory() as d, ThreadPoolExecutor() as pool:
        found = [line for lines in pool.map(lambda s: over(s, names_h, Path(d)), srcs) for line in lines]
    if found:
        sys.exit("static buffers in the runtime:\n" + "\n".join(found))
    print(f"ok: {len(srcs)} files")


if __name__ == "__main__":
    main()
