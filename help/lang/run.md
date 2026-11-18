# RUN Statement
keys: RUN

Syntax
    RUN [{line | file$}]

Restarts the program, or loads and starts another.

  RUN           start the current program again from its first line
  RUN line      start at line
  RUN file$     load the BASIC source file file$ (.BAS if no
                extension) and run it

RUN closes all files and clears every variable first. Use
[CHAIN](CHAIN) to start another program while keeping COMMON
variables.

## Example
    RUN "MENU.BAS"

See also [CHAIN](CHAIN), [SHELL](SHELL), [CLEAR](CLEAR)
