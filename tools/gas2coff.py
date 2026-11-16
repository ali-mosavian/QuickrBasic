"""gcc's ELF assembly (i686-elf-gcc -S -fleading-underscore) made assemblable for COFF by clang.

gas2coff.py IN.s OUT.s
"""

import re
import sys
import base64
from pathlib import Path

ELF_ONLY = re.compile(r"/(APP|NO_APP|\s)|\s*\.(type|size|ident)\b|\s*\.section\s+\.note\.GNU-stack")
SECTION = re.compile(r"\s*\.section\s+([^\s,]+)")
LOCAL = re.compile(r"\s*\.local\s+(\S+)")
COMM = re.compile(r"(\s*)\.comm\s+([^,\s]+)\s*,(.*)")
BASE64 = re.compile(r'(\s*)\.base64\s+"([^"]*)"')  # gcc 16; clang has no .base64


def section(name: str) -> str:
    for prefix, coff in ((".rodata", '.section .rdata,"dr"'), (".text", ".text"), (".data", ".data"), (".bss", ".bss")):
        if name.startswith(prefix):
            return "\t" + coff
    sys.exit(f"gas2coff: section {name}")


def convert(lines: list[str]) -> list[str]:
    local = {m.group(1) for line in lines if (m := LOCAL.match(line))}
    out = []
    for line in lines:
        if ELF_ONLY.match(line) or LOCAL.match(line):
            continue
        if m := SECTION.match(line):
            line = section(m.group(1))
        elif m := BASE64.match(line):
            line = m.group(1) + ".byte " + ",".join(str(b) for b in base64.b64decode(m.group(2)))
        elif (m := COMM.match(line)) and m.group(2) in local:
            line = f"{m.group(1)}.lcomm {m.group(2)},{m.group(3)}"
        out.append(line)
    return out


def main() -> None:
    src, dst = sys.argv[1:3]
    lines = Path(src).read_text().splitlines()
    Path(dst).write_text("\n".join(convert(lines)) + "\n")


if __name__ == "__main__":
    main()
