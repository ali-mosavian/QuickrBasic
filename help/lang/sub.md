# SUB Statement
keys: SUB, END SUB

Syntax
    SUB name [(parameters)] [STATIC]
        statements
    END SUB

Defines a procedure that is run with CALL name or just name.

See also [CALL](CALL), [DECLARE](DECLARE), [FUNCTION](FUNCTION),
[EXIT](EXIT), [SHARED](SHARED)

## Details

Defines a procedure that is run with CALL name or just name.

parameters is a list of  var [()] [AS type]. Arguments are passed by
reference: the SUB changes the caller's variable. Put an argument in
parentheses to pass a copy instead. An empty () marks an array.

STATIC keeps all of the SUB's variables between calls. Variables are
otherwise local; see [SHARED](SHARED) to reach module-level ones.
Leave early with EXIT SUB. A SUB can call itself.

The IDE shows each SUB in its own view; F2 lists them. Declare a SUB
with DECLARE if it is called before it is defined in the file.

Call syntax: CALL Name (a, b) or Name a, b, with no parentheses in the
second form. A SUB cannot be used in an expression; use FUNCTION.

## Example
    DECLARE SUB Swap2 (a%, b%)
    x% = 1: y% = 2
    Swap2 x%, y%
    PRINT x%; y%                    ' 2  1
    SUB Swap2 (a%, b%)
        t% = a%: a% = b%: b% = t%
    END SUB
