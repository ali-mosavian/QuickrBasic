"""libcalls_test.py: no C object at -Os calls a compiler helper the DOS link lacks.

At -Os clang called __aulldiv for dubl_digits's 64-bit /10 and the link failed.
"""

import re
import sys
import tempfile
import subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parent.parent
LLVM = Path("/opt/homebrew/opt/llvm/bin")
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
HELPER = re.compile(r"__(a(ll|ull)(div|rem|shl|shr|mul)|u?(div|mod|udivmod)di3|udivmoddi4)\b")


def helpers(src: Path, out: Path) -> list[str]:
    obj = out / (str(src.relative_to(ROOT)).replace("/", "_") + ".obj")
    subprocess.run([LLVM / "clang", *FLAGS, "-c", src, "-o", obj], cwd=ROOT, check=True)
    nm = subprocess.run([LLVM / "llvm-nm", "-u", obj], capture_output=True, text=True, check=True).stdout
    return [f"{src}: {s}" for s in nm.split() if HELPER.search(s)]


def main() -> None:
    srcs = sorted((ROOT / "src/core").rglob("*.c"))
    with tempfile.TemporaryDirectory() as d, ThreadPoolExecutor() as pool:
        found = [h for hs in pool.map(lambda s: helpers(s, Path(d)), srcs) for h in hs]
    if found:
        sys.exit("\n".join(found))
    print(f"ok: {len(srcs)} files")


if __name__ == "__main__":
    main()
