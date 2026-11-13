# QuickrBasic

QBasic 4.5 for DOS, as a 32-bit DOS/32A program: a compiler to bytecode and its runtime
(`basic32.exe`), and the same with a QBasic-style editor (`ide32.exe`). No limits but memory:
programs, lines, strings, arrays and symbol tables grow as needed.

- `make` builds both into `build/x86/`; `make test` runs the regressions and llrm's QB 4.5
  conformance suite under DOSBox.
- `docs/design.md`: the design. `docs/asm-style.md`: the assembly style.
- Toolchain: jwasm, jwlink, DOS/32A and dosbox-x from llrm (`LLRM_BIN`, `DOS32A`).
