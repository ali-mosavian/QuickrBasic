# PEEK Function
keys: PEEK

Syntax
    PEEK(address)

Returns the byte, 0 to 255, stored at address.

See also [POKE](POKE), [DEF SEG](DEF SEG), [VARPTR](VARPTR)

## Details
QuickrBasic addresses are flat: an address in the program's own
memory, such as VARPTR gives. [DEF SEG](DEF SEG) is accepted for old
programs but changes nothing, so PC memory such as the BIOS area or
&HB800 is not there to read. There is no 64 KB limit.

## Example
    n% = &H1234
    p = VARPTR(n%)
    PRINT PEEK(p); PEEK(p + 1)     ' 52  18: low byte first
