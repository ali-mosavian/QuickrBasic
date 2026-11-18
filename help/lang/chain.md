# CHAIN Statement
keys: CHAIN

Syntax
    CHAIN file$

Stops the current program and runs the program in file$, which
replaces it. Variables named in COMMON, in the same order and types
in both programs, keep their values; everything else starts empty.

See also [COMMON](COMMON), [RUN](RUN), [CLEAR](CLEAR)

## Details
Stops the current program and runs the program in file$, which
replaces it. Variables named in COMMON, in the same order and types
in both programs, keep their values; everything else starts empty.

A file that cannot be found gives File not found.

## Example
    ' MAIN.BAS
    COMMON total
    total = 42
    CHAIN "NEXT.BAS"
    ' NEXT.BAS
    COMMON total
    PRINT total          ' 42
