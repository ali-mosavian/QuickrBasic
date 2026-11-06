# cbmbasic-bc

A compiled-only BASIC for the Microsoft-derived 6502 machines (X16 first; C64, C128, PET, VIC-20,
Plus/4 later): every statement is compiled to byte code, one execution path, 10x faster than stock
at the same ROM size.

- `docs/bytecode.md`: architecture and byte-code spec (draft, nothing implemented).
- `docs/structure.md`: source layout, naming, platform interface.
- `tables/stmts.txt`: statement and function table that drives the compiler, the decompiler and
  executor dispatch.

Reference code lives in `/Users/alim/work/x16basic-c`.
