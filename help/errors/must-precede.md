# COMMON and DECLARE must precede executable statements
keys: COMMON and DECLARE must precede executable statements, Must precede executable statements, Must be first statement on the line

## Cause

A COMMON, DECLARE or similar statement comes after a statement that
does something, or must stand first on its line.

## Remedy

Move it to the top of the module or procedure, or onto a line of its
own.

See also: [COMMON](COMMON), [DECLARE](DECLARE)
