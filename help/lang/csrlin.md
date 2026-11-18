# CSRLIN Function
keys: CSRLIN

Syntax
    CSRLIN

Returns the text cursor's current row, counted from 1 at the top of
the screen. Use POS(0) for the column.

## Example
    row = CSRLIN
    PRINT "here"
    LOCATE row, 20: PRINT "same row"

See also [LOCATE](LOCATE), [POS](POS)
