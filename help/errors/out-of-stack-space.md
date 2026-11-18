# Out of stack space
keys: Out of stack space, error 255

Error 255 (reported without a number)

## Cause

Procedures call each other too deeply, usually a SUB or FUNCTION that
calls itself without end. It cannot be trapped.

## Remedy

Give the recursion a stop condition, or use a loop.

See also: [SUB](SUB), [FUNCTION](FUNCTION)
