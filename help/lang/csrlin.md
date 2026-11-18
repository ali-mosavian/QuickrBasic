# CSRLIN Function
keys: CSRLIN

Syntax
    CSRLIN

Returns the text cursor's current row, counted from 1 at the top of
the screen. Use POS(0) for the column.

See also [LOCATE](LOCATE), [POS](POS)

## Details
Returns the text cursor's current row, counted from 1 at the top of
the screen. Use POS(0) for the column.

CSRLIN counts the row in the current screen mode's text grid, so
the top row is 1 in every mode.

## Example
    row = CSRLIN
    PRINT "here"
    LOCATE row, 20: PRINT "same row"
