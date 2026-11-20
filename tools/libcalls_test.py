"""libcalls_test.py: no C object at -Os calls a compiler helper the DOS link lacks: src/lib/crt.c's
division helpers it has, and they call none.

At -Os clang called __aulldiv for dubl_digits's 64-bit /10 and the link failed; the CONSTs' folding
called __alldiv and __allrem before crt.c had them.
"""

import os
import re
import sys
import tempfile
import subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parent.parent
CLANG = os.environ.get("CLANG", "clang")
FLAGS = [
    "--target=i386-pc-win32",
    "-march=i386",
    "-mregparm=3",
    "-std=c23",
    "-Os",
    "-ffreestanding",
    "-fno-asynchronous-unwind-tables",
    "-fno-stack-protector",
    "-mno-stack-arg-probe",
    "-mstack-alignment=4",
    "-DARENA_AT_ZERO=1",
    "-w",
    "-isystem",
    "src/lib/include",
    "-Isrc",
    "-Isrc/core",
]
HAVE = {f"__{n}" for n in re.findall(r"^HELPER\(\w+, (\w+)\);", (ROOT / "src/lib/crt.c").read_text(), re.M)}
HELPER = re.compile(r"__(a(ll|ull)(div|rem|shl|shr|mul)|u?(div|mod|udivmod)di3|udivmoddi4)\b")


def helpers(src: Path, out: Path) -> list[str]:
    asm = subprocess.run([CLANG, *FLAGS, "-S", src, "-o", "-"], cwd=ROOT, capture_output=True, text=True, check=True)
    calls = re.findall(r"^\s*(?:call|jmp)l?\s+(\S+)", asm.stdout, re.M)
    return [f"{src}: {s}" for s in sorted(set(calls)) if HELPER.search(s)]


def main() -> None:
    assert HAVE == {"__alldiv", "__allrem", "__aulldiv", "__aullrem"}, HAVE
    lib = sorted((ROOT / "src/lib").rglob("*.c"))
    srcs = sorted((ROOT / "src/core").rglob("*.c")) + lib
    with tempfile.TemporaryDirectory() as d, ThreadPoolExecutor() as pool:
        calls = dict(zip(srcs, pool.map(lambda s: helpers(s, Path(d)), srcs)))
    found = [h for s, hs in calls.items() for h in hs if s in lib or h.rsplit(" ", 1)[1] not in HAVE]
    if found:
        sys.exit("\n".join(found))
    print(f"ok: {len(srcs)} files")


if __name__ == "__main__":
    main()
