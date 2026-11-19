# FUNCTION Statement
keys: FUNCTION, END FUNCTION

Syntax
    FUNCTION name[suffix][(parameterlist)] [AS type] [STATIC]
      statements
      name = expression
    END FUNCTION

A FUNCTION is a procedure that returns a value, set by assigning to
the function's own name.

See also [SUB](SUB), [DECLARE](DECLARE), [DEF FN](DEF FN),
[SHARED](SHARED), [STATIC](STATIC)

## Details
A FUNCTION is a procedure that returns a value. The type of the
result is given by the suffix, AS type, or DEFtype. Set the result by
assigning to the function's own name; if you never do, it is 0 or "".
Leave early with [EXIT](EXIT) FUNCTION.

QuickrBasic adds AS type, written without a suffix. It names a number
type, STRING, or a [TYPE](TYPE). A record result is set field by
field (`Mk.x = 1`) or whole (`Mk = v`), starts empty at each call,
and is used where a record is: `v = Mk(1)`, `Show Mk(1)`,
`PRINT Mk(1).x`.

Parameters are passed by reference: changing one changes the
caller's variable. Pass an expression, or wrap a variable in
parentheses, to pass a copy. Variables are local, and recreated at
each call, unless STATIC is written or the names are declared
SHARED. A FUNCTION may call itself, to any depth memory allows.
Define it in a view of its own; the IDE adds the [DECLARE](DECLARE).

## Example
    DECLARE FUNCTION Fact# (n AS INTEGER)
    PRINT Fact#(10)          ' 3628800
    FUNCTION Fact# (n AS INTEGER)
      IF n < 2 THEN Fact# = 1 ELSE Fact# = n * Fact#(n - 1)
    END FUNCTION
