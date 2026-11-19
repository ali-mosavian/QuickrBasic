# RUN Statement
keys: RUN

Syntax
    RUN [{line | file$}]

Restarts the program, or loads and starts another.

See also [CHAIN](CHAIN), [SHELL](SHELL), [CLEAR](CLEAR)

## Details

Restarts the program, or loads and starts another.

  RUN           start the current program again from its first line
  RUN line      start at line
  RUN file$     load the BASIC source file file$ (.BAS if no
                extension) and run it

RUN closes all files and clears every variable first. Use
[CHAIN](CHAIN) to start another program while keeping COMMON
variables.

A file that cannot be found is "File not found". A program started by
RUN keeps no variables; pass values in a file, or use CHAIN with
COMMON.

## Example
    RUN "MENU.BAS"
