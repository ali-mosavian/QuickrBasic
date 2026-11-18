# SHARED, STATIC Statements
keys: SHARED, STATIC

Syntax
    SHARED var[()] [AS type] [, var[()] [AS type]]...
    STATIC var[()] [AS type] [, var[()] [AS type]]...
    DIM SHARED var ...

SHARED, inside a SUB or FUNCTION, makes the listed module-level
variables visible to it. Without it a procedure sees only its own
variables. DIM SHARED at module level shares a variable with every
procedure at once.

STATIC makes a procedure's variable keep its value between calls.
SUB name STATIC or FUNCTION name STATIC does this for all of the
procedure's variables.

A variable named in either statement needs the same type as its
module-level twin (SHARED) and may not be declared twice. An array
is named with empty parentheses.

## Example
    DIM total%
    SUB Add (n%)
        SHARED total%
        STATIC calls%
        calls% = calls% + 1
        total% = total% + n%
    END SUB

See also [DIM](DIM), [SUB](SUB), [FUNCTION](FUNCTION),
[COMMON](COMMON)
