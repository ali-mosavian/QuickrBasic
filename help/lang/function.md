# FUNCTION Statement
keys: FUNCTION, END FUNCTION

Syntax
    FUNCTION name[suffix][(parameterlist)] [STATIC]
      statements
      name = expression
    END FUNCTION

A FUNCTION is a procedure that returns a value, set by assigning to
the function's own name.

See also [SUB](SUB), [DECLARE](DECLARE), [DEF FN](DEF FN),
[SHARED](SHARED), [STATIC](STATIC)

## Details
A FUNCTION is a procedure that returns a value. The type of the
result is given by the suffix, or by DEFtype. Set the result by
assigning to the function's own name; if you never do, it is 0 or "".
Leave early with [EXIT](EXIT) FUNCTION.

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
