# QuickrBasic

QBasic 4.5 for DOS, as a 32-bit DOS/32A program, written in C23: a compiler to bytecode and its
runtime (`basic32.exe`), and the same with a QBasic-style editor (`ide32.exe`). No limits but
memory: programs, lines, strings, arrays and symbol tables grow as needed.

![QBasic 1.1 and QuickrBasic, the same program](docs/img/versus.gif)

## Showcase

![Every feature, zoomed into one by one](docs/img/tour.gif)

| | |
|---|---|
| **From DOS**: `basic demo.bas`; highlighting from the compiler's lexer<br>![](docs/img/clips/start.gif) | **Completion** offers only what fits where the cursor is<br>![](docs/img/clips/complete.gif) |
| **Tab completes**; the indent follows the block<br>![](docs/img/clips/indent.gif) | **Each line checked** as it is entered<br>![](docs/img/clips/check.gif) |
| **SUBs and FUNCTIONs**, each in its own view<br>![](docs/img/clips/procedures.gif) | **QBasic's runtime**: graphics, sound, files<br>![](docs/img/clips/run.gif) |
| **Split windows**; drag any line between them<br>![](docs/img/clips/split.gif) | **Breakpoints**, stepping, Locals<br>![](docs/img/clips/breakpoint.gif) |
| **Watches** in the stopped call's scope<br>![](docs/img/clips/watch.gif) | **The call stack**: any caller's frame<br>![](docs/img/clips/stack.gif) |
| **25, 43 and 50 lines**<br>![](docs/img/clips/rows.gif) | **Nine themes**<br>![](docs/img/clips/themes.gif) |
| **GORILLA.BAS**, unchanged, opened and run<br>![](docs/img/clips/gorillas.gif) | **NIBBLES.BAS**, unchanged, opened and run<br>![](docs/img/clips/nibbles.gif) |

`python3 tools/demo.py` plays the IDE in DOSBox-X and cuts its video recording into these.

## Building

Needs clang, make, python3, git and a C compiler. The rest is built from pinned sources on first
use (`tools/deps.sh`, into `build/deps`) or is in `third_party/`.

```sh
make            # build/c/basic32.exe and ide32.exe, with DOS32A.EXE and BASIC.HLP beside them
make test       # the regressions and QB 4.5 conformance under DOSBox-X
make host       # the same core for this machine: build/host/basic
make host-test  # the unit tests and the suites on the host build
```

- `GCC=1` builds with i686-elf-gcc instead, into `build/gcc`; `make lto` optimizes the whole
  program as one module (LLVM's tools).
- `src/`: the C. `docs/architecture.md`: its layout. `tools/`: the build's and the tests' scripts.
- Copy `BASIC32.EXE` or `IDE32.EXE`, `DOS32A.EXE` and `BASIC.HLP` to a DOS machine to run them.

## Retired

The assembly QuickrBasic was ported from, kept buildable, each with its own Makefile:

- `retired/asm32/`: the 32-bit DOS/32A asm; `make -C retired/asm32` builds `build/asm32/`.
  `retired/design.md`, `retired/asm-style.md`: its design and style.
- `retired/asm16/`: the 16-bit DOS `.COM` before it, from cbmbasic-bc (`316224a`);
  `make -C retired/asm16` builds `build/asm16/`.

## Third-party

- DOS/32A (`third_party/dos32a`, its licence beside it). This product uses DOS/32 Advanced DOS
  Extender technology.
- llrm's QB runtime (`src/qbrt/llrm`), DOS/32A start-up (`src/plat/dos32/startup`), and its QB
  conformance cases (`tests/qb45`, `tests/qbrun`).
- jwasm and jwlink, built from the pinned forks by `tools/deps.sh`.
