# Expressions
keys: expressions, expression, precedence, order of operations, operator precedence

An expression is a value or a combination of values, variables,
function calls and [operators](operators).

## Order of evaluation
Higher rows are done first. Operators on one row go left to right.

```
 1  ( )  function calls
 2  ^
 3  - (negation)
 4  *  /
 5  \
 6  MOD
 7  +  -
 8  =  <>  <  >  <=  >=
 9  NOT
10  AND
11  OR
12  XOR
13  EQV
14  IMP
```
Parentheses override the order: `2 + 3 * 4` is 14, `(2 + 3) * 4` is
20. Negation binds tighter than `*` but looser than `^`:
`-2 ^ 2` is -4.

## Mixed types
When a number of one type meets another, both become the wider type:
INTEGER with SINGLE gives SINGLE, anything with DOUBLE gives DOUBLE.
`/` and `^` always give a floating-point result. A number and a
string cannot be mixed: [Type mismatch](Type mismatch).

## Numeric and string
A numeric expression gives a number, a string expression a string. A
relational expression compares two of the same kind and gives -1 or 0.

## Example
```
IF x > 0 AND x < 10 OR flag THEN PRINT "in range"
```
means `((x > 0) AND (x < 10)) OR flag`.

See also [Operators](operators), [Data types](data types)
