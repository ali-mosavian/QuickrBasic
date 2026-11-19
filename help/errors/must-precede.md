# COMMON and DECLARE must precede executable statements
keys: COMMON and DECLARE must precede executable statements, Must precede executable statements, Must be first statement on the line

COMMON or DECLARE follows a statement that does something.

See also: [COMMON](COMMON), [DECLARE](DECLARE)

## Details

Cause: A COMMON, DECLARE or similar statement comes after a statement
that does something, or must stand first on its line.

Remedy: Move it to the top of the module or procedure, or onto a line
of its own.

They must stand before all executable statements of the module.

## Example
    PRINT "x"
    DECLARE SUB T        ' error: too late
    ' fix: DECLARE first
    DECLARE SUB T
    PRINT "x"
