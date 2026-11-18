# Illegal in direct mode
keys: Illegal in direct mode, error 12

Error 12

The statement only works inside a program.

See also: [survival](survival)

## Details

Cause: A statement that only makes sense inside a program was typed in
the Immediate window.

Remedy: Put it in the program, or use a statement valid in the
Immediate window.

Examples are SUB, FUNCTION, DECLARE and DEF FN typed in the Immediate
window.

## Example
    ' In the Immediate window:
    '   DECLARE SUB x   -> error 12
    '   PRINT 2 + 2     -> 4
