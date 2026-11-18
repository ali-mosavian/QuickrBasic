# Formal parameter errors
keys: Formal parameter specification illegal, ~Formal parameter specification illegal /BYVAL, ~Formal parameter specification illegal /SEG, ~Formal parameter specification illegal /(), Formal parameters not unique

## Cause

A parameter list is not valid: two parameters share a name, or it uses
a form the selected dialect lacks (BYVAL, SEG, an empty `()`).

## Remedy

Rename duplicates. Use plain by-reference parameters: `SUB name (a AS
INTEGER)`. See the dialect option in your project.

See also: [SUB](SUB), [FUNCTION](FUNCTION), [DECLARE](DECLARE)
