#!/usr/bin/env python3
"""shaders.py: gfx's one shader (src/plat/gpu) as each host takes it, its vertex stage twice: for
plain vertices and, with DISC, for discs. glslang makes SPIR-V, and spirv-cross MSL for the Mac's
SDL_GPU and GLSL ES 3.0 for the page's WebGL2. The files it writes are committed, so a build needs
neither tool: run `make shaders` after an edit (tools/deps.sh shaders builds them)."""

import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIN = ROOT / "build" / "deps" / "bin"
SOURCE = ROOT / "src" / "plat" / "gpu"
# name: source, defines
SHADERS = {"draw_vert": ("draw.vert", ()), "disc_vert": ("draw.vert", ("-DDISC",)), "draw_frag": ("draw.frag", ())}
MSL_OUT = ROOT / "src" / "plat" / "sdl" / "shaders.h"
ESSL_OUT = ROOT / "src" / "plat" / "wasm" / "web" / "shaders.mjs"
# SDL_GPU's Metal names a resource by its binding, as SDL_shadercross does
MSL = ["--msl", "--msl-version", "20000", "--msl-decoration-binding"]
ESSL = ["--es", "--version", "300", "--glsl-emit-ubo-as-plain-uniforms"]
NOTE = "Made by tools/shaders.py from src/plat/gpu/draw.*: run make shaders, never edit."


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


def main() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        spv = {name: spirv(name, Path(tmp)) for name in SHADERS}
        msl = {name: run(BIN / "spirv-cross", spv[name], *MSL) for name in SHADERS}
        essl = {name: run(BIN / "spirv-cross", spv[name], *ESSL) for name in SHADERS}

    MSL_OUT.write_text(
        f"/* {NOTE} */\n#ifndef PLAT_SDL_SHADERS_H\n#define PLAT_SDL_SHADERS_H\n"
        + "".join(c_string(f"{name}_msl", msl[name]) for name in SHADERS)
        + "#endif\n"
    )
    ESSL_OUT.write_text(f"// {NOTE}\n" + "".join(f"export const {n.upper()} = `{essl[n]}`;\n" for n in SHADERS))


if __name__ == "__main__":
    sys.exit(main())
