# LOCATE Statement
keys: LOCATE

Syntax
    LOCATE [row] [, [column] [, [visible] [, [start] [, stop]]]]

Moves the text cursor and sets its look. Leave out a part to keep it.

    row, column   1-based position; outside the screen or window
                  is an Illegal function call
    visible       0 hides the cursor, 1 shows it
    start, stop   first and last scan line of the cursor block,
                  0 to 31

Inside a [VIEW PRINT](VIEW PRINT) window, row counts from the top of
the screen, not of the window, but must lie inside it. In a graphics
mode the cursor is not drawn but PRINT still goes where it points.

## Example
    CLS
    LOCATE 12, 30
    PRINT "Centered-ish"
    LOCATE , , 1, 0, 7          ' a full block cursor

See also [PRINT](PRINT), [CSRLIN](CSRLIN), [POS](POS), [WIDTH](WIDTH)
