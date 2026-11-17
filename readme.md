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

`python3 tools/x86demo.py` plays the IDE in DOSBox and cuts its video recording into these.

## Building

- `make` builds `build/c/basic32.exe` and `ide32.exe` with clang; `GCC=1` with i686-elf-gcc, into
  `build/cgcc/`. `make test` runs the regressions and llrm's QB 4.5 conformance suite under DOSBox.
- `make chost` builds the same core for this machine (`build/host/basic`); `make chosttest` runs
  the unit tests and the suites on it.
- `src/`: the C. `docs/architecture.md`: its layout.
- Toolchain: clang, and llrm's jwasm, jwlink, DOS/32A and dosbox-x (`LLRM`, `LLRM_BIN`, `DOS32A`).

## Retired

The assembly QuickrBasic was ported from, kept buildable:

- `retired/asm32/`: the 32-bit DOS/32A asm; `make asm` builds `build/x86/`. `retired/design.md`,
  `retired/asm-style.md`: its design and style.
- `retired/asm16/`: the 16-bit DOS `.COM` before it, from cbmbasic-bc (`316224a`); `make asm16`.
