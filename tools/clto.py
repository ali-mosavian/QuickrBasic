#!/usr/bin/env python3
"""clto.py CLANG OUT.obj [--profiling | --hot FILE] BITCODE... -- ASM...: the C program's bitcode as
one COFF object, optimized whole.

The modules are linked into one (llvm-link), every symbol but those the asm objects and the start-up
use made internal (internalize), the whole optimized (opt -O2), then compiled for the 386 (llc)
as one object. jwlink reads no bitcode, so this is the link-time optimization it cannot do.

--profiling: every function kept and its own (none internal, none inlined; a static's name after
q$), so a run's profile (tools/x86pgo.py) counts each by name. --hot FILE: the functions that profile ran, one a line;
every other is cold and optimized for size, so none is inlined for speed it never needed.
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
    src = Path("src") / obj.relative_to("build/c/obj").with_suffix(".asm")
    return set(re.findall(r"(?im)^\s*extern(?:def|n)?\s+(\w[\w$]*)", src.read_text(errors="replace")))


def ir_name(sym: str) -> str:
    """A COFF symbol's IR name: C's lose the leading '_'; names given by redefine_extname keep theirs."""
    return sym[1:] if sym.startswith("_") else "\1" + sym


def run(*argv: str) -> None:
    subprocess.run(argv, check=True)


def defined(bc: Path) -> list[str]:
    """The C functions bc defines, by IR name: llvm-nm gives the object's, '_' before."""
    nm = subprocess.run([str(BIN / "llvm-nm"), "--defined-only", str(bc)], capture_output=True, text=True,
                        check=True)
    return [f[2][1:] for f in (line.split() for line in nm.stdout.splitlines())
            if len(f) == 3 and f[1] in "tT" and f[2].startswith("_")]


args = sys.argv[1:]
cut = args.index("--")
clang, out, rest, asm = args[0], Path(args[1]), args[2:cut], [Path(a) for a in args[cut + 1:]]
profiling = rest[:1] == ["--profiling"]
hot = Path(rest[1]) if rest[:1] == ["--hot"] else None
bitcode = rest[1:] if profiling else rest[2:] if hot else rest
BIN = Path(clang).parent
keep = set(KEEP)
for obj in asm:
    for sym in imports(obj):
        keep |= {ir_name(sym), sym}
merged, optimized = out.with_suffix(".bc"), out.with_suffix(".opt.bc")
run(str(BIN / "llvm-link"), "-o", str(merged), *bitcode)
# CLTO_BISECT=n: opt's first n passes only (-opt-bisect-limit), to find one that miscompiles
bisect = [f"-opt-bisect-limit={os.environ['CLTO_BISECT']}"] if "CLTO_BISECT" in os.environ else []
if profiling:
    # each function external under its own name, in the map, and called, not inlined
    ir = subprocess.run([str(BIN / "llvm-dis"), "-o", "-", str(merged)], capture_output=True, text=True,
                        check=True).stdout
    # a static's own name could be the start-up's (report): q$ before it, which x86pgo.py drops
    statics = re.findall(r"(?m)^define (?:internal|private) [^@]*@([-\w$.]+)\(", ir)
    ir = re.sub(r"(?m)^define (internal|private) ", "define ", ir)
    if statics:
        ir = re.sub(r"@(" + "|".join(map(re.escape, statics)) + r")(?![-\w$.])", r"@q$\1", ir)
    subprocess.run([str(BIN / "llvm-as"), "-o", str(merged)], input=ir, text=True, check=True)
    passes = ["-force-attribute=noinline", "-passes=forceattrs,default<O2>"]
else:
    passes = ["-passes=forceattrs,internalize,default<O2>", "-internalize-public-api-list=" + ",".join(sorted(keep))]
if hot:
    ran = set(hot.read_text().split())
    every = defined(merged)
    cold = [f for f in every if f not in ran]
    if len(cold) == len(every):  # names that matched none: everything cold, nothing said
        sys.exit(f"clto: none of {hot}'s functions is in the program")
    attrs = out.with_suffix(".cold.csv")
    attrs.write_text("".join(f"{f},cold\n{f},optsize\n" for f in cold))
    passes.append(f"-forceattrs-csv-path={attrs}")
run(str(BIN / "opt"), *bisect, *passes, "-o", str(optimized), str(merged))
run(str(BIN / "llc"), "-O2", "-mtriple=i386-pc-win32", "-filetype=obj", "-o", str(out), str(optimized))
run(str(BIN / "llvm-objcopy"), *[f"--redefine-sym=_{f}=_lib_{f}" for f in ("memcpy", "memmove", "memset")],
    str(out))
