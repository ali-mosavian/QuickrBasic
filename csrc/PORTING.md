# Porting src/ (x86 asm) to csrc/ (C23)

Temporary: the rules every module's port follows. Deleted when the asm is.

## Goal

Same behaviour as the asm build, byte for byte where a program, a test or a file can see it:
output, errors and their messages, the stored lines, the bytecode (an error's `at address` is a
code-cache offset), records in files, PEEK of a variable's bytes. The suites run both builds.

Idiomatic C, not transliterated asm: structs for state, readable control flow, no register
emulation, no `goto` webs. Keep the asm's algorithms and data layouts; rewrite its control flow.

## Language and libraries

- C23 as clang 24 takes it: `bool`, `nullptr`, `constexpr`, `static_assert`, typed enums
  (`enum : uint8_t`), `[[noreturn]]`, `[[maybe_unused]]`, `auto` sparingly.
- No inline asm: llrm-llc refuses it. Machine work is the platform's (below).
- No C library beyond `csrc/lib/include`: `string.h` (mem*, str*), `setjmp.h`, and the compiler's
  `stdint.h stddef.h stdbool.h stdarg.h limits.h float.h`. No printf, no malloc, no math.h. The
  same code builds on a host with its own libc.
- Build: `make cbasic32` (and `cide32`). Every module compiles with `-Wall -Wextra` clean.
  A quick syntax check of one file on the Mac: `clang -std=c23 -fsyntax-only -Icsrc -Icsrc/core
  -DIDE=0 FILE.c`.

## Portability

Nothing in `csrc/core` touches DOS, the BIOS, DPMI, ports, video memory, interrupts or the x87.
It calls the platform's headers, `csrc/plat/p????.h` (prefix below), each a narrow interface. Their
DOS32 implementations are `csrc/plat/dos32/` (C over `pdos_int`, `plat/dos32/dos.h`, and jwasm
files where C cannot: interrupt handlers, x87 instructions). A later host or wasm build is just
another directory. When a module needs the platform, add the function to the right `plat/p????.h`
and implement it in `plat/dos32/p????.c`.

llrm's OS layer (`<llrm_os.h>`, `llrm_os_*`) does files, the console's keys, the command line and
the environment on DOS32; `plat/dos32` may use it. Core code never includes it.

## Memory

`core/base.h`: BASIC-visible memory and everything that stores an address of it is one arena.
An `Addr` is a 32-bit offset in it; `at(a)` its pointer; `rd16/wr32` etc. Where the asm kept a
linear address or a DS offset (Desc's ptr, a string block's owner, an array's data, a variable's
cell, a frame's line) the C keeps an `Addr`, so records keep their sizes. Records in the arena
are `PACKED` structs reached by `(Line *)at(a)`. The asm's static regions that BASIC or a stored
address can reach (`g_dsb`, `g_pokeb`, the save stack, temporaries' Descs) are carved from the
arena at start, not C statics. Plain C arrays are fine for anything no Addr points into.

`core/defs.h` holds the shared constants and records (from `src/def.inc`). A module's own records
go in its header.

## Naming

- Every external function and global starts with its module's 4-letter prefix:
  `strg_push`, `prog_find`, `chnl_open`. Statics need no prefix.
- Names are understandable and short: `prog_find(num)`, not `prg_fnd` nor
  `program_store_find_line_by_number`. Parameters too: `len`, `name`, `line`, `at`.
- Types are CamelCase (`Line`, `Desc`, `Frame`). Constants and enum members are UPPER; `defs.h`
  keeps the asm's (`E_SN`, `T_INT`, `NONUM`).
- A module's state is one struct, `typedef struct { ... } Comp;` and `extern Comp comp;` named by
  its prefix, its fields readable (`comp.type`, not `cty`). Only what other modules use is
  extern; the rest is `static`.
- One `.c` and one `.h` per asm module, in the same directory, named by the prefix.

| asm | C | prefix |
|---|---|---|
| mem/far | mem/copy | `copy` |
| mem/alloc | mem/allo | `allo` |
| mem/tab | mem/tabl | `tabl` |
| mem/prog | mem/prog | `prog` |
| mem/sym | mem/symb | `symb` |
| mem/scope | mem/scop | `scop` |
| mem/str | mem/strg | `strg` |
| mem/arr | mem/arry | `arry` |
| float/flt | num/sngl | `sngl` |
| float/ftr | num/rndm | `rndm` |
| float/fpu | num/fmth | `fmth` |
| float/dbl | num/dubl | `dubl` |
| comp/lex | comp/lexr | `lexr` |
| comp/decl | comp/decl | `decl` |
| comp/block | comp/blok | `blok` |
| comp/ctl | comp/ctrl | `ctrl` |
| comp/comp | comp/comp | `comp` |
| comp/expr | comp/expr | `expr` |
| comp/link | comp/link | `link` |
| comp/list | comp/list | `list` |
| vm/exec | vm/exec | `exec` |
| vm/bc | vm/oper | `oper` |
| vm/flow | vm/flow | `flow` |
| vm/calls | vm/stmt | `stmt` |
| vm/builtins | vm/func | `func` |
| vm/sys | vm/syst | `syst` |
| vm/bin | vm/mkcv | `mkcv` |
| vm/using | vm/usng | `usng` |
| vm/dbg | vm/dbug | `dbug` |
| vm/event | vm/evnt | `evnt` |
| vm/chain | vm/chai | `chai` |
| io/io | io/cons | `cons` |
| io/errmsg | io/emsg | `emsg` |
| io/diag | io/diag | `diag` |
| io/srd | io/srcr | `srcr` |
| io/repl | io/repl | `repl` |
| io/chan | io/chnl | `chnl` |
| io/fio | io/file | `file` |
| io/plat | plat/* | (below) |
| main | main.c | `main` |

Platform: `pmem` memory, `pkey` keyboard, `pscr` text screen, `pmou` mouse, `pclk` clock, date
and tick, `pfil` files and directories, `psys` process (exit, command line, environment, SHELL,
ports, the STOP key), `pfpu` the x87's functions, `psnd` sound.

The UI (`src/ui` → `csrc/core/ui`):

| asm | C | prefix |
|---|---|---|
| ui/scr | ui/scrn | `scrn` |
| ui/mouse | ui/mous | `mous` |
| ui/theme | ui/them | `them` |
| ui/syntax | ui/synt | `synt` |
| ui/lmap | ui/lmap | `lmap` |
| ui/text | ui/text | `text` |
| ui/view | ui/view | `view` |
| ui/win | ui/wind | `wind` |
| ui/draw | ui/draw | `draw` |
| ui/ui | ui/uimn | `uimn` |
| ui/edit | ui/edit | `edit` |
| ui/menu | ui/menu | `menu` |
| ui/dlg | ui/dlog | `dlog` |
| ui/compl | ui/cmpl | `cmpl` |
| ui/dbg | ui/udbg | `udbg` |
| ui/dpane | ui/dpan | `dpan` |

Their platform: `pscr` the text screen (cells, cursor, mode, the DAC's palette, retrace),
`pmou` the mouse. Name map: `csrc/portmap/ui.md`.

## Control flow

- An asm routine's `in:`/`out:` registers become parameters and a return value; a flag result
  (`CF set: ...`) a `bool`. A routine that returns several values takes pointers or returns a
  small struct.
- Errors: `[[noreturn]] void repl_error(uint8_t code)` (io/repl, the asm's `err_raise`) unwinds
  with `longjmp` to the compiler, the error handler or READY, as `err_raise` does. `err_sn` and
  friends are `repl_error(E_SN)`.
- The VM: the asm's `ESI` and `ESP` are the VM's `ip` and data stack `sp`, fields of `exec`; a
  handler is a function the dispatch loop calls; `jmp ex_lp` is a return.

## Porting a module

1. Read the asm module and the headers it uses. Its comments say what each routine does.
2. Write its `.h`: its state struct, its records, its constants, its public functions with
   readable names and a one-line comment each.
3. Record the mapping in `csrc/portmap/<group>.md` (mem, num, comp, vm, io): each asm label and
   `g_state` field or global the module defines → its C name. Other modules' ports look names up
   there.
4. Write its `.c`. Call other modules by their C names (csrc/portmap/); if one is not ported yet,
   use the name its section lists, or add the declaration to that module's `.h` and a line to its
   portmap section marked `(declared by <you>)`.
5. It compiles clean. Comments only where something is not obvious.
