# mem: asm → C

Headers in `csrc/core/mem/`. A program-store line is a `uint32_t` offset in the store (the asm's
GS offsets); `prog_line(off)` is its `Line *`. A string value is the `Addr` of its Desc. Selectors
(FS, GS, `alloc_sel`, `sel_set`, `progsel`, `vsel`) are gone: `fs:[id*8].TVAR` is `symb_var(id)`,
`gs:[x]` is `prog_line(x)` / `at(prog.base + x)`. `mm.dslin` is 0 and dropped: a DS offset is an
`Addr`.

## regions.asm, alloc.asm → alloc

In `alloc.c`'s header.

## far.asm → copy

In `copy.c`'s header.

## tab.asm → table

In `table.c`'s header.

## prog.asm → program

In `program.c`'s header.

## sym.asm → symbols

In `symbols.c`'s header.

## scope.asm → scope

In `scope.c`'s header.

## str.asm → string

In `string.c`'s header.

## arr.asm → array

In `array.c`'s header.
