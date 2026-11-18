# VARPTR, VARSEG Functions
keys: VARPTR, VARSEG

Syntax
    VARPTR(variable)
    VARSEG(variable)

VARPTR returns the offset of a variable; VARSEG returns its segment.
Pass both to PEEK, POKE or a CALL ABSOLUTE routine, with DEF SEG set
to the segment.

See also [VARPTR$](VARPTR$), [SADD](SADD), [DEF SEG](DEF SEG),
[PEEK](PEEK), [POKE](POKE)

## Details

VARPTR returns the offset of a variable; VARSEG returns its segment.
Pass both to PEEK, POKE or a CALL ABSOLUTE routine, with DEF SEG set
to the segment.

variable is any variable, an array element or a record element.

Array elements are stored one after another, so the address of the
first element plus the element size reaches the second.

VARPTR of a variable-length string is the address of its descriptor,
not of its text; use SADD for the text.

## Example
    n% = 0
    DEF SEG = VARSEG(n%)
    POKE VARPTR(n%), 65
    DEF SEG
    PRINT n%                            ' 65
