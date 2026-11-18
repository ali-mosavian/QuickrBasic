# Illegal inside / outside of SUB or FUNCTION
keys: Illegal inside SUB or FUNCTION, Illegal outside of SUB or FUNCTION

A statement is at the wrong level.

See also: [SUB](SUB), [FUNCTION](FUNCTION), [EXIT](EXIT)

## Details

Cause: A statement was put where it does not belong: module-level only
(such as TYPE or SHARED lists) inside a procedure, or procedure-only
(such as EXIT SUB) outside one.

Remedy: Move it to the right level.

Module-level only: TYPE, DECLARE, COMMON. Procedure-only: EXIT SUB,
EXIT FUNCTION.

## Example
    SUB T
      TYPE P             ' error: TYPE inside a SUB
      END TYPE
    END SUB
    EXIT SUB             ' error: outside a SUB
