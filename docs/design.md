# Design

`make` builds `build/x86/basic32.exe` from `src`: flat 32-bit under DOS/32A, an LE
behind `stub32a.exe` with `DOS32A.EXE` beside it, its banner off (`tools/x86d32a.py`). DOS/32A
and DOSBox are llrm's pinned builds. Bytecode only: every op has a handler, `vm/bc.asm` the
simple ones.
The heap, arrays and strings, is DPMI memory: the largest free block at start, less 64K for the
host (`plt_heap`), so FRE grows with the machine. Under 256K the program says Out of memory and
exits.

Its dialect is QBasic. `basic32.exe FILE.BAS` loads the file a line at a time, runs it and exits;
with no file it is a prompt. Line numbers are optional; a label is `name:` at a line's start. A
line with no number is NONUM (FFFFh) in the store, never found by number.
A stored line is `len:2 num:2 cp:2 body`, 2047 bytes at most; its text is 255 characters at most.
A statement's place, in block targets and the statement map, is its line and its body offset, 11
bits beside a 5-bit mode or kind.

Names are up to 40 letters, digits and dots, with a suffix `% & ! # $`. The lexer emits each as
`M_VAR len text nam`, its entry in the name table (`mem/sym.asm`); what a name is, the compiler
asks the tables, as QBasic's parser does. A variable is found by name, type and whether an array,
so `a` and `a()` are two, `b` and `b!` one. A name's type is its suffix, else its `DIM AS` or
`CONST` type, else the DEFtype of its first letter, SINGLE by default. Declarations go into the
table when their line is entered (`comp/decl.asm`). DEFtype holds from its line to the file's end,
SUBs after it too, as BC's: each line is compiled with the DEFtype its place sees (`dc_deft`).

Declarations are scoped (`mem/scope.asm`): a binding is a name's in the module or in one
procedure, holding its declared types, record, `STRING * n`, label line and flags. The compiler
keeps a stack of scopes, the module at its bottom; a name is looked up from the top down, the
first binding the scope above sees. From a SUB or FUNCTION the module's names are seen only when
DIM SHARED or a module CONST (`BF_SHR`), or SHARED there (`BF_REF`); a DEF FN sees all. A
procedure's own binding shadows the module's, so its DIM AS and CONST are its own. A label, or a
line number, is its scope's; ON ERROR's, an event's, RUN's and RESTORE's are the module's. A procedure's REDIM of
a shared array is the module's. Each line is compiled on its own, so `scp_proc` sets the stack to
its procedure.

The scope errors are BC's (QB45, PDS 7.1; VBDOS where they agree): a label or line number used
twice in any scopes is Duplicate label, a jump to another scope's label Subprogram error; a suffix other
than the name's AS type Duplicate definition, a CONST's name with any suffix the CONST; a DIM, or
a CONST (Syntax error), in a procedure of a DIM SHARED name; DIM AS after a use with a suffix, AS
clause required on first declaration; assigning a SUB's name Syntax error, a FUNCTION's outside
it Variable required. Declarations' errors stop the load; the others, like any compile error, are
raised when their line runs.

INT is 32-bit: INTEGER and LONG are one type, -2^31..2^31-1, past it ?OVERFLOW. `/` gives a
FLOAT, `\` and MOD an INT; a FLOAT stored to an INT rounds, a half to even.
PRINT zones are 14 columns, TAB counts from 1, `INPUT "p", x` prints no `?`.

Block statements are paired across lines before a run: `cm_blocks` (`comp/block.asm`) walks
every statement with a stack and gives each IF, ELSEIF, ELSE, SELECT, CASE, DO, LOOP, WHILE,
WEND, FOR and EXIT its targets, a line and a statement offset. A jump op holds its target and,
once run, the address it found: the target line's code, compiled if it has none, carries a map
of its statements' code offsets (`cm_xlate`). A one-line IF's ELSE jumps within its line. FOR
does not run its body when the start is past the limit.

SUB and FUNCTION are procedures (`g_prc`), found by the same pass. A name in one is its local
as the scope stack finds it; STATIC keeps one across calls. `PCALL` saves the
procedure's locals on a save stack and zeroes them, then binds the arguments; `PRET` restores
them. A scalar parameter is reached through its cell (`vt`, `V.LD`, `V.ST`): passed a variable
(`V.ADDR`), an array's element or a record's number field (`P.REF`), the cell is the argument's,
so the parameter is that variable while the call runs (BYREF, aliasing as BC's); passed a value,
its own. An array freed moves the data after it, and the cells in it follow (`arr_free`). An
array parameter ends, at the return, as the caller's array. BYVAL (PDS's), a CONST and an
expression go by value; a variable or array of another type is Parameter type mismatch. A
string moves
between a variable and its save slot by changing its heap block's owner. Inside a procedure a
statement frees only the temporaries above the call's (`tspb`). The control stack has 7 KB, the
data stack 32 KB.

An array keeps a lower bound per dimension after its counts; OPTION BASE sets the default.
`arr_free` drops an array: the headers and data after it slide down, and the headers' data
pointers, the string blocks' owners in the moved data, and the headers in `vt` and on the save
stack follow. REDIM frees the old array; a reference to it, such as the caller's array a SUB
was handed, then names the new one. REDIM PRESERVE copies each old element whose subscripts
the new array has. ERASE frees a REDIM's array and zeroes any other. `PRET`
frees the procedure's local arrays.

A record is one zeroed block, its fields in order with no padding: INTEGER, LONG and SINGLE 4
bytes, DOUBLE 8, `STRING * n` n, a TYPE inline. `SHORT`, ours, is QBasic's 16-bit INTEGER, so a record
can match QBasic's layout byte for byte. Outside a record it is held as an INT, but SHORT
arithmetic (`+ - * \ MOD` of SHORTs and literals that fit) and a store to a SHORT raise Overflow
past 16 bits (`I.CHK16`), and HEX$ and OCT$ show its 16 bits. `DEFSHORT`, ours too, is its DEFtype. A `&` literal is not told from a `%` one. A record array is one block of
elements. `R.ADDR` or `RA.ADDR` pushes the record's address, and `FL.LD`, `FL.ST`, `SF.LD`,
`SF.ST` reach a field at an offset the compiler adds up; `a = b` is `R.COPY`. `STRING * n` pads
or cuts each store to n chars.

A data stack cell is 8 bytes; a DOUBLE fills one as an IEEE double on the x87. `D.*` ops round
each result to 53 bits, so a chain of operations can differ from BC, which keeps 80-bit
intermediates, in the last digit. DOSBox emulates the x87 with host doubles: powers of ten come
from an exact table, and LOG and EXP can differ in the last digit. EXP and `^` raise ?OVERFLOW past
2^32767.5, as BC does, and give infinity, printed `1.#INF`, below that. SIN, COS and TAN raise
?ILLEGAL QUANTITY past 2^31; SQR of a negative does not.

PRINT and STR$ round a number once to 7 digits, a SINGLE, or 16, a DOUBLE, as QB45 does; PDS and
VBDOS give a DOUBLE 15. It takes an exponent, `E` or `D`, from 10^digits, and under 1 when it
needs more decimals than digits. A literal with more than 7 digits, leading zeros after the point
counted, is a DOUBLE. `^` gives a SINGLE, or a DOUBLE from one.

BC keeps a SINGLE expression at the x87's precision until it is stored. So an assignment to a
DOUBLE compiles `/` and `^`, `+ - *` with a SINGLE, and SQR, SIN and the like as DOUBLE ops. An
operator of constants stays a SINGLE, as BC folds it; `^` is never folded.

ON ERROR GOTO keeps the handler's line. An error then sets ERR, QBasic's number, and ERL, the
last line number at or before the failing line, and keeps where RESUME goes: the failing
statement and the next, found in the line's statement map. The handler runs on the failing
procedure's frames and data stack (each `PCALL` frame keeps where its data stack starts), so
RESUME NEXT goes on inside a FUNCTION called mid-expression. An error in the handler, ERROR n with
no handler, and ON ERROR GOTO 0 in the handler stop the program with QBasic's message for it,
"Unprintable error" when the number has none. A file run adds BC's where: `Division by zero in
line 10 in module PROG at address 0000:0717`, "No line number" for a line without one, the address
the op's in the code cache. A full stack is "Out of stack space", which ON ERROR does not trap.

PDS's ON LOCAL ERROR GOTO keeps its line in its procedure's frame. An error goes to the innermost
such handler; the procedures its procedure called are dropped as by their returns, and the error's
place becomes the call, where RESUME and RESUME NEXT go. With none, ON ERROR's handler takes it.

Events (`vm/event.asm`): the tick interrupt counts ON TIMER down and takes a key ON KEY traps from
the BIOS buffer's head; a pending event an ON slot can run sets `stpflg`'s `SF_EV`. The STOP checks
at jumps and line ends then run its GOSUB, an `M_EV` frame that returns to the checking op; its
slot runs no other event until that RETURN. COM, PEN and STRIG are accepted and never fire.

Addresses are 32-bit offsets in the data segment, whose base DOS/32A picks (not 0). VARPTR gives
a variable's, SADD a string's chars', and PEEK and POKE take one. VARSEG and SSEG are 0 and DEF SEG does nothing; each prints a warning on
standard error, with the file's line number. INP, OUT and WAIT reach the I/O ports.
`rpl_read` enters a file's lines, a `'$INCLUDE: 'f'` line's file's in place, nested; the
editor's lines go through `rpl_incl`, so its includes are read too.
CHAIN (`vm/chain.asm`) saves the values of the variables COMMON named, in order, loads its file
with the loader a command-line file uses (`rpl_load`) and runs it; there each COMMON takes the
next value by position, an array with its bounds. Files stay open. In the editor it is Advanced
feature unavailable.
SHELL runs `COMSPEC /C` through DOS's EXEC in DOS memory, the child given ENVIRON's environment.

A statement's syntax is a line of `GRAMMAR` in `tools/x86dfa.py`, its keyword one byte or
`FFh n`; its actions are compiler routines (lower case) or `CALL K_*` (`!NAME`). The QBasic
routines' `K_*` and their handlers are one list, `qbklist` in `vm/isa.inc`; a function is a row of
`g_xfn` in `comp/expr.asm`: its token, routine, argument kinds and result, with none or optional
`( )` for TIMER, DATE$, INKEY$, ERR and the like. `vm/sys.asm` has the clock, INKEY$, SLEEP,
COMMAND$ (the command line after the program's file, upper case) and ENVIRON, which changes a
copy of the environment taken at the start. `vm/bin.asm` has MKI$ to CVD and the MBF forms: MKI$
is 2 bytes. RND is QBasic's 24-bit generator, seeded 50000h; RANDOMIZE puts the DOUBLE's two high
words, xor-ed, into the seed's middle word.

All output and input go through channels, `io/chan.asm`. Channel 0 is the console; the others
are OPEN's files by number, a DOS handle and a 512-byte buffer each, or the devices SCRN:, CONS:
and NUL:. `io_putc` writes to the current output channel, so a new device, such as an editor
window, is a new channel kind. `io/fio.asm` has the file statements and functions. A RANDOM
file's record is a buffer of its own, which FIELD's strings point into; GET and PUT take a
variable's place and kind from the compiler's `va`. An INTEGER is 2 bytes in a file.

The flat build needs an x87: it has no soft float, and without one it says so and exits. Its float
routines are `float/fpu.asm`; `float/flt.asm` keeps the compares, conversions and number text.

`make ide32` adds the editor, `src/ui`: `build/x86/ide32.exe`. Its state is `g_ui` in
`ui.asm`, structures in `ui/ui.inc`. `X86_IDE=build/x86/ide32.exe tools/x86ide_test.py` plays it.

The editor builds the program as a file loads, labels and all. Its debugger is QBasic's Debug
menu: Step (F8), Step Over (F7, and QBasic's F10), Step Out (Shift+F8, ours: on till the
procedure or GOSUB returns), Trace On, Toggle Breakpoint (F9), Clear All Breakpoints, Set Next
Statement, Ctrl+F7 to the cursor and Run, Restart. In the editor the compiler starts each
program statement with a `DBG` op (`vm/dbg.asm`); `basic32.exe` has none. `DBG` stops for a step,
a breakpoint at its line's first statement or F7's line, and with trace on hands its place to a
hook, `ui/dbg.asm`'s, which lights the statement a moment. A stop is a STOP: CONT goes on past the
`DBG`. Ctrl+Break in the editor becomes a step, so it too stops at a statement's start. A
breakpoint is a mark on its text line (`ui/text.asm`), moving with the line, given to the VM by
line number before each run. To light a statement the editor tokenizes its line again: the
statement map gives its body offset, the tokenizer's column map its column. TRON and TROFF set
trace; on their own they do nothing.

At a stop the debug pane (`ui/dpane.asm`) opens right of the code, which keeps 50 columns: Watch,
Locals and Call stack, stacked; a run closes it. The call stack is the stop's procedure, then each
`M_PC` frame's caller and the line the call returns to. F6 reaches the pane, Tab its windows; Enter
on a frame puts the cursor on its line. Locals lists that frame's procedure's variables (the
module's at the module), parameters first. An outer call's values are where the next call of the
same procedure saved them: its parameters' cells from `F_LSP0`, its locals' values from `F_LSP1`.
In Watch, Ins adds an expression and Del drops one; at each stop `dbg_eval` (`vm/dbg.asm`) PRINTs
each into `sbuf` as a direct line compiled in the stopped procedure's scope. The line's end and any
error come back to it instead of READY, and the run's state the line could change is put back, so
CONT goes on as before. With the cursor or the mouse on a variable of the selected frame's
procedure, or a module one it sees (DIM SHARED, SHARED), a tooltip under it shows its value.

The windows (`ui/win.asm`) are QBasic's: Split, F6 and Shift+F6, Alt+Plus and Alt+Minus a row,
Ctrl+F10 the whole screen. Each has its rows from its height; the inactive edit window's state is
swapped in while it draws. Each procedure has its own view (`ui/view.asm`): the text keeps the
module's lines first, then each SUB or FUNCTION to its END, and a view is a range of its lines
that the text routines see as the whole (`tx_view`). Building the program and finding a line
work on the whole text. Typing a SUB or FUNCTION line in the module moves it to a view of its own
with its END; F2 lists the views, Shift+F2 and Ctrl+F2 step through them. View, Split
Vertically puts the second edit window beside the first, in its rows: each window has its own
first column and width.

The editor draws with attribute slots, not attributes; `ui/scr.asm` maps each through the theme
(`ui/theme.asm`). A theme is its attributes and the RGB of the 16 text colours, which go to the
DAC while the editor's screen shows: QBasic Colors keeps VGA's; One Dark and Light, Ayu Mirage
and Light, Gruvbox Dark Soft and Light, JetBrains Dark and Light take Zed's colours, over one
layout of the 16 so that they share their attributes. The program's screen always has VGA's. Syntax highlighting (`ui/syntax.asm`) tokenizes each
row with the compiler's lexer and colours each item from its column in the column map. Its
classes are Zed's: keywords, strings, numbers, comments, operators, types after `AS` or `TYPE`,
and the name a SUB, FUNCTION or DEF FN declares with its parameters.

Each `.asm` is a module of its own, linked by jwlink. They share `x86f.inc`: the model, the
constants, the macros, and `syms.inc`, the EXTERNDEFs `tools/x86syms.py` writes for every label
one module defines and another uses. `x86syms.py check` fails when `syms.inc` is stale or a
constant or macro two modules share is outside a header.

The core's state is one structure, `g_state` in `main.asm`, of a structure per part
(`state.inc`): `g_state.cm` the compiler, `g_state.rn` the executor, `g_state.io` input and
output, and so on. A buffer lives in the module that owns it. Every global's name starts `g_`.
`regions.asm` is linked last: the state zeroed at the start runs from `g_bss_s` to its `g_bss_e`.

Its lexer and statement parser are table-driven automata in `comp/dfa.inc`, generated by
`tools/x86dfa.py` and minimized. The scanner reads a whole word and gives the QBasic keyword it
spells, else it is a name: a token is 80h..FEh, or FFh and a byte. Each statement has a DFA built
from an LL(1) grammar, with the compiler's actions on its edges; expressions stay precedence
climbing. Listing spaces a line as QBasic's editor does.
`tools/x86f_test.py` holds its regressions. `tools/qbconf.py` runs llrm's qb45 suite and
`tests/run/qb` against it.

## Editor

`build/x86/ide32.exe` is the same build with `IDE=1`: a QBasic-style editor. `ide32 NAME.BAS`
opens a file. The code is in `src/ui/`. The menus' status-bar help is in `BASIC.HLP`, read from the
program's directory.

## The C build

`csrc/` is the same program in C23: `make cbasic32 cide32` builds `build/c/basic32.exe` and
`build/c/ide32.exe`, and the suites run either build (`X86_COM`, `X86_IDE`). Each asm module has a
C module of the same part, named by a 4-letter prefix that also starts its external names
(`csrc/PORTING.md`); `csrc/portmap/` maps every asm label to its C name.

The compiler is llrm-cc for `x86_32-dos` (llrm's `release/next`, `LLRM`), over the patched clang
(`LLRM_CLANG_M16`), linked with llrm's start-up and OS layer by its `link_objects.py`. There is no C
library: `csrc/lib` has the string routines and setjmp.

The core, `csrc/core`, is portable C. Everything a BASIC program can see, and every record that
holds an address of it, lives in one arena (`core/base.h`): an address is a 32-bit offset in it,
so records keep the asm's layout and the bytecode stays byte for byte the same. On DOS32 the
arena's origin is the flat segment's 0, so an offset is the address VARPTR and PEEK see. Errors
unwind with longjmp to the compiler, the error handler or READY (`repl_error`).

The platform is `csrc/plat`: one narrow header per part (`pmem` memory, `pfil` files, `psys`
process and the STOP key, `pkey` keyboard, `pclk` clock and tick, `pscr` text screen, `pmou` mouse,
`pfpu` the x87's functions). `csrc/plat/dos32` implements them with `pdos_int`, a software
interrupt from C, and jwasm files for what C cannot do: the interrupt handlers, which only set
bits the core polls, port I/O and the x87. A host or wasm build is another directory beside it.

llrm-llc at -O2 sends a switch's second case value of a shared block past the block's first load:
no stacked case labels where the block starts with a load. Its `LL$COMMAND_LINE` reads its
arguments a slot high, so the command tail comes from `pdos_cmdline`.
