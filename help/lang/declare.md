# DECLARE Statement
keys: DECLARE

Syntax
    DECLARE SUB name[(parameterlist)]
    DECLARE FUNCTION name[suffix][(parameterlist)]

DECLARE tells the compiler a SUB or FUNCTION's name and parameter
types before its first use, so that calls can be checked and, where
no CALL is written, recognised. Each parameter is written like a
variable, with a type suffix or AS type; an array is name().

The IDE writes the DECLARE lines itself when a procedure is created
or its header is changed, so they rarely need typing. DECLARE must
come before any procedure in the module and cannot be used inside
one.

A call whose arguments do not match the declaration is a Parameter
type mismatch.

## Example
    DECLARE FUNCTION Twice% (n AS INTEGER)
    PRINT Twice%(21)
    FUNCTION Twice% (n AS INTEGER)
      Twice% = n * 2
    END FUNCTION

See also [SUB](SUB), [FUNCTION](FUNCTION), [CALL](CALL)
