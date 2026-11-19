# CALL Statement
keys: CALL

Syntax
    CALL name [(argumentlist)]
    name [argumentlist]

Runs the SUB called name, then continues with the next statement.
Without CALL, leave out the parentheses: Show a, b.

See also [SUB](SUB), [FUNCTION](FUNCTION), [DECLARE](DECLARE),
[BYVAL](BYVAL)

## Details
Runs the SUB called name, then continues with the next statement.
Without CALL, leave out the parentheses: Show a, b. With CALL they
are required when there are arguments: CALL Show(a, b).

Each argument is matched to a parameter by position and must have
the type the parameter was declared with. A variable is passed by
reference, so the SUB can change it; an expression or a variable in
extra parentheses is passed by value. A wrong count is an error,
and so is a type that differs (Parameter type mismatch). Arrays
are passed as name().

A SUB must be DECLAREd before a call without CALL can use it. To
get a value back, use a FUNCTION, which is called in an expression
without CALL.

CALL ABSOLUTE, CALL INTERRUPT and CALLS reach machine code and
DOS, and are not implemented.

## Example
    DECLARE SUB Greet (who$)
    CALL Greet("world")      ' Hello, world
    SUB Greet (who$)
        PRINT "Hello, "; who$
    END SUB
