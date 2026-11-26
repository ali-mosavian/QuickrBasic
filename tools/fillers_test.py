"""fillers_test.py: gfx's DOS fillers that draw texels call or jump to nothing outside themselves, so no pixel costs a
call. clang once left words_of out of line, reached by a tail jump, and put_next called from it a pixel: a lit span
cost 137 instructions a pixel, not 16. A polygon's row is one of them, called once a row: CUBE's 16 pixels went
through seven calls (span_persp, span_texels, deep, kept, run, filled, drawn), 4.5 times mgl's cost a row."""

import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FILL = "build/c/gfx/stdlib/gfx/src/backend/soft/fill.obj"
OBJDUMP = os.environ.get("OBJDUMP", "objdump")
# inked, and rows keyed or flipped; a span's fill and an opaque row's copy are one call a span
DRAWS = re.compile(r"_fill\d+_(from\d+_\d+|keyed\w*|opaque_flipped|affine_\d+|persp_\d+)$")
ROWS = re.compile(r"_fill\d+_(affine|persp)_\d+$")  # a polygon's row fillers, each format's
CALL = re.compile(r"\b(?:call|jmp)\w*\s+\S+ <(\w+)")
RELOC = re.compile(r"(?:IMAGE_REL_I386_REL32|DISP32)\s+(\S+)")  # a call's relocation, by LLVM's name and GNU's


def main() -> int:
    subprocess.run(["make", FILL], cwd=ROOT, check=True, capture_output=True)
    version = subprocess.run([OBJDUMP, "--version"], capture_output=True, text=True).stdout
    coff = ["-b", "pe-i386"] if "GNU" in version else []  # GNU's cannot tell COFF's flavours apart
    listing = subprocess.run([OBJDUMP, *coff, "-d", "-r", "--no-show-raw-insn", FILL], cwd=ROOT, check=True,
                             capture_output=True, text=True).stdout
    out: dict[str, set[str]] = {}
    fn = ""
    for line in listing.splitlines():
        m = re.match(r"[0-9a-f]+ <(\w+)>:", line)
        fn = m[1] if m else fn
        if not DRAWS.search(fn):
            continue
        to = CALL.search(line) or RELOC.search(line)
        if to and to[1] != fn:
            out.setdefault(fn, set()).add(to[1])
    drawn = {f for f in re.findall(r"<(\w+)>:", listing) if DRAWS.search(f)}
    assert len(drawn) > 100, f"{len(drawn)} fillers found"
    rows = {f for f in drawn if ROWS.search(f)}
    assert len(rows) == 4 * (24 + 8), f"{len(rows)} row fillers found"
    assert not out, f"fillers that leave themselves: {sorted(out.items())[:4]} ({len(out)})"
    print("ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
