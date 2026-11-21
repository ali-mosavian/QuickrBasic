# IMPORT and FROM Statements
keys: IMPORT, FROM, PRIVATE, PUBLIC, DIM PUBLIC, packages, BASICLIB, module search

Syntax
    IMPORT module [AS name] [, module [AS name]]...
    FROM module IMPORT name[()] [AS name] [, ...]
    FROM module IMPORT *
    FROM .[.]...[module] IMPORT ...
    PRIVATE SUB | FUNCTION | CONST | TYPE ...
    DIM PUBLIC variable[()] [AS type] [, ...]

IMPORT and FROM load other files of BASIC as modules of the program.
QuickrBasic adds them to QBasic.

See also [Procedures and Modules](procedures), [$INCLUDE](metacommands),
[examples](import examples)

## Details
A module is a file: `gfx` is `gfx.bas`. A dotted name is a package
path: `lib.gfx` is `lib/gfx.bas`, and a package is a folder with no
other file needed. `FROM lib IMPORT gfx` loads `lib/gfx.bas` too.

A module can be native, `gfx.qbx`: compiled code and the BASIC
interface its procedures are DECLAREd in. It is imported and used as
a `.bas` is; `gfx.bas` is found first when both are there. One with no
code for this machine is an Advanced feature unavailable error.

A module is looked for in the main program's folder, then in each
folder of the BASICLIB environment variable, separated by `;`, then in
`lib` beside the interpreter. The first found is used.

FROM's leading dots start at the importing file's folder instead: `.`
is that folder, `..` its parent, and so on.

Each module is loaded once, after the program's own lines, however
many lines import it, cycles included. Its declarations go before
those of the file importing it, so its TYPEs can be used there. One
that is not found is a [File not found](File not found) error at the
importing line. An error in a module is reported at its own file and
line.

Each file's names are its own: its variables, CONSTs, SUBs,
FUNCTIONs, TYPEs, labels and line numbers. DEFtype, OPTION BASE and
OPTION EXPLICIT start again at its first line.

## Running
A module's top-level code runs the first time a line imports it,
then the program goes on after that line. Later imports run nothing.
In a cycle a module may be used before its code has finished. END and
SYSTEM in it end the program; STOP breaks.

ON ERROR GOTO in a file covers the errors in that file, its SUBs
included. An error in a file with no handler goes to the handler of
the nearest caller's file that has one, as if it happened at the call
or IMPORT there: RESUME NEXT goes on after it. READ and RESTORE use
the file's own DATA.

## Names
A module exports its SUBs, FUNCTIONs, CONSTs and TYPEs, but those
declared PRIVATE, and its DIM PUBLIC variables. DIM PUBLIC is DIM
SHARED, exported. A name a module imports is not exported again.

`IMPORT gfx` binds the name `gfx`, and the module's names are used
through it: `gfx.plot`, `gfx.pt`, `gfx.origin.x`. `IMPORT lib.gfx`
binds `lib.gfx`; AS binds another name instead. A name the module does
not export is [Name not exported](Name not exported) where used.

`FROM gfx IMPORT plot, pt AS spot` binds the names listed; `grid()`
names an array, and a name may have its type's suffix: `total%`. A
name gfx does not export is Name not exported at the FROM line.
`FROM gfx IMPORT *` binds each SUB, FUNCTION, CONST and TYPE gfx
exports, never a variable.

An imported variable is the exporter's own, seen in the whole file,
SUBs too, with the exporter's type: if `total` is an INTEGER there,
`total%` is the same variable and `total!` is Duplicate definition.

A listed name or a module's name that the file also declares (DIM,
CONST, SUB, FUNCTION, TYPE), or binds twice, is [Duplicate definition](Duplicate definition);
AS renames one. A name * binds yields to the file's own. One two *
imports bind is Duplicate definition where used: write `gfx.plot`.

## COMMON
COMMON joins variables across a program's files by position, as QB
linked its modules: the first variable of each file's COMMON is the
same variable, then the second, and so on. A named block,
`COMMON /name/`, joins by its name, apart from the unnamed one. Each
position's type must match, a TYPE laid out the same included; else
[Type mismatch](Type mismatch).

CHAIN passes each such variable once. The new program loads its own
modules; the old one's are dropped.

## In the Editor
Opening a program opens the modules it imports too. Each module's
lines are edited where they are: F2 lists each file with its
procedures under it, and the window's title names the file. A run
uses the lines as edited. Typing an IMPORT loads its module.

Save writes the program and each module changed. Save As names the
program only. A module no IMPORT names any more closes at the next
run, once its changes are saved.

## Keywords
IMPORT and FROM are keywords only at the start of a line's first
statement, outside SUBs and FUNCTIONs. PRIVATE is one only before
SUB, FUNCTION, CONST or TYPE, PUBLIC only after DIM. A variable called
import still works; a SUB called import is called as `CALL import(x)`.

## Example
    ' gfx.bas
    CONST maxw = 319
    DIM PUBLIC plotted
    SUB plot (x, y)
      PSET (clip(x), y)
      plotted = plotted + 1
    END SUB
    PRIVATE FUNCTION clip (x)
      clip = x: IF x > maxw THEN clip = maxw
    END FUNCTION

    ' main.bas
    IMPORT gfx
    FROM gfx IMPORT plot
    SCREEN 13
    plot 10, 10
    plot 400, 20
    PRINT gfx.plotted; gfx.maxw
