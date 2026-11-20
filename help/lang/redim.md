# REDIM Statement
keys: REDIM, PRESERVE

Syntax
    REDIM [SHARED] array(subscripts) [AS type][, ...]
    REDIM PRESERVE array(subscripts) [AS type]

Sets a dynamic array's size anew, at run time. It frees the old
elements first, so they are lost and the new ones start at zero or
"". An array becomes dynamic by being declared with variables as
bounds, with DIM or REDIM, or in a $DYNAMIC part of the program.

See also [DIM](DIM), [ERASE](ERASE), [LBOUND](LBOUND), [UBOUND](UBOUND)

## Details

Sets a dynamic array's size anew, at run time. It frees the old
elements first, so they are lost and the new ones start at zero or
"". An array becomes dynamic by being declared with variables as
bounds, with DIM or REDIM, or in a $DYNAMIC part of the program.

With PRESERVE the old values are kept. Only the last dimension may
change then, and the number of dimensions stays.

A record's dynamic array element is REDIMed as v.pts(1 TO n), in
QuickrBasic; see [TYPE](TYPE). In a FUNCTION AS type(), REDIM of the
FUNCTION's name gives its result array its bounds.

The type and the number of dimensions cannot change: "Array already
dimensioned". A static array cannot be REDIMed at all; ERASE it first
or declare it dynamic.

QuickrBasic has no 64 KB limit; arrays grow until memory is out
("Out of memory").

## Example
    n = 3
    REDIM a(n)
    a(2) = 9
    REDIM PRESERVE a(10)
    PRINT a(2)                     ' 9
