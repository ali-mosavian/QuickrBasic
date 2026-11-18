# FOR index variable errors
keys: FOR index variable already in use, FOR index variable illegal

The FOR counter is not a usable numeric variable.

See also: [FOR](FOR), [NEXT](NEXT)

## Details

Cause: The [FOR](FOR) counter is not a numeric simple variable, or an
enclosing FOR already uses it.

Remedy: Use a numeric variable such as `i%`, and a different one for
each nested loop.

A nested FOR needs its own counter.

## Example
    FOR i% = 1 TO 2
      FOR i% = 1 TO 2    ' error: i% in use
      NEXT
    NEXT
    FOR i% = 1 TO 2
      FOR j% = 1 TO 2    ' fixed
      NEXT
    NEXT
