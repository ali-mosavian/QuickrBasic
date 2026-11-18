# Blocks crossed
keys: Blocks crossed

A block ends while another block opened inside it is still open.

See also: [FOR](FOR), [IF](IF), [DO](DO)

## Details

Cause: A line ends a block while another block started inside it is
still open, as a NEXT that comes before the END IF of an IF opened
inside the loop.

Remedy: Blocks end in the reverse order they began. Close the inner
block first.

Blocks nest like parentheses: the one opened last closes first.

## Example
    FOR i% = 1 TO 3
      IF i% > 1 THEN
    NEXT i%              ' error: IF still open
      END IF
    ' fix: END IF before NEXT
    FOR i% = 1 TO 3
      IF i% > 1 THEN PRINT i%
    NEXT i%
