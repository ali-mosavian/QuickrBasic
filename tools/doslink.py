#!/usr/bin/env python3
"""doslink.py [--map MAP] EXE OBJECT...: the objects linked for DOS/32A, an LE behind stub32a.exe.

The start-up (src/plat/dos32/startup, from llrm) comes first and the OS layer last; they are
assembled beside EXE. JWASM, JWLINK and DOS32A name the tools (build/deps/bin, third_party).
"""

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STARTUP = ROOT / "src/plat/dos32/startup"
JWASM = Path(os.environ.get("JWASM", ROOT / "build/deps/bin/jwasm"))
JWLINK = Path(os.environ.get("JWLINK", ROOT / "build/deps/bin/jwlink"))
DOS32A = Path(os.environ.get("DOS32A", ROOT / "third_party/dos32a"))

# What the start-up and the OS layer are told of DOS/32A: DOS's and DPMI's functions, the sizes.
DEFINES = {
    "DOS_INT": 33, "DOS_OPEN": 61, "DOS_CREATE": 60, "DOS_CLOSE": 62, "DOS_DELETE": 65, "DOS_RENAME": 86,
    "DOS_MKDIR": 57, "DOS_CHDIR": 59, "DOS_RMDIR": 58, "DOS_SET_DTA": 26, "DOS_GET_DRIVE": 25,
    "DOS_FIND_FIRST": 78, "DOS_FIND_NEXT": 79, "DOS_FREE_SPACE": 54, "DOS_GET_DIRECTORY": 71, "DOS_SEEK": 66,
    "DOS_READ": 63, "DOS_WRITE": 64, "DOS_READ_KEY": 8, "DOS_KEY_READY": 11, "DOS_EXIT": 76, "DOS_EXEC": 75,
    "DOS_RESIZE": 74, "DOS_GET_VECTOR": 53, "DOS_SET_VECTOR": 37, "DOS_IOCTL": 68, "DOS_DPMI_INT": 49,
    "DOS_DPMI_ALLOC": 1281, "DOS_DPMI_DOS_ALLOC": 256, "DOS_DPMI_DOS_FREE": 257, "DOS_DPMI_SIMULATE": 768,
    "DOS_DPMI_GET_VECTOR": 512, "DOS_DPMI_SET_VECTOR": 513, "DOS_HANDLE_BITS": 16, "DOS_STDIN": 0,
    "DOS_STDOUT": 1, "DOS_STDERR": 2, "HEAP_BYTES": 16777216, "STACK_RESERVE": 512, "DIVIDE_FAULT_ERROR": 11,
    "STACK_BYTES": 131072,
}


def run(command: list[str]) -> None:
    done = subprocess.run(command, capture_output=True, text=True)
    if done.returncode != 0:
        sys.exit(f"{Path(command[0]).name}: {(done.stdout + done.stderr).strip()}")


def assemble(name: str, out: Path) -> Path:
    source, obj = STARTUP / f"{name}.asm", out / f"{name}.obj"
    if not obj.exists() or obj.stat().st_mtime < max(source.stat().st_mtime, Path(__file__).stat().st_mtime):
        defines = [f"-D{key}={value}" for key, value in DEFINES.items()]
        run([str(JWASM), "-q", "-c", "-Cp", "-Zg", "-omf", *defines, f"-Fo{obj}", f"-Fw{obj.with_suffix('.err')}", str(source)])
    return obj


def link(exe: Path, objects: list[Path], map_: Path | None = None) -> None:
    out = exe.parent / "startup"
    out.mkdir(parents=True, exist_ok=True)
    first, last = [assemble("start", out)], [assemble("ext", out), assemble("os", out)]
    files = [f"file {Path(one).resolve()}" for one in (*first, *objects, *last)]
    mapping = [f"option map={map_}"] if map_ else []
    stub = [f"option stub={DOS32A / 'stub32a.exe'}"]
    # jwlink copies its command line into 10 KiB: the object paths, a directive file's words
    directives = out / f"{exe.stem}.lnk"
    directives.write_text("\n".join(["option quiet", *mapping, "format os2 le", f"name {exe}", *stub, *files]) + "\n")
    run([str(JWLINK), f"@{directives}"])


def main() -> None:
    args = sys.argv[1:]
    map_ = None
    if args[:1] == ["--map"] and len(args) > 1:
        map_, args = Path(args[1]), args[2:]
    if len(args) < 2:
        sys.exit(__doc__)
    link(Path(args[0]).resolve(), [Path(one) for one in args[1:]], map_)


if __name__ == "__main__":
    main()
