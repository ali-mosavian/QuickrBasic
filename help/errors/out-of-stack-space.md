# Out of stack space
keys: Out of stack space, error 255

Error 255 (reported without a number)

Procedures call each other too deeply.

See also: [SUB](SUB), [FUNCTION](FUNCTION)

## Details

Cause: Procedures call each other too deeply, usually a SUB or
FUNCTION that calls itself without end. It cannot be trapped.

Remedy: Give the recursion a stop condition, or use a loop.

The usual cause is recursion with no stop. ON ERROR cannot trap it.

## Example
    SUB Down (n%)
      CALL Down(n% + 1)   ' never stops: error 255
    END SUB
    SUB Down (n%)
      IF n% < 100 THEN CALL Down(n% + 1)   ' fixed
    END SUB
