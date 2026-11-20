# IMPORT and FROM Statements
keys: IMPORT, FROM, packages, BASICLIB, module search

Syntax
    IMPORT module [AS name] [, module [AS name]]...
    FROM module IMPORT name[()] [AS name] [, ...]
    FROM module IMPORT *
    FROM .[.]...[module] IMPORT ...

IMPORT and FROM load other files of BASIC as modules of the program.
QuickrBasic adds them to QBasic.

See also [Procedures and Modules](procedures), [$INCLUDE](metacommands)

## Details
A module is a file: `gfx` is `gfx.bas`. A dotted name is a package
path: `lib.gfx` is `lib/gfx.bas`, and a package is a folder with no
other file needed. `FROM lib IMPORT gfx` loads `lib/gfx.bas` too.

A module is looked for in the main program's folder, then in each
folder of the BASICLIB environment variable, separated by `;`, then in
`lib` beside the interpreter. The first found is used.

FROM's leading dots start at the importing file's folder instead: `.`
is that folder, `..` its parent, and so on.

Each module is loaded once, after the program's own lines, however
many lines import it, cycles included. One that is not found is a
[File not found](File not found) error at the importing line. An error
in a module is reported at its own file and line.

Each file's variables, its DIM SHARED ones too, its CONSTs, labels
and line numbers are its own. DEFtype, OPTION BASE and OPTION EXPLICIT
start again at its first line.

For now, as in QuickBASIC's programs of several modules, SUBs,
FUNCTIONs, DEF FNs and TYPEs are the whole program's, under their plain
names, and a module's top-level code does not run.

IMPORT and FROM are keywords only at the start of a line's first
statement, outside SUBs and FUNCTIONs. A variable called import still
works; a SUB called import is called as `CALL import(x)`.

## Example
    ' gfx.bas
    SUB plot (x, y)
      PSET (x, y)
    END SUB

    ' main.bas
    FROM gfx IMPORT plot
    SCREEN 13
    plot 10, 10
