# POS Function
keys: POS

Syntax
    POS(0)

Returns the text cursor's column, 1 up to the screen width.

See also [CSRLIN](CSRLIN), [LOCATE](LOCATE), [PRINT](PRINT)

## Details
The argument is ignored, but must be there. [CSRLIN](CSRLIN) gives
the row.

## Example
    PRINT "abc";
    PRINT POS(0)                   ' 4
