# DEF FN Statement
keys: DEF FN, FN, END DEF

Syntax
    DEF FNname[(parameterlist)] = expression
    DEF FNname[(parameterlist)]
      ...
      FNname = expression
    END DEF

DEF FN defines a function of your own, called as FNname(args). The
one-line form holds a single expression. The block form may hold any
statements; assign the result to FNname, and leave early with
EXIT DEF.

See also [FUNCTION](FUNCTION), [EXIT](EXIT)

## Details
DEF FN defines a function of your own, called as FNname(args). The
one-line form holds a single expression. The block form may hold any
statements; assign the result to FNname, and leave early with
EXIT DEF.

Names follow variable rules and a type suffix gives the result type.
Parameters are local to the definition. Other variables of the
module are shared with it, unless a parameter has the same name.
A DEF FN must be written before the first call, and needs no
DECLARE. For anything larger, prefer a [FUNCTION](FUNCTION).

## Example
    DEF FNarea (r) = 3.14159 * r * r
    PRINT FNarea(2)          ' 12.56636
