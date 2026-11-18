# FOR index variable errors
keys: FOR index variable already in use, FOR index variable illegal

## Cause

The [FOR](FOR) counter is not a numeric simple variable, or an
enclosing FOR already uses it.

## Remedy

Use a numeric variable such as `i%`, and a different one for each
nested loop.

See also: [FOR](FOR), [NEXT](NEXT)
