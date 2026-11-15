#!/usr/bin/env python3
"""clto.py CLANG OUT.obj BITCODE... -- ASM...: the C program's bitcode as one COFF object, optimized whole.

The modules are linked into one (llvm-link), every symbol but those the asm objects and the start-up
use made internal (internalize), the whole optimized (opt -O2), then compiled for the 386 (llc)
as one object. jwlink reads no bitcode, so this is the link-time optimization it cannot do.
"""

import os
import re
import subprocess
import sys
from pathlib import Path

# the start-up's calls into C; what codegen calls or names itself: the mem routines, MSVC's float marker
KEEP = {"main", "memcpy", "memmove", "memset", "_fltused"}


def imports(obj: Path) -> set[str]:
    """The names an asm object takes from elsewhere: COFF's from llvm-nm, OMF's from its source."""
    nm = subprocess.run([str(BIN / "llvm-nm"), "-u", str(obj)], capture_output=True, text=True)
    if nm.returncode == 0:
        return {line.split()[-1] for line in nm.stdout.splitlines() if line.strip()}
    src = Path("csrc") / obj.relative_to("build/c/obj").with_suffix(".asm")
    return set(re.findall(r"(?im)^\s*extern(?:def|n)?\s+(\w[\w$]*)", src.read_text(errors="replace")))


def ir_name(sym: str) -> str:
    """A COFF symbol's IR name: C's lose the leading '_'; names given by redefine_extname keep theirs."""
    return sym[1:] if sym.startswith("_") else "\1" + sym


def run(*argv: str) -> None:
    subprocess.run(argv, check=True)


args = sys.argv[1:]
cut = args.index("--")
clang, out, bitcode, asm = args[0], Path(args[1]), args[2:cut], [Path(a) for a in args[cut + 1:]]
BIN = Path(clang).parent
keep = set(KEEP)
for obj in asm:
    for sym in imports(obj):
        keep |= {ir_name(sym), sym}
merged, optimized = out.with_suffix(".bc"), out.with_suffix(".opt.bc")
run(str(BIN / "llvm-link"), "-o", str(merged), *bitcode)
# CLTO_BISECT=n: opt's first n passes only (-opt-bisect-limit), to find one that miscompiles
bisect = [f"-opt-bisect-limit={os.environ['CLTO_BISECT']}"] if "CLTO_BISECT" in os.environ else []
run(str(BIN / "opt"), *bisect, "-passes=internalize,default<O2>",
    "-internalize-public-api-list=" + ",".join(sorted(keep)), "-o", str(optimized), str(merged))
run(str(BIN / "llc"), "-O2", "-mtriple=i386-pc-win32", "-filetype=obj", "-o", str(out), str(optimized))
run(str(BIN / "llvm-objcopy"), *[f"--redefine-sym=_{f}=_lib_{f}" for f in ("memcpy", "memmove", "memset")],
    str(out))
