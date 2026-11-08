# cbmbasic-bc

A compiled-only BASIC for the Microsoft-derived 6502 machines (X16 first; C64, C128, PET, VIC-20,
Plus/4 later): every statement is compiled to byte code, one execution path, 10x faster than stock
at the same ROM size.

- `docs/bytecode.md`: architecture and byte-code spec (draft, nothing implemented).
- `docs/structure.md`: source layout, naming, platform interface.
- `tables/keywords.txt`: the keywords, in token order.
- `make test` runs the unit and tool tests; the tools are in `tools/` (see docs/structure.md).

Reference code lives in `/Users/alim/work/x16basic-c`.
