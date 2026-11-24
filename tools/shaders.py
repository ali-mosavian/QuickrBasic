#!/usr/bin/env python3
"""shaders.py: gfx's one shader (stdlib/gfx/src/backend/gpu/shaders) in each language a host takes, its vertex
stage twice: for plain vertices and, with DISC, for discs. glslang makes SPIR-V, for SDL_GPU's Vulkan,
and spirv-cross MSL for its Metal and ESSL for the page's WebGL2. The header it writes is committed, so a build
needs neither tool: run `make shaders` after an edit (tools/deps.sh shaders builds them)."""

import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIN = ROOT / "build" / "deps" / "bin"
SOURCE = ROOT / "stdlib" / "gfx" / "src" / "backend" / "gpu" / "shaders"
# name: source, defines
SHADERS = {"draw_vert": ("draw.vert", ()), "disc_vert": ("draw.vert", ("-DDISC",)), "draw_frag": ("draw.frag", ())}
OUT = ROOT / "stdlib" / "gfx" / "src" / "backend" / "gpu" / "shaders.h"
# SDL_GPU's Metal names a resource by its binding, as SDL_shadercross does
MSL = ["--msl", "--msl-version", "20000", "--msl-decoration-binding"]
LANGUAGES = {"msl": MSL, "essl": ["--es", "--version", "300", "--fixup-clipspace"]}
NOTE = "Made by tools/shaders.py from gpu/shaders: run make shaders, never edit."


def run(*args: str | Path) -> str:
    return subprocess.run([str(a) for a in args], check=True, capture_output=True, text=True).stdout


def spirv(name: str, out: Path) -> Path:
    source, defines = SHADERS[name]
    spv = out / f"{name}.spv"
    run(BIN / "glslang", "-V", "--quiet", *defines, SOURCE / source, "-o", spv)
    return spv


def c_string(name: str, text: str) -> str:
    lines = "".join(f'    "{line}\\n"\n' for line in text.replace("\\", "\\\\").replace('"', '\\"').splitlines())
    return f"static const char {name}[] =\n{lines};\n"


def c_bytes(name: str, data: bytes) -> str:
    rows = (", ".join(f"0x{b:02x}" for b in data[i : i + 16]) for i in range(0, len(data), 16))
    return f"static const uint8_t {name}[] = {{\n" + "".join(f"    {row},\n" for row in rows) + "};\n"


def main() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        spvs = {name: spirv(name, Path(tmp)) for name in SHADERS}
        code = {
            f"{name}_{language}": run(BIN / "spirv-cross", spvs[name], *flags)
            for language, flags in LANGUAGES.items()
            for name in SHADERS
        }
        binary = {f"{name}_spirv": spv.read_bytes() for name, spv in spvs.items()}

    OUT.write_text(
        f"/* {NOTE} */\n#ifndef GFX_GPU_SHADERS_H\n#define GFX_GPU_SHADERS_H\n#include <stdint.h>\n"
        + "".join(c_string(name, text) for name, text in code.items())
        + "".join(c_bytes(name, data) for name, data in binary.items())
        + "#endif\n"
    )


if __name__ == "__main__":
    sys.exit(main())
