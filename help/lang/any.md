# ANY Keyword
keys: ANY

Syntax
    DECLARE SUB name (param AS ANY)

In a DECLARE, a parameter AS ANY accepts an argument of whatever
type the caller passes, a user-defined TYPE included. The
procedure's own SUB or FUNCTION line must still name a real type;
ANY exists only in DECLARE.

See also [DECLARE](DECLARE), [SUB](SUB), [BYVAL](BYVAL)

## Details
In a DECLARE, a parameter AS ANY accepts an argument of whatever
type the caller passes, a user-defined TYPE included. The
procedure's own SUB or FUNCTION line must still name a real type;
ANY exists only in DECLARE.

The compiler checks arguments against the declared types, so a call
such as Foo x with x of a type the SUB does not take is still
rejected as Parameter type mismatch.

## Example
    DECLARE SUB Zero (n AS ANY)
    x% = 9: Zero x%
    PRINT x%                 ' 0
    SUB Zero (n AS INTEGER)
        n = 0
    END SUB
