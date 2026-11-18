# PRINT Statement
keys: PRINT, ?

Syntax
    PRINT [expressionlist][{; | ,}]
    ? [expressionlist]

Writes the values to the screen at the text cursor, then moves to the
next line. ? is a shortcut for PRINT.

  ;          next item follows at once. Numbers print with a leading
             space for the sign and a trailing space.
  ,          next item starts at the next 14-column zone
  TAB(n)     move to column n
  SPC(n)     print n spaces
  (nothing)  a PRINT alone prints a blank line

A separator at the end of the list keeps the cursor on the line, so
the next PRINT continues there. Text past the screen's width wraps.

To format numbers, see [PRINT USING](PRINT USING). To print on paper,
[LPRINT](LPRINT). To a file, [PRINT #](PRINT #).

## Example
    PRINT "Total:"; 42            ' Total: 42
    PRINT "a", "b"                ' b in column 15
    PRINT TAB(10); "indented"
    PRINT 1; 2;
    PRINT 3                       ' on one line: " 1  2  3"

See also [PRINT USING](PRINT USING), [PRINT #](PRINT #),
[LOCATE](LOCATE), [WIDTH](WIDTH)
