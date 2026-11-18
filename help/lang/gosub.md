# GOSUB Statement
keys: GOSUB, GOSUB...RETURN

Syntax
    GOSUB label
    ...
    label:
      statements
      RETURN [label2]

GOSUB jumps to a labelled subroutine in the same procedure; RETURN
brings execution back to the statement after it.

See also [GOTO](GOTO), [ON GOSUB](ON GOTO), [SUB](SUB)

## Details
GOSUB jumps to a labelled subroutine inside the same module or SUB
and RETURN brings execution back to the statement after it. GOSUBs
may nest as deep as memory allows. A RETURN without a GOSUB is an
error (RETURN without GOSUB). RETURN label returns to another place
in the same procedure instead, and should be avoided.

GOSUB and its label must be in the same procedure; for reusable code
with parameters and local variables, use a [SUB](SUB). Place the
subroutine where it will not run by accident, usually after END.

## Example
    GOSUB Hello
    PRINT "back"            ' back
    END
    Hello:
      PRINT "hi"           ' hi
      RETURN
