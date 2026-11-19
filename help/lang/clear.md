# CLEAR Statement
keys: CLEAR

Syntax
    CLEAR

Resets the program's variables: numbers become 0, strings become
empty, and arrays are released. All open files are closed. Variables
shared through COMMON are cleared too.

See also [ERASE](ERASE), [CLOSE](CLOSE), [COMMON](COMMON)

## Details
Resets the program's variables: numbers become 0, strings become
empty, and arrays are released. All open files are closed. Variables
shared through COMMON are cleared too.

CLEAR takes no arguments. QBasic's options for string and stack
size are not needed, as memory is not limited to 64 KB; CLEAR , , n
is a syntax error.

## Example
    x = 5
    CLEAR
    PRINT x          ' 0
