# Procedures and Modules
keys: procedures, procedure, modules, module, main module, scope, local variables, parameters

A program is a main module plus any number of procedures: [SUB](SUB)s
that do something and [FUNCTION](FUNCTION)s that return a value. Each
procedure is shown in a window of its own; the SUBs command on the
View menu lists them.

See also [Data types](data types), [Metacommands](metacommands)

## Details
DECLARE
[DECLARE](DECLARE) names a procedure and its parameters before it is
called, so that a call is checked and a call with arguments is not
taken for an array.

Calling
- A SUB is called by name, `Greet "x"`, or with [CALL](CALL) and
  parentheses: `CALL Greet("x")`.
- A FUNCTION is used in an expression: `Twice%(21)`.
- A wrong number of arguments is
  [Argument-count mismatch](Argument-count mismatch); a wrong type is
  [Parameter type mismatch](Parameter type mismatch).

Parameters
Arguments are passed by reference: the procedure changes the caller's
variable. An expression, a constant, or a variable in parentheses is
passed as a copy. An array is passed as `a()`.

Scope
- A variable made in a procedure is local to it, and is cleared on each
  call, unless the procedure is declared STATIC.
- A variable in the main module belongs to that module only.
- [DIM SHARED](DIM) in the main module makes a variable visible in every
  procedure of the module.
- [SHARED](SHARED) inside a procedure names the module-level variables
  it uses.
- [COMMON](COMMON) passes variables between programs run with CHAIN.
- A [CONST](CONST) in the main module is known to every procedure.

Recursion
A procedure may call itself. The depth is limited by the stack:
[Out of stack space](Out of stack space).

Leaving
[EXIT](EXIT) SUB and EXIT FUNCTION leave early. A function returns the
value last assigned to its own name.

DEF FN
[DEF FN](DEF FN) defines a one-line function for use in the module.

## Example
    DECLARE SUB Greet (name$)
    DECLARE FUNCTION Twice% (n%)

    Greet "World"
    PRINT Twice%(21)

    SUB Greet (name$)
        PRINT "Hello, "; name$
    END SUB

    FUNCTION Twice% (n%)
        Twice% = n% * 2
    END FUNCTION
    ' Hello, World
    ' 42
