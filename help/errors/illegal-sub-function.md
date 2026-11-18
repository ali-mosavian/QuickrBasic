# Illegal inside / outside of SUB or FUNCTION
keys: Illegal inside SUB or FUNCTION, Illegal outside of SUB or FUNCTION

## Cause

A statement was put where it does not belong: module-level only (such
as TYPE or SHARED lists) inside a procedure, or procedure-only (such
as EXIT SUB) outside one.

## Remedy

Move it to the right level.

See also: [SUB](SUB), [FUNCTION](FUNCTION), [EXIT](EXIT)
