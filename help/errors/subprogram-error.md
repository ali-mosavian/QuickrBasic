# Subprogram error
keys: Subprogram error, error 240

Error 240

A procedure is defined or declared in a way not allowed.

See also: [SUB](SUB), [FUNCTION](FUNCTION), [DECLARE](DECLARE)

## Details

Cause: A SUB or FUNCTION is defined in a way not allowed, such as
inside another procedure, or its DECLARE does not agree with it.

Remedy: Define procedures at module level, and make the DECLARE match
the heading.

Procedures cannot nest, and the DECLARE must match the heading.

## Example
    SUB A
      SUB B              ' error 240: nested
      END SUB
    END SUB
    ' fix: define B at module level
