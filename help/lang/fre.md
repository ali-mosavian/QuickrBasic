# FRE Function
keys: FRE

Syntax
    FRE(string$)
    FRE(numeric)

FRE returns free memory in bytes. With a string argument it first
compacts string space and returns what is free there; with a number
it returns free general memory. The argument's value is ignored. 
There are no 64 KB data or stack segments, so FRE reports the whole
of what memory still allows, and the figures are approximate.

See also [DIM](DIM), [CLEAR](CLEAR)

## Details
FRE returns free memory in bytes. With a string argument it first
compacts string space and returns what is free there; with a number
it returns free general memory. The argument's value is ignored. 
There are no 64 KB data or stack segments, so FRE reports the whole
of what memory still allows, and the figures are approximate.

## Example
    PRINT FRE("")     ' free string space, in bytes
    PRINT FRE(0)      ' free general memory, in bytes
