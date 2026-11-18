# POKE Statement
keys: POKE

Syntax
    POKE address, byte

Stores byte (0 to 255) at address.

See also [PEEK](PEEK), [DEF SEG](DEF SEG), [VARPTR](VARPTR)

## Details
address is a flat address in the program's own memory such as VARPTR
returns. [DEF SEG](DEF SEG) changes nothing. PC memory (video, BIOS
data) is not mapped.

POKE writes where it is told, with no checks. A wrong address can
corrupt the program or its variables.

## Example
    n% = 0
    POKE VARPTR(n%), 7
    PRINT n%                       ' 7
